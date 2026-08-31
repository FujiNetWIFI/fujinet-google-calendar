/*
 * FujiNet transport for the GCAL: network protocol adapter.
 *
 * Device specs, all of which the adapter's own header documents:
 *
 *   N:GCAL://                                aux1 6 (DIR)  aux2 80  calendars
 *   N:GCAL://<sel>/DAY/2026-08-28            aux1 6 (DIR)  aux2 80  listing
 *   N:GCAL://<sel>/DAY/2026-08-28/2          aux1 4 (READ) aux2 0   detail
 *
 * Three things about that are easy to get wrong and expensive to debug:
 *
 *   - The scheme ends at "//" with no third slash. Each view literal carries
 *     its own leading '/', so folding one in produces "GCAL:////DAY/..." --
 *     which happens to parse with an empty selector and is wrong for any other.
 *
 *   - /N goes in the path, before the query string, and the AGENDA query has
 *     to be repeated on the detail open. parse_devicespec() treats everything
 *     after the first '?' as query, and the adapter numbers events within the
 *     ?count=/&days= window, so dropping it addresses a different event.
 *
 *   - The selector must never be "*". util_devicespec_fix_for_parsing()
 *     rewrites it to an embedded NUL on any non-DIRECTORY open, which would
 *     corrupt the detail fetch of the very event just listed.
 */

#include <string.h>

#include <fujinet-network.h>
#include <fujinet-fuji.h>       /* fn_default_timeout lives here, not in -network */

#include "gcal.h"

#ifndef GC_FAKE_DATA
static char          url[112];
static unsigned char rxbuf[GC_RXBUF];

/* uint16_t, not unsigned int: the types are the same width on every
   toolchain here, but Watcom is the one that treats the pointers as
   distinct and network_status() takes a uint16_t*. */
static uint16_t      st_bw;
static unsigned char st_conn;
static unsigned char st_err;

/* ------------------------------------------------------------------ */
/* Device spec composition                                             */
/* ------------------------------------------------------------------ */

static const char *view_lit(unsigned char view)
{
    switch (view) {
    case VIEW_DAY:   return "/DAY";
    case VIEW_WEEK:  return "/WEEK";
    case VIEW_MONTH: return "/MONTH";
    default:         return "/AGENDA";
    }
}

/*
 * ?count= is set to MAX_EVENTS: asking for more than we can store is transfer
 * we throw away, asking for fewer truncates the agenda for no reason.
 */
static void put_agenda_query(void)
{
    char n[6];
    unsigned char i = 0, j = 0;
    unsigned int  v = MAX_EVENTS;

    strcat(url, "?count=");
    do {
        n[i++] = (char) ('0' + (v % 10));
        v /= 10;
    } while (v);
    while (i)
        n[j++] = n[--i];
    n[j] = '\0';
    strcat(url, n);
    strcat(url, "&days=90");
}

static void build_url(unsigned char view, const char *evnum)
{
    char iso[11];

    strcpy(url, "N:GCAL://");
    if (gc_cal[0] && gc_cal[0] != '*')
        strcat(url, gc_cal);

    strcat(url, view_lit(view));

    strcat(url, "/");
    date_iso(iso, cur_y, cur_mo, cur_d);
    strcat(url, iso);

    if (evnum) {
        strcat(url, "/");
        strcat(url, evnum);
    }

    if (view == VIEW_AGENDA)
        put_agenda_query();
}

/* ------------------------------------------------------------------ */
/* Status handling                                                     */
/* ------------------------------------------------------------------ */

/* Ask the device where it stands, returning the NDEV status byte, or
   GC_NOREPLY when the status call itself failed. */
static unsigned char probe(void)
{
    st_bw = 0;
    st_conn = 0;
    st_err = 0;

    if (network_status(url, &st_bw, &st_conn, &st_err) != FN_ERR_OK) {
        gc_dev_ecode = fn_device_error;
        return GC_NOREPLY;
    }

    return st_err;
}

/*
 * Is this status byte a success?
 *
 * NetworkProtocolCalendar::status() reports END_OF_FILE the moment its buffer
 * drains, so *every* complete fetch ends on 136 -- treating anything but
 * SUCCESS as a failure would abort the last read of every listing. Over SIO a
 * healthy channel also reports 0 rather than 1, so all three are success and
 * every value that means something (165, 167, 170, 210, 212) is distinct from
 * all of them.
 */
static unsigned char st_ok(unsigned char code)
{
    return (unsigned char) (code == 0 || code == GC_OK || code == GC_EOF);
}

