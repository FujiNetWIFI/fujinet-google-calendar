/*
 * Host-side tests for the portable core.
 *
 * date.c, color.c, index.c, lines.c, agenda.c, wrap.c, sanitize.c and detail.c
 * have no platform or network dependency, so they build and run on a normal
 * machine. That matters: the column derivation, the whole-token colour match,
 * the month-end clamping and the line-ending soup are the fiddliest logic in
 * the program, and iterating on them through a 6502 cross-compile and an
 * emulator round-trip is far too slow.
 *
 * Built twice, because the core's fixed widths are overridable and the
 * backends do override them: hosttest is the Atari's shape and hosttest80 the
 * Apple II's. Assertions that only make sense under one of them are guarded by
 * the same macro the backend sets.
 *
 *   make -C tests
 */

#include <stdio.h>
#include <string.h>

#include "../src/gcal.h"

static int failures;
static int checks;

static void eq_str(const char *what, const char *got, const char *want)
{
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("  FAIL %s\n       got  \"%s\"\n       want \"%s\"\n",
               what, got, want);
    }
}

static void eq_int(const char *what, long got, long want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("  FAIL %s: got %ld, want %ld\n", what, got, want);
    }
}

/* ------------------------------------------------------------------ */
/* date.c                                                              */
/* ------------------------------------------------------------------ */

static void test_date(void)
{
    unsigned int  y;
    unsigned char mo, d;
    char iso[11];

    puts("date");

    /* The full Gregorian rule: the century exceptions are the whole point of
       spelling it out rather than testing y % 4. */
    eq_int("leap 2024", date_leap(2024), 1);
    eq_int("leap 2025", date_leap(2025), 0);
    eq_int("leap 1900", date_leap(1900), 0);
    eq_int("leap 2000", date_leap(2000), 1);
    eq_int("leap 2100", date_leap(2100), 0);

    eq_int("dim feb 2024", date_dim(2024, 2), 29);
    eq_int("dim feb 2025", date_dim(2025, 2), 28);
    eq_int("dim feb 2100", date_dim(2100, 2), 28);
    eq_int("dim apr", date_dim(2026, 4), 30);

    y = 2025; mo = 2; d = 28;
    date_addday(&y, &mo, &d);
    eq_int("feb 28 +1 non-leap m", mo, 3);
    eq_int("feb 28 +1 non-leap d", d, 1);

    y = 2024; mo = 2; d = 28;
    date_addday(&y, &mo, &d);
    eq_int("feb 28 +1 leap d", d, 29);

    y = 2024; mo = 3; d = 1;
    date_subday(&y, &mo, &d);
    eq_int("mar 1 -1 leap m", mo, 2);
    eq_int("mar 1 -1 leap d", d, 29);

    y = 2025; mo = 3; d = 1;
    date_subday(&y, &mo, &d);
    eq_int("mar 1 -1 non-leap d", d, 28);

    y = 2026; mo = 12; d = 31;
    date_addday(&y, &mo, &d);
    eq_int("dec 31 +1 y", y, 2027);
    eq_int("dec 31 +1 m", mo, 1);
    eq_int("dec 31 +1 d", d, 1);

    y = 2027; mo = 1; d = 1;
    date_subday(&y, &mo, &d);
    eq_int("jan 1 -1 y", y, 2026);
    eq_int("jan 1 -1 m", mo, 12);
    eq_int("jan 1 -1 d", d, 31);

    /* A month step must clamp, or the next device spec carries Feb 31 and the
       adapter rejects it as an invalid spec -- which reads as a net failure. */
    y = 2025; mo = 1; d = 31;
    date_addmonth(&y, &mo, &d);
    eq_int("jan 31 +1mo m", mo, 2);
    eq_int("jan 31 +1mo d", d, 28);

    y = 2024; mo = 1; d = 31;
    date_addmonth(&y, &mo, &d);
    eq_int("jan 31 +1mo leap d", d, 29);

    y = 2025; mo = 3; d = 31;
    date_submonth(&y, &mo, &d);
    eq_int("mar 31 -1mo m", mo, 2);
    eq_int("mar 31 -1mo d", d, 28);

    y = 2026; mo = 1; d = 15;
    date_submonth(&y, &mo, &d);
    eq_int("jan -1mo y", y, 2025);
    eq_int("jan -1mo m", mo, 12);

    /* Sakamoto, 0 = Sunday. */
    eq_int("dow 2026-08-28", date_dow(2026, 8, 28), 5);   /* Friday    */
    eq_int("dow 2000-01-01", date_dow(2000, 1, 1), 6);    /* Saturday  */
    eq_int("dow 1970-01-01", date_dow(1970, 1, 1), 4);    /* Thursday  */
    eq_int("dow 2024-02-29", date_dow(2024, 2, 29), 4);   /* Thursday  */
    eq_int("dow 1900-01-01", date_dow(1900, 1, 1), 1);    /* Monday    */
    eq_int("dow 2100-03-01", date_dow(2100, 3, 1), 1);    /* Monday    */
    eq_int("dow 2026-01-01", date_dow(2026, 1, 1), 4);    /* Thursday  */

    date_iso(iso, 2026, 8, 28);
    eq_str("iso", iso, "2026-08-28");
    date_iso(iso, 999, 1, 2);
    eq_str("iso pads", iso, "0999-01-02");

    eq_str("dow3", date_dow3(5), "Fri");
    eq_str("mon3", date_mon3(8), "Aug");
    eq_str("mon3 bad", date_mon3(0), "???");
}

