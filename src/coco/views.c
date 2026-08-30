/*
 * The four calendar views, the event detail screen, and the alarm banner.
 *
 * Every list row paints columns 1 to 31 and leaves column 0 alone. That column
 * is the chip gutter, where a semigraphics cell carries the event's Google
 * colour, and the selection bar is XOR $40 -- which on a byte >= $80 is part
 * of the colour field, so an inverted chip would silently change hue rather
 * than highlight. The Atari keeps its column 0 out because an inverse space is
 * COLPF1 and covers the player; the Apple because MouseText has no inverse
 * form; the Intellivision because the colour-stack run has to continue past
 * the selection. Four machines, four unrelated reasons, one rule.
 *
 * Two views here are better than they are at 40 or 80 columns, and both for
 * the same reason: an SG4 cell is a colour, and neither the Atari's players
 * nor the Apple's MouseText can be spent one per event.
 *
 *   WEEK   draws the day's whole load as a row of chips, one per event in its
 *          own colour -- the Intellivision's design (intv/st_week.bas), which
 *          the 40-column backends dropped in favour of a lead-event title.
 *   MONTH  gives each day a four-cell density bar in the colour of its leading
 *          event: sixteen steps against the Atari's monochrome four.
 */

#include <cmoc.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* DAY and AGENDA: eleven list rows, then the two-row panel. */
#define LIST_TOP        CONTENT_TOP             /* 2  */
#define PANEL_ROW       13                      /* 13-14 */

/* Columns shared by the DAY and AGENDA rows. */
#define COL_CHIP        0
#define COL_MARK        1
#define COL_TIME        3
#define W_TIME          5
#define COL_TITLE       9
#define W_TITLE         (SCR_COLS - COL_TITLE)  /* 23 */

/*
 * WEEK: seven day rows, a rule, then the selected day's events.
 *
 * There is no caret and no lead-event title. The bar is the cursor, and the
 * chip strip says more about a day than the name of one of its events does.
 */
#define WEEK_TOP        CONTENT_TOP             /* 2..8  */
#define WEEK_DAYS       7
#define WEEK_RULE       9
#define WEEK_PANEL_HDR  10
#define WEEK_PANEL_TOP  11
#define WEEK_PANEL_ROWS (FOOT_ROW - WEEK_PANEL_TOP)     /* 4 */
#define WK_DOW_COL      1
#define WK_DAY_COL      5
#define WK_CNT_COL      8
#define WK_CHIP_COL     10
#define WK_CHIPS        (SCR_COLS - WK_CHIP_COL)        /* 22 */

/* MONTH: a headings row, then six bands on a two-row pitch. */
#define MO_HEAD_ROW     2
#define MO_TOP          3
#define MO_PITCH        2
#define MO_CELL_W       4
#define MO_LEFT         2
#define MO_BARS         (MO_CELL_W * 4)                 /* 16 quadrants */

/* Event detail: a twelve-row window over the wrapped text. */
#define DET_TOP         3

/* The page indicator sits at the right of the footer, where both other
   backends keep theirs. The hints are sized to leave it room. */
#define PAGE_RIGHT      RIGHT_COL

static char sbuf[64];
static char wrapbuf[2][SCR_COLS + 1];

/* Fill order inside a density cell: left column top to bottom, then right.
   That makes a bar that grows rightward one half-cell at a time. */
static const unsigned char qorder[4] = { Q_TL, Q_BL, Q_TR, Q_BR };

/* ------------------------------------------------------------------ */
/* Shared row painting                                                 */
/* ------------------------------------------------------------------ */

/* "09:00", or "ALLDY" for an event with no meaningful start minute. */
static void time_field(char *dst, const struct event *e)
{
    if (e->flags & EVF_ALLDAY) {
        strcpy(dst, "ALLDY");
        return;
    }
    ui_hhmm(dst, e->sh, e->sm);
}

/*
 * '+' for recurring, where the Atari and the Apple both use '~'. The 6847 has
 * sixty-four glyphs and a tilde is not one of them: sc() would fold it to '?',
 * which reads as "something went wrong" rather than "this repeats".
 */
static char marker(const struct event *e)
{
    if (e->flags & EVF_ALLDAY)
        return '*';
    if (e->flags & EVF_RECURRING)
        return '+';
    return ' ';
}

/*
 * One event on one row, in the layout DAY and AGENDA share. The caller has
 * already decided which event and whether it is selected.
 */