/*
 * Why a failed open is not followed by another network_status(): the Atari bus
 * layer has already done it. bus_status.s sees the device error (144),
 * re-queries to pull the extended information, leaves the protocol's own
 * status byte in fn_network_error, and only then reports the failure. Asking
 * again would mean querying a channel that never opened.
 *
 * Anything other than 144 -- 138 timeout, 139 NAK, 143 checksum -- means the
 * device never gave us a usable reply at all, which is the timeout case.
 *
 * The INT F5 bus is different: its layer never writes fn_network_error at
 * all, so the Atari reading above would report every failed open as a
 * timeout -- including the 212 "authorize Google in the Web UI" a first-time
 * user is guaranteed to hit. The fix is to ask, once. The channel is still
 * addressable because network_open set the unit before issuing the control
 * command, and network_status needs nothing else. (The apple2enh build
 * likely shares this gap -- widening the gate needs an Apple regression
 * capture, so it keeps the Atari branch for now.)
 */
static unsigned char open_error(void)
{
    /* Capture the raw device code now: the network_close() on the way out of
       the failure path issues another SIO command and overwrites it. */
    gc_dev_ecode = fn_device_error;

#if defined(__MSDOS__)
    {
        unsigned char dev  = gc_dev_ecode;
        unsigned char code = probe();

        /* probe() overwrites gc_dev_ecode when the status call itself fails.
           The open's code is the one worth reporting, so put it back. */
        gc_dev_ecode = dev;

        /* GC_NOREPLY means the status call failed too, which on this bus is
           the only thing that really is "no reply". */
        return (code == GC_NOREPLY) ? 0 : code;
    }
#else
    if (fn_device_error == 144)
        return fn_network_error;
    return 0;
#endif
}

/*
 * Settle the status after an open. The adapter stages the whole reply during
 * open(), so one status call normally reports the full byte count with no
 * polling at all. The bounded retry is insurance against a slower transport
 * reporting zero for a moment -- it must never become an unbounded wait,
 * because that is exactly the failure mode being avoided.
 */
static unsigned char settle(void)
{
    unsigned char tries;
    unsigned char code = GC_NOREPLY;

    for (tries = 0; tries < 8; tries++) {
        code = probe();
        if (!st_ok(code))
            return code;                /* an error or no reply -- decided */
        if (st_bw != 0)
            return code;
        if (st_conn == 0)
            return code;                /* nothing waiting and nothing coming */
    }

    return code;
}

static void fail(unsigned char code)
{
    gc_ecode = code;
    network_close(url);
    plat_net_end();
}

#endif /* !GC_FAKE_DATA */

#ifndef GC_FAKE_DATA

/* ------------------------------------------------------------------ */
/* The streaming read, shared by every fetch                           */
/* ------------------------------------------------------------------ */

/*
 * Never ask network_read() for more than the status just said is waiting: its
 * internal loop spins without a timeout when nothing is available but the
 * connection is still up, so staying inside st_bw keeps that spin unreachable.
 */
static int read_chunk(void)
{
    unsigned int chunk = st_bw;

    if (chunk > sizeof(rxbuf))
        chunk = sizeof(rxbuf);

    gc_stage = "read";
    return network_read(url, rxbuf, chunk);
}

static unsigned char open_and_settle(unsigned char aux1, unsigned char aux2)
{
    unsigned char code;

    gc_ecode = 0;
    gc_dev_ecode = 0;
    split_reset();

    plat_net_begin();

    /* A window open is one upstream HTTPS round trip per calendar, so widen
       the SIO timeout around it and put it straight back afterwards. */
    gc_stage = "open";
    fn_default_timeout = TMO_LONG;
    code = network_open(url, aux1, aux2);
    fn_default_timeout = TMO_NORM;

    if (code != FN_ERR_OK) {
        fail(open_error());
        return 0;
    }

    gc_stage = "status";
    code = settle();
    if (!st_ok(code)) {
        fail(code == GC_NOREPLY ? 0 : code);
        return 0;
    }

    return 1;
}

#endif /* !GC_FAKE_DATA */

/* ------------------------------------------------------------------ */
/* Fetches                                                              */
/* ------------------------------------------------------------------ */

