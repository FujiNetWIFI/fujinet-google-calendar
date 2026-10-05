/*
 * WEEK and MONTH, the two views that are grids of days rather than lists of
 * events. Overlay bank 2 (OVL_GRID); reached only through tramp.c.
 *
 * WEEK
 *   rows 6-12  the seven days: "Sun", the date (today's in Google's blue
 *              disc), how many events, and a strip of squares, one per event
 *              in its own colour -- five at most, then "+n": seven days
 *              of five, the dots below and the mark are 44 of the PPU's 64
 *              sprites
 *   row  13    a rule
 *   rows 14-19 the selected day's events, as the DAY list shows them
 *   row  20    a rule; rows 21-22 the selected day, written out
 *
 * MONTH
 *   row  6     Su Mo Tu We Th Fr Sa
 *   rows 8-19  six weeks on a two-row pitch: the date, and under it a
 *              sprite of one to three dots in the colour of the day's
 *              leading event. Each day is a 4x2-tile cell, which is exactly
 *              two attribute blocks, so nothing in the grid fights the
 *              PPU's 16x16 colour cells. Today is the blue disc; the
 *              selected day is the highlight ground across its cell.
 *   rows 21-22 the selected day and its count
 *
 * Moving the selection repaints only the two days it touched and the panel.
 * MONTH's cursor is main.c's anchor date, so main asks for a whole new view
 * on every move; scr_gen says whether this month is still the one on
 * screen, and if it is the move is drawn in place.
 */

#include <string.h>

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

/* WEEK */
#define WK_TOP          LIST_TOP                /* 6-12 */
#define WK_RULE         13
#define WK_EV_TOP       14                      /* 14-19 */
#define WK_EV_ROWS      6
#define WK_DOW          2
#define WK_DAY          6                       /* 6-7: the date (a disc) */
#define WK_CNT          9                       /* 9-10 */
#define WK_CHIP         12                      /* 12-16: sprites */
#define WK_CHIPS        5
#define WK_MORE         (WK_CHIP + WK_CHIPS)    /* "+n" */
#define WK_EV_SPR       (SPR_CONTENT + 7 * WK_CHIPS)    /* the events' dots */
#define WK_BAR          2
#define WK_BAR_W        28

/* MONTH */
#define MO_HEAD         6
#define MO_TOP          8
#define MO_LEFT         2
#define MO_CELL         4

static const char LEGEND_WEEK[] = "\x10" "Day {Select}View {Start}Menu";
static const char LEGEND_MONTH[] = "\x10" "Day {Select}View {Start}Menu";

static uint8_t mo_gen, mo_y_lo, mo_mo, mo_d;    /* what the grid shows */

/* ------------------------------------------------------------------ */
/* Shared                                                              */
/* ------------------------------------------------------------------ */

/* A day's number at (row, col), two columns: Google's blue disc for
   today, plain digits otherwise. */
static void day_number(uint8_t row, uint8_t col, uint8_t d, uint8_t today,
                       uint8_t style)
{
    if (today) {
        if (d < 10) {
            scr_tile(row, col, T_DISC_L);
            scr_tile(row, (uint8_t) (col + 1), (uint8_t) (T_DISC_R1 + d));
        } else {
            scr_tile(row, col, (uint8_t) (T_DISC_L + d / 10));
            scr_tile(row, (uint8_t) (col + 1), (uint8_t) (T_DISC_R2 + d % 10));
        }
        return;
    }
    sbuf[0] = (char) (d < 10 ? ' ' : '0' + d / 10);
    sbuf[1] = (char) ('0' + d % 10);
    sbuf[2] = '\0';
    scr_text(row, col, sbuf, style);
}

/* Back to the Sunday of the anchor's week. */
static void week_start(unsigned int *y, uint8_t *mo, uint8_t *d)
{
    uint8_t n;

    *y = cur_y;
    *mo = cur_mo;
    *d = cur_d;
    n = date_dow(*y, *mo, *d);
    while (n--)
        date_subday(y, mo, d);
}

static void day_heading(uint8_t row, unsigned int y, uint8_t mo, uint8_t d,
                        uint8_t count)
{
    char n[6];

    date_line(sbuf, y, mo, d);
    if (clk_is_today(y, mo, d))
        strcat(sbuf, "  (today)");
    scr_field(row, 2, sbuf, 28, 0);

    if (count == 0) {
        strcpy(sbuf, "No events");
    } else {
        num(n, count);
        strcpy(sbuf, n);
        strcat(sbuf, count == 1 ? " event" : " events");
    }
    scr_field((uint8_t) (row + 1), 2, sbuf, 28, 0);
}