static void draw_event(unsigned char row, unsigned char ev, unsigned char sel)
{
    struct event *e = &gc_index[ev];
    char t[6];
    char m[2];

    m[0] = marker(e);
    m[1] = '\0';
    time_field(t, e);

    scr_cell(row, COL_CHIP, chip_sg(e->chip));
    scr_field(row, COL_MARK, m, 1, sel);
    scr_field(row, 2, "", 1, sel);
    scr_field(row, COL_TIME, t, W_TIME, sel);
    scr_field(row, 8, "", 1, sel);
    scr_field(row, COL_TITLE, e->title, W_TITLE, sel);
}

/*
 * The two-row panel that spells the selection out in full.
 *
 * The list column is twenty-three characters wide and a Google summary
 * routinely runs to fifty, so this is what earns storing the longer title --
 * and it earns it harder here than on the Atari, which has thirty.
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
        strcpy(sbuf, "ALL DAY  ");
    }
    strcat(sbuf, e->title);

    rows = (unsigned char) wrap_text(sbuf, (char *) wrapbuf, 2, SCR_COLS,
                                     SCR_COLS + 1);

    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (PANEL_ROW + i), 0,
                  (i < rows) ? (const char *) wrapbuf[i] : "", SCR_COLS, 0);
}

/*
 * "1-11/23" at the right of the footer, which is what fixes the hint strings
 * at twenty-five cells: anything longer and the two collide.
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

    scr_right(FOOT_ROW, PAGE_RIGHT, sbuf, 0);
}

/* ------------------------------------------------------------------ */
/* Hints                                                               */
/* ------------------------------------------------------------------ */

/*
 * The single place the footer is composed. The alarm banner borrows row 15 and
 * calls this to give it back, so every view's hints have to be reachable from
 * the view number alone.
 *
 * Two strings rather than four, and both twenty-five cells or fewer so the
 * page indicator has its seven at the right. "1-4:VIEW" is here because there
 * is no tab strip to carry it, and it costs the arrow and period keys their
 * captions -- those move to the settings screen's legend, which is what the
 * Intellivision did with its second hint page.
 */