/* ------------------------------------------------------------------ */
/* color.c                                                             */
/* ------------------------------------------------------------------ */

static unsigned char match(const char *s)
{
    return color_match(s, (unsigned char) strlen(s));
}

static void test_color(void)
{
    puts("color");

    eq_int("lavender",  match("Lavender      "), COL_LAVENDER);
    eq_int("sage",      match("Sage          "), COL_SAGE);
    eq_int("grape",     match("Grape         "), COL_GRAPE);
    eq_int("flamingo",  match("Flamingo      "), COL_FLAMINGO);
    eq_int("banana",    match("Banana        "), COL_BANANA);
    eq_int("tangerine", match("Tangerine     "), COL_TANGERINE);
    eq_int("peacock",   match("Peacock       "), COL_PEACOCK);
    eq_int("graphite",  match("Graphite      "), COL_GRAPHITE);
    eq_int("blueberry", match("Blueberry     "), COL_BLUEBERRY);
    eq_int("basil",     match("Basil         "), COL_BASIL);
    eq_int("tomato",    match("Tomato        "), COL_TOMATO);

    eq_int("upper", match("TOMATO"), COL_TOMATO);
    eq_int("mixed", match("ToMaTo"), COL_TOMATO);

    /* Prefix matching would get all of these wrong. */
    eq_int("grape vs graphite", match("Graphite"), COL_GRAPHITE);
    eq_int("basil not banana",  match("Basil"), COL_BASIL);
    eq_int("blueberry",         match("Blueberry"), COL_BLUEBERRY);

    /* A calendar named after a colour plus something else is a calendar. */
    eq_int("tomato soup", match("Tomato Soup"), COL_NONE);
    eq_int("sagebrush",   match("Sagebrush"), COL_NONE);

    eq_int("unknown", match("Work"), COL_NONE);
    eq_int("empty",   match(""), COL_NONE);

    /* The column is the last thing on a truncated final line as often as not,
       so a name that ends exactly at end-of-line still has to match. */
    eq_int("no trailing space", color_match("Peacock", 7), COL_PEACOCK);

    eq_int("chip tomato",  color_chip(COL_TOMATO), CHIP_RED);
    eq_int("chip banana",  color_chip(COL_BANANA), CHIP_YELLOW);
    eq_int("chip basil",   color_chip(COL_BASIL), CHIP_GREEN);
    eq_int("chip peacock", color_chip(COL_PEACOCK), CHIP_BLUE);
    eq_int("chip graphite", color_chip(COL_GRAPHITE), CHIP_GRAPHITE);
    eq_int("chip none",    color_chip(COL_NONE), CHIP_BLUE);

    eq_str("name lavender",  color_name(COL_LAVENDER), "LAVENDER");
    eq_str("name tangerine", color_name(COL_TANGERINE), "TANGERINE");
    eq_str("name blueberry", color_name(COL_BLUEBERRY), "BLUEBERRY");
    eq_str("name tomato",    color_name(COL_TOMATO), "TOMATO");

    /* COL_NONE is not a colour, it is "the category was a calendar name", so
       there is nothing to name -- the event's own cat[] holds that. */
    eq_str("name none", color_name(COL_NONE), "");
    eq_str("name out of range", color_name(200), "");
}

/* ------------------------------------------------------------------ */
/* index.c                                                             */
/* ------------------------------------------------------------------ */

static void feed(const char *s)
{
    idx_line(s, (unsigned char) strlen(s));
}

/*
 * Real wire bytes. dashed() replaces every space in the header with '-', so
 * the '#' is the only surviving character and it sits at 2 + numW - 1.
 *
 * DAY, numW = 1:  used = 2+1+1+11 = 15, timec = 4, catc = 16, titlec = 31.
 */
