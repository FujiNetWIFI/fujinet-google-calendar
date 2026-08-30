/*
 * The four calendar views, the event detail screen, and the alarm banner.
 *
 * Every list row paints columns 1 to 31 and leaves column 0 alone. That column
 * is the chip gutter, and here the reason is the plainest of the four: the
 * chip's attribute byte is a Google colour and the selection bar's is dark
 * blue, so a chip inside the bar would stop being the event's colour. The
 * Atari keeps its column 0 out because an inverse space is COLPF1 and covers
 * the player; the Apple because MouseText has no inverse form; the CoCo
 * because XOR $40 on a semigraphics byte recolours it; the Intellivision
 * because the colour-stack run has to continue past the selection. Five
 * machines, five unrelated reasons, one rule.
 *
 * Three things here are better than on any other backend, and all three come
 * from the same two facts -- fifteen inks per cell, and seventeen content rows
 * because the SmartKeys carry the hints:
 *
 *   colour  every event shows its own Google colour rather than one of five
 *           quantised chips. color_chip() is never called in this directory.
 *   WEEK    the chip strip the Intellivision invented and the 40-column
 *           backends dropped, over a seven-row panel rather than the CoCo's
 *           four.
 *   MONTH   a real six-by-seven grid with a 32-step density bar under each
 *           day, in the true colour of that day's leading event -- against the
 *           CoCo's 16 steps quantised to five colours and the Atari's four
 *           monochrome ones.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* DAY and AGENDA: fourteen list rows, then the two-row panel. */
#define LIST_TOP        CONTENT_TOP                     /* 4  */
#define PANEL_ROW       18                              /* 18-19 */

/* Columns shared by the DAY and AGENDA rows. */
#define COL_CHIP        0
#define COL_MARK        1
#define COL_TIME        3
#define W_TIME          7
#define TIMEBUF         (W_TIME + 1)    /* "All day" is the longest, at 7 */
#define COL_TITLE       11
#define W_TITLE         (SCR_COLS - COL_TITLE)          /* 21 */

/*
 * WEEK: seven day rows, a rule, then the selected day's events.
 *
 * There is no caret and no lead-event title. The bar is the cursor, and the
 * chip strip says more about a day than the name of one of its events does.
 */
#define WEEK_TOP        CONTENT_TOP                     /* 4..10 */
#define WEEK_DAYS       7
#define WEEK_RULE       11
#define WEEK_PANEL_HDR  12
#define WEEK_PANEL_TOP  13
#define WEEK_PANEL_ROWS (CONTENT_BOT - WEEK_PANEL_TOP + 1)      /* 7 */
#define WK_DOW_COL      1
#define WK_DAY_COL      5
#define WK_CNT_COL      8
#define WK_CHIP_COL     10
#define WK_CHIPS        (SCR_COLS - WK_CHIP_COL)        /* 22 */

/* MONTH: six bands on a two-row pitch, then the selected day spelled out. */
#define MO_TOP          CONTENT_TOP                     /* 4..15 */
#define MO_PITCH        2
#define MO_CELL_W       4
#define MO_LEFT         2
#define MO_SUM_RULE     16
#define MO_SUM_ROW      17

/*
 * The density bar is four cells -- thirty-two pixel columns -- and each event
 * lights four of them, so eight events fill it.
 *
 * The step is what makes the bar readable, not the range. At one pixel per
 * event the bar would count to thirty-two and a two-event day would be a
 * smudge; a calendar day is almost never busier than eight, and the exact
 * count is on the summary line underneath for the days that are.
 */
#define MO_BAR_STEP     4
#define MO_BARS         32

/* Event detail: DET_WIN rows below the three-row head and its rule. */
#define DET_TOP         CONTENT_TOP                     /* 4 */

static char sbuf[64];
static char wrapbuf[2][SCR_COLS + 1];

/* The event the alarm banner last wrote glyphs for; see ui_alarm(). */
static unsigned char banner_ev = AL_NONE;

/* Pattern bytes for one cell of a density bar: a four-scanline band with two
   blank rows above and below, so a full bar reads as a bar and not as a block
   of colour indistinguishable from a chip. */
static unsigned char barcell[8];

/* ------------------------------------------------------------------ */
/* Shared row painting                                                 */
/* ------------------------------------------------------------------ */