#ifdef GC_FAKE_DATA
/*
 * Canned wire data for headless testing, built with -DGC_FAKE_DATA.
 *
 * These emit *real* wire text -- window title, a dashed header with the '#' at
 * the right index, properly spaced columns -- and push it through split_lines
 * in awkwardly sized chunks, so the parser and the line splitter are exercised
 * rather than bypassed. Canned data that skips the parser tests nothing but
 * the painters.
 *
 * The cases are chosen to be awkward rather than pretty: a two-digit event
 * number so numW is not 1, an all-day row, an open-ended "HH:MM->" row, a
 * category that is a colour name, one that is a calendar name, one that is a
 * colour name with something after it, and a final line with no terminator.
 *
 * Two cc65 traps make a byte array the only safe way to spell the terminator:
 * a source-literal '\t' charmaps to ATASCII $7F, which is a *high* byte, and
 * '\n' charmaps to $9B -- so "a\nb" is already ATASCII and "\r\n" is CR + $9B.
 */
static const char *const fake_day[] = {
    "Fri 28 Aug 2026",
    "---#-Time--------Category-------Event----------------------------------",
    "   1 09:00-10:00 Peacock        Daily standup with the whole team",
    "*  2 all day     Tomato         Company holiday",
    "~  3 11:30-12:15 Work           Weekly review @Room 3",
    "   4 14:00-14:30 Banana         Dentist",
    "  17 23:30->     Basil          Overnight bake",
    "  18 16:00-17:00 Tomato Soup    Cooking club",
    0
};

static const char *const fake_week[] = {
    "Week of Sun 23 Aug 2026",
    "--#-Dat-Time--------Category-------Event-------------------------------",
    "  1 Sun 10:00-11:00 Basil          Farmers market",
    "  2 Tue 09:00-09:30 Peacock        Standup",
    "  3 Tue 13:00-14:00 Tomato         Budget review",
    "* 4 Wed all day     Banana         Conference day one",
    "  5 Fri 18:30-21:00 Graphite       Dinner with Sam",
    0
};

static const char *const fake_month[] = {
    "August 2026",
    "--#-Date--Time--------Category-------Event-----------------------------",
    "  1 Sa 01 10:00-11:00 Basil          Market",
    "  2 Mo 03 09:00-09:30 Peacock        Standup",
    "  3 Mo 03 13:00-14:00 Tomato         Review",
    "  4 Mo 03 16:00-16:30 Banana         Call",
    "  5 Fr 28 09:00-10:00 Peacock        Standup",
    "  6 Fr 28 14:00-15:00 Tomato         Retro",
    "  7 Sa 29 11:00-12:00 Graphite       Repairs",
    0
};

static const char *const fake_agenda[] = {
    "Agenda from 28 Aug 2026",
    "--#-Date---Time--------Category-------Event----------------------------",
    "  1 28 Aug 09:00-10:00 Peacock        Daily standup",
    "  2 28 Aug 14:00-15:00 Tomato         Retrospective",
    "* 3 29 Aug all day     Banana         Bank holiday",
    "  4 01 Sep 09:00-10:00 Basil          Quarter kickoff",
    "  5 01 Sep 16:00-16:30 Graphite       One to one",
    0
};

static const char *const fake_cals[] = {
    "Name--------------------------------------------------Category",
    "Work                                                  google",
    "Family                                                google",
    "thom.cherryhomes@gmail.com                            google",
    0
};

static const char *const fake_det[] = {
    "Daily standup with the whole team",
    "Fri 28 Aug 2026 09:00-10:00",
    "Category: Peacock",
    "Where: Room 3, second floor",
    "",
    "Round the table, two minutes each. Bring the roadmap",
    "printout; we are going to mark up the Q4 slip on it and",
    "hand it back to planning before lunch.",
    "",
    "Dial-in is the usual bridge.",
    0
};

/*
 * Push canned lines through the real splitter in 37-byte chunks, so a line
 * boundary lands mid-chunk as often as not.
 */
static void fake_stream(const char *const *lines,
                        void (*emit)(const char *, unsigned char),
                        unsigned char raw)
{
    unsigned char buf[37];
    unsigned char n = 0;
    unsigned char i;
    const char *s;

    while (*lines) {
        s = *lines++;

        for (i = 0; ; i++) {
            /* One past the string is the ATASCII terminator the wire uses. */
            buf[n++] = s[i] ? (unsigned char) s[i] : 0x9B;

            if (n == sizeof(buf)) {
                if (raw)
                    detail_ingest(buf, n);
                else
                    split_lines(buf, n, emit);
                n = 0;
            }

            if (!s[i])
                break;
        }
    }

    if (n) {
        if (raw)
            detail_ingest(buf, n);
        else
            split_lines(buf, n, emit);
    }
}
#endif

