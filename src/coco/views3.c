#ifdef COCO3

/*
 * The four calendar views, the event detail screen, and the alarm banner --
 * the MS-DOS backend's painters, on the CoCo 3's GIME text page.
 *
 * They are that backend's because they name attribute roles rather than an
 * inverse flag, which is what an attribute plane needs and what the 32-column
 * VDG painters in views.c cannot express. Only one of the two pairs compiles.
 *
 * Two things differ from the backend they came from. The geometry is
 * compile-time here -- this machine is always 80x24, so platform.h has
 * ui_geom()'s 80-column arm as constants and the scr_wide branches fold away.
 * And there is no category column: GC_KEEP_CAT costs 960 bytes to hold the
 * field, w_cat is 0, and the title simply starts where the category would.
 *
 * Every list row paints from column 1 to the edge and leaves column 0 alone.
 * That column is the chip gutter, and every backend keeps it out of the
 * selection bar for its own hardware reason -- the Atari because inverse video
 * draws in front of the player carrying the chip, the Apple because MouseText
 * has no inverse form. Here neither constraint exists, and the rule is kept
 * because it is right: the chip belongs to the event, not to the cursor.
 *
 * Colour chips are ink_attr()'s seven, not the MS-DOS backend's eleven -- see
 * the palette note in platform.h -- painted as a space on a colored ground,
 * because on this machine a cell's color is its background.
 */

#include <cmoc.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* DAY and AGENDA: eighteen list rows, a blank, two rows of detail. */
#define LIST_TOP        CONTENT_TOP             /* 3     */
#define DETAIL_ROW      (FOOT_ROW - 2)          /* 21..22 */

/* Columns shared by the DAY and AGENDA rows; the rest are in platform.h. */
#define COL_CHIP        0
#define COL_CARET       1
#define COL_MARK        2
#define COL_TIME        4
#define W_TIME          5

/* WEEK: seven day rows, then the selected day's events. */
#define WEEK_TOP        CONTENT_TOP             /* 3..9  */
#define WEEK_DAYS       7
#define WEEK_PANEL_HDR  11
#define WEEK_PANEL_TOP  12
#define WEEK_PANEL_ROWS (FOOT_ROW - WEEK_PANEL_TOP)

/* MONTH: a headings row, then six week bands on a three-row pitch, and the
   25th line is what pays for a summary row with a blank above it. */
#define MO_HEAD_ROW     3
#define MO_TOP          4
#define MO_PITCH        3
#define MO_NUMW         4
#define MO_SUM_ROW      (FOOT_ROW - 1)          /* 22 */

/* Event detail: a twenty-row window over the wrapped text. */
#define DET_TOP         3

static char sbuf[96];
static char detrow[DET_STRIDE];
static char wrapbuf[2][81];

/* ------------------------------------------------------------------ */
/* Shared row painting                                                 */
/* ------------------------------------------------------------------ */

/* The event's color, in the gutter: its own ink in color mode, its ramp
   glyph on the two tables without one. */
static void chip_cell(unsigned char row, const struct event *e)
{
    if (scr_color)
        scr_cell(row, COL_CHIP, GL_BLOCK, ink_attr(e->color));
    else
        scr_cell(row, COL_CHIP, chip_glyph(e->chip), scr_attr_byte(A_TEXT));
}

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
    unsigned char r = (unsigned char) (sel ? A_SEL : A_TEXT);
    char t[6];
    char m[2];

    m[0] = marker(e);
    m[1] = '\0';
    time_field(t, e);

    chip_cell(row, e);
    scr_field(row, COL_CARET, sel ? ">" : " ", 1, r);
    scr_field(row, COL_MARK, m, 1, r);
    scr_field(row, 3, "", 1, r);
    scr_field(row, COL_TIME, t, W_TIME, r);
    scr_field(row, 9, "", 1, r);
#ifdef GC_KEEP_CAT
    if (w_cat) {
        scr_field(row, col_cat, e->cat, w_cat, r);
        scr_field(row, (unsigned char) (col_cat + w_cat), "", 1, r);
    }
#endif
    scr_field(row, col_title, ev_title(ev), w_title, r);
}

/*
 * The two-row panel that spells the selection out in full: the whole time
 * range, which the list column only shows the start of, and the title again
 * in case the list column clipped it. At 80 columns one row would usually
 * do, but the Atari's two-row panel costs nothing here and stops a long
 * title being clipped twice.
 */
/*
 * wrap_text() writes only the rows it produces and returns how many -- it
 * does not blank the rest of the array. Both callers here reuse one static
 * buffer, so anything past the returned count is the *previous* selection's
 * text and has to be cleared rather than drawn.
 */
