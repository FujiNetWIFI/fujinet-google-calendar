/*
 * The four calendar views, the event detail screen, and the alarm banner.
 *
 * Every list row here paints columns 1 to 79 and leaves column 0 alone, the
 * same rule src/atari/views.c follows and for a rhyming reason. There column 0
 * is the chip gutter and an inverse space is COLPF1, which draws in front of
 * the player carrying the chip. Here the chip is a MouseText glyph, and
 * MouseText occupies the very character codes the inverse forms would have
 * used -- so there is no inverse of a glyph, and a chip inside the bar would
 * have to fall back to ASCII and change shape on the one row the cursor is on.
 *
 * What the extra forty columns actually buy is the category column. The wire
 * has always sent it -- colour name, or the calendar's own name when the event
 * has no colorId -- and the Atari has to throw it away and keep only the
 * colour it implies. Here it is a column, and the chip beside it is the
 * at-a-glance grouping rather than the whole story.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* DAY and AGENDA: eighteen list rows, a blank, one row of detail. */
#define LIST_TOP        CONTENT_TOP             /* 3     */
#define DETAIL_ROW      22

/*
 * Columns shared by the DAY and AGENDA rows.
 *
 * The chip sits in column 0 and the selection bar starts at column 1, which is
 * the same split src/atari/views.c makes for a different reason. There it is
 * that an inverse space is COLPF1 and would draw in front of the player
 * carrying the chip; here it is that MouseText occupies the character codes
 * the inverse forms would have used, so there is no inverse of a glyph -- a
 * chip inside the bar would have to fall back to ASCII and change shape on
 * exactly the row the cursor is on.
 */
#define COL_CHIP        0
#define COL_CARET       1
#define COL_MARK        2
#define COL_TIME        4
#define W_TIME          5
#define COL_CAT         10
#define W_CAT           GC_CATW                 /* 14    */
#define COL_TITLE       25
#define W_TITLE         (SCR_COLS - COL_TITLE)  /* 55    */

/* WEEK: seven day rows, then the selected day's events. */
#define WEEK_TOP        CONTENT_TOP             /* 3..9  */
#define WEEK_DAYS       7
#define WEEK_PANEL_HDR  11
#define WEEK_PANEL_TOP  12
#define WEEK_PANEL_ROWS (FOOT_ROW - WEEK_PANEL_TOP)

/* MONTH: a headings row, then six week bands on a three-row pitch. */
#define MO_HEAD_ROW     3
#define MO_TOP          4
#define MO_PITCH        3
#define MO_CELL_W       11
#define MO_LEFT         1
#define MO_BARS         8                       /* densest bar we draw */
#define MO_SUM_ROW      22

/* Event detail: an eighteen-row window over the wrapped text. */
#define DET_TOP         3

/* Right-hand status ends here, leaving column 79 blank. */
#define FOOT_RIGHT      78

static char sbuf[96];
static char wrapbuf[2][SCR_COLS + 1];

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
    char t[6];
    char m[2];

    m[0] = marker(e);
    m[1] = '\0';
    time_field(t, e);

    scr_field(row, COL_CHIP, chip_glyph(e->chip), 1, 0);
    scr_field(row, COL_CARET, sel ? ">" : " ", 1, sel);
    scr_field(row, COL_MARK, m, 1, sel);
    scr_field(row, 3, "", 1, sel);
    scr_field(row, COL_TIME, t, W_TIME, sel);
    scr_field(row, 9, "", 1, sel);
    scr_field(row, COL_CAT, e->cat, W_CAT, sel);
    scr_field(row, 24, "", 1, sel);
    scr_field(row, COL_TITLE, e->title, W_TITLE, sel);
}

/*
 * The row that spells the selection out in full: the whole time range, which
 * the list column only shows the start of, and the title again in case the
 * fifty-five columns above still clipped it.
 */
static void draw_detail_line(unsigned char ev)
{
    struct event *e;
    char t[6];

    scr_row_clear(DETAIL_ROW);
    if (ev == 0xFF || ev >= gc_count)
        return;

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
        strcpy(sbuf, "All day      ");
    }
    strcat(sbuf, e->title);

    scr_field(DETAIL_ROW, COL_TIME, sbuf,
              (unsigned char) (SCR_COLS - COL_TIME), 0);
}