static void test_index_day(void)
{
    puts("index: day");

    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("--#-Time--------Category-------Event---------------------------------------");
    feed("  1 09:00-10:00 Peacock        Standup");
    feed("* 2 all day     Tomato         Company holiday");
    feed("~ 3 14:00-14:30 Work           Weekly review");
    feed("  4 23:30->     Banana         Overnight bake");

    eq_str("window title", gc_wtitle, "Fri 28 Aug 2026");
    eq_int("count", gc_count, 4);
    eq_int("trunc", gc_trunc, 0);

    eq_str("e0 num", gc_index[0].num, "1");
    eq_str("e0 title", gc_index[0].title, "Standup");
    eq_int("e0 sh", gc_index[0].sh, 9);
    eq_int("e0 sm", gc_index[0].sm, 0);
    eq_int("e0 eh", gc_index[0].eh, 10);
    eq_int("e0 em", gc_index[0].em, 0);
    eq_int("e0 flags", gc_index[0].flags, 0);
    eq_int("e0 color", gc_index[0].color, COL_PEACOCK);
    eq_int("e0 chip", gc_index[0].chip, CHIP_BLUE);
    eq_int("e0 day", gc_index[0].day, 0);

#ifdef GC_KEEP_CAT
    /* The category column verbatim, padding gone. It is worth keeping next to
       the colour because for an uncoloured event it is the *calendar* name,
       which the colour cannot say. */
    eq_str("e0 cat", gc_index[0].cat, "Peacock");
#endif

    eq_int("e1 allday", gc_index[1].flags & EVF_ALLDAY, EVF_ALLDAY);
    eq_int("e1 sh", gc_index[1].sh, 0);
    eq_str("e1 title", gc_index[1].title, "Company holiday");
    eq_int("e1 chip", gc_index[1].chip, CHIP_RED);

    eq_int("e2 recurring", gc_index[2].flags & EVF_RECURRING, EVF_RECURRING);
    eq_int("e2 color none", gc_index[2].color, COL_NONE);
#ifdef GC_KEEP_CAT
    eq_str("e2 cat is the calendar name", gc_index[2].cat, "Work");
#endif

    eq_int("e3 openend", gc_index[3].flags & EVF_OPENEND, EVF_OPENEND);
    eq_int("e3 sh", gc_index[3].sh, 23);
    eq_int("e3 sm", gc_index[3].sm, 30);
    eq_int("e3 eh mirrors start", gc_index[3].eh, 23);
}

/* numW = 3: used = 2+3+1+11 = 17, timec = 6, catc = 18, titlec = 33. */
static void test_index_numw3(void)
{
    puts("index: numW = 3");

    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("----#-Time--------Category-------Event-----------------------------------");
    feed("  128 09:00-10:00 Peacock        Standup");

    eq_int("count", gc_count, 1);
    eq_str("num", gc_index[0].num, "128");
    eq_int("sh", gc_index[0].sh, 9);
    eq_str("title", gc_index[0].title, "Standup");
}

/* WEEK, numW = 1: used = 2+1+1+3+1+11 = 19, datec = 4, timec = 8,
   catc = 20, titlec = 35. */
static void test_index_week(void)
{
    puts("index: week");

    idx_reset(VIEW_WEEK);
    feed("Week of Sun 23 Aug 2026");
    feed("--#-Dat-Time--------Category-------Event-------------------------------");
    feed("  1 Sun 09:00-10:00 Peacock        Church");
    feed("  2 Tue 11:00-12:00 Basil          Dentist");
    feed("  3 Thu 13:00-14:00 Banana         Lunch");
    feed("  4 Sat 15:00-16:00 Tomato         Match");

    eq_int("count", gc_count, 4);
    eq_int("sun", gc_index[0].day, 0);
    eq_int("tue", gc_index[1].day, 2);
    eq_int("thu", gc_index[2].day, 4);
    eq_int("sat", gc_index[3].day, 6);
    eq_str("title", gc_index[1].title, "Dentist");
    eq_int("sh", gc_index[3].sh, 15);
}

/* AGENDA, numW = 1: used = 2+1+1+6+1+11 = 22, datec = 4, timec = 11,
   catc = 23, titlec = 38. */
static void test_index_agenda(void)
{
    puts("index: agenda");

    idx_reset(VIEW_AGENDA);
    feed("Agenda from 28 Aug 2026");
    feed("--#-Date---Time--------Category-------Event----------------------------");
    feed("  1 28 Aug 09:00-10:00 Peacock        Standup");
    feed("  2 28 Aug 14:00-15:00 Basil          Review");
    feed("  3 01 Sep 09:00-10:00 Tomato         Kickoff");

    eq_int("count", gc_count, 3);
    eq_int("e0 day", gc_index[0].day, 28);
    eq_int("e0 mon", gc_index[0].mon, 8);
    eq_int("e2 day", gc_index[2].day, 1);
    eq_int("e2 mon", gc_index[2].mon, 9);
    eq_str("e2 title", gc_index[2].title, "Kickoff");
}

/* MONTH, numW = 1: used = 2+1+1+5+1+11 = 21, datec = 4, timec = 10,
   catc = 22, titlec = 37. MONTH stores no events, only tallies. */