static void draw_wrapped(unsigned char row, unsigned char rows)
{
    unsigned char i;

    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (row + i), 0,
                  (i < rows) ? wrapbuf[i] : "", scr_cols, A_TEXT);
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
        strcpy(sbuf, "All day      ");
    }
    strcat(sbuf, ev_title(ev));

    draw_wrapped(DETAIL_ROW,
                 (unsigned char) wrap_text(sbuf, (char *) wrapbuf, 2,
                                           scr_cols, 81));
}

/*
 * `total` is not always gc_count: the agenda's rows include date
 * separators, so its window is measured against gc_agd_count or the
 * indicator claims to be showing more rows than there are events.
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
 * The single place the footer is composed. The alarm banner borrows row 24
 * and calls this to give it back, so every view's hints have to be
 * reachable from the view number alone. Two tables, VIEW_* order: the
 * 40-column strings stop early enough to clear the page indicator.
 */
static const char *const hints80[4] = {
    "RET:OPEN  " GL_UPDOWN ":MOVE  " GL_LR ":DAY  ESC:SETTINGS",
    "RET:DAY  " GL_UPDOWN ":MOVE  " GL_LR ":WEEK  ESC:SETTINGS",
    "RET:DAY  " GL_UPDOWN GL_LR ":MOVE  ESC:SETTINGS",
    "RET:OPEN  " GL_UPDOWN ":MOVE  " GL_LR ":WEEK  ESC:SETTINGS",
};

static const char *const hints40[4] = {
    "RET:OPEN  " GL_LR ":DAY  ESC:SET",
    "RET:DAY  " GL_LR ":WEEK  ESC:SET",
    "RET:DAY  " GL_UPDOWN GL_LR ":MOVE  ESC:SET",
    "RET:OPEN  " GL_LR ":WEEK  ESC:SET",
};

void ui_hints(unsigned char view)
{
    ui_footer((scr_wide ? hints80 : hints40)[view & 3], 0);
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
    draw_detail_lines(gc_count ? sel : 0xFF);

    ui_hints(VIEW_DAY);
    page_indicator(first, shown, gc_count);
    scr_right(FOOT_ROW, right_col, sbuf, A_FOOT);
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
               and the caret both stay empty on it. A_TITLE gives it the
               blue accent in color and the MDA's own underline in mode 7,
               and at 80 columns a rule runs out to the edge -- the Atari
               has room for the date and nothing else. */
            scr_row_clear(row);
            sbuf[0] = (char) ('0' + gc_index[ev].day / 10);
            sbuf[1] = (char) ('0' + gc_index[ev].day % 10);
            sbuf[2] = ' ';
            strcpy(sbuf + 3, date_mon3(gc_index[ev].mon));
            scr_text(row, COL_TIME, sbuf, A_TITLE);
            if (scr_wide)
                scr_fill(row, COL_TIME + 8, GL_RULE,
                         (unsigned char) (scr_cols - COL_TIME - 8), A_DIM);
            continue;
        }

        draw_event(row, ev, (unsigned char) (first + i == sel));
    }

    scr_row_clear(LIST_TOP + LIST_ROWS);
    if (sel < gc_agd_count && !(gc_agd[sel] & AGD_SEP))
        draw_detail_lines((unsigned char) (gc_agd[sel] & AGD_IDX));
    else
        draw_detail_lines(0xFF);

    ui_hints(VIEW_AGENDA);
    page_indicator(first, shown, gc_agd_count);
    scr_right(FOOT_ROW, right_col, sbuf, A_FOOT);
}

/* ------------------------------------------------------------------ */
/* WEEK                                                                */
/* ------------------------------------------------------------------ */

