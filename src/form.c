/*
 * Not built into the CoCo 3 client: there it is a separate binary and
 * src/coco/chain.c is the seam. Every other platform, the CoCo 1/2
 * included, compiles this in-process as usual.
 */
#ifndef GC_CHAIN_EDIT

/*
 * The compose/edit form model: field storage, validation, and the exact
 * KEY: value lines the calendar adapter's draft parser takes.
 *
 * Pure -- no platform, no network. compose.c owns the screen loop and the
 * cursor; net.c owns the channel; this file owns what is *in* the form and
 * what goes out on the wire, which is the half a host test can pin down to
 * the byte.
 *
 * The one asymmetry worth internalising is compose versus edit. A compose
 * sends every non-empty field. An edit sends only what the user touched,
 * because the adapter patches exactly the keys it receives and nothing else:
 * an untouched field never goes out, so the server's full-length title can
 * never be clobbered by the truncated copy this client keeps. The corollary
 * is that a blank field on an edit means "leave it alone", not "clear it" --
 * with one deliberate exception: blanking the START time converts the event
 * to all-day, because a date-only START is precisely how the wire spells
 * all-day and there is no other way to say it.
 */

#include <string.h>

#include "gcal.h"

/*
 * The overlays are only sound while what they park actually fits in the
 * RAM it borrows. These are the link-time BSS check's little siblings:
 * the build fails right here when a knob moves and it stops being true.
 * The cals guard is the *sum* -- the picker's list sits behind the form.
 */
#ifdef COCO3
/* The borrowed base is gc_scratch here, not gc_det -- see gcal.h. */
#define GC_DET_SIZE     GC_SCRATCH_SIZE
#else
#define GC_DET_SIZE     (DET_ROWS * DET_STRIDE)
#endif

#ifdef GC_FORM_OVERLAY
#if FRMBUF_SIZE > GC_DET_SIZE
#error "the compose form no longer fits in the detail buffer it borrows"
#endif
#endif

#ifdef GC_CALS_OVERLAY
#if GC_CALS_OFF + GC_CALS_SIZE > GC_DET_SIZE
#error "the picker's list no longer fits in the gc_det RAM it borrows"
#endif
#endif

/*
 * Field access as offsets off one base, not a switch: a struct of nothing
 * but chars has no padding to make the arithmetic and the members drift,
 * and tests/hosttest.c asserts each pointer is the member it claims.
 */
#define O_TITLE 0
#define O_DATE  (O_TITLE + FRM_TITLE_MAX + 1)
#define O_START (O_DATE + FRM_DATE_MAX + 1)
#define O_END   (O_START + FRM_TIME_MAX + 1)
#define O_LOC   (O_END + FRM_TIME_MAX + 1)
#define O_DESC  (O_LOC + FRM_LOC_MAX + 1)
#define O_CAT   (O_DESC + FRM_DESC_MAX + 1)

static const unsigned int field_off[FRM_NFIELDS] = {
    O_TITLE, O_DATE, O_START, O_END, O_LOC, O_DESC, O_CAT
};

static const unsigned char field_max[FRM_NFIELDS] = {
    FRM_TITLE_MAX, FRM_DATE_MAX, FRM_TIME_MAX, FRM_TIME_MAX,
    FRM_LOC_MAX, FRM_DESC_MAX, FRM_CAT_MAX
};

char *form_field_ptr(unsigned char f)
{
    return (char *) &frm + field_off[f];
}

unsigned char form_field_max(unsigned char f)
{
    return field_max[f];
}

/* "HH:MM", zero padded, into a six-byte buffer. */
static void put_hm(char *dst, unsigned char h, unsigned char m)
{
    dst[0] = (char) ('0' + h / 10);
    dst[1] = (char) ('0' + h % 10);
    dst[2] = ':';
    dst[3] = (char) ('0' + m / 10);
    dst[4] = (char) ('0' + m % 10);
    dst[5] = '\0';
}

/*
 * With an event: prefill for an edit, from the index record -- which is all
 * the client keeps. Location, description and category are not in it, and
 * they start blank rather than costing a detail round trip: blank means
 * unchanged, so nothing is lost. Without one: a blank compose.
 *
 * The date is the caller's, not the record's, because struct event's day
 * field is polymorphic per view and only the caller knows which view it is
 * reading (compose_edit does the AGENDA year inference).
 */
