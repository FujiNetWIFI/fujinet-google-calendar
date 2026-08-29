/*
 * The four calendar views, the event detail screen, and the alarm banner.
 *
 * Every list row here paints columns 1 to 39 and leaves column 0 alone. That
 * column is the chip gutter, where a player draws the event's Google colour,
 * and an inverse space would be COLPF1 -- which sits in front of the players
 * and would cover the chip on exactly the row the selection is on. The
 * Intellivision kept its column 0 out of the selection bar for the same
 * reason. It is the one place this client departs from the Gmail client's
 * paint-all-forty-columns rule.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* DAY and AGENDA: sixteen list rows, a blank, two rows of detail, a blank. */
#define LIST_TOP        CONTENT_TOP             /* 3  */
#define DETAIL_ROW      20

/* Columns shared by the DAY and AGENDA rows. */
#define COL_CARET       1
#define COL_MARK        2
#define COL_TIME        4
#define W_TIME          5
#define COL_TITLE       10
#define W_TITLE         (SCR_COLS - COL_TITLE)

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
#define MO_CELL_W       5
#define MO_LEFT         2
#define MO_SUM_ROW      22

/* Event detail: an eighteen-row window over the wrapped text. */
#define DET_TOP         3

/* Right-hand status ends here, leaving column 39 blank. */
#define FOOT_RIGHT      38

static unsigned char chips[CONTENT_ROWS];
static char          sbuf[64];
static char          wrapbuf[2][41];

static void chips_reset(void)
{
    memset(chips, CHIP_NONE, sizeof(chips));
}

static void chips_flush(void)
{
    pmg_chips(chips, CONTENT_TOP, CONTENT_ROWS);
}

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
 * already decided which event and whether it is selected; the chip is left to
 * the caller too, because a WEEK day row's chip comes from a different event
 * than its text does.
 */
static void draw_event(unsigned char row, unsigned char ev, unsigned char sel)
{
    struct event *e = &gc_index[ev];
    char t[6];
    char m[2];

    m[0] = marker(e);
    m[1] = '\0';
    time_field(t, e);

    scr_field(row, COL_CARET, sel ? ">" : " ", 1, sel);
    scr_field(row, COL_MARK, m, 1, sel);
    scr_field(row, 3, "", 1, sel);
    scr_field(row, COL_TIME, t, W_TIME, sel);
    scr_field(row, 9, "", 1, sel);
    scr_field(row, COL_TITLE, e->title, W_TITLE, sel);
}

/*
 * The two-row panel that spells the selection out in full. The list column is
 * thirty characters wide and a Google summary routinely runs to fifty, so this
 * is what earns storing the longer title.
 */
/*
 * wrap_text() writes only the rows it produces and returns how many -- it does
 * not blank the rest of the array. Both callers here reuse one static buffer,
 * so anything past the returned count is the *previous* selection's text and
 * has to be cleared rather than drawn.
 */
static void draw_wrapped(unsigned char row, unsigned char rows)
{
    unsigned char i;

    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (row + i), 0,
                  (i < rows) ? wrapbuf[i] : "", SCR_COLS, 0);
}