/*
 * The adapter's WEEK date column is only "Fri", with no day number, so the
 * dates down the left are worked out here: walk back to the week's Sunday
 * and step forward. wkst is left at the adapter's own default of Sunday.
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

/* The day's event count, and the index of its earliest -- the adapter sorts
   by start time, so the first match is the one worth showing. */
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
        unsigned char on = (unsigned char) (i == sel);
        unsigned char r = (unsigned char) (on ? A_SEL : A_TEXT);
        char dd[3];

        row = (unsigned char) (WEEK_TOP + i);
        n = week_day_events(i, &lead);

        /* Column 0 stays out of the bar, same as the list rows. */
        if (lead != 0xFF)
            chip_cell(row, &gc_index[lead]);
        else
            scr_field(row, COL_CHIP, " ", 1, A_TEXT);

        scr_field(row, 1, on ? ">" : " ", 1, r);
        scr_field(row, 2, "", 1, r);
        scr_field(row, 3, date_dow3(i), 3, r);
        scr_field(row, 6, "", 1, r);

        /* The day number carries the today mark as well as the selection,
           so the week still reads at a glance when the cursor is elsewhere
           -- and A_TODAY is the underline the MDA was asked for. */
        dd[0] = (char) ('0' + d / 10);
        dd[1] = (char) ('0' + d % 10);
        dd[2] = '\0';
        scr_field(row, 7, dd, 2,
                  on ? A_SEL
                     : (unsigned char) (clk_is_today(y, mo, d) ? A_TODAY
                                                               : A_TEXT));

        scr_field(row, 9, "", 1, r);
        scr_field(row, 10, "", (unsigned char) (wk_ncol - 9), r);
        if (n) {
            utoa(n, sbuf, 10);
            scr_right(row, wk_ncol, sbuf, r);
        }
        scr_field(row, (unsigned char) (wk_ncol + 1), "", 1, r);

        if (lead != 0xFF) {
            time_field(t, &gc_index[lead]);
            strcpy(sbuf, t);
            strcat(sbuf, "  ");
            strcat(sbuf, ev_title(lead));
            scr_field(row, wk_tcol, sbuf,
                      (unsigned char) (scr_cols - wk_tcol), r);
        } else {
            scr_field(row, wk_tcol, "(nothing)",
                      (unsigned char) (scr_cols - wk_tcol),
                      on ? A_SEL : A_DIM);
        }

        date_addday(&y, &mo, &d);
    }

    /* The panel below costs no fetch: the WEEK index already carries a day
       of week for every event, so this is a redraw straight out of RAM. */
    scr_row_clear(WEEK_TOP + WEEK_DAYS);

    week_start(&y, &mo, &d);
    for (i = 0; i < sel; i++)
        date_addday(&y, &mo, &d);

    strcpy(sbuf, date_dow3(sel));
    strcat(sbuf, " ");
    utoa(d, sbuf + strlen(sbuf), 10);
    strcat(sbuf, " ");
    strcat(sbuf, date_mon3(mo));
    scr_field(WEEK_PANEL_HDR, 2, sbuf, (unsigned char) (scr_cols - 2),
              A_TITLE);

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
 * A day cell is mo_cellw columns on two rows: the number, and under it a
 * bar one cell per event up to mo_bars. In color the bar is full blocks in
 * the day's leading color -- gc_daycol keeps the COL_* exactly so a
 * backend with eleven inks can use it -- which makes this the first month
 * grid since the Adam's that says *what kind* of busy. Monochrome falls
 * back to the inverse-space bar.
 *
 * Only the four-character number box takes the selection. At eleven
 * columns a fully inverted cell would be a slab with the bar swallowed
 * inside it; at five the four-char box is nearly the whole cell anyway,
 * so one rule serves both widths.
 */