/*
 * "09:00", or "All day" for an event with no meaningful start minute.
 *
 * Seven columns rather than the five a clock needs, and it costs the title
 * column two. The CoCo abbreviates to "ALLDY" because thirty-two columns of
 * uppercase have nothing to spare; here two characters of title is the
 * cheaper price than a label nobody can read at a glance.
 */
static void time_field(char *dst, const struct event *e)
{
    /* dst is TIMEBUF, which is W_TIME + 1 for exactly this reason: the label
       is as wide as the column, so the two have to move together. */

    if (e->flags & EVF_ALLDAY) {
        strcpy(dst, "All day");
        return;
    }
    ui_hhmm(dst, e->sh, e->sm);
}

/* '~' for recurring, as on the Atari and the Apple. The Adam's font has a
   tilde; the CoCo's sixty-four-glyph ROM does not, which is why that backend
   settles for '+'. */
static char marker(const struct event *e)
{
    if (e->flags & EVF_ALLDAY)
        return '*';
    if (e->flags & EVF_RECURRING)
        return '~';
    return ' ';
}

/*
 * One event on one row, in the layout DAY and AGENDA share. The caller has
 * already decided which event and whether it is selected.
 */
static void draw_event(unsigned char row, unsigned char ev, unsigned char sel)
{
    struct event *e = &gc_index[ev];
    unsigned char attr = sel ? A_SEL : A_BODY;
    char t[TIMEBUF];
    char m[2];

    m[0] = marker(e);
    m[1] = '\0';
    time_field(t, e);

    scr_field(row, COL_MARK, m, 2, attr);
    scr_field(row, COL_TIME, t, W_TIME, attr);
    scr_field(row, COL_TIME + W_TIME, "", 1, attr);
    scr_field(row, COL_TITLE, e->title, W_TITLE, attr);

    /* Last, and outside everything above: this cell is the event's colour and
       nothing the selection does may take it away. */
    scr_cell(row, COL_CHIP, ink_for_color(e->color));
}

/*
 * The two-row panel that spells the selection out in full.
 *
 * The list column is twenty-three characters wide and a Google summary
 * routinely runs to fifty, so this is what earns storing the longer title.
 *
 * wrap_text() writes only the rows it produces and returns how many, so
 * anything past the returned count is the previous selection's text and has to
 * be cleared rather than drawn.
 */
static void draw_panel(unsigned char ev)
{
    struct event *e;
    unsigned char rows, i;
    char t[6];

    if (ev == 0xFF || ev >= gc_count) {
        scr_rows_clear(PANEL_ROW, PANEL_ROW + 1);
        return;
    }

    e = &gc_index[ev];

    sbuf[0] = '\0';
    if (!(e->flags & EVF_ALLDAY)) {
        ui_hhmm(t, e->sh, e->sm);
        strcpy(sbuf, t);
        if (e->flags & EVF_OPENEND) {
            strcat(sbuf, "->");
        } else {
            strcat(sbuf, "-");
            ui_hhmm(t, e->eh, e->em);
            strcat(sbuf, t);
        }
        strcat(sbuf, "  ");
    } else {
        strcpy(sbuf, "All day  ");
    }
    strcat(sbuf, e->title);

    /*
     * Column 0 is the panel's own colour bar -- the selection's colour is
     * worth repeating here, where the chip it came from may be fourteen rows
     * up the screen. So the text starts at column 1 and wraps to 31, not 32:
     * painting the bar over a field that began at column 0 would eat its first
     * character, which on a timed event is the leading zero of the hour.
     */
    rows = (unsigned char) wrap_text(sbuf, (char *) wrapbuf, 2, SCR_COLS - 1,
                                     SCR_COLS + 1);

    for (i = 0; i < 2; i++) {
        scr_field((unsigned char) (PANEL_ROW + i), 1,
                  (i < rows) ? (const char *) wrapbuf[i] : "",
                  (unsigned char) (SCR_COLS - 1), A_BODY);
        scr_cell((unsigned char) (PANEL_ROW + i), 0, ink_for_color(e->color));
    }
}

