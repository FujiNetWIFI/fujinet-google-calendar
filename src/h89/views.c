/*
 * The four calendar views, the event detail screen, and the alarm banner.
 *
 * The rows are the Apple backend's (src/apple2enh/views.c) -- the chip in
 * column 0 outside the selection bar, then time, category and title -- set
 * in a table of the H19's line characters: a rule over the list and one
 * under it, a vertical line between the columns, and a tee or a cross where
 * the two meet. The month is a real grid. The detail is a box.
 *
 * Column 0 stays out of the bar for an H19 reason rather than the Apple's:
 * one of the chips is a reverse space, which a reverse bar would swallow.
 *
 * Everything is painted into h19.c's copy of the screen and sent by
 * difference, so a full repaint costs only what changed. The one thing the
 * difference cannot make cheap is a list moving under the window by one row
 * -- every row changes -- so that is done with the terminal's own delete and
 * insert line (h19_scroll), and the difference then sends the one row that
 * came in.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* DAY and AGENDA: a rule, eighteen list rows, a rule, the selection. */
#define TOP_RULE        CONTENT_TOP             /* 3     */
#define LIST_TOP        (TOP_RULE + 1)          /* 4..21 */
#define BOT_RULE        (LIST_TOP + LIST_ROWS)  /* 22    */
#define DETAIL_ROW      LAST_ROW                /* 23    */

/*
 * Columns shared by the DAY, AGENDA and WEEK rows. The three lines are the
 * table's; the cells between them are padded one space each side.
 */
#define COL_CHIP        0
#define COL_CARET       1
#define COL_MARK        2
#define SEP1            3
#define COL_TIME        4                       /* " 09:00 " or "ALL DAY" */
#define W_TIME          7
#define SEP2            11
#define COL_CAT         13
#define W_CAT           GC_CATW                 /* 14    */
#define SEP3            28
#define COL_TITLE       30
#define W_TITLE         (SCR_COLS - COL_TITLE)  /* 50    */

/* Where the label goes when a rule carries one: in the category column,
   which is the one wide enough for "| Fri 28 Aug |", a line's width in from
   the cross. */
#define RULE_LABEL_COL  (SEP2 + 1)
#define RULE_LABEL_IN   1

/* WEEK: seven day rows, a rule naming the day, that day's events. */
#define WEEK_TOP        LIST_TOP                /* 4..10  */
#define WEEK_DAYS       7
#define WEEK_RULE       (WEEK_TOP + WEEK_DAYS)  /* 11     */
#define WEEK_PANEL_TOP  (WEEK_RULE + 1)         /* 12..21 */
#define WEEK_PANEL_ROWS (BOT_RULE - WEEK_PANEL_TOP)

/* MONTH: a headings row, then a grid of six weeks, three rows a week. */
#define MO_HEAD_ROW     3
#define MO_GRID_TOP     4
#define MO_PITCH        3
#define MO_CELL_W       11
#define MO_LEFT         1                       /* the grid's left line */
#define MO_BARS         8
#define MO_SUM_ROW      LAST_ROW

/* Event detail: a box, its title, and a window over the wrapped text. */
#define DET_BOX_TOP     CONTENT_TOP             /* 3      */
#define DET_TITLE_ROW   (DET_BOX_TOP + 1)       /* 4      */
#define DET_RULE_ROW    (DET_BOX_TOP + 2)       /* 5      */
#define DET_TOP         (DET_BOX_TOP + 3)       /* 6..22  */
#define DET_BOX_BOT     (DET_TOP + DET_WIN)     /* 23     */
#define DET_TEXT_COL    2

#if DET_BOX_BOT > LAST_ROW
#error "DET_WIN is too tall for the detail box"
#endif
#if BOT_RULE > LAST_ROW - 1
#error "LIST_ROWS is too tall for the list"
#endif