unsigned char gc_fetch_index(unsigned char view)
{
    idx_reset(view);

#ifdef GC_FAKE_DATA
    gc_ecode = 0;
    split_reset();
    switch (view) {
    case VIEW_DAY:   fake_stream(fake_day, idx_line, 0);   break;
    case VIEW_WEEK:  fake_stream(fake_week, idx_line, 0);  break;
    case VIEW_MONTH: fake_stream(fake_month, idx_line, 0); break;
    default:         fake_stream(fake_agenda, idx_line, 0); break;
    }
    split_finish(idx_line);
    if (view == VIEW_AGENDA)
        agenda_build();
    return 1;
#else
    {
    int got;

    build_url(view, 0);

    if (!open_and_settle(GC_MODE_DIR, GC_WIDTH))
        return 0;

    while (st_bw != 0) {
        got = read_chunk();
        if (got <= 0)
            break;

        split_lines(rxbuf, (unsigned int) got, idx_line);

        if (!st_ok(probe()))
            break;
    }
    split_finish(idx_line);

    network_close(url);
    plat_net_end();

    if (view == VIEW_AGENDA)
        agenda_build();

    return 1;
    }
#endif
}

unsigned char gc_fetch_detail(unsigned char view, const char *evnum)
{
    detail_reset();

#ifdef GC_FAKE_DATA
    (void) view;
    (void) evnum;
    gc_ecode = 0;
    fake_stream(fake_det, 0, 1);
    detail_finish();
    return 1;
#else
    {
    int got;

    build_url(view, evnum);

    /* aux2 is ignored on a READ: NetworkProtocolCalendar composes every byte
       itself with lineEnding already applied and overrides read() so that no
       translation is layered on top. */
    if (!open_and_settle(GC_MODE_READ, 0))
        return 0;

    while (st_bw != 0) {
        got = read_chunk();
        if (got <= 0)
            break;

        detail_ingest(rxbuf, (unsigned int) got);
        if (gc_det_trunc)
            break;

        if (!st_ok(probe()))
            break;
    }
    detail_finish();

    network_close(url);
    plat_net_end();
    return 1;
    }
#endif
}

/* ------------------------------------------------------------------ */
/* The calendar list                                                   */
/* ------------------------------------------------------------------ */

/*
 * format_list_human() emits a header row and then one row per calendar, with
 * *no* window title line -- so this cannot reuse the event index parser, which
 * takes line 0 as the title. At width 80 the name column is
 * lineW - lineW/3 - 1 = 53 wide, and the category that follows it is ragged.
 */
#define PK_NAMEW    53

static unsigned char cal_line_no;

static void cals_line(const char *p, unsigned char len)
{
    struct cal   *c;
    unsigned char n, i;
    char          buf[PK_NAMEW + 1];

    if (cal_line_no == 0) {             /* the header row */
        cal_line_no = 1;
        return;
    }

    if (len == 0 || gc_cal_count >= CAL_MAX)
        return;

    n = (len < PK_NAMEW) ? len : PK_NAMEW;
    while (n > 0 && p[n - 1] == ' ')
        n--;
    if (n == 0)
        return;

    /* An account with no calendars at all prints "(none)". */
    if (p[0] == '(')
        return;

    memcpy(buf, p, n);
    buf[n] = '\0';

    c = &gc_cals[gc_cal_count];
    copy_san(c->sel, buf, CAL_SEL_LEN);
    copy_san(c->name, buf, CAL_NAME_LEN);

    /* The picker column is upper-cased for legibility at 24 characters; the
       selector keeps its case, because resolve_selector() matches the name. */
    for (i = 0; c->name[i]; i++)
        if (c->name[i] >= 'a' && c->name[i] <= 'z')
            c->name[i] = (char) (c->name[i] - 32);

    gc_cal_count++;
}

unsigned char gc_fetch_cals(void)
{
    /* Entry 0 is always "every calendar Google is showing", which is what an
       empty selector means to the adapter. */
    gc_cal_count = 0;
    copy_san(gc_cals[0].name, "ALL SHOWN CALENDARS", CAL_NAME_LEN);
    gc_cals[0].sel[0] = '\0';
    gc_cal_count = 1;

    cal_line_no = 0;

#ifdef GC_FAKE_DATA
    gc_ecode = 0;
    split_reset();
    fake_stream(fake_cals, cals_line, 0);
    split_finish(cals_line);
    return 1;
#else
    {
    int got;

    strcpy(url, "N:GCAL://");

    if (!open_and_settle(GC_MODE_DIR, GC_WIDTH))
        return 0;

    while (st_bw != 0) {
        got = read_chunk();
        if (got <= 0)
            break;

        split_lines(rxbuf, (unsigned int) got, cals_line);

        if (!st_ok(probe()))
            break;
    }
    split_finish(cals_line);

    network_close(url);
    plat_net_end();
    return 1;
    }
#endif
}