static void test_index_month(void)
{
    puts("index: month");

    idx_reset(VIEW_MONTH);
    feed("August 2026");
    feed("--#-Date--Time--------Category-------Event-----------------------------");
    feed("  1 Fr 28 09:00-10:00 Peacock        Standup");
    feed("  2 Fr 28 14:00-15:00 Tomato         Review");
    feed("  3 Sa 29 09:00-10:00 Banana         Market");

    eq_str("window title", gc_wtitle, "August 2026");
    eq_int("total", gc_count, 3);
    eq_int("day 28 count", gc_daycnt[28], 2);
    eq_int("day 29 count", gc_daycnt[29], 1);
    eq_int("day 27 count", gc_daycnt[27], 0);

    /* The adapter sorts by start time, so the first row for a day is that
       day's leading event and its colour is the one worth keeping. */
    eq_int("day 28 chip is the earliest", gc_daychip[28], CHIP_BLUE);
    eq_int("day 29 chip", gc_daychip[29], CHIP_YELLOW);
}

static void test_index_edges(void)
{
    unsigned int i;
    char row[96];

    puts("index: edges");

    /* An empty window prints a line with no columns at all. */
    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("--#-Time--------Category-------Event------------------------------------");
    feed("  (no events)");
    eq_int("no events", gc_count, 0);

    /* A final line cut short past the title column is dropped rather than
       parsed out of whatever happens to follow in memory. */
    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("--#-Time--------Category-------Event------------------------------------");
    feed("  1 09:00-10:00 Peaco");
    eq_int("short line", gc_count, 0);

    /* A title that runs past TITLE_LEN is clipped, not overrun. */
    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("--#-Time--------Category-------Event------------------------------------");
    feed("  1 09:00-10:00 Peacock        "
         "0123456789012345678901234567890123456789012345678901234567890");
    eq_int("clipped length", (long) strlen(gc_index[0].title), TITLE_LEN - 1);

#ifdef GC_KEEP_CAT
    /* A category filling the whole column keeps all of it, and one running
       past the column is clipped to it rather than eating into the title. */
    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("--#-Time--------Category-------Event------------------------------------");
    feed("  1 09:00-10:00 FourteenChars_ Standup");
    eq_str("full-width cat", gc_index[0].cat, "FourteenChars_");
    eq_str("title after it", gc_index[0].title, "Standup");

    /* An event with neither a colour nor a category leaves the column blank,
       which has to come out empty rather than as fourteen spaces. */
    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("--#-Time--------Category-------Event------------------------------------");
    feed("  1 09:00-10:00                Standup");
    eq_str("blank cat", gc_index[0].cat, "");
    eq_int("blank cat is COL_NONE", gc_index[0].color, COL_NONE);
    eq_str("title still found", gc_index[0].title, "Standup");
#endif

    /* Past MAX_EVENTS the flag goes up and nothing is written past the end. */
    idx_reset(VIEW_DAY);
    feed("Fri 28 Aug 2026");
    feed("--#-Time--------Category-------Event------------------------------------");
    for (i = 0; i < MAX_EVENTS + 5; i++) {
        sprintf(row, "  1 09:00-10:00 Peacock        Event %u", i);
        feed(row);
    }
    eq_int("capped", gc_count, MAX_EVENTS);
    eq_int("trunc set", gc_trunc, 1);
}

/* ------------------------------------------------------------------ */
/* agenda.c                                                            */
/* ------------------------------------------------------------------ */

static void stage(unsigned char n, const unsigned char *days,
                  const unsigned char *mons)
{
    unsigned char i;

    gc_count = n;
    for (i = 0; i < n; i++) {
        gc_index[i].day = days[i];
        gc_index[i].mon = mons[i];
    }
}

static void test_agenda(void)
{
    static const unsigned char d1[] = { 28, 28, 28 };
    static const unsigned char m1[] = {  8,  8,  8 };
    static const unsigned char d2[] = { 28, 29, 30 };
    static const unsigned char m2[] = {  8,  8,  8 };
    static const unsigned char d3[] = { 31,  1 };
    static const unsigned char m3[] = {  8,  9 };
    unsigned char days[MAX_EVENTS], mons[MAX_EVENTS];
    unsigned char i;

    puts("agenda");

    stage(3, d1, m1);
    agenda_build();
    eq_int("one group rows", gc_agd_count, 4);
    eq_int("row0 is a separator", gc_agd[0] & AGD_SEP, AGD_SEP);
    eq_int("separator points at the first of the group", gc_agd[0] & AGD_IDX, 0);
    eq_int("row1 is an event", gc_agd[1] & AGD_SEP, 0);
    eq_int("row3 event index", gc_agd[3] & AGD_IDX, 2);

    stage(3, d2, m2);
    agenda_build();
    eq_int("a group per event", gc_agd_count, 6);

    /* A separator has to fire on the month as well as the day, or 31 Aug and
       31 Sep would share one heading. */
    stage(2, d3, m3);
    agenda_build();
    eq_int("month boundary rows", gc_agd_count, 4);

    /* Same day of month, different month: still two groups. */
    {
        static const unsigned char d4[] = { 15, 15 };
        static const unsigned char m4[] = {  8,  9 };
        stage(2, d4, m4);
        agenda_build();
        eq_int("same dom different month", gc_agd_count, 4);
    }

    /* Overflow stops at the buffer rather than running past it. */
    for (i = 0; i < MAX_EVENTS; i++) {
        days[i] = (unsigned char) (i + 1);
        mons[i] = 1;
    }
    stage(MAX_EVENTS, days, mons);
    agenda_build();
    eq_int("overflow capped", gc_agd_count <= AGD_MAX, 1);
}