/*
 * "1-14/23" at the right of the status row. Nothing shares that row except the
 * alarm banner, which takes the whole of it, so there is no hint string here to
 * collide with the way there is on the other three backends.
 *
 * `total` is not always gc_count -- the agenda's rows include date separators,
 * so its window is measured against gc_agd_count or the indicator claims to be
 * showing more rows than there are events.
 */
static void page_indicator(unsigned char first, unsigned char shown,
                           unsigned char total)
{
    char n[6];

    if (total == 0)
        return;

    utoa((unsigned int) (first + 1), sbuf, 10);
    strcat(sbuf, "-");
    utoa((unsigned int) (first + shown), n, 10);
    strcat(sbuf, n);
    strcat(sbuf, "/");
    utoa(total, n, 10);
    strcat(sbuf, n);

    scr_right(STAT_ROW, RIGHT_COL, sbuf, A_DIM);
}

/*
 * The headings row. It is a real column header rather than a rule, which is
 * the other thing seventeen content rows buy: the Atari and the Apple label
 * their columns in the header band, and the CoCo cannot label them at all.
 */
static void col_header(unsigned char view)
{
    scr_fill(RULE_ROW, 0, VDP_INK_GRAY, SCR_COLS);

    switch (view) {
    case VIEW_WEEK:
        scr_text(RULE_ROW, WK_DOW_COL, "Day", A_RULE);
        scr_text(RULE_ROW, WK_CHIP_COL, "Events", A_RULE);
        break;
    case VIEW_MONTH:
        break;                          /* draw_month paints the weekdays */
    default:
        scr_text(RULE_ROW, COL_TIME, "Time", A_RULE);
        scr_text(RULE_ROW, COL_TITLE, "Event", A_RULE);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* DAY                                                                 */
/* ------------------------------------------------------------------ */

static void draw_day(unsigned char sel, unsigned char first)
{
    unsigned char i, ev, shown;

    shown = (gc_count > first) ? (unsigned char) (gc_count - first) : 0;
    if (shown > LIST_ROWS)
        shown = LIST_ROWS;

    for (i = 0; i < LIST_ROWS; i++) {
        if (i >= shown) {
            scr_row_clear((unsigned char) (LIST_TOP + i));
            continue;
        }
        ev = (unsigned char) (first + i);
        draw_event((unsigned char) (LIST_TOP + i), ev,
                   (unsigned char) (ev == sel));
    }

    draw_panel(gc_count ? sel : 0xFF);

    ui_hints(VIEW_DAY);
    page_indicator(first, shown, gc_count);
}

/* ------------------------------------------------------------------ */
/* AGENDA                                                              */
/* ------------------------------------------------------------------ */

static void draw_agenda(unsigned char sel, unsigned char first)
{
    unsigned char i, row, slot, ev, shown;

    shown = (gc_agd_count > first) ? (unsigned char) (gc_agd_count - first) : 0;
    if (shown > LIST_ROWS)
        shown = LIST_ROWS;

    for (i = 0; i < LIST_ROWS; i++) {
        row = (unsigned char) (LIST_TOP + i);

        if (i >= shown) {
            scr_row_clear(row);
            continue;
        }

        slot = gc_agd[first + i];
        ev = (unsigned char) (slot & AGD_IDX);

        if (slot & AGD_SEP) {
            /* A separator is a heading, never a selection, so it takes the
               whole row in the heading colour and has no chip. */
            sbuf[0] = (char) ('0' + gc_index[ev].day / 10);
            sbuf[1] = (char) ('0' + gc_index[ev].day % 10);
            sbuf[2] = ' ';
            strcpy(sbuf + 3, date_mon3(gc_index[ev].mon));
            scr_field(row, 0, "", SCR_COLS, A_RULE);
            scr_text(row, COL_TIME, sbuf, A_RULE);
            continue;
        }

        draw_event(row, ev, (unsigned char) (first + i == sel));
    }

    if (sel < gc_agd_count && !(gc_agd[sel] & AGD_SEP))
        draw_panel((unsigned char) (gc_agd[sel] & AGD_IDX));
    else
        draw_panel(0xFF);

    ui_hints(VIEW_AGENDA);
    page_indicator(first, shown, gc_agd_count);
}

/* ------------------------------------------------------------------ */
/* WEEK                                                                */
/* ------------------------------------------------------------------ */

/*
 * The adapter's WEEK date column is only "Fri", with no day number, so the
 * dates down the left are worked out here: walk back to the week's Sunday and
 * step forward. wkst is left at the adapter's own default of Sunday.
 */
static void week_start(unsigned int *y, unsigned char *mo, unsigned char *d)
{
    unsigned char n;

    *y = cur_y;
    *mo = cur_mo;
    *d = cur_d;

    n = date_dow(*y, *mo, *d);
    while (n--)
        date_subday(y, mo, d);
}

/*
 * Draw the day's whole load as one chip per event, in the event's own Google
 * colour, and return how many there were -- including any past the twenty-two
 * the strip holds, because the count column has to be honest about them.
 *
 * Twenty-two columns will not hold seven days of text side by side, which is
 * what pushed the Intellivision to this at twenty. It reads as a density map at
 * a glance and still names every event's colour -- and here it names the real
 * one, not one of five.
 */
static unsigned char week_day_chips(unsigned char row, unsigned char dow)
{
    unsigned char i, n = 0;

    for (i = 0; i < gc_count; i++) {
        if (gc_index[i].day != dow)
            continue;
        if (n < WK_CHIPS)
            scr_cell(row, (unsigned char) (WK_CHIP_COL + n),
                     ink_for_color(gc_index[i].color));
        n++;
    }

    if (n < WK_CHIPS)
        scr_fill(row, (unsigned char) (WK_CHIP_COL + n), VDP_INK_WHITE,
                 (unsigned char) (WK_CHIPS - n));

    return n;
}

static void draw_week(unsigned char sel)
{
    unsigned int  y;
    unsigned char mo, d;
    unsigned char i, row, n, panel;
    unsigned char attr, dattr;
    char dd[3];

    week_start(&y, &mo, &d);

    for (i = 0; i < WEEK_DAYS; i++) {
        row = (unsigned char) (WEEK_TOP + i);
        attr = (i == sel) ? A_SEL : A_BODY;

        /* Today's date is blue where the rest are black, so the week still
           reads at a glance when the cursor is somewhere else. Under the
           cursor the bar wins -- two markings on one cell say nothing. */
        dattr = (i == sel) ? A_SEL
                           : (clk_is_today(y, mo, d) ? A_TODAY : A_BODY);

        scr_field(row, 0, "", 1, A_BODY);
        scr_field(row, WK_DOW_COL, date_dow3(i), 3, attr);
        scr_field(row, 4, "", 1, attr);

        dd[0] = (char) ('0' + d / 10);
        dd[1] = (char) ('0' + d % 10);
        dd[2] = '\0';
        scr_field(row, WK_DAY_COL, dd, 2, dattr);

        scr_field(row, 7, "", 1, attr);

        scr_field(row, WK_CNT_COL, "", 2, attr);
        n = week_day_chips(row, i);
        if (n) {
            utoa(n, sbuf, 10);
            scr_right(row, (unsigned char) (WK_CNT_COL + 1), sbuf, attr);
        }

        date_addday(&y, &mo, &d);
    }

    scr_fill(WEEK_RULE, 0, VDP_INK_GRAY, SCR_COLS);

    /* The panel below costs no fetch: the WEEK index already carries a day of
       week for every event, so this is a redraw straight out of RAM. */
    week_start(&y, &mo, &d);
    for (i = 0; i < sel; i++)
        date_addday(&y, &mo, &d);

    strcpy(sbuf, date_dow3(sel));
    strcat(sbuf, " ");
    utoa(d, sbuf + strlen(sbuf), 10);
    strcat(sbuf, " ");
    strcat(sbuf, date_mon3(mo));
    scr_field(WEEK_PANEL_HDR, 0, "", SCR_COLS, A_RULE);
    scr_text(WEEK_PANEL_HDR, 1, sbuf, A_RULE);

    panel = 0;
    for (i = 0; i < gc_count && panel < WEEK_PANEL_ROWS; i++) {
        if (gc_index[i].day != sel)
            continue;
        draw_event((unsigned char) (WEEK_PANEL_TOP + panel), i, 0);
        panel++;
    }
    for (; panel < WEEK_PANEL_ROWS; panel++)
        scr_row_clear((unsigned char) (WEEK_PANEL_TOP + panel));

    ui_hints(VIEW_WEEK);
}

/* ------------------------------------------------------------------ */
/* MONTH                                                               */
/* ------------------------------------------------------------------ */

static unsigned char mo_first;      /* weekday of the 1st */
static unsigned char mo_ndays;

static void month_geom(void)
{
    mo_first = date_dow(cur_y, cur_mo, 1);
    mo_ndays = date_dim(cur_y, cur_mo);
}

/*
 * The density bar under one day: four cells, two pixel columns per event, so
 * thirty-two steps -- and drawn in the true colour of the day's leading event
 * rather than in one of five chips, which is what changing gc_daycol from a
 * chip to a COL_* was for.
 *
 * The lit band is four scanlines out of eight so a full bar still reads as a
 * bar. A cell filled top to bottom would be indistinguishable from an event
 * chip, and the two mean different things.
 */
static void month_bar(unsigned char row, unsigned char col, unsigned char n,
                      unsigned char ink)
{
    unsigned char i, k, mask;
    unsigned int  bits = (unsigned int) n * MO_BAR_STEP;

    if (bits > MO_BARS)
        bits = MO_BARS;

    for (i = 0; i < MO_CELL_W; i++) {
        k = (bits > (unsigned int) i * 8)
                ? (unsigned char) (bits - (unsigned int) i * 8) : 0;
        if (k > 8)
            k = 8;

        mask = k ? (unsigned char) (0xFF << (8 - k)) : 0x00;

        barcell[0] = 0x00;
        barcell[1] = 0x00;
        barcell[2] = mask;
        barcell[3] = mask;
        barcell[4] = mask;
        barcell[5] = mask;
        barcell[6] = 0x00;
        barcell[7] = 0x00;

        vdp_vwrite(barcell, PAT_ADDR(row, col + i), 8);
        scr_attr(row, (unsigned char) (col + i), 1, ATTR(ink, VDP_INK_WHITE));
    }
}

/*
 * A day cell is four columns on two rows: the number, and the bar under it.
 *
 * Today is coloured rather than bracketed. The CoCo puts [ ] round it because
 * it has no second way to mark a cell; here today is blue and the selection is
 * the bar, and when they coincide the bar wins.
 */
static void month_cell(unsigned char band, unsigned char dow, unsigned char day,
                       unsigned char sel)
{
    unsigned char col = (unsigned char) (MO_LEFT + dow * MO_CELL_W);
    unsigned char nrow = (unsigned char) (MO_TOP + band * MO_PITCH);
    unsigned char brow = (unsigned char) (nrow + 1);
    unsigned char attr;

    if (day == 0) {
        scr_field(nrow, col, "", MO_CELL_W, A_BODY);
        scr_fill(brow, col, VDP_INK_WHITE, MO_CELL_W);
        return;
    }

    attr = sel ? A_SEL
               : (clk_is_today(cur_y, cur_mo, day) ? A_TODAY : A_BODY);

    sbuf[0] = ' ';
    sbuf[1] = (day < 10) ? ' ' : (char) ('0' + day / 10);
    sbuf[2] = (char) ('0' + day % 10);
    sbuf[3] = ' ';
    sbuf[4] = '\0';
    scr_field(nrow, col, sbuf, MO_CELL_W, attr);

    if (gc_daycnt[day])
        month_bar(brow, col, gc_daycnt[day], ink_for_color(gc_daycol[day]));
    else
        scr_fill(brow, col, VDP_INK_WHITE, MO_CELL_W);
}

/*
 * The selected day spelled out under the grid. The Atari and the Apple show a
 * total for the whole month here; a total for the day under the cursor is
 * worth more, and there is room for both the date and the count.
 */
static void month_summary(void)
{
    char n[6];

    scr_fill(MO_SUM_RULE, 0, VDP_INK_GRAY, SCR_COLS);

    strcpy(sbuf, date_dow3(date_dow(cur_y, cur_mo, cur_d)));
    strcat(sbuf, " ");
    utoa(cur_d, sbuf + strlen(sbuf), 10);
    strcat(sbuf, " ");
    strcat(sbuf, date_mon3(cur_mo));
    strcat(sbuf, "   ");
    if (gc_daycnt[cur_d] == 0) {
        strcat(sbuf, "no events");
    } else {
        utoa(gc_daycnt[cur_d], n, 10);
        strcat(sbuf, n);
        strcat(sbuf, gc_daycnt[cur_d] == 1 ? " event" : " events");
    }

    scr_rows_clear(MO_SUM_ROW, CONTENT_BOT);
    scr_field(MO_SUM_ROW, 1, sbuf, (unsigned char) (SCR_COLS - 1), A_BODY);

    /* The leading event's colour, so the bar in the grid has a key. */
    if (gc_daycnt[cur_d])
        scr_cell(MO_SUM_ROW, 0, ink_for_color(gc_daycol[cur_d]));
}

static void draw_month(void)
{
    static const char *const head[7] = {
        "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
    };
    unsigned char band, dow, day, cell;

    month_geom();

    for (dow = 0; dow < 7; dow++)
        scr_text(RULE_ROW, (unsigned char) (MO_LEFT + dow * MO_CELL_W + 1),
                 head[dow], A_RULE);

    for (band = 0; band < 6; band++) {
        for (dow = 0; dow < 7; dow++) {
            cell = (unsigned char) (band * 7 + dow);
            day = (cell < mo_first) ? 0 : (unsigned char) (cell - mo_first + 1);
            if (day > mo_ndays)
                day = 0;
            month_cell(band, dow, day,
                       (unsigned char) (day && day == cur_d));
        }
    }

    month_summary();
    ui_hints(VIEW_MONTH);
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

void ui_view(unsigned char view, unsigned char sel, unsigned char first)
{
    /* A full repaint takes the status row with it, so the banner's memory of
       what it last drew there has to go too. */
    banner_ev = AL_NONE;

    ui_header(view);
    col_header(view);

    switch (view) {
    case VIEW_WEEK:   draw_week(sel); break;
    case VIEW_MONTH:  draw_month(); break;
    case VIEW_AGENDA: draw_agenda(sel, first); break;
    default:          draw_day(sel, first); break;
    }
}

/*
 * Repaint only what a selection move touched: the row being left, the row
 * being landed on, and the panel. The chips are deliberately not touched -- a
 * chip belongs to an event, not to the selection, so moving the cursor cannot
 * change one. That is what keeps holding a cursor key from flickering the whole
 * list, and it matters more here than anywhere else in the family: a full row
 * on this screen is thirty-two eight-byte writes through a VDP port.
 */
void ui_view_sel(unsigned char view, unsigned char from, unsigned char to,
                 unsigned char first)
{
    unsigned char ev;

    if (view == VIEW_DAY) {
        if (from >= first && from < first + LIST_ROWS && from < gc_count)
            draw_event((unsigned char) (LIST_TOP + from - first), from, 0);
        if (to >= first && to < first + LIST_ROWS && to < gc_count)
            draw_event((unsigned char) (LIST_TOP + to - first), to, 1);
        draw_panel(gc_count ? to : 0xFF);
        return;
    }

    if (view == VIEW_AGENDA) {
        if (from >= first && from < first + LIST_ROWS && from < gc_agd_count &&
            !(gc_agd[from] & AGD_SEP))
            draw_event((unsigned char) (LIST_TOP + from - first),
                       (unsigned char) (gc_agd[from] & AGD_IDX), 0);
        if (to >= first && to < first + LIST_ROWS && to < gc_agd_count &&
            !(gc_agd[to] & AGD_SEP)) {
            ev = (unsigned char) (gc_agd[to] & AGD_IDX);
            draw_event((unsigned char) (LIST_TOP + to - first), ev, 1);
            draw_panel(ev);
        }
        return;
    }

    /* WEEK repaints its day rows and its panel together, and MONTH's cursor
       moves between cells on two rows -- neither is worth an incremental path,
       and both are a redraw out of RAM with no fetch behind them. */
    ui_view(view, to, first);
}

/* ------------------------------------------------------------------ */
/* Event detail                                                        */
/* ------------------------------------------------------------------ */

void ui_detail(unsigned char ev, unsigned int top)
{
    struct event *e = &gc_index[ev];
    unsigned int  i;
    unsigned char rows;
    char t[6];
    char n[6];

    scr_clear();

    /*
     * The mark goes away here rather than moving. Every other screen paints it
     * somewhere -- the views and the two settings screens in the header, the
     * flat screens in the middle -- but this header is three rows of text
     * against a colour bar and has nowhere to put it. scr_clear() would not
     * have removed it: sprites are not in the character planes, so the large
     * mark ui_busy() left behind would still be sitting over the description.
     */
    logo_hide();

    /* Rows 0-2 are built from the index record rather than from the reply, so
       the screen identifies itself before a single byte has arrived. */
    scr_attr(0, 0, SCR_COLS, A_HEADER);
    scr_attr(1, 0, SCR_COLS, A_HEADER);
    scr_attr(2, 0, SCR_COLS, A_HEADER);

    strcpy(sbuf, date_dow3(date_dow(cur_y, cur_mo, cur_d)));
    strcat(sbuf, " ");
    utoa(cur_d, sbuf + strlen(sbuf), 10);
    strcat(sbuf, " ");
    strcat(sbuf, date_mon3(cur_mo));
    strcat(sbuf, "  ");
    if (e->flags & EVF_ALLDAY) {
        strcat(sbuf, "All day");
    } else {
        ui_hhmm(t, e->sh, e->sm);
        strcat(sbuf, t);
        if (e->flags & EVF_OPENEND) {
            strcat(sbuf, "->");
        } else {
            strcat(sbuf, "-");
            ui_hhmm(t, e->eh, e->em);
            strcat(sbuf, t);
        }
    }
    scr_field(0, 2, sbuf, (unsigned char) (SCR_COLS - 2), A_HEADER);

    /* The event's colour, in the two cells the header band leaves at the left.
       It is the one thing about an event the detail text never says. */
    scr_cell(0, 0, ink_for_color(e->color));
    scr_cell(1, 0, ink_for_color(e->color));
    scr_cell(2, 0, ink_for_color(e->color));

    rows = (unsigned char) wrap_text(e->title, (char *) wrapbuf, 2,
                                     SCR_COLS - 2, SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (1 + i), 2,
                  (i < rows) ? (const char *) wrapbuf[i] : "",
                  (unsigned char) (SCR_COLS - 2), A_HEADER);

    scr_fill(RULE_ROW, 0, VDP_INK_GRAY, SCR_COLS);

    for (i = 0; i < DET_WIN; i++) {
        if (top + i < gc_det_rows)
            scr_field((unsigned char) (DET_TOP + i), 0, gc_det[top + i],
                      SCR_COLS, A_BODY);
        else
            scr_row_clear((unsigned char) (DET_TOP + i));
    }

    utoa((unsigned int) (top + 1), sbuf, 10);
    strcat(sbuf, "/");
    utoa(gc_det_rows ? gc_det_rows : 1u, n, 10);
    strcat(sbuf, n);
    if (gc_det_trunc)
        strcat(sbuf, "+");

    scr_row_clear(STAT_ROW);
    scr_right(STAT_ROW, RIGHT_COL, sbuf, A_DIM);

    ui_keys_detail();
}

/* ------------------------------------------------------------------ */
/* Alarm banner                                                        */
/* ------------------------------------------------------------------ */

/*
 * The banner takes over the status row, which is why alarms only fire in a
 * view: a view is the only screen that knows how to paint that row back.
 *
 * The flash is two attribute writes and nothing else. The Atari's display list
 * interrupt picks up two colour bytes and repaints nothing; this is the same
 * trick by another route -- the glyphs stay where they are and only the colour
 * plane is touched, thirty-two bytes twice a second.
 */
void ui_alarm(unsigned char phase)
{
    struct event *e;
    char t[6];

    if (al_ev >= gc_count)
        return;

    /* Only the first paint of a given event writes glyphs. Every flash after
       it is the attribute run alone, which is what makes the claim above
       true -- and keeps a twice-a-second repaint off the VDP port. */
    if (banner_ev != al_ev) {
        banner_ev = al_ev;

        e = &gc_index[al_ev];
        ui_hhmm(t, e->sh, e->sm);

        strcpy(sbuf, "! ");
        strcat(sbuf, t);
        strcat(sbuf, " ");
        strcat(sbuf, e->title);

        scr_field(STAT_ROW, 0, sbuf, SCR_COLS, phase ? A_ALARM : A_ALARM_ALT);
        return;
    }

    scr_attr(STAT_ROW, 0, SCR_COLS, phase ? A_ALARM : A_ALARM_ALT);
}