static const char view_keys[] =
    "[f1] Day [f2] Week [f3] Month [f4] Agenda [f5] Today  "
    "[BLUE] New [RED] Edit [GRAY] Sync";

static char sbuf[96];

/* The list window last painted, so a move of one row can be a scroll. */
static unsigned char lst_valid, lst_view, lst_first;
static unsigned char det_valid, det_ev;
static unsigned int  det_top;

unsigned char ui_view_now;

void views_forget(void)
{
    lst_valid = 0;
    det_valid = 0;
}

/* ------------------------------------------------------------------ */
/* The table                                                           */
/* ------------------------------------------------------------------ */

/* A rule across the table, with `tee` where each column line meets it. */
static void table_rule(unsigned char row, unsigned char tee)
{
    scr_fill(row, 0, G_HLINE, SCR_COLS);
    scr_cell(row, SEP1, tee);
    scr_cell(row, SEP2, tee);
    scr_cell(row, SEP3, tee);
}

/* The same with a label set into it, "-| label |-". */
static void table_rule_label(unsigned char row, unsigned char tee,
                             const char *label)
{
    table_rule(row, tee);
    scr_rule(row, RULE_LABEL_COL, (unsigned char) (SEP3 - RULE_LABEL_COL),
             label, RULE_LABEL_IN);
}

/* A row with nothing on it but the column lines. */
static void table_blank(unsigned char row)
{
    scr_row_clear(row);
    scr_cell(row, SEP1, G_VLINE);
    scr_cell(row, SEP2, G_VLINE);
    scr_cell(row, SEP3, G_VLINE);
}

/*
 * The bottom rule, which carries what the 25th line has no room for: how to
 * open the selection and how to leave, and where the window is in the list.
 */
static void table_bottom(const char *hints, const char *where)
{
    unsigned char len;

    table_rule(BOT_RULE, G_TEE_UP);
    scr_rule(BOT_RULE, SEP3 + 1, (unsigned char) (SCR_COLS - SEP3 - 1),
             hints, 1);
    if (where && *where) {
        len = (unsigned char) strlen(where);
        scr_rule(BOT_RULE, (unsigned char) (SCR_COLS - len - 5),
                 (unsigned char) (len + 4), where, 0);
    }
}

/* ------------------------------------------------------------------ */
/* Shared row painting                                                 */
/* ------------------------------------------------------------------ */

static char marker(const struct event *e)
{
    if (e->flags & EVF_ALLDAY)
        return '*';
    if (e->flags & EVF_RECURRING)
        return '~';
    return ' ';
}

/* " 09:00 ", or "ALL DAY" for an event with no meaningful start minute. */
static void time_field(char *dst, const struct event *e)
{
    if (e->flags & EVF_ALLDAY) {
        strcpy(dst, "ALL DAY");
        return;
    }
    dst[0] = ' ';
    ui_hhmm(dst + 1, e->sh, e->sm);
    dst[6] = ' ';
    dst[7] = '\0';
}

/* Column 0, then the bar's first cell: the caret when it is the bar. */
static void row_lead(unsigned char row, unsigned char chip, unsigned char sel)
{
    scr_cell(row, COL_CHIP, chip);
    scr_cell(row, COL_CARET, sel ? (G_ARROW_R | REV) : ' ');
}

/* The three column lines and the padding beside them, in the bar's video. */
static void row_lines(unsigned char row, unsigned char a)
{
    scr_cell(row, SEP1, G_VLINE | a);
    scr_cell(row, SEP2, G_VLINE | a);
    scr_cell(row, SEP2 + 1, ' ' | a);
    scr_cell(row, COL_CAT + W_CAT, ' ' | a);
    scr_cell(row, SEP3, G_VLINE | a);
    scr_cell(row, SEP3 + 1, ' ' | a);
}