static void month_cell(unsigned char band, unsigned char dow,
                       unsigned char day, unsigned char sel)
{
    unsigned char col = (unsigned char) (mo_left + dow * mo_cellw);
    unsigned char nrow = (unsigned char) (MO_TOP + band * MO_PITCH);
    unsigned char brow = (unsigned char) (nrow + 1);
    unsigned char n, i, today;

    if (day == 0) {
        scr_field(nrow, col, "", mo_cellw, A_TEXT);
        scr_field(brow, col, "", mo_cellw, A_TEXT);
        return;
    }

    /* Today is bracketed as well as accented: on the black-and-white table
       the accent is only intensity, and the brackets are what survive. */
    today = clk_is_today(cur_y, cur_mo, day);
    if (today) {
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

    scr_field(nrow, col, sbuf, MO_NUMW,
              sel ? A_SEL : (unsigned char) (today ? A_TODAY : A_TEXT));
    scr_field(nrow, (unsigned char) (col + MO_NUMW), "",
              (unsigned char) (mo_cellw - MO_NUMW), A_TEXT);

    /* Left-aligned under the number rather than right-aligned in the cell:
       a bar on the far side would read as belonging to the day after. */
    n = gc_daycnt[day];
    if (n > mo_bars)
        n = mo_bars;

    if (scr_color)
        for (i = 0; i < n; i++)
            scr_cell(brow, (unsigned char) (col + 1 + i), GL_BLOCK,
                     ink_attr(gc_daycol[day]));
    else
        scr_fill(brow, (unsigned char) (col + 1), ' ', n, A_SEL);

    scr_field(brow, (unsigned char) (col + 1 + n), "",
              (unsigned char) (mo_cellw - 1 - n), A_TEXT);
    scr_field(brow, col, "", 1, A_TEXT);
}

static void draw_month(void)
{
    static const char *const head[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    unsigned char band, dow, day, cell;
    unsigned char today_dow = 0xFF;
    char n[6];

    month_geom();

    /* Today's column heading gets the accent -- the underline, on an MDA --
       but only while the grid is actually showing today's month. */
    if (clk_ok && cur_y == clk_y && cur_mo == clk_mo)
        today_dow = date_dow(clk_y, clk_mo, clk_d);

    scr_rows_clear(MO_HEAD_ROW, FOOT_ROW - 1);
    for (dow = 0; dow < 7; dow++)
        scr_text(MO_HEAD_ROW, (unsigned char) (mo_left + dow * mo_cellw + 1),
                 head[dow],
                 (unsigned char) (dow == today_dow ? A_TODAY : A_DIM));

    for (band = 0; band < 6; band++) {
        for (dow = 0; dow < 7; dow++) {
            cell = (unsigned char) (band * 7 + dow);
            day = (cell < mo_first) ? 0
                                    : (unsigned char) (cell - mo_first + 1);
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
    scr_field(MO_SUM_ROW, 2, sbuf, (unsigned char) (scr_cols - 2), A_TEXT);

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
 * being landed on, and the detail panel. That is what keeps holding a
 * cursor key from flickering the whole list.
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
#ifdef GC_KEEP_CAT
    if (w_cat && e->cat[0]) {
        strcat(sbuf, "   ");
        strcat(sbuf, e->cat);
    }
#endif
    scr_field(0, 0, sbuf, scr_cols, A_BAR);

    /*
     * The title rows take A_TITLE -- the requirement the MDA table was
     * built for: a real underline under the title in mode 7. scr_text
     * rather than a padded field, so the underline runs under the words and
     * not out to the edge of the row.
     */
    rows = wrap_text(ev_title(ev), (char *) wrapbuf, 2, scr_cols, 81);
    for (i = 0; i < 2; i++) {
        scr_row_clear((unsigned char) (1 + i));
        if (i < rows)
            scr_text((unsigned char) (1 + i), 0, wrapbuf[i], A_TITLE);
    }

    /* The wrapped text is at most gc_wrap_cols wide, painted from column 1
       -- which is why plat_init() set the wrap to scr_cols - 2. */
    for (i = 0; i < DET_WIN; i++) {
        scr_row_clear((unsigned char) (DET_TOP + i));
        if (top + i < gc_det_rows) {
            /* One row at a time out of far storage -- see src/coco/far.c. */
            far_get(detrow,
                    (unsigned int) (FAR_DET + (top + i) * DET_STRIDE),
                    DET_STRIDE);
            scr_text((unsigned char) (DET_TOP + i), 1, detrow, A_TEXT);
        }
    }
    scr_rows_clear(DET_TOP + DET_WIN, FOOT_ROW - 1);

    utoa((unsigned int) (top + 1), sbuf, 10);
    strcat(sbuf, "/");
    utoa(gc_det_rows ? gc_det_rows : 1, n, 10);
    strcat(sbuf, n);
    if (gc_det_trunc)
        strcat(sbuf, "+");

    ui_footer(GL_UPDOWN ":LINE  " GL_LR ":PAGE  ESC:BACK", sbuf);
}

/* ------------------------------------------------------------------ */
/* Alarm banner                                                        */
/* ------------------------------------------------------------------ */

/*
 * The banner takes over the footer, which is why alarms only fire in a
 * view: a view is the only screen that knows how to paint its hints back.
 *
 * The Atari flashes it by alternating two color registers, which repaints
 * nothing at all. Here the flash is the row being repainted between two
 * roles -- red band and red-on-page in color, reverse and bright on the
 * monochrome tables -- which is eighty stores twice a second and cheap.
 */
void ui_alarm(unsigned char phase)
{
    struct event *e;
    unsigned char r = (unsigned char) (phase ? A_ALARM_A : A_ALARM_B);
    char t[6];

    if (al_ev >= gc_count) {
        scr_field(FOOT_ROW, 0, "", scr_cols, r);
        return;
    }

    e = &gc_index[al_ev];
    ui_hhmm(t, e->sh, e->sm);

    strcpy(sbuf, "! ");
    strcat(sbuf, t);
    strcat(sbuf, "  ");
    strcat(sbuf, ev_title(al_ev));

    scr_field(FOOT_ROW, 0, "", 1, r);
    scr_field(FOOT_ROW, 1, sbuf, (unsigned char) (scr_cols - 1), r);
}

#endif /* COCO3 */