void form_init(const struct event *e, unsigned int y,
               unsigned char mo, unsigned char d)
{
    unsigned char f;

    memset(&frm, 0, sizeof(struct frmbuf));
    for (f = 0; f < FRM_NFIELDS; f++)
        frm_dirty[f] = 0;

    date_iso(frm.date, y, mo, d);

    if (e) {
#ifdef COCO3
        /* The title is in far storage on that build, so it is fetched by index
           -- e always points into gc_index, which is where the index comes
           from. See src/coco/far.c. */
        strcpy(frm.title, ev_title((unsigned char) (e - gc_index)));
#else
        strcpy(frm.title, e->title);
#endif
        if (!(e->flags & EVF_ALLDAY)) {
            put_hm(frm.start, e->sh, e->sm);
            if (!(e->flags & EVF_OPENEND))
                put_hm(frm.end, e->eh, e->em);
        }
    }
}

/*
 * The shape checks run through one tiny matcher rather than hand-rolled
 * positional loops: 'd' in the pattern is any digit, everything else is
 * itself, and the whole of s must be consumed. The first cut of these
 * validators spelled the loops out and cost four times the bytes on the
 * 6809 -- and the CoCo has none to spare.
 */
static unsigned char match(const char *s, const char *pat)
{
    for (; *pat; s++, pat++) {
        if (*pat == 'd') {
            if (*s < '0' || *s > '9')
                return 0;
        } else if (*s != *pat)
            return 0;
    }
    return (unsigned char) (*s == '\0');
}

/* Two digits as a byte -- the caller has already checked they are digits. */
static unsigned char two_dig(const char *s)
{
    return (unsigned char) ((s[0] - '0') * 10 + (s[1] - '0'));
}

/* Strict YYYY-MM-DD, with the day checked against the month's real length --
   letting Feb 30 through would come back as an opaque draft rejection. */
unsigned char form_date_ok(const char *s)
{
    unsigned char mo, d;

    if (!match(s, "dddd-dd-dd"))
        return 0;

    mo = two_dig(s + 5);
    d = two_dig(s + 8);

    if (mo < 1 || mo > 12)
        return 0;
    if (d < 1 ||
        d > date_dim((unsigned int) (two_dig(s) * 100 + two_dig(s + 2)), mo))
        return 0;

    return 1;
}

/* HH:MM or H:MM. The single-digit-hour form is accepted here for the
   typist's sake and normalised to two digits on emit, so the adapter only
   ever sees the format its parser documents. */
unsigned char form_time_ok(const char *s)
{
    unsigned char h, m;

    if (match(s, "dd:dd")) {
        h = two_dig(s);
        m = two_dig(s + 3);
    } else if (match(s, "d:dd")) {
        h = (unsigned char) (s[0] - '0');
        m = two_dig(s + 2);
    } else
        return 0;

    return (unsigned char) (h <= 23 && m <= 59);
}