/* ------------------------------------------------------------------ */
/* alarm.c                                                             */
/* ------------------------------------------------------------------ */

/*
 * alarm.c is the one piece the emulator harness cannot reach: the banner needs
 * the view loop to keep turning, and a headless run has to block somewhere.
 * Its firing rules are also the subtlest thing in the program, so they are
 * tested here instead, against stubs for the handful of things a scan touches.
 */
static int tones;

unsigned char clk_is_today(unsigned int y, unsigned char mo, unsigned char d)
{
    return (unsigned char) (clk_ok && y == clk_y && mo == clk_mo &&
                            d == clk_d);
}

void ui_alarm(unsigned char phase)      { (void) phase; }
void ui_hints(unsigned char view)       { (void) view; }
void plat_tone(unsigned char note)      { (void) note; tones++; }
void plat_silence(void)                 { }

/* One timed event at hh:mm, on the day the clock and the anchor both name. */
static void stage_alarm(unsigned char hh, unsigned char mm,
                        unsigned char flags)
{
    alarm_reset();

    clk_ok = 1;
    clk_y = 2026; clk_mo = 8; clk_d = 28;
    clk_h = 9; clk_mi = 0;

    cur_y = 2026; cur_mo = 8; cur_d = 28;

    al_lead = 10;
    gc_count = 1;
    memset(&gc_index[0], 0, sizeof(gc_index[0]));
    gc_index[0].sh = hh;
    gc_index[0].sm = mm;
    gc_index[0].flags = flags;
    strcpy(gc_index[0].title, "Standup");
}

static void test_alarm(void)
{
    puts("alarm");

    /* Inside the lead window: fires, and marks the event so it cannot fire
       twice off the same listing. */
    stage_alarm(9, 5, 0);
    eq_int("fires inside the lead", alarm_scan(VIEW_DAY), 0);
    eq_int("marked fired", gc_index[0].flags & EVF_FIRED, EVF_FIRED);

    /* Exactly at the lead boundary still counts. */
    stage_alarm(9, 10, 0);
    eq_int("fires at the boundary", alarm_scan(VIEW_DAY), 0);

    stage_alarm(9, 11, 0);
    eq_int("silent beyond the lead", alarm_scan(VIEW_DAY), AL_NONE);

    /* An alarm for something already underway is noise. */
    stage_alarm(8, 59, 0);
    eq_int("silent once started", alarm_scan(VIEW_DAY), AL_NONE);

    /* An all-day event has no meaningful start minute. */
    stage_alarm(9, 5, EVF_ALLDAY);
    eq_int("silent for all-day", alarm_scan(VIEW_DAY), AL_NONE);

    stage_alarm(9, 5, EVF_FIRED);
    eq_int("silent once fired", alarm_scan(VIEW_DAY), AL_NONE);

    /* Only the day view knows how to paint the footer back, so a banner
       raised anywhere else would be stranded. */
    stage_alarm(9, 5, 0);
    eq_int("silent in week", alarm_scan(VIEW_WEEK), AL_NONE);
    stage_alarm(9, 5, 0);
    eq_int("silent in month", alarm_scan(VIEW_MONTH), AL_NONE);
    stage_alarm(9, 5, 0);
    eq_int("silent in agenda", alarm_scan(VIEW_AGENDA), AL_NONE);

    /* Browsing tomorrow must not set off tomorrow's alarms today. */
    stage_alarm(9, 5, 0);
    cur_d = 29;
    eq_int("silent when browsing another day", alarm_scan(VIEW_DAY), AL_NONE);

    stage_alarm(9, 5, 0);
    clk_ok = 0;
    eq_int("silent without a clock", alarm_scan(VIEW_DAY), AL_NONE);

    stage_alarm(9, 5, 0);
    gc_count = 0;
    eq_int("silent with no events", alarm_scan(VIEW_DAY), AL_NONE);

    /* The banner runs itself down and hands the footer back. */
    stage_alarm(9, 5, 0);
    alarm_scan(VIEW_DAY);
    eq_int("banner up", al_active, 1);
    tones = 0;
    {
        int i;
        for (i = 0; i < 400 && alarm_step(VIEW_DAY); i++)
            ;
        eq_int("banner ends on its own", al_active, 0);
        eq_int("three notes", tones, 3);
    }

    /* Dismissing tears down on the next step rather than leaving sound on. */
    stage_alarm(9, 5, 0);
    alarm_scan(VIEW_DAY);
    alarm_dismiss();
    alarm_step(VIEW_DAY);
    eq_int("dismissed", al_active, 0);
}