/*
 * `total` is not always gc_count: the agenda's rows include date separators,
 * so its window is measured against gc_agd_count or the indicator claims to be
 * showing more rows than there are events.
 */
static void page_indicator(unsigned char first, unsigned char shown,
                           unsigned char total)
{
    char n[6];

    if (total == 0) {
        sbuf[0] = '\0';
        return;
    }

    utoa((unsigned int) (first + 1), sbuf, 10);
    strcat(sbuf, "-");
    utoa((unsigned int) (first + shown), n, 10);
    strcat(sbuf, n);
    strcat(sbuf, "/");
    utoa(total, n, 10);
    strcat(sbuf, n);
}

/* ------------------------------------------------------------------ */
/* Hints                                                               */
/* ------------------------------------------------------------------ */

/*
 * The single place the footer is composed. The alarm banner borrows row 23 and
 * calls this to give it back, so every view's hints have to be reachable from
 * the view number alone.
 */
void ui_hints(unsigned char view)
{
    switch (view) {
    case VIEW_MONTH:
        ui_footer("RET" MT_RETURN ":DAY   " MT_UP MT_DOWN MT_LEFT MT_RIGHT
                  ":MOVE   ESC:SETTINGS", 0);
        break;
    case VIEW_WEEK:
        ui_footer("RET" MT_RETURN ":DAY   " MT_UP MT_DOWN ":MOVE   "
                  MT_LEFT MT_RIGHT ":WEEK   ESC:SETTINGS", 0);
        break;
    case VIEW_AGENDA:
        ui_footer("RET" MT_RETURN ":OPEN   " MT_UP MT_DOWN ":MOVE   "
                  MT_LEFT MT_RIGHT ":WEEK   ESC:SETTINGS", 0);
        break;
    default:
        ui_footer("RET" MT_RETURN ":OPEN   " MT_UP MT_DOWN ":MOVE   "
                  MT_LEFT MT_RIGHT ":DAY   ESC:SETTINGS", 0);
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

    scr_row_clear(LIST_TOP + LIST_ROWS);
    draw_detail_line(gc_count ? sel : 0xFF);

    ui_hints(VIEW_DAY);
    page_indicator(first, shown, gc_count);
    scr_right(FOOT_ROW, FOOT_RIGHT, sbuf, 0);
}

/* ------------------------------------------------------------------ */
/* AGENDA                                                              */
/* ------------------------------------------------------------------ */

static void draw_agenda(unsigned char sel, unsigned char first)
{
    unsigned char i, row, slot, ev;
    unsigned char shown;

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
            /* A separator is a heading, never a selection: the chip column
               and the caret both stay empty on it. The rule running out to
               the edge is what the extra width is worth spending here -- at
               forty columns the Atari has room for the date and nothing
               else. */
            scr_row_clear(row);
            sbuf[0] = (char) ('0' + gc_index[ev].day / 10);
            sbuf[1] = (char) ('0' + gc_index[ev].day % 10);
            sbuf[2] = ' ';
            strcpy(sbuf + 3, date_mon3(gc_index[ev].mon));
            scr_text(row, COL_TIME, sbuf, 1);
            scr_fill(row, (unsigned char) (COL_TIME + 8), MT_RULE,
                     (unsigned char) (SCR_COLS - COL_TIME - 8), 0);
            continue;
        }

        draw_event(row, ev, (unsigned char) (first + i == sel));
    }

    scr_row_clear(LIST_TOP + LIST_ROWS);
    if (sel < gc_agd_count && !(gc_agd[sel] & AGD_SEP))
        draw_detail_line((unsigned char) (gc_agd[sel] & AGD_IDX));
    else
        draw_detail_line(0xFF);

    ui_hints(VIEW_AGENDA);
    page_indicator(first, shown, gc_agd_count);
    scr_right(FOOT_ROW, FOOT_RIGHT, sbuf, 0);
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

/* The day's event count, and the index of its earliest -- the adapter sorts by
   start time, so the first match is the one worth showing. */
static unsigned char week_day_events(unsigned char dow, unsigned char *lead)
{
    unsigned char i, n = 0;

    *lead = 0xFF;
    for (i = 0; i < gc_count; i++) {
        if (gc_index[i].day != dow)
            continue;
        if (*lead == 0xFF)
            *lead = i;
        n++;
    }

    return n;
}