/* ------------------------------------------------------------------ */
/* WEEK                                                                */
/* ------------------------------------------------------------------ */

static uint8_t week_count(uint8_t dow)
{
    uint8_t i, n = 0;

    for (i = 0; i < gc_count; i++)
        if (gc_index[i].day == dow)
            n++;
    return n;
}

static void week_row(uint8_t i, unsigned int y, uint8_t mo, uint8_t d,
                     uint8_t sel)
{
    uint8_t row = (uint8_t) (WK_TOP + i);
    uint8_t st = sel ? HL : 0;
    uint8_t n = 0, k;
    char cnt[6];

    scr_field(row, WK_BAR, "", WK_BAR_W, st);
    scr_text(row, WK_DOW, date_dow3(i), st);
    day_number(row, WK_DAY, d, clk_is_today(y, mo, d), st);

    for (k = 0; k < gc_count; k++) {
        if (gc_index[k].day != i)
            continue;
        if (n < WK_CHIPS)
            spr_color((uint8_t) (SPR_CONTENT + i * WK_CHIPS + n),
                      (uint8_t) ((WK_CHIP + n) * 8), (uint8_t) (row * 8),
                      S_SQUARE, gc_index[k].color);
        n++;
    }
    for (k = n; k < WK_CHIPS; k++)
        spr_hide((uint8_t) (SPR_CONTENT + i * WK_CHIPS + k),
                 (uint8_t) (SPR_CONTENT + i * WK_CHIPS + k));

    if (n) {
        num(cnt, n);
        scr_right(row, (uint8_t) (WK_CNT + 1), cnt, st);
    }
    if (n > WK_CHIPS) {
        cnt[0] = '+';
        num(cnt + 1, (unsigned int) (n - WK_CHIPS));
        scr_text(row, WK_MORE, cnt, st);
    }
}

/* The selected day's events, under the strip; when there are more than
   the rows, the last row says how many more. */
static void week_events(uint8_t sel)
{
    uint8_t n = week_count(sel);
    uint8_t fit = (n > WK_EV_ROWS) ? WK_EV_ROWS - 1 : n;
    uint8_t i, r = 0, row;
    struct event *e;
    char t[8];

    for (i = 0; i < gc_count && r < fit; i++) {
        if (gc_index[i].day != sel)
            continue;
        e = &gc_index[i];
        row = (uint8_t) (WK_EV_TOP + r);
        if (e->flags & EVF_ALLDAY)
            strcpy(t, "All day");
        else
            ui_hhmm(t, e->sh, e->sm);
        scr_field(row, 2, t, 8, 0);
        scr_field(row, 10, e->title, 20, 0);
        spr_color((uint8_t) (WK_EV_SPR + r), 8, (uint8_t) (row * 8), S_DOT, e->color);
        r++;
    }

    if (n == 0 || r < n) {
        row = (uint8_t) (WK_EV_TOP + r);
        if (n == 0) {
            strcpy(sbuf, "Nothing planned");
        } else {
            strcpy(sbuf, "+");
            num(sbuf + 1, (unsigned int) (n - r));
            strcat(sbuf, " more: A opens the day");
        }
        scr_field(row, 2, sbuf, 28, 0);
        spr_hide((uint8_t) (WK_EV_SPR + r), (uint8_t) (WK_EV_SPR + r));
        r++;
    }
    for (; r < WK_EV_ROWS; r++) {
        scr_field((uint8_t) (WK_EV_TOP + r), 2, "", 28, 0);
        spr_hide((uint8_t) (WK_EV_SPR + r), (uint8_t) (WK_EV_SPR + r));
    }
}

static void week_panel(uint8_t sel)
{
    unsigned int y;
    uint8_t mo, d, i;

    week_start(&y, &mo, &d);
    for (i = 0; i < sel; i++)
        date_addday(&y, &mo, &d);
    week_events(sel);
    day_heading(PANEL_ROW, y, mo, d, week_count(sel));
}

static void draw_week(uint8_t sel)
{
    unsigned int y;
    uint8_t mo, d, i;

    week_start(&y, &mo, &d);
    for (i = 0; i < 7; i++) {
        week_row(i, y, mo, d, (uint8_t) (i == sel));
        date_addday(&y, &mo, &d);
    }
    scr_rule(WK_RULE);
    scr_rule(PANEL_RULE);
    week_panel(sel);
}