/* ------------------------------------------------------------------ */
/* wrap.c and sanitize.c                                               */
/* ------------------------------------------------------------------ */

static void test_sanitize(void)
{
    char b[16];

    puts("sanitize");

    copy_san(b, "hello", sizeof(b));
    eq_str("plain", b, "hello");

    copy_san(b, "a\tb\x01\x1f" "c", sizeof(b));
    eq_str("controls to space", b, "a b  c");

    /* Each run of non-ASCII collapses to one '?', so a UTF-8 title does not
       explode into a row of question marks. */
    copy_san(b, "Jos\xc3\xa9", sizeof(b));
    eq_str("utf-8 run", b, "Jos?");

    copy_san(b, "0123456789abcdefghij", sizeof(b));
    eq_int("clipped", (long) strlen(b), 15);
}

static void test_wrap(void)
{
    char rows[6][21];
    unsigned int n;

    puts("wrap");

    n = wrap_text("the quick brown fox", (char *) rows, 6, 20, 21);
    eq_int("fits one row", n, 1);
    eq_str("row0", rows[0], "the quick brown fox");

    n = wrap_text("the quick brown fox jumps over", (char *) rows, 6, 20, 21);
    eq_int("two rows", n, 2);
    eq_str("row0", rows[0], "the quick brown fox");
    eq_str("row1", rows[1], "jumps over");

    n = wrap_text("", (char *) rows, 6, 20, 21);
    eq_int("empty yields one blank row", n, 1);
    eq_str("blank", rows[0], "");

    /* Rows past the return value are left alone, not blanked. Callers that
       reuse one buffer have to honour the count or they redraw the previous
       string's tail -- which is exactly what the event detail panel did. */
    strcpy(rows[1], "stale");
    n = wrap_text("short", (char *) rows, 6, 20, 21);
    eq_int("one row", n, 1);
    eq_str("row beyond the count is untouched", rows[1], "stale");
}

/* ------------------------------------------------------------------ */
/* lines.c                                                             */
/* ------------------------------------------------------------------ */

static char split_seen[8][96];
static int  split_count;

static void split_take(const char *p, unsigned char len)
{
    if (split_count < 8) {
        memcpy(split_seen[split_count], p, len);
        split_seen[split_count][len] = '\0';
        split_count++;
    }
}

static void feed_bytes(const char *s)
{
    split_lines((const unsigned char *) s, (unsigned int) strlen(s),
                split_take);
}

static void test_lines(void)
{
    puts("lines");

    /*
     * Protocol.h's lineEnding is per bus and they do not agree: SIO leaves it
     * at $9B, but iwm/network.cpp -- the Apple II's -- sets CR, and so do
     * drivewire and adamnet. Treating CR as the leading half of a CRLF and
     * dropping it ran every line of an Apple II listing into the next: the
     * window title swallowed the header row and not one event was parsed.
     */
    split_reset(); split_count = 0;
    feed_bytes("one\x9B""two\x9B""three\x9B");
    eq_int("9B count", split_count, 3);
    eq_str("9B row0", split_seen[0], "one");
    eq_str("9B row2", split_seen[2], "three");

    split_reset(); split_count = 0;
    feed_bytes("one\rtwo\rthree\r");
    eq_int("CR count", split_count, 3);
    eq_str("CR row0", split_seen[0], "one");
    eq_str("CR row2", split_seen[2], "three");

    split_reset(); split_count = 0;
    feed_bytes("one\ntwo\n");
    eq_int("LF count", split_count, 2);
    eq_str("LF row1", split_seen[1], "two");

    /* A CRLF is one break, not two -- or every listing gains a blank line
       between each pair and the parser's line numbering slides. */
    split_reset(); split_count = 0;
    feed_bytes("one\r\ntwo\r\nthree\r\n");
    eq_int("CRLF count", split_count, 3);
    eq_str("CRLF row1", split_seen[1], "two");

    /* Only the LF *immediately* after a CR is swallowed. */
    split_reset(); split_count = 0;
    feed_bytes("one\r\n\ntwo\r\n");
    eq_int("CR LF LF count", split_count, 3);
    eq_str("blank between", split_seen[1], "");

    /* A chunk boundary inside a CRLF pair is the normal case, not the odd
       one: the reply arrives in 512-byte reads. */
    split_reset(); split_count = 0;
    feed_bytes("one\r");
    feed_bytes("\ntwo\r");
    eq_int("split CRLF count", split_count, 2);
    eq_str("split CRLF row1", split_seen[1], "two");

    /* A last line with no terminator is kept. */
    split_reset(); split_count = 0;
    feed_bytes("one\rno terminator");
    split_finish(split_take);
    eq_int("unterminated count", split_count, 2);
    eq_str("unterminated", split_seen[1], "no terminator");

    /* Nothing pending means nothing emitted. */
    split_reset(); split_count = 0;
    feed_bytes("one\r");
    split_finish(split_take);
    eq_int("no phantom line", split_count, 1);
}