static void draw_week(unsigned char sel)
{
    unsigned int  y;
    unsigned char mo, d;
    unsigned char i, row, n, lead, panel, shown;
    char t[6];

    week_start(&y, &mo, &d);

    for (i = 0; i < WEEK_DAYS; i++) {
        unsigned char inv = (unsigned char) (i == sel);
        char dd[3];

        row = (unsigned char) (WEEK_TOP + i);
        n = week_day_events(i, &lead);

        /* Column 0 stays out of the bar, same as the list rows. */
        scr_field(row, COL_CHIP,
                  lead == 0xFF ? " " : chip_glyph(gc_index[lead].chip), 1, 0);

        scr_field(row, 1, inv ? ">" : " ", 1, inv);
        scr_field(row, 2, "", 1, inv);
        scr_field(row, 3, date_dow3(i), 3, inv);
        scr_field(row, 6, "", 1, inv);

        /* The day number goes inverse for today as well as for the selection,
           so the week still reads at a glance when the cursor is elsewhere. */
        dd[0] = (char) ('0' + d / 10);
        dd[1] = (char) ('0' + d % 10);
        dd[2] = '\0';
        scr_field(row, 7, dd, 2,
                  (unsigned char) (inv || clk_is_today(y, mo, d)));

        scr_field(row, 9, "", 1, inv);
        scr_field(row, 10, "", 3, inv);
        if (n) {
            utoa(n, sbuf, 10);
            scr_right(row, 12, sbuf, inv);
        }
        scr_field(row, 13, "", 1, inv);

        if (lead != 0xFF) {
            time_field(t, &gc_index[lead]);
            strcpy(sbuf, t);
            strcat(sbuf, "  ");
            strcat(sbuf, gc_index[lead].title);
            scr_field(row, 14, sbuf, SCR_COLS - 14, inv);
        } else {
            scr_field(row, 14, "(nothing)", SCR_COLS - 14, inv);
        }

        date_addday(&y, &mo, &d);
    }

    /* The panel below costs no fetch: the WEEK index already carries a day of
       week for every event, so this is a redraw straight out of RAM. */
    scr_row_clear(WEEK_TOP + WEEK_DAYS);

    week_start(&y, &mo, &d);
    for (i = 0; i < sel; i++)
        date_addday(&y, &mo, &d);

    strcpy(sbuf, date_dow3(sel));
    strcat(sbuf, " ");
    utoa(d, sbuf + strlen(sbuf), 10);
    strcat(sbuf, " ");
    strcat(sbuf, date_mon3(mo));
    scr_field(WEEK_PANEL_HDR, 2, sbuf, SCR_COLS - 2, 0);

    panel = 0;
    for (i = 0; i < gc_count && panel < WEEK_PANEL_ROWS; i++) {
        if (gc_index[i].day != sel)
            continue;
        row = (unsigned char) (WEEK_PANEL_TOP + panel);
        draw_event(row, i, 0);
        panel++;
    }
    for (shown = panel; shown < WEEK_PANEL_ROWS; shown++)
        scr_row_clear((unsigned char) (WEEK_PANEL_TOP + shown));

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
 * A day cell is eleven columns on two rows: the number, and under it a bar of
 * inverse cells one per event up to eight. There is no colour to grade a cell
 * with, so the bar is what carries "how busy", and eleven columns is enough
 * for it to mean something rather than saturating at four the way the Atari's
 * five-column cell does.
 *
 * Only the four-character number box takes the selection, not the whole cell.
 * At five columns the Atari can invert everything and still read as a cursor;
 * at eleven columns on two rows it would be a slab of white with the density
 * bar swallowed inside it.
 */
#define MO_NUMW     4

static void month_cell(unsigned char band, unsigned char dow, unsigned char day,
                       unsigned char sel)
{
    unsigned char col = (unsigned char) (MO_LEFT + dow * MO_CELL_W);
    unsigned char nrow = (unsigned char) (MO_TOP + band * MO_PITCH);
    unsigned char brow = (unsigned char) (nrow + 1);
    unsigned char n;

    if (day == 0) {
        scr_field(nrow, col, "", MO_CELL_W, 0);
        scr_field(brow, col, "", MO_CELL_W, 0);
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
        sbuf[1] = (char) ('0' + day / 10);
        sbuf[2] = (char) ('0' + day % 10);
        sbuf[3] = ' ';
        if (day < 10)
            sbuf[1] = ' ';
    }
    sbuf[4] = '\0';

    scr_field(nrow, col, sbuf, MO_NUMW, sel);
    scr_field(nrow, (unsigned char) (col + MO_NUMW), "",
              (unsigned char) (MO_CELL_W - MO_NUMW), 0);

    /* Left-aligned under the number rather than right-aligned in the cell:
       eleven columns is wide enough that a bar on the far side would read as
       belonging to the day after this one. */
    n = gc_daycnt[day];
    if (n > MO_BARS)
        n = MO_BARS;

    scr_fill(brow, (unsigned char) (col + 1), " ", n, 1);
    scr_field(brow, (unsigned char) (col + 1 + n), "",
              (unsigned char) (MO_CELL_W - 1 - n), 0);
    scr_field(brow, col, "", 1, 0);
}

static void draw_month(void)
{
    static const char *const head[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    unsigned char band, dow, day, cell;
    char n[6];

    month_geom();

    scr_rows_clear(MO_HEAD_ROW, FOOT_ROW - 1);
    for (dow = 0; dow < 7; dow++)
        scr_text(MO_HEAD_ROW, (unsigned char) (MO_LEFT + dow * MO_CELL_W + 1),
                 head[dow], 0);

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
    scr_field(MO_SUM_ROW, 2, sbuf, SCR_COLS - 2, 0);

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
 * being landed on, and the detail line. That is what keeps holding a cursor
 * key from flickering the whole list.
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
        draw_detail_line(gc_count ? to : 0xFF);
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
            draw_detail_line(ev);
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
    unsigned int  rows;
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
        strcat(sbuf, "all day");
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
    if (e->cat[0]) {
        strcat(sbuf, "   ");
        strcat(sbuf, e->cat);
    }
    scr_field(0, 0, sbuf, SCR_COLS, 1);

    /* wrap_text() writes only the rows it produces and returns how many -- it
       does not blank the rest, so anything past the count is the previous
       event's title and has to be cleared rather than drawn. */
    rows = wrap_text(e->title, (char *) wrapbuf, 2, SCR_COLS, SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (1 + i), 0,
                  (i < rows) ? wrapbuf[i] : "", SCR_COLS, 0);

    for (i = 0; i < DET_WIN; i++) {
        if (top + i < gc_det_rows)
            scr_field((unsigned char) (DET_TOP + i), 0, gc_det[top + i],
                      SCR_COLS, 0);
        else
            scr_row_clear((unsigned char) (DET_TOP + i));
    }
    scr_rows_clear(DET_TOP + DET_WIN, FOOT_ROW - 1);

    utoa((unsigned int) (top + 1), sbuf, 10);
    strcat(sbuf, "/");
    utoa(gc_det_rows ? gc_det_rows : 1, n, 10);
    strcat(sbuf, n);
    if (gc_det_trunc)
        strcat(sbuf, "+");

    ui_footer(MT_UP MT_DOWN ":LINE   " MT_LEFT MT_RIGHT ":PAGE   ESC:BACK",
              sbuf);
}

/* ------------------------------------------------------------------ */
/* Alarm banner                                                        */
/* ------------------------------------------------------------------ */

/*
 * The banner takes over the footer, which is why alarms only fire in a view: a
 * view is the only screen that knows how to paint its hints back.
 *
 * The Atari flashes it by alternating two colour registers the display list
 * interrupt reads, which repaints nothing at all. There is no such trick here
 * -- the flash is the row being repainted in the other video sense, which is
 * eighty stores twice a second and cheap enough.
 */
void ui_alarm(unsigned char phase)
{
    struct event *e;
    char t[6];

    if (al_ev >= gc_count) {
        scr_field(FOOT_ROW, 0, "", SCR_COLS, phase);
        return;
    }

    e = &gc_index[al_ev];
    ui_hhmm(t, e->sh, e->sm);

    strcpy(sbuf, "! ");
    strcat(sbuf, t);
    strcat(sbuf, "  ");
    strcat(sbuf, e->title);

    scr_field(FOOT_ROW, 0, "", 1, phase);
    scr_field(FOOT_ROW, 1, sbuf, SCR_COLS - 1, phase);
}