void ui_hints(unsigned char view)
{
    if (view == VIEW_WEEK || view == VIEW_MONTH)
        ui_footer("1-4:VIEW ENT:DAY BRK:SET");
    else
        ui_footer("1-4:VIEW ENT:OPEN BRK:SET");
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
            scr_cell((unsigned char) (LIST_TOP + i), COL_CHIP, SG_BLACK);
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
            scr_cell(row, COL_CHIP, SG_BLACK);
            continue;
        }

        slot = gc_agd[first + i];
        ev = (unsigned char) (slot & AGD_IDX);

        if (slot & AGD_SEP) {
            /* A separator is a heading, never a selection: the chip gutter
               stays black on it, and the rule out to the right edge is what
               the extra columns buy over the Intellivision's version. */
            scr_row_clear(row);
            scr_cell(row, COL_CHIP, SG_BLACK);
            sbuf[0] = (char) ('0' + gc_index[ev].day / 10);
            sbuf[1] = (char) ('0' + gc_index[ev].day % 10);
            sbuf[2] = ' ';
            strcpy(sbuf + 3, date_mon3(gc_index[ev].mon));
            scr_text(row, COL_TIME, sbuf, 1);
            scr_fill(row, (unsigned char) (COL_TIME + 7), SG_BLACK,
                     (unsigned char) (SCR_COLS - COL_TIME - 7));
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
 * Draw the day's whole load as one chip per event, in the event's own colour,
 * and return how many there were -- including any past the twenty-two the
 * strip holds, because the count column has to be honest about them.
 *
 * Twenty-two columns will not hold seven days of text side by side, which is
 * what pushed the Intellivision to this at twenty. It reads as a density map
 * at a glance and still names every event's colour, which the Atari's single
 * lead-event title does not.
 */
static unsigned char week_day_chips(unsigned char row, unsigned char dow)
{
    unsigned char i, n = 0;

    for (i = 0; i < gc_count; i++) {
        if (gc_index[i].day != dow)
            continue;
        if (n < WK_CHIPS)
            scr_cell(row, (unsigned char) (WK_CHIP_COL + n),
                     chip_sg(gc_index[i].chip));
        n++;
    }

    if (n < WK_CHIPS)
        scr_fill(row, (unsigned char) (WK_CHIP_COL + n), SG_BLACK,
                 (unsigned char) (WK_CHIPS - n));

    return n;
}

static void draw_week(unsigned char sel)
{
    unsigned int  y;
    unsigned char mo, d;
    unsigned char i, row, n, panel;
    char dd[3];

    week_start(&y, &mo, &d);

    for (i = 0; i < WEEK_DAYS; i++) {
        unsigned char inv = (unsigned char) (i == sel);

        row = (unsigned char) (WEEK_TOP + i);

        scr_cell(row, 0, SG_BLACK);
        scr_field(row, WK_DOW_COL, date_dow3(i), 3, inv);
        scr_field(row, 4, "", 1, inv);

        /* The day number goes inverse for today as well as for the selection,
           so the week still reads at a glance when the cursor is elsewhere. */
        dd[0] = (char) ('0' + d / 10);
        dd[1] = (char) ('0' + d % 10);
        dd[2] = '\0';
        scr_field(row, WK_DAY_COL, dd, 2,
                  (unsigned char) (inv || clk_is_today(y, mo, d)));

        scr_field(row, 7, "", 1, inv);

        n = week_day_chips(row, i);

        scr_field(row, WK_CNT_COL, "", 2, inv);
        if (n) {
            utoa(n, sbuf, 10);
            scr_right(row, (unsigned char) (WK_CNT_COL + 1), sbuf, inv);
        }

        date_addday(&y, &mo, &d);
    }

    /* A black rule, which is what SG_BLACK gives this backend in place of the
       Apple's MouseText one. */
    scr_fill(WEEK_RULE, 0, SG_BLACK, SCR_COLS);

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
    scr_field(WEEK_PANEL_HDR, 1, sbuf, (unsigned char) (SCR_COLS - 1), 0);
    scr_cell(WEEK_PANEL_HDR, 0, SG_BLACK);

    panel = 0;
    for (i = 0; i < gc_count && panel < WEEK_PANEL_ROWS; i++) {
        if (gc_index[i].day != sel)
            continue;
        draw_event((unsigned char) (WEEK_PANEL_TOP + panel), i, 0);
        panel++;
    }
    for (; panel < WEEK_PANEL_ROWS; panel++) {
        row = (unsigned char) (WEEK_PANEL_TOP + panel);
        scr_row_clear(row);
        scr_cell(row, COL_CHIP, SG_BLACK);
    }

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
 * A day cell is four columns on two rows: the number, and under it four
 * semigraphics cells in the colour of the day's leading event, with one
 * quadrant lit per event up to sixteen.
 *
 * The Atari draws four inverse spaces here and the Apple eight, both
 * monochrome, because neither can give a single cell its own hue. This one
 * can, so the bar says both how busy the day is and what kind of thing is on
 * it -- which is the whole reason gc_daycol[] is tallied.
 */
static void month_cell(unsigned char band, unsigned char dow, unsigned char day,
                       unsigned char sel)
{
    unsigned char col = (unsigned char) (MO_LEFT + dow * MO_CELL_W);
    unsigned char nrow = (unsigned char) (MO_TOP + band * MO_PITCH);
    unsigned char brow = (unsigned char) (nrow + 1);
    unsigned char n, i, q, hue, mask;

    if (day == 0) {
        scr_field(nrow, col, "", MO_CELL_W, 0);
        scr_fill(brow, col, SG_BLACK, MO_CELL_W);
        return;
    }

    /* Today is bracketed rather than given a second inverse style: pressing 0
       puts the selection on it, and two inverses on one cell say nothing. */
    if (clk_is_today(cur_y, cur_mo, day)) {
        sbuf[0] = '[';
        sbuf[1] = (char) ('0' + day / 10);
        sbuf[2] = (char) ('0' + day % 10);
        sbuf[3] = ']';
    } else {
        sbuf[0] = ' ';
        sbuf[1] = (day < 10) ? ' ' : (char) ('0' + day / 10);
        sbuf[2] = (char) ('0' + day % 10);
        sbuf[3] = ' ';
    }
    sbuf[4] = '\0';
    scr_field(nrow, col, sbuf, MO_CELL_W, sel);

    n = gc_daycnt[day];
    if (n > MO_BARS)
        n = MO_BARS;

    /* The hue the chip would have been drawn in, taken back out of the byte
       rather than kept in a second table. */
    hue = (unsigned char) ((chip_sg(color_chip(gc_daycol[day])) >> 4) & 7);

    for (i = 0; i < MO_CELL_W; i++) {
        mask = 0;
        for (q = 0; q < 4; q++)
            if ((unsigned char) (i * 4 + q) < n)
                mask |= qorder[q];
        scr_cell(brow, (unsigned char) (col + i), SG4(hue, mask));
    }
}

/*
 * The selected day's own count, in the header's status field. The Atari and
 * the Apple each spend a content row on this; there is no row to spare, and a
 * total for the whole month is worth less than a total for the day under the
 * cursor anyway.
 */
static void month_status(void)
{
    char n[6];

    strcpy(sbuf, date_dow3(date_dow(cur_y, cur_mo, cur_d)));
    strcat(sbuf, " ");
    utoa(cur_d, sbuf + strlen(sbuf), 10);
    strcat(sbuf, "  ");
    if (gc_daycnt[cur_d] == 0) {
        strcat(sbuf, "NO EVENTS");
    } else {
        utoa(gc_daycnt[cur_d], n, 10);
        strcat(sbuf, n);
        strcat(sbuf, gc_daycnt[cur_d] == 1 ? " EVENT" : " EVENTS");
    }

    /* The full width of row 1: MONTH has no page indicator to leave room
       for, which is the one place this line is roomier than the others. */
    scr_field(1, HDR_TEXT_COL, sbuf,
              (unsigned char) (SCR_COLS - HDR_TEXT_COL), 0);
}

static void draw_month(void)
{
    static const char *const head[7] = {
        "SU", "MO", "TU", "WE", "TH", "FR", "SA"
    };
    unsigned char band, dow, day, cell;

    month_geom();

    scr_rows_clear(MO_HEAD_ROW, FOOT_ROW - 1);
    for (dow = 0; dow < 7; dow++)
        scr_text(MO_HEAD_ROW,
                 (unsigned char) (MO_LEFT + dow * MO_CELL_W + 1), head[dow], 0);

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

    month_status();
    ui_hints(VIEW_MONTH);
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

void ui_view(unsigned char view, unsigned char sel, unsigned char first)
{
    ui_header(view);

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
 * change one. That is what keeps holding a cursor key from flickering the
 * whole list.
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
       moves between cells on two rows -- neither is worth an incremental
       path, and both are a redraw out of RAM with no fetch behind them. */
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

    /* Row 0 is built from the index record rather than the reply, so the
       screen identifies itself before a single byte has arrived. */
    strcpy(sbuf, date_dow3(date_dow(cur_y, cur_mo, cur_d)));
    strcat(sbuf, " ");
    utoa(cur_d, sbuf + strlen(sbuf), 10);
    strcat(sbuf, " ");
    strcat(sbuf, date_mon3(cur_mo));
    strcat(sbuf, "  ");
    if (e->flags & EVF_ALLDAY) {
        strcat(sbuf, "ALL DAY");
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
    scr_field(0, 0, sbuf, SCR_COLS, 1);

    rows = (unsigned char) wrap_text(e->title, (char *) wrapbuf, 2, SCR_COLS,
                                     SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (1 + i), 0,
                  (i < rows) ? (const char *) wrapbuf[i] : "", SCR_COLS, 0);

    for (i = 0; i < DET_WIN; i++) {
        if (top + i < gc_det_rows)
            scr_field((unsigned char) (DET_TOP + i), 0, gc_det[top + i],
                      SCR_COLS, 0);
        else
            scr_row_clear((unsigned char) (DET_TOP + i));
    }

    utoa((unsigned int) (top + 1), sbuf, 10);
    strcat(sbuf, "/");
    utoa(gc_det_rows ? gc_det_rows : 1u, n, 10);
    strcat(sbuf, n);
    if (gc_det_trunc)
        strcat(sbuf, "+");

    /* '^' is glyph $1E on a 6847, which is an up arrow, so this footer reads
       with a real arrow on it. There is no down or right arrow in the set. */
    ui_footer("^V:LINE <>:PAGE BRK:BACK");
    scr_right(FOOT_ROW, RIGHT_COL, sbuf, 0);
}

/* ------------------------------------------------------------------ */
/* Alarm banner                                                        */
/* ------------------------------------------------------------------ */

/*
 * The banner takes over the footer, which is why alarms only fire in a view:
 * a view is the only screen that knows how to paint its hints back.
 *
 * The Atari flashes by writing two colour bytes a display list interrupt picks
 * up, repainting nothing. There is no such trick here, so the flash is the row
 * drawn in the other video sense -- thirty-two stores twice a second, which is
 * the Apple's answer and cheap enough.
 */
void ui_alarm(unsigned char phase)
{
    struct event *e;
    char t[6];

    if (al_ev >= gc_count)
        return;

    e = &gc_index[al_ev];
    ui_hhmm(t, e->sh, e->sm);

    strcpy(sbuf, "! ");
    strcat(sbuf, t);
    strcat(sbuf, " ");
    strcat(sbuf, e->title);

    scr_field(FOOT_ROW, 0, sbuf, SCR_COLS, phase);
}