/* ------------------------------------------------------------------ */
/* detail.c                                                            */
/* ------------------------------------------------------------------ */

static void ingest(const char *s)
{
    detail_ingest((const unsigned char *) s, (unsigned int) strlen(s));
}

static void test_detail(void)
{
    static const unsigned char eol = 0x9B;

    puts("detail");

    /* The first two lines are the summary and the when line, and the event
       screen already prints both from the index record. */
    detail_reset();
    ingest("Standup");
    detail_ingest(&eol, 1);
    ingest("Fri 28 Aug 2026 09:00-10:00");
    detail_ingest(&eol, 1);
    ingest("Where: Room 3");
    detail_ingest(&eol, 1);
    ingest("Bring the roadmap.");
    detail_finish();

    eq_int("summary and when dropped", gc_det_rows, 2);
    eq_str("first kept row", gc_det[0], "Where: Room 3");
    eq_str("second kept row", gc_det[1], "Bring the roadmap.");

    /* Text the adapter already wrapped to 38 columns has to pass through
       untouched -- that is what BUILD_ATARI sends. */
    detail_reset();
    ingest("skip1");
    detail_ingest(&eol, 1);
    ingest("skip2");
    detail_ingest(&eol, 1);
    ingest("A line of exactly thirty-eight chars.");
    detail_finish();
    eq_int("38-col passthrough rows", gc_det_rows, 1);
    eq_str("38-col passthrough", gc_det[0], "A line of exactly thirty-eight chars.");

    /* An 80-column line, which is what fujinet-pc sends, splits cleanly. */
    detail_reset();
    ingest("skip1");
    detail_ingest(&eol, 1);
    ingest("skip2");
    detail_ingest(&eol, 1);
    ingest("aaaa bbbb cccc dddd eeee ffff gggg hhhh "
           "iiii jjjj kkkk llll mmmm nnnn oooo pppp");
    detail_finish();
    eq_int("80-col splits", gc_det_rows, 2);

    /* CRLF must not produce a blank row between every line. */
    detail_reset();
    ingest("skip1\r\nskip2\r\nkept one\r\nkept two");
    detail_finish();
    eq_int("crlf rows", gc_det_rows, 2);
    eq_str("crlf row0", gc_det[0], "kept one");

    /* A reply with no terminator on its last line still keeps it. */
    detail_reset();
    ingest("skip1");
    detail_ingest(&eol, 1);
    ingest("skip2");
    detail_ingest(&eol, 1);
    ingest("no terminator");
    detail_finish();
    eq_int("unterminated kept", gc_det_rows, 1);
    eq_str("unterminated", gc_det[0], "no terminator");
}

#ifdef DET_REFLOW
/*
 * Paragraph reflow, which only an 80-column backend turns on. The fixtures
 * below are shaped exactly the way format_detail() shapes a reply: summary,
 * when line, the Repeats/Category/Where block, a blank line, then the
 * description put through append_wrapped().
 */

/*
 * Rows `first`..`last` glued back together with single spaces.
 *
 * Reflow is about undoing the *adapter's* breaks, not about where ours land,
 * so that is what these assert on. Checking exact 78-column rows instead would
 * be a test of wrap_text, which test_wrap already covers, and would have to be
 * rewritten every time DET_COLS moved.
 */
static const char *joined(unsigned int first, unsigned int last)
{
    static char buf[DET_ROWS * DET_STRIDE];
    unsigned int i;

    buf[0] = '\0';
    for (i = first; i <= last && i < gc_det_rows; i++) {
        if (i > first)
            strcat(buf, " ");
        strcat(buf, gc_det[i]);
    }

    return buf;
}

/* No row may exceed the width it was wrapped to. */
static void rows_fit(const char *what)
{
    unsigned int i;

    for (i = 0; i < gc_det_rows; i++) {
        if (strlen(gc_det[i]) > DET_COLS) {
            checks++;
            failures++;
            printf("  FAIL %s: row %u is %u wide, max %u\n",
                   what, i, (unsigned) strlen(gc_det[i]), (unsigned) DET_COLS);
            return;
        }
    }

    checks++;
}