static void draw_detail_lines(unsigned char ev)
{
    struct event *e;
    char t[6];

    scr_rows_clear(DETAIL_ROW, DETAIL_ROW + 1);
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
        strcpy(sbuf, "All day  ");
    }
    strcat(sbuf, e->title);

    draw_wrapped(DETAIL_ROW,
                 (unsigned char) wrap_text(sbuf, (char *) wrapbuf, 2,
                                           SCR_COLS, 41));
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
    dli_foot_colors(C_FOOT_BG, C_FOOT_FG);

    switch (view) {
    /* Kept short enough to leave a gap before the page indicator, which is
       right-aligned to column 38. The arrow keys need no caption. */
    case VIEW_MONTH:
        ui_footer("RET:DAY  ^v<>:MOVE  ESC:SET", 0);
        break;
    case VIEW_WEEK:
        ui_footer("RET:DAY  <>:WEEK  ESC:SET", 0);
        break;
    case VIEW_AGENDA:
        ui_footer("RET:OPEN  <>:WEEK  ESC:SET", 0);
        break;
    default:
        ui_footer("RET:OPEN  <>:DAY  ESC:SET", 0);
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
        chips[i] = gc_index[ev].chip;
    }

    scr_row_clear(LIST_TOP + LIST_ROWS);
    draw_detail_lines(gc_count ? sel : 0xFF);
    scr_row_clear(FOOT_ROW - 1);

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
            /* A separator is a heading, never a selection: the chip gutter and
               the caret column both stay empty on it. */
            scr_row_clear(row);
            sbuf[0] = (char) ('0' + gc_index[ev].day / 10);
            sbuf[1] = (char) ('0' + gc_index[ev].day % 10);
            sbuf[2] = ' ';
            strcpy(sbuf + 3, date_mon3(gc_index[ev].mon));
            scr_text(row, COL_TIME, sbuf, 1);
            continue;
        }

        draw_event(row, ev, (unsigned char) (first + i == sel));
        chips[i] = gc_index[ev].chip;
    }

    scr_row_clear(LIST_TOP + LIST_ROWS);
    if (sel < gc_agd_count && !(gc_agd[sel] & AGD_SEP))
        draw_detail_lines((unsigned char) (gc_agd[sel] & AGD_IDX));
    else
        draw_detail_lines(0xFF);
    scr_row_clear(FOOT_ROW - 1);

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

        scr_field(row, COL_CARET, inv ? ">" : " ", 1, inv);
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
        scr_field(row, 10, "", 2, inv);
        if (n) {
            utoa(n, sbuf, 10);
            scr_right(row, 11, sbuf, inv);
        }
        scr_field(row, 12, "", 1, inv);

        if (lead != 0xFF) {
            time_field(t, &gc_index[lead]);
            strcpy(sbuf, t);
            strcat(sbuf, " ");
            strcat(sbuf, gc_index[lead].title);
            scr_field(row, 13, sbuf, SCR_COLS - 13, inv);
            chips[i] = gc_index[lead].chip;
        } else {
            scr_field(row, 13, "(nothing)", SCR_COLS - 13, inv);
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
        chips[row - CONTENT_TOP] = gc_index[i].chip;
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
 * A day cell is five columns on two rows: the number, and under it a bar of
 * inverse spaces one per event up to four. GRAPHICS 0 cannot give a single
 * cell its own hue -- COLPF2 is a whole-scanline property and seven days share
 * a row -- so the density bar is what carries "how busy" here, and it needs no
 * custom character set to do it.
 */
static void month_cell(unsigned char band, unsigned char dow, unsigned char day,
                       unsigned char sel)
{
    unsigned char col = (unsigned char) (MO_LEFT + dow * MO_CELL_W);
    unsigned char nrow = (unsigned char) (MO_TOP + band * MO_PITCH);
    unsigned char brow = (unsigned char) (nrow + 1);
    unsigned char n, i;

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
        sbuf[4] = '\0';
        scr_field(nrow, col, " ", 1, sel);
        scr_field(nrow, (unsigned char) (col + 1), sbuf, 4, sel);
    } else {
        sbuf[0] = ' ';
        sbuf[1] = ' ';
        sbuf[2] = (char) ('0' + day / 10);
        sbuf[3] = (char) ('0' + day % 10);
        sbuf[4] = '\0';
        if (day < 10)
            sbuf[2] = ' ';
        scr_field(nrow, col, sbuf, MO_CELL_W, sel);
    }

    n = gc_daycnt[day];
    if (n > 4)
        n = 4;

    scr_field(brow, col, "", (unsigned char) (MO_CELL_W - n), sel);
    for (i = 0; i < n; i++)
        scr_field(brow, (unsigned char) (col + MO_CELL_W - n + i), "", 1, 1);
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
    chips_reset();
    ui_header(view);

    switch (view) {
    case VIEW_WEEK:   draw_week(sel); break;
    case VIEW_MONTH:  draw_month(); break;
    case VIEW_AGENDA: draw_agenda(sel, first); break;
    default:          draw_day(sel, first); break;
    }

    chips_flush();
}

/*
 * Repaint only what a selection move touched: the row being left, the row
 * being landed on, and the detail panel. The chips are deliberately not
 * touched -- a chip belongs to an event, not to the selection, so moving the
 * caret cannot change one. That is what keeps holding a cursor key from
 * flickering the whole list.
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
        draw_detail_lines(gc_count ? to : 0xFF);
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
            draw_detail_lines(ev);
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
    char t[6];
    char n[6];

    dli_bands();
    pmg_hide();
    pmg_chips_clear();
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
    scr_field(0, 0, sbuf, SCR_COLS, 0);

    draw_wrapped(1, (unsigned char) wrap_text(e->title, (char *) wrapbuf, 2,
                                              SCR_COLS, 41));

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

    dli_foot_colors(C_FOOT_BG, C_FOOT_FG);
    ui_footer("^v:LINE  <>:PAGE  ESC:BACK", sbuf);
}

/* ------------------------------------------------------------------ */
/* Alarm banner                                                        */
/* ------------------------------------------------------------------ */

/*
 * The banner takes over the footer, which is why alarms only fire in a view:
 * a view is the only screen that knows how to paint its hints back. The flash
 * is two bytes the DLI reads on its next pass, so alternating the colour
 * repaints nothing at all.
 */
void ui_alarm(unsigned char phase)
{
    struct event *e;
    char t[6];

    dli_foot_colors(phase ? C_ALARM_A : C_ALARM_B, C_FOOT_FG);

    if (al_ev >= gc_count)
        return;

    e = &gc_index[al_ev];
    ui_hhmm(t, e->sh, e->sm);

    strcpy(sbuf, "! ");
    strcat(sbuf, t);
    strcat(sbuf, " ");
    strcat(sbuf, e->title);

    scr_field(FOOT_ROW, 0, "", 1, 0);
    scr_field(FOOT_ROW, 1, sbuf, SCR_COLS - 1, 0);
}
