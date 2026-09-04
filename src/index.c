/*
 * The event listing parser.
 *
 * A DIR open on a calendar view returns a window title line, a header line,
 * and then one line per event, laid out by format_index_human():
 *
 *     marker(1) sp num(numW) sp [date(dateW) sp] time(11) sp cat(14) sp title
 *
 * marker is '*' all-day, '~' recurring, ' ' timed. An all-day *recurring*
 * event reports '*', because the adapter's marker is
 * allDay ? '*' : (recurring ? '~' : ' ') -- the detail screen's "Repeats" line
 * is where EVF_RECURRING comes from for those.
 *
 * numW is not fixed: the adapter sizes it to the largest event number in the
 * window. It is recovered from the header line, which dashed() builds by
 * replacing every space with '-', so the '#' is the only character that
 * survives and it sits at index 2 + numW - 1.
 *
 * dateW is fixed per view, because each view's date column is a different
 * shape: nothing for DAY, "Fri" for WEEK, "Fr 28" for MONTH, "28 Aug" for
 * AGENDA.
 *
 * Pure: no platform, no network. tests/hosttest.c feeds it real wire bytes.
 */

#include <string.h>

#include "gcal.h"

static unsigned char view;
static unsigned char line;      /* 0 = window title, 1 = header, 2+ = events */
static unsigned char numw;
static unsigned char datew;
static unsigned char datec;     /* column of the date field, when datew > 0 */
static unsigned char timec;
static unsigned char catc;
static unsigned char titlec;

void idx_reset(unsigned char v)
{
    unsigned char i;

    view = v;
    line = 0;
    gc_count = 0;
    gc_trunc = 0;
    gc_wtitle[0] = '\0';

    numw = 1;
    datew = 0;
    datec = 0;
    timec = 0;
    catc = 0;
    titlec = 0;

    if (v == VIEW_MONTH) {
        for (i = 0; i < 32; i++) {
            gc_daycnt[i] = 0;
            gc_daycol[i] = COL_NONE;
        }
    }
}

/* ------------------------------------------------------------------ */

static unsigned char is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

/*
 * Read a right-justified, space-padded decimal out of a fixed-width column.
 * Non-digits are skipped rather than treated as zero, so the padding does not
 * shift the value.
 */
static unsigned int read_num(const char *p, unsigned char n)
{
    unsigned int v = 0;
    unsigned char i;

    for (i = 0; i < n; i++)
        if (is_digit(p[i]))
            v = v * 10 + (unsigned int) (p[i] - '0');

    return v;
}

static unsigned char read_num2(const char *p)
{
    return (unsigned char) read_num(p, 2);
}

/* Derive every column offset from the header line. */
static void layout(const char *p, unsigned char len)
{
    unsigned char i;
    unsigned char used;

    numw = 1;
    for (i = 0; i < len; i++) {
        if (p[i] == '#') {
            numw = (unsigned char) (i - 1);
            break;
        }
    }
    if (numw < 1)
        numw = 1;

    switch (view) {
    case VIEW_DAY:    datew = 0; break;
    case VIEW_WEEK:   datew = 3; break;      /* "Fri"    */
    case VIEW_MONTH:  datew = 5; break;      /* "Fr 28"  */
    default:          datew = 6; break;      /* "28 Aug" */
    }

    used = (unsigned char) (2 + numw + 1 + GC_TIMEW);
    if (datew)
        used = (unsigned char) (used + datew + 1);

    timec  = (unsigned char) (used - GC_TIMEW);
    datec  = datew ? (unsigned char) (timec - datew - 1) : 0;
    catc   = (unsigned char) (used + 1);
    titlec = (unsigned char) (catc + GC_CATW + 1);
}

/* ------------------------------------------------------------------ */

/*
 * Three literal shapes fill the eleven-column time field and it is never
 * empty: "HH:MM-HH:MM", "HH:MM->" when the event runs past midnight, and
 * "all day". Anything that does not start with a digit is the last of those.
 */
static void parse_time(struct event *e, const char *p)
{
    e->sh = e->sm = e->eh = e->em = 0;

    if (!is_digit(p[timec])) {
        e->flags |= EVF_ALLDAY;
        return;
    }

    e->sh = read_num2(p + timec);
    e->sm = read_num2(p + timec + 3);

    if (!is_digit(p[timec + 6])) {
        e->flags |= EVF_OPENEND;
        e->eh = e->sh;
        e->em = e->sm;
        return;
    }

    e->eh = read_num2(p + timec + 6);
    e->em = read_num2(p + timec + 9);
}

/*
 * Day of week from the three-letter name. The first letters collide (Sun/Sat,
 * Tue/Thu); the first two do not.
 */
static unsigned char dow_from_text(const char *p)
{
    char a = p[0], b = p[1];

    if (a == 'S' && b == 'u') return 0;
    if (a == 'M')             return 1;
    if (a == 'T' && b == 'u') return 2;
    if (a == 'W')             return 3;
    if (a == 'T' && b == 'h') return 4;
    if (a == 'F')             return 5;
    if (a == 'S' && b == 'a') return 6;
    return 0;
}