/* One event on one row, in the layout DAY, AGENDA and WEEK's panel share. */
static void draw_event(unsigned char row, unsigned char ev, unsigned char sel)
{
    struct event *e = &gc_index[ev];
    unsigned char a = sel ? REV : 0;
    char t[8];

    row_lead(row, chip_cell(e->chip), sel);
    scr_cell(row, COL_MARK, (unsigned char) (marker(e) | a));
    row_lines(row, a);
    time_field(t, e);
    scr_field(row, COL_TIME, t, W_TIME, sel);
    scr_field(row, COL_CAT, e->cat, W_CAT, sel);
    scr_field(row, COL_TITLE, e->title, W_TITLE, sel);
}

/*
 * The row under the table that spells the selection out: the whole time
 * range, which the list column only shows the start of.
 */
static void draw_detail_line(unsigned char ev)
{
    struct event *e;
    char t[6];

    scr_row_clear(DETAIL_ROW);
    if (ev == 0xFF || ev >= gc_count)
        return;

    e = &gc_index[ev];

    if (!(e->flags & EVF_ALLDAY)) {
        ui_hhmm(sbuf, e->sh, e->sm);
        if (e->flags & EVF_OPENEND) {
            strcat(sbuf, " onward");
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
 * so its window is measured against gc_agd_count.
 */
static void page_indicator(unsigned char first, unsigned char shown,
                           unsigned char total)
{
    char n[6];

    sbuf[0] = '\0';
    if (total == 0)
        return;

    utoa((unsigned int) (first + 1), sbuf, 10);
    strcat(sbuf, "-");
    utoa((unsigned int) (first + shown), n, 10);
    strcat(sbuf, n);
    strcat(sbuf, "/");
    utoa(total, n, 10);
    strcat(sbuf, n);
}

/* "Fri 28 Aug" */
static void day_label(char *dst, unsigned char dow, unsigned char d,
                      unsigned char mo)
{
    strcpy(dst, date_dow3(dow));
    strcat(dst, " ");
    utoa(d, dst + strlen(dst), 10);
    strcat(dst, " ");
    strcat(dst, date_mon3(mo));
}

/*
 * Before a list is painted: if it is the same list as last time with the
 * window one row along, move what the terminal already shows by that row.
 * The paint that follows then differs from it in one row only.
 */
static void list_scroll(unsigned char view, unsigned char first)
{
    if (lst_valid && lst_view == view) {
        if (first == (unsigned char) (lst_first + 1))
            h19_scroll(LIST_TOP, LIST_TOP + LIST_ROWS - 1, 1);
        else if (first + 1 == lst_first)
            h19_scroll(LIST_TOP, LIST_TOP + LIST_ROWS - 1, -1);
    }
    lst_valid = 1;
    lst_view = view;
    lst_first = first;
}

/* ------------------------------------------------------------------ */
/* Hints                                                               */
/* ------------------------------------------------------------------ */

/* The 25th line. The alarm banner borrows it and calls this to give it
   back, so it has to be reachable from the view number alone. */
void ui_hints(unsigned char view)
{
    (void) view;
    scr_keys(view_keys);
}

/* ------------------------------------------------------------------ */
/* DAY                                                                 */
/* ------------------------------------------------------------------ */

static void draw_day(unsigned char sel, unsigned char first)
{
    unsigned char i, ev, shown;

    list_scroll(VIEW_DAY, first);

    shown = (gc_count > first) ? (unsigned char) (gc_count - first) : 0;
    if (shown > LIST_ROWS)
        shown = LIST_ROWS;

    table_rule(TOP_RULE, G_TEE_DOWN);
    for (i = 0; i < LIST_ROWS; i++) {
        if (i >= shown) {
            table_blank((unsigned char) (LIST_TOP + i));
            continue;
        }
        ev = (unsigned char) (first + i);
        draw_event((unsigned char) (LIST_TOP + i), ev,
                   (unsigned char) (ev == sel));
    }

    page_indicator(first, shown, gc_count);
    table_bottom("RETURN open  ESC setup  Q quit", sbuf);
    draw_detail_line(gc_count ? sel : 0xFF);
}

/* ------------------------------------------------------------------ */
/* AGENDA                                                              */
/* ------------------------------------------------------------------ */

static void draw_agenda(unsigned char sel, unsigned char first)
{
    unsigned char i, row, slot, ev;
    unsigned char shown;

    list_scroll(VIEW_AGENDA, first);

    shown = (gc_agd_count > first) ? (unsigned char) (gc_agd_count - first) : 0;
    if (shown > LIST_ROWS)
        shown = LIST_ROWS;

    table_rule(TOP_RULE, G_TEE_DOWN);
    for (i = 0; i < LIST_ROWS; i++) {
        row = (unsigned char) (LIST_TOP + i);

        if (i >= shown) {
            table_blank(row);
            continue;
        }

        slot = gc_agd[first + i];
        ev = (unsigned char) (slot & AGD_IDX);

        if (slot & AGD_SEP) {
            /* A separator is a heading, never a selection: a rule across
               the table, the columns crossing it, the date set into it. */
            day_label(sbuf,
                      date_dow(cur_y + (gc_index[ev].mon < cur_mo),
                               gc_index[ev].mon, gc_index[ev].day),
                      gc_index[ev].day, gc_index[ev].mon);
            table_rule_label(row, G_CROSS, sbuf);
            continue;
        }

        draw_event(row, ev, (unsigned char) (first + i == sel));
    }

    page_indicator(first, shown, gc_agd_count);
    table_bottom("RETURN open  ESC setup  Q quit", sbuf);

    if (sel < gc_agd_count && !(gc_agd[sel] & AGD_SEP))
        draw_detail_line((unsigned char) (gc_agd[sel] & AGD_IDX));
    else
        draw_detail_line(0xFF);
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
    unsigned char i, row, n, lead, panel, a;
    char t[8];

    lst_valid = 0;
    week_start(&y, &mo, &d);

    table_rule(TOP_RULE, G_TEE_DOWN);
    for (i = 0; i < WEEK_DAYS; i++) {
        a = (i == sel) ? REV : 0;
        row = (unsigned char) (WEEK_TOP + i);
        n = week_day_events(i, &lead);

        row_lead(row, lead == 0xFF ? ' ' : chip_cell(gc_index[lead].chip),
                 (unsigned char) (i == sel));
        /* Today is a bullet in the marker column, so the week reads at a
           glance whichever day the bar is on. */
        scr_cell(row, COL_MARK,
                 (unsigned char) ((clk_is_today(y, mo, d) ? G_BULLET : ' ') | a));
        row_lines(row, a);

        strcpy(sbuf, date_dow3(i));
        strcat(sbuf, d < 10 ? "  " : " ");
        utoa(d, sbuf + strlen(sbuf), 10);
        scr_field(row, COL_TIME, sbuf, W_TIME, a);

        sbuf[0] = '\0';
        if (n) {
            utoa(n, sbuf, 10);
            strcat(sbuf, n == 1 ? " event" : " events");
        }
        scr_field(row, COL_CAT, sbuf, W_CAT, a);

        if (lead != 0xFF) {
            time_field(t, &gc_index[lead]);
            strcpy(sbuf, t + (t[0] == ' '));
            strcat(sbuf, " ");
            strcat(sbuf, gc_index[lead].title);
            scr_field(row, COL_TITLE, sbuf, W_TITLE, a);
        } else {
            scr_field(row, COL_TITLE, "", W_TITLE, a);
        }

        date_addday(&y, &mo, &d);
    }

    /* The panel below costs no fetch: the WEEK index already carries a day of
       week for every event, so this is a redraw straight out of RAM. */
    week_start(&y, &mo, &d);
    for (i = 0; i < sel; i++)
        date_addday(&y, &mo, &d);
    day_label(sbuf, sel, d, mo);
    table_rule_label(WEEK_RULE, G_CROSS, sbuf);

    panel = 0;
    for (i = 0; i < gc_count && panel < WEEK_PANEL_ROWS; i++) {
        if (gc_index[i].day != sel)
            continue;
        draw_event((unsigned char) (WEEK_PANEL_TOP + panel), i, 0);
        panel++;
    }
    for (; panel < WEEK_PANEL_ROWS; panel++)
        table_blank((unsigned char) (WEEK_PANEL_TOP + panel));

    table_bottom("RETURN day  ESC setup  Q quit", 0);

    n = week_day_events(sel, &lead);
    sbuf[0] = '\0';
    if (n > WEEK_PANEL_ROWS) {
        utoa(n - WEEK_PANEL_ROWS, sbuf, 10);
        strcat(sbuf, " more -- RETURN shows the day");
    }
    scr_row_clear(DETAIL_ROW);
    scr_text(DETAIL_ROW, COL_TIME, sbuf, 0);
}

/* ------------------------------------------------------------------ */
/* MONTH                                                               */
/* ------------------------------------------------------------------ */

static unsigned char mo_first;      /* weekday of the 1st */
static unsigned char mo_ndays;

/*
 * A grid line across the month: `left`, `mid` and `right` where it meets
 * the grid's verticals, a line between.
 */
static void month_line(unsigned char row, unsigned char left,
                       unsigned char mid, unsigned char right)
{
    unsigned char i;

    scr_row_clear(row);
    scr_fill(row, MO_LEFT, G_HLINE, 7 * MO_CELL_W);
    scr_cell(row, MO_LEFT, left);
    for (i = 1; i < 7; i++)
        scr_cell(row, (unsigned char) (MO_LEFT + i * MO_CELL_W), mid);
    scr_cell(row, MO_LEFT + 7 * MO_CELL_W, right);
}

/*
 * A day cell is ten columns on two rows inside the grid: the number, and
 * under it a bar of reverse cells one per event up to eight. There is no
 * colour to grade a cell with, so the bar is what carries "how busy".
 *
 * The cursor is the whole cell in reverse -- inside a frame, the cell reads
 * as one thing -- with the bar showing through it as gaps. Today is marked
 * with a bullet, so that pressing f5 puts the one on the other and both
 * still show.
 */
static void month_cell(unsigned char band, unsigned char dow, unsigned char day,
                       unsigned char sel)
{
    unsigned char col = (unsigned char) (MO_LEFT + 1 + dow * MO_CELL_W);
    unsigned char nrow = (unsigned char) (MO_GRID_TOP + 1 + band * MO_PITCH);
    unsigned char brow = (unsigned char) (nrow + 1);
    unsigned char a = sel ? REV : 0;
    unsigned char n;

    scr_fill(nrow, col, ' ' | a, MO_CELL_W - 1);
    scr_fill(brow, col, ' ' | a, MO_CELL_W - 1);
    if (day == 0)
        return;

    if (clk_is_today(cur_y, cur_mo, day))
        scr_cell(nrow, col, G_BULLET | a);
    if (day >= 10)
        scr_cell(nrow, (unsigned char) (col + 1),
                 (unsigned char) (('0' + day / 10) | a));
    scr_cell(nrow, (unsigned char) (col + 2), (unsigned char) (('0' + day % 10) | a));

    n = gc_daycnt[day];
    scr_fill(brow, (unsigned char) (col + 1), (unsigned char) (' ' | (a ^ REV)),
             n > MO_BARS ? MO_BARS : n);
    if (n > MO_BARS)
        scr_cell(brow, (unsigned char) (col + 1 + MO_BARS), '+' | a);
}

static void draw_month(void)
{
    static const char *const head[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    unsigned char band, dow, day, cell, row, i;
    char n[6];

    lst_valid = 0;
    mo_first = date_dow(cur_y, cur_mo, 1);
    mo_ndays = date_dim(cur_y, cur_mo);

    scr_row_clear(MO_HEAD_ROW);
    for (dow = 0; dow < 7; dow++)
        scr_text(MO_HEAD_ROW, (unsigned char) (MO_LEFT + dow * MO_CELL_W + 5),
                 head[dow], 0);

    month_line(MO_GRID_TOP, G_CORNER_UL, G_TEE_DOWN, G_CORNER_UR);

    for (band = 0; band < 6; band++) {
        row = (unsigned char) (MO_GRID_TOP + 1 + band * MO_PITCH);
        for (i = 0; i < 2; i++) {
            scr_row_clear((unsigned char) (row + i));
            for (dow = 0; dow <= 7; dow++)
                scr_cell((unsigned char) (row + i),
                         (unsigned char) (MO_LEFT + dow * MO_CELL_W), G_VLINE);
        }
        for (dow = 0; dow < 7; dow++) {
            cell = (unsigned char) (band * 7 + dow);
            day = (cell < mo_first) ? 0 : (unsigned char) (cell - mo_first + 1);
            if (day > mo_ndays)
                day = 0;
            month_cell(band, dow, day, (unsigned char) (day && day == cur_d));
        }
        if (band < 5)
            month_line((unsigned char) (row + 2), G_TEE_RIGHT, G_CROSS,
                       G_TEE_LEFT);
        else
            month_line((unsigned char) (row + 2), G_CORNER_LL, G_TEE_UP,
                       G_CORNER_LR);
    }

    day_label(sbuf, date_dow(cur_y, cur_mo, cur_d), cur_d, cur_mo);
    strcat(sbuf, "   ");
    if (gc_daycnt[cur_d] == 0) {
        strcat(sbuf, "no events");
    } else {
        utoa(gc_daycnt[cur_d], n, 10);
        strcat(sbuf, n);
        strcat(sbuf, gc_daycnt[cur_d] == 1 ? " event" : " events");
    }
    scr_row_clear(MO_SUM_ROW);
    scr_text(MO_SUM_ROW, 2, sbuf, 0);
    scr_right(MO_SUM_ROW, 78, "RETURN day  ESC setup  Q quit", 0);
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

void ui_view(unsigned char view, unsigned char sel, unsigned char first)
{
    det_valid = 0;
    ui_view_now = view;
    ui_header(view);

    switch (view) {
    case VIEW_WEEK:   draw_week(sel); break;
    case VIEW_MONTH:  draw_month(); break;
    case VIEW_AGENDA: draw_agenda(sel, first); break;
    default:          draw_day(sel, first); break;
    }

    ui_hints(view);
}

/*
 * Repaint only what a selection move touched: the row being left, the row
 * being landed on, and the detail line. With the difference doing the
 * sending a full repaint would cost the wire the same, but not the CPU.
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

    ui_view(view, to, first);
}

/* ------------------------------------------------------------------ */
/* Event detail                                                        */
/* ------------------------------------------------------------------ */

/*
 * The event's own date. The anchor is only right in DAY: in WEEK an event
 * carries its day of the week, in AGENDA its day and month -- and the agenda
 * runs forward from the anchor, so a month before the anchor's is next
 * year's. (The Apple's detail always shows the anchor.)
 */
static void ev_date(const struct event *e, unsigned int *y, unsigned char *mo,
                    unsigned char *d)
{
    unsigned char n;

    *y = cur_y;
    *mo = cur_mo;
    *d = cur_d;

    if (ui_view_now == VIEW_AGENDA && e->mon) {
        if (e->mon < cur_mo)
            (*y)++;
        *mo = e->mon;
        *d = e->day;
    } else if (ui_view_now == VIEW_WEEK) {
        week_start(y, mo, d);
        for (n = e->day; n; n--)
            date_addday(y, mo, d);
    }
}

void ui_detail(unsigned char ev, unsigned int top)
{
    struct event *e = &gc_index[ev];
    unsigned int  i, y;
    unsigned char mo, d;
    char t[6];
    char n[6];

    /* Scrolled by one line: let the terminal move the text. */
    if (det_valid && det_ev == ev) {
        if (top == det_top + 1)
            h19_scroll(DET_TOP, DET_TOP + DET_WIN - 1, 1);
        else if (top + 1 == det_top)
            h19_scroll(DET_TOP, DET_TOP + DET_WIN - 1, -1);
    }
    lst_valid = 0;
    det_valid = 1;
    det_ev = ev;
    det_top = top;

    /* The view's header stays, so going in and out of an event sends only
       the box. */
    ui_header(ui_view_now);

    ev_date(e, &y, &mo, &d);
    day_label(sbuf, date_dow(y, mo, d), d, mo);
    strcat(sbuf, "  ");
    if (e->flags & EVF_ALLDAY) {
        strcat(sbuf, "all day");
    } else {
        ui_hhmm(t, e->sh, e->sm);
        strcat(sbuf, t);
        if (e->flags & EVF_OPENEND) {
            strcat(sbuf, " onward");
        } else {
            strcat(sbuf, "-");
            ui_hhmm(t, e->eh, e->em);
            strcat(sbuf, t);
        }
    }
    if (e->cat[0]) {
        strcat(sbuf, "  ");
        strcat(sbuf, e->cat);
    }

    for (i = DET_BOX_TOP; i <= DET_BOX_BOT; i++)
        scr_row_clear((unsigned char) i);
    scr_box(DET_BOX_TOP, 0, DET_BOX_BOT, SCR_COLS - 1);
    scr_rule(DET_BOX_TOP, 1, SCR_COLS - 2, sbuf, 1);

    /* The title, as a bar of its own across the box */
    scr_field(DET_TITLE_ROW, 1, " ", 1, 1);
    scr_field(DET_TITLE_ROW, DET_TEXT_COL, e->title, SCR_COLS - 1 - DET_TEXT_COL,
              1);
    scr_cell(DET_RULE_ROW, 0, G_TEE_RIGHT);
    scr_fill(DET_RULE_ROW, 1, G_HLINE, SCR_COLS - 2);
    scr_cell(DET_RULE_ROW, SCR_COLS - 1, G_TEE_LEFT);

    for (i = 0; i < DET_WIN && top + i < gc_det_rows; i++)
        scr_field((unsigned char) (DET_TOP + i), DET_TEXT_COL, gc_det[top + i],
                  DET_COLS, 0);

    utoa((unsigned int) (top + 1), sbuf, 10);
    strcat(sbuf, "/");
    utoa(gc_det_rows ? gc_det_rows : 1, n, 10);
    strcat(sbuf, n);
    if (gc_det_trunc)
        strcat(sbuf, "+");
    scr_rule(DET_BOX_BOT, (unsigned char) (SCR_COLS - 7 - strlen(sbuf)),
             (unsigned char) (strlen(sbuf) + 4), sbuf, 0);

    scr_keys("[^v] Line [<>] Page  [RED] Edit  [ESC] Back");
}

/* ------------------------------------------------------------------ */
/* Alarm banner                                                        */
/* ------------------------------------------------------------------ */

/*
 * The banner takes over the 25th line, which is why alarms only fire in a
 * view: a view is the only screen that knows how to paint its keys back.
 * Only the time flashes -- the line is resent by difference, so a flash is
 * eight cells rather than eighty.
 */
void ui_alarm(unsigned char phase)
{
    struct event *e;

    scr_keys(0);
    if (al_ev >= gc_count)
        return;

    e = &gc_index[al_ev];
    sbuf[0] = '!';
    sbuf[1] = ' ';
    ui_hhmm(sbuf + 2, e->sh, e->sm);
    scr_field(KEY_ROW, 1, sbuf, 7, phase);
    scr_field(KEY_ROW, 10, e->title, SCR_COLS - 11, 0);
}