unsigned char form_any_dirty(void)
{
    unsigned char f;

    for (f = 0; f < FRM_NFIELDS; f++)
        if (frm_dirty[f])
            return 1;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Which lines a save would send                                       */
/* ------------------------------------------------------------------ */

/*
 * One rule covers five of the six lines: the field goes out when it holds
 * something and -- on an edit -- was touched. (Compose reaches this only
 * after validation, so the mandatory SUMMARY is never empty here.) A blank
 * touched field on an edit is "leave it alone", not "clear it".
 *
 * START is the exception, in both directions. A dirty START *time* needs
 * the date to give it a day, and a dirty date needs the time to keep a
 * timed event timed -- so either one sends the whole START value, with the
 * untouched half riding along from the prefill. And a blank START time is
 * the one meaningful blank: date-only START is how the wire spells
 * all-day, and there is no other way to say it.
 */
static unsigned char want_field(unsigned char editing, unsigned char f)
{
    if (!form_field_ptr(f)[0])
        return 0;
    return (unsigned char) (!editing || frm_dirty[f]);
}

static unsigned char want_start(unsigned char editing)
{
    if (!editing)
        return 1;
    return (unsigned char) (frm_dirty[FRM_DATE] || frm_dirty[FRM_START]);
}

/* ------------------------------------------------------------------ */
/* Validation                                                          */
/* ------------------------------------------------------------------ */

/*
 * Catch before sending what the adapter would only bounce as an opaque
 * "rejected" -- it collapses every draft error to one code, so the field
 * name in the message here is the only diagnosis the user will ever get.
 * Returns FM_NONE when the draft is sound, else the message code, with
 * *bad the field to put the cursor back on.
 */
unsigned char form_validate(unsigned char editing, unsigned char *bad)
{
    if (!editing && !frm.title[0]) {
        *bad = FRM_TITLE;
        return FM_NEEDTITLE;
    }

    /* The START value embeds the date whenever it goes out at all. */
    if (want_start(editing) && !form_date_ok(frm.date)) {
        *bad = FRM_DATE;
        return FM_BADDATE;
    }

    if (frm.start[0] && !form_time_ok(frm.start)) {
        *bad = FRM_START;
        return FM_BADTIME;
    }

    if (want_field(editing, FRM_END)) {
        if (!form_time_ok(frm.end)) {
            *bad = FRM_END;
            return FM_BADTIME;
        }
        /* An END time on an all-day form is the adapter's MIXED_FORMS;
           refuse it here where it can still be fixed. */
        if (!frm.start[0]) {
            *bad = FRM_START;
            return FM_ENDALONE;
        }
    }

    return FM_NONE;
}

/* ------------------------------------------------------------------ */
/* Emission                                                            */
/* ------------------------------------------------------------------ */

/*
 * Build a START/END value: the date, then the time when there is one,
 * normalised to two digits of hour so the adapter only ever sees the form
 * its parser documents. A blank time leaves a date-only value -- the
 * wire's spelling of all-day.
 */
static void build_when(char *v, const char *t)
{
    strcpy(v, frm.date);
    if (t[0]) {
        strcat(v, " ");
        if (strlen(t) == 4)
            strcat(v, "0");
        strcat(v, t);
    }
}

/* '\n' below is deliberate: each toolchain's charmap turns it into that
   platform's native terminator -- $9B on the Atari, CR on the Apple, LF
   elsewhere -- and the adapter's line splitter takes all of them. */
static void put_line(const char *key, const char *val)
{
    strcpy(frm.line, key);
    strcat(frm.line, val);
    strcat(frm.line, "\n");
    form_put(frm.line);
}

/*
 * Send the draft through form_put(), one field line at a time, and return
 * how many lines went out -- zero tells the caller nothing was worth a
 * commit. The caller has already validated; this only decides what goes
 * and formats it. A failed put is form_put's own to remember: the channel
 * still has to be closed, and gc_save_end() is where the failure reports,
 * so there is nothing useful to do here but keep going.
 *
 * Walking the fields in screen order gives the draft its line order:
 * SUMMARY, START (built at the DATE slot from date + start time), END,
 * LOCATION, DESCRIPTION, CATEGORY. The adapter is indifferent to order;
 * the host tests are not, which is what pins the format down.
 */
/*
 * The keys as one literal at a fixed 14-byte stride (the longest,
 * "DESCRIPTION: ", is 13 plus its NUL) rather than an array of pointers:
 * CMOC initialises a static pointer array with run-time code, one store
 * per entry, and a string literal in an expression costs none of that on
 * any of the five compilers. The FRM_START slot is dead air -- the loop
 * folds it into the DATE slot before looking here.
 */
#define FRM_KEYS \
    "SUMMARY: \0\0\0\0\0" \
    "START: \0\0\0\0\0\0\0" \
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0" \
    "END: \0\0\0\0\0\0\0\0\0" \
    "LOCATION: \0\0\0\0" \
    "DESCRIPTION: \0" \
    "CATEGORY: \0\0\0"

unsigned char form_emit(unsigned char editing)
{
    char          v[FRM_DATE_MAX + FRM_TIME_MAX + 3];
    unsigned char f;
    unsigned char n = 0;

    for (f = 0; f < FRM_NFIELDS; f++) {
        if (f == FRM_START)
            continue;                   /* folded into the DATE slot */

        if (f == FRM_DATE) {
            if (!want_start(editing))
                continue;
            build_when(v, frm.start);
        } else if (!want_field(editing, f)) {
            continue;
        } else if (f == FRM_END) {
            build_when(v, frm.end);
        } else {
            v[0] = '\0';                /* the field itself goes out */
        }

        put_line(FRM_KEYS + f * 14, v[0] ? v : form_field_ptr(f));
        n++;
    }

    return n;
}

#endif /* !GC_CHAIN_EDIT */