static void test_detail_reflow(void)
{
    puts("detail: reflow");

    /* Four lines the adapter wrapped at 38 rejoin into one paragraph. The
       short last line is what ends it. */
    detail_reset();
    ingest("Standup\x9B""Fri 28 Aug 2026 09:00-10:00\x9B");
    ingest("Round the table, two minutes each.\x9B");
    ingest("Bring the roadmap printout; we are\x9B");
    ingest("going to mark up the Q4 slip on it\x9B");
    ingest("before lunch.\x9B");
    detail_finish();
    eq_str("38-col paragraph rejoined", joined(0, gc_det_rows - 1),
           "Round the table, two minutes each. Bring the roadmap printout; "
           "we are going to mark up the Q4 slip on it before lunch.");
    rows_fit("38-col paragraph");
    /* Four wire lines of 34 do not fit on two rows of 40, so this is also the
       assertion that the join actually happened rather than passing through. */
    eq_int("38-col paragraph rows", gc_det_rows, 2);

    /* A blank line ends the paragraph and survives as a blank row, or two
       paragraphs run together into a wall of text. */
    detail_reset();
    ingest("Standup\x9B""Fri 28 Aug 2026 09:00-10:00\x9B");
    ingest("Round the table, two minutes each.\x9B");
    ingest("Bring the roadmap.\x9B");
    ingest("\x9B");
    ingest("Dial-in is the usual bridge.\x9B");
    detail_finish();
    eq_int("blank splits into three rows", gc_det_rows, 3);
    eq_str("para one", gc_det[0],
           "Round the table, two minutes each. Bring the roadmap.");
    eq_str("blank row kept", gc_det[1], "");
    eq_str("para two", gc_det[2], "Dial-in is the usual bridge.");

    /*
     * The header block. Each line is a field in its own right, and a long one
     * must not swallow the next -- the one case the length rule cannot see
     * coming, so the prefixes are tested for by name.
     */
    detail_reset();
    ingest("Standup\x9B""Fri 28 Aug 2026 09:00-10:00\x9B");
    ingest("Repeats\x9B");
    ingest("Category: Personal Projects And Errands\x9B");
    ingest("Where: Room 3, second floor\x9B");
    detail_finish();
    eq_int("header rows", gc_det_rows, 3);
    eq_str("repeats alone", gc_det[0], "Repeats");
    eq_str("category alone", gc_det[1],
           "Category: Personal Projects And Errands");
    eq_str("where alone", gc_det[2], "Where: Room 3, second floor");

    /* Nothing is joined until a line long enough to estimate the adapter's
       wrap width has been seen, so a reply of nothing but short lines passes
       straight through. */
    detail_reset();
    ingest("Standup\x9B""Fri 28 Aug 2026 09:00-10:00\x9B");
    ingest("one\x9B""two\x9B""three\x9B");
    detail_finish();
    eq_int("short lines stand alone", gc_det_rows, 3);
    eq_str("short row0", gc_det[0], "one");
    eq_str("short row2", gc_det[2], "three");

    /*
     * The same rule at the other width. A fujinet-pc RS232 build wraps at 80,
     * where a 60-character line is a continuation rather than an end -- which
     * a fixed threshold tuned for 38 would get backwards.
     */
    detail_reset();
    ingest("Standup\x9B""Fri 28 Aug 2026 09:00-10:00\x9B");
    ingest("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm\x9B");
    ingest("nnnn oooo pppp qqqq rrrr ssss tttt uuuu vvvv wwww xxxx yyyy zzzz\x9B");
    ingest("tail.\x9B");
    detail_finish();
    eq_str("80-col paragraph rejoined", joined(0, gc_det_rows - 1),
           "aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm "
           "nnnn oooo pppp qqqq rrrr ssss tttt uuuu vvvv wwww xxxx yyyy zzzz "
           "tail.");
    rows_fit("80-col paragraph");

    /* The summary is the one line format_detail() emits unwrapped, so it can
       be longer than anything else in the reply. Joining it to the when line
       would drop one line where two were meant to go, taking the first kept
       line down with it. */
    detail_reset();
    ingest("A rather long event summary that runs well past the wrap width\x9B");
    ingest("Fri 28 Aug 2026 09:00-10:00\x9B");
    ingest("Where: Room 3\x9B");
    detail_finish();
    eq_int("skip still drops exactly two", gc_det_rows, 1);
    eq_str("first kept after long summary", gc_det[0], "Where: Room 3");
}
#endif /* DET_REFLOW */

/* ------------------------------------------------------------------ */

int main(void)
{
    test_date();
    test_color();
    test_index_day();
    test_index_numw3();
    test_index_week();
    test_index_agenda();
    test_index_month();
    test_index_edges();
    test_agenda();
    test_alarm();
    test_sanitize();
    test_wrap();
    test_lines();
    test_detail();
#ifdef DET_REFLOW
    test_detail_reflow();
#endif

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