/* Month from the three-letter name. Jun and Jul need the third letter. */
static unsigned char mon_from_text(const char *p)
{
    char a = p[0], b = p[1], c = p[2];

    if (a == 'J' && b == 'a')             return 1;
    if (a == 'F')                         return 2;
    if (a == 'M' && c == 'r')             return 3;
    if (a == 'A' && b == 'p')             return 4;
    if (a == 'M' && c == 'y')             return 5;
    if (a == 'J' && b == 'u' && c == 'n') return 6;
    if (a == 'J' && b == 'u' && c == 'l') return 7;
    if (a == 'A' && b == 'u')             return 8;
    if (a == 'S')                         return 9;
    if (a == 'O')                         return 10;
    if (a == 'N')                         return 11;
    if (a == 'D')                         return 12;
    return 1;
}

/*
 * MONTH stores no events at all, only a per-day count and the colour of that
 * day's leading event -- the adapter sorts by start time, so the first row for
 * a day is the earliest. A three-hundred-event month costs 64 bytes this way.
 */
static void tally_month(const char *p, unsigned char len)
{
    unsigned char d;

    /* "Fr 28": the day number is three characters into the date field. */
    d = read_num2(p + datec + 3);
    if (d < 1 || d > 31)
        return;

    if (gc_daycnt[d] == 0)
        gc_daycol[d] = color_match(p + catc, (unsigned char) (len - catc));
    if (gc_daycnt[d] < 255)
        gc_daycnt[d]++;

    if (gc_count < 255)
        gc_count++;
}

static void take_event(const char *p, unsigned char len)
{
    struct event *e;
    unsigned int  n;
    char          buf[TITLE_LEN];
    unsigned char tlen;

    if (gc_count >= MAX_EVENTS) {
        gc_trunc = 1;
        return;
    }

    e = &gc_index[gc_count];
    memset(e, 0, sizeof(*e));

    if (p[0] == '*')
        e->flags |= EVF_ALLDAY;
    else if (p[0] == '~')
        e->flags |= EVF_RECURRING;

    parse_time(e, p);

    /* The event number goes straight back out in a /N device spec and is never
       used for arithmetic, so it is stored as the decimal text it will be
       sent as. Reading it through read_num() first drops the column's padding
       and normalises whatever width the adapter chose. */
    n = read_num(p + 2, numw);
    {
        char          tmp[EVNUM_LEN];
        unsigned char i = 0, j = 0;

        do {
            tmp[i++] = (char) ('0' + (n % 10));
            n /= 10;
        } while (n && i < EVNUM_LEN - 1);

        while (i)
            e->num[j++] = tmp[--i];
        e->num[j] = '\0';
    }

    switch (view) {
    case VIEW_WEEK:
        e->day = dow_from_text(p + datec);
        break;
    case VIEW_AGENDA:
        e->day = read_num2(p + datec);
        e->mon = mon_from_text(p + datec + 3);
        break;
    default:
        e->day = 0;
        break;
    }

    e->color = color_match(p + catc, (unsigned char) (len - catc));
    e->chip  = color_chip(e->color);

#ifdef GC_KEEP_CAT
    /*
     * The category column verbatim, trailing padding removed. It is worth
     * keeping alongside the colour it was matched against: for an event with
     * no colorId the adapter puts the *calendar* name here, which nothing in
     * the colour derived from it can express.
     *
     * The column is GC_CATW wide and space padded, but the last event of a
     * listing is not padded out, so the line can end inside it.
     */
    {
        unsigned char clen = (unsigned char) (len - catc);
        char          cbuf[CAT_LEN];

        if (clen > GC_CATW)
            clen = GC_CATW;
        while (clen > 0 && p[catc + clen - 1] == ' ')
            clen--;
        memcpy(cbuf, p + catc, clen);
        cbuf[clen] = '\0';
        copy_san(e->cat, cbuf, CAT_LEN);
    }
#endif

    tlen = (unsigned char) (len - titlec);
    if (tlen > TITLE_LEN - 1)
        tlen = TITLE_LEN - 1;
    memcpy(buf, p + titlec, tlen);
    buf[tlen] = '\0';
#ifdef COCO3
    /* The title lives in far storage on this build -- sanitize into a local
       and hand the whole field over in one write. */
    {
        static char t[TITLE_LEN];

        copy_san(t, buf, TITLE_LEN);
        ev_set_title(gc_count, t);
    }
#else
    copy_san(e->title, buf, TITLE_LEN);
#endif

    gc_count++;
}

/* ------------------------------------------------------------------ */

void idx_line(const char *p, unsigned char len)
{
    if (line == 0) {
        char buf[41];
        unsigned char n = len;

        if (n > 40)
            n = 40;
        memcpy(buf, p, n);
        buf[n] = '\0';
        copy_san(gc_wtitle, buf, sizeof(gc_wtitle));
        line = 1;
        return;
    }

    if (line == 1) {
        layout(p, len);
        line = 2;
        return;
    }

    /* "  (no events)" has no columns at all, and a truncated final line has
       nothing useful past the title offset either. */
    if (len < titlec)
        return;

    if (view == VIEW_MONTH)
        tally_month(p, len);
    else
        take_event(p, len);
}