static void week_sel(uint8_t from, uint8_t to)
{
    unsigned int y;
    uint8_t mo, d, i;

    week_start(&y, &mo, &d);
    for (i = 0; i < 7; i++) {
        if (i == from || i == to)
            week_row(i, y, mo, d, (uint8_t) (i == to));
        date_addday(&y, &mo, &d);
    }
    week_panel(to);
}

/* ------------------------------------------------------------------ */
/* MONTH                                                               */
/* ------------------------------------------------------------------ */

static uint8_t mo_first;        /* the weekday of the 1st */
static uint8_t mo_ndays;

static void cell_pos(uint8_t day, uint8_t *row, uint8_t *col)
{
    uint8_t c = (uint8_t) (mo_first + day - 1);

    *row = (uint8_t) (MO_TOP + (c / 7) * 2);
    *col = (uint8_t) (MO_LEFT + (c % 7) * MO_CELL);
}

static void month_cell(uint8_t day, uint8_t sel)
{
    uint8_t row, col, n, slot;

    cell_pos(day, &row, &col);
    scr_field(row, col, "", MO_CELL, sel ? HL : 0);
    day_number(row, (uint8_t) (col + 1), day,
               clk_is_today(cur_y, cur_mo, day), sel ? HL : 0);

    /* The dots: one sprite per day, sprites SPR_CONTENT + day - 1. */
    slot = (uint8_t) (SPR_CONTENT + day - 1);
    n = gc_daycnt[day];
    if (n == 0) {
        spr_hide(slot, slot);
        return;
    }
    spr_color(slot, (uint8_t) ((col + 1) * 8 + 4), (uint8_t) ((row + 1) * 8),
              n == 1 ? S_MDOT1 : (n == 2 ? S_MDOT2 : S_MDOT3), gc_daycol[day]);
}

static void month_panel(void)
{
    day_heading(PANEL_ROW, cur_y, cur_mo, cur_d, gc_daycnt[cur_d]);
    if (gc_daycnt[cur_d])
        spr_color(SPR_CONTENT + 40, 8, PANEL_ROW * 8, S_BAR, gc_daycol[cur_d]);
    else
        spr_hide(SPR_CONTENT + 40, SPR_CONTENT + 40);
}

static void draw_month(void)
{
    static const char head[] = "Su  Mo  Tu  We  Th  Fr  Sa";
    uint8_t day;

    mo_first = date_dow(cur_y, cur_mo, 1);
    mo_ndays = date_dim(cur_y, cur_mo);

    scr_text(MO_HEAD, MO_LEFT + 1, head, 0);
    for (day = 1; day <= mo_ndays; day++)
        month_cell(day, (uint8_t) (day == cur_d));
    scr_rule(PANEL_RULE);
    month_panel();
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

static void status_row(uint8_t view)
{
    unsigned int total = 0;
    uint8_t i;
    char n[6];

    if (gc_ecode) {
        ui_status_text(ui_err_text(gc_ecode));
        return;
    }
    if (view == VIEW_WEEK) {
        total = gc_count;
    } else {
        for (i = 1; i <= 31; i++)
            total += gc_daycnt[i];
    }
    num(n, total);
    strcpy(sbuf, n);
    strcat(sbuf, total == 1 ? " event this " : " events this ");
    strcat(sbuf, view == VIEW_WEEK ? "week" : "month");
    if (gc_trunc)
        strcat(sbuf, ", more");
    ui_status_text(sbuf);
}

void g_view(unsigned char view, unsigned char sel, unsigned char first)
{
    (void) first;

    /* MONTH, the same month still up: just the cursor. */
    if (view == VIEW_MONTH && mo_gen == scr_gen && mo_mo == cur_mo
        && mo_y_lo == (uint8_t) cur_y && mo_d != cur_d) {
        month_cell(mo_d, 0);
        month_cell(cur_d, 1);
        month_panel();
        mo_d = cur_d;
        return;
    }

    scr_begin();
    ui_header(gc_wtitle, gc_cal[0] ? gc_cal : "All calendars");
    ui_tabs(view);
    if (view == VIEW_WEEK) {
        draw_week(sel);
        ui_legend(LEGEND_WEEK);
    } else {
        draw_month();
        ui_legend(LEGEND_MONTH);
    }
    status_row(view);
    in_mode = M_VIEW;
    scr_end();

    mo_gen = scr_gen;
    mo_y_lo = (uint8_t) cur_y;
    mo_mo = cur_mo;
    mo_d = cur_d;
}

void g_view_sel(unsigned char view, unsigned char from, unsigned char to,
                unsigned char first)
{
    if (view == VIEW_WEEK)
        week_sel(from, to);
    else
        g_view(view, to, first);
}
