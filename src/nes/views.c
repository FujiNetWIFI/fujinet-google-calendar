/*
 * DAY and AGENDA -- the two list views -- the panel under them, the status
 * row and the alarm band. Overlay bank 1 (OVL_VIEWS); reached only through
 * tramp.c.
 *
 *   row  2-3   header: the mark, the adapter's window title, the clock;
 *              the calendar
 *   row  4     tabs, row 5 a rule
 *   rows 6-19  the list: the event's colour as a dot in column 1 (a sprite,
 *              so it is the true Google colour and the selection bar cannot
 *              take it away), the time, a repeat mark, the title -- and a
 *              scroll bar in column 30 when the list runs past the window
 *   row 20     a rule
 *   rows 21-22 the selection spelled out: a bar of its colour, the times
 *              and as much of the title as two rows hold
 *   row 24     status, and "1-14/23" at the right
 *   row 25     the buttons
 *
 * AGENDA's date separators are a heading and a rule, Google's agenda in
 * small: "Tue 6 Oct" (or "Today") and a line to the edge.
 */

#include <string.h>

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

#define COL_CHIP        1
#define COL_TIME        2
#define W_TIME          7               /* "10:30am" */
#define COL_MARK        (COL_TIME + W_TIME)     /* 9 */
#define COL_TITLE       (COL_MARK + 1)          /* 10 */
#define W_TITLE         (RIGHT_COL - COL_TITLE) /* 20: 10-29 */
#define COL_SCROLL      RIGHT_COL
#define BAR_COL         COL_TIME
#define BAR_W           (COL_SCROLL - COL_TIME) /* 28: 2-29 */

static unsigned char banner_ev = AL_NONE;
static unsigned char last_first, last_shown, last_total;
static char wrapbuf[2][SCR_W + 1];

static const char LEGEND[] = "\x10Open {Select}View {Start}Menu";

/* ------------------------------------------------------------------ */
/* Rows                                                                */
/* ------------------------------------------------------------------ */

static void time_text(char *dst, const struct event *e)
{
    if (e->flags & EVF_ALLDAY)
        strcpy(dst, "All day");
    else
        ui_hhmm(dst, e->sh, e->sm);
}

/* One event on one row. The chip is sprite SPR_CONTENT + its row. */
static void draw_event(unsigned char row, unsigned char ev, unsigned char sel)
{
    struct event *e = &gc_index[ev];
    unsigned char st = sel ? HL : 0;
    char t[8];

    time_text(t, e);
    scr_field(row, COL_TIME, t, W_TIME, st);
    scr_tile(row, COL_MARK, (e->flags & EVF_RECURRING)
                                ? T_REPEAT
                                : (unsigned char) (' ' | st));
    scr_field(row, COL_TITLE, e->title, W_TITLE, st);

    spr_color((unsigned char) (SPR_CONTENT + row - LIST_TOP), COL_CHIP * 8,
              (unsigned char) (row * 8), S_DOT, e->color);
}

/* An agenda heading: the date, then a rule to the scroll bar. */
static void draw_sep(unsigned char row, unsigned char ev)
{
    struct event *e = &gc_index[ev];
    unsigned int  y = cur_y;
    unsigned char n;
    char d[4];

    /* The window runs forward from the anchor, so a month before the
       anchor's is next year's (compose.c reasons the same way). */
    if (e->mon < cur_mo)
        y++;

    if (clk_is_today(y, e->mon, e->day)) {
        strcpy(sbuf, "Today");
    } else {
        strcpy(sbuf, date_dow3(date_dow(y, e->mon, e->day)));
        strcat(sbuf, " ");
        num(d, e->day);
        strcat(sbuf, d);
        strcat(sbuf, " ");
        strcat(sbuf, date_mon3(e->mon));
    }

    n = (unsigned char) strlen(sbuf);
    scr_text(row, COL_TIME, sbuf, 0);
    scr_tile(row, (unsigned char) (COL_TIME + n), T_BLANK);
    scr_fill(row, (unsigned char) (COL_TIME + n + 1), T_RULE,
             (unsigned char) (COL_SCROLL - COL_TIME - n - 1));
    spr_hide((unsigned char) (SPR_CONTENT + row - LIST_TOP),
             (unsigned char) (SPR_CONTENT + row - LIST_TOP));
}

static void blank_row(unsigned char row)
{
    scr_fill(row, COL_TIME, T_BLANK, BAR_W);
    spr_hide((unsigned char) (SPR_CONTENT + row - LIST_TOP),
             (unsigned char) (SPR_CONTENT + row - LIST_TOP));
}

/* The scroll bar: a track the height of the window, a thumb for the part
   of the list that is showing. Absent when it all fits. */
static void scroll_bar(unsigned char first, unsigned char total)
{
    unsigned char i, t0, t1;

    if (total <= LIST_ROWS) {
        for (i = 0; i < LIST_ROWS; i++)
            scr_tile((unsigned char) (LIST_TOP + i), COL_SCROLL, T_BLANK);
        return;
    }
    t0 = (unsigned char) ((unsigned int) first * LIST_ROWS / total);
    t1 = (unsigned char) (((unsigned int) (first + LIST_ROWS) * LIST_ROWS
                           + total - 1) / total);
    for (i = 0; i < LIST_ROWS; i++)
        scr_tile((unsigned char) (LIST_TOP + i), COL_SCROLL,
                 (i >= t0 && i < t1) ? T_THUMB : T_TRACK);
}

/* ------------------------------------------------------------------ */
/* The panel                                                           */
/* ------------------------------------------------------------------ */

static void draw_panel(unsigned char ev)
{
    struct event *e;
    unsigned char rows, i;

    if (ev == AL_NONE || ev >= gc_count) {
        scr_rows_clear(PANEL_ROW, PANEL_ROW + 1);
        spr_hide(SPR_CONTENT + LIST_ROWS, SPR_CONTENT + LIST_ROWS + 1);
        return;
    }

    e = &gc_index[ev];
    ui_time_range(sbuf, e);
    strcat(sbuf, "  ");
    strcat(sbuf, e->title);

    rows = (unsigned char) wrap_text(sbuf, (char *) wrapbuf, 2, BAR_W, SCR_W + 1);
    for (i = 0; i < 2; i++) {
        scr_field((unsigned char) (PANEL_ROW + i), COL_TIME,
                  (i < rows) ? (const char *) wrapbuf[i] : "", BAR_W, 0);
        spr_color((unsigned char) (SPR_CONTENT + LIST_ROWS + i), COL_CHIP * 8,
                  (unsigned char) ((PANEL_ROW + i) * 8), S_BAR, e->color);
    }
}

/* ------------------------------------------------------------------ */
/* The status row                                                      */
/* ------------------------------------------------------------------ */

static void status_row(void)
{
    char n[6];

    scr_fill(STAT_ROW, 0, T_BLANK, SCR_W);

    if (gc_ecode) {
        ui_status_text(ui_err_text(gc_ecode));
    } else if (gc_count == 0) {
        ui_status_text(cur_view == VIEW_AGENDA ? "Nothing in the next 90 days"
                                               : "No events");
    } else {
        num(n, gc_count);
        strcpy(sbuf, n);
        strcat(sbuf, gc_count == 1 ? " event" : " events");
        if (gc_trunc)
            strcat(sbuf, ", more");
        ui_status_text(sbuf);
    }

    if (last_total > LIST_ROWS && last_shown) {
        num(sbuf, (unsigned int) (last_first + 1));
        strcat(sbuf, "-");
        num(n, (unsigned int) (last_first + last_shown));
        strcat(sbuf, n);
        strcat(sbuf, "/");
        num(n, last_total);
        strcat(sbuf, n);
        scr_right(STAT_ROW, RIGHT_COL, sbuf, 0);
    }
}

void v_hints(unsigned char view)
{
    (void) view;

    banner_ev = AL_NONE;
    spr_hide(SPR_MISC, SPR_MISC);
    scr_pal(STAT_ROW, 0, SCR_W, 2, BP_BODY);
    scr_theme();
    status_row();
    ui_legend(LEGEND);
    in_mode = M_VIEW;
}

/* ------------------------------------------------------------------ */
/* DAY and AGENDA                                                      */
/* ------------------------------------------------------------------ */

static unsigned char shown_of(unsigned char first, unsigned char total)
{
    unsigned char n = (total > first) ? (unsigned char) (total - first) : 0;

    return (n > LIST_ROWS) ? LIST_ROWS : n;
}

static void draw_list(unsigned char view, unsigned char sel, unsigned char first)
{
    unsigned char total = (view == VIEW_AGENDA) ? gc_agd_count : gc_count;
    unsigned char shown = shown_of(first, total);
    unsigned char i, slot, row;

    for (i = 0; i < LIST_ROWS; i++) {
        row = (unsigned char) (LIST_TOP + i);
        if (i >= shown) {
            blank_row(row);
            continue;
        }
        if (view == VIEW_AGENDA) {
            slot = gc_agd[first + i];
            if (slot & AGD_SEP)
                draw_sep(row, (unsigned char) (slot & AGD_IDX));
            else
                draw_event(row, slot, (unsigned char) (first + i == sel));
        } else {
            draw_event(row, (unsigned char) (first + i),
                       (unsigned char) (first + i == sel));
        }
    }
    scroll_bar(first, total);

    last_first = first;
    last_shown = shown;
    last_total = total;
}

static unsigned char sel_event(unsigned char view, unsigned char sel)
{
    if (view == VIEW_AGENDA) {
        if (sel >= gc_agd_count || (gc_agd[sel] & AGD_SEP))
            return AL_NONE;
        return (unsigned char) (gc_agd[sel] & AGD_IDX);
    }
    return (sel < gc_count) ? sel : AL_NONE;
}

void v_view(unsigned char view, unsigned char sel, unsigned char first)
{
    banner_ev = AL_NONE;

    scr_begin();
    ui_header(gc_wtitle, gc_cal[0] ? gc_cal : "All calendars");
    ui_tabs(view);
    draw_list(view, sel, first);
    scr_rule(PANEL_RULE);
    draw_panel(sel_event(view, sel));
    status_row();
    ui_legend(LEGEND);
    in_mode = M_VIEW;
    scr_end();
}

/*
 * A selection move repaints only what it touched: the bar comes off one row
 * and goes on another, out of the shadow, and the panel follows. The chips
 * never move -- a chip belongs to an event, not to the bar.
 */
void v_view_sel(unsigned char view, unsigned char from, unsigned char to,
                unsigned char first)
{
    unsigned char ev;

    if (from >= first && from < first + LIST_ROWS
        && sel_event(view, from) != AL_NONE)
        scr_hl((unsigned char) (LIST_TOP + from - first), BAR_COL, BAR_W, 0);
    ev = sel_event(view, to);
    if (to >= first && to < first + LIST_ROWS && ev != AL_NONE)
        scr_hl((unsigned char) (LIST_TOP + to - first), BAR_COL, BAR_W, 1);
    draw_panel(ev);
}

/* ------------------------------------------------------------------ */
/* The alarm band                                                      */
/* ------------------------------------------------------------------ */

/*
 * Rows 24-25 go over to BP_ALERT and the highlight font -- white on red --
 * and flash by trading the palette's two colours (scr_alert_flash): after
 * the first paint, a flash is two bytes of palette in the blank and nothing
 * else. A bell beside it rocks with the flash.
 */
void v_alarm(unsigned char phase)
{
    struct event *e;
    char t[8];

    if (al_ev >= gc_count)
        return;

    if (banner_ev != al_ev) {
        banner_ev = al_ev;
        e = &gc_index[al_ev];

        strcpy(sbuf, "  ");
        ui_hhmm(t, e->sh, e->sm);
        strcat(sbuf, t);
        strcat(sbuf, "  ");
        strcat(sbuf, e->title);
        scr_pal(STAT_ROW, 0, SCR_W, 2, BP_ALERT);
        scr_field(STAT_ROW, LEFT_COL, sbuf, TEXT_W, HL);
        scr_fill(STAT_ROW, 0, ' ' | HL, 1);
        scr_fill(STAT_ROW, SCR_W - 1, ' ' | HL, 1);
        scr_field(LEGEND_ROW, 0, "", SCR_W, HL);
        scr_center(LEGEND_ROW, "Any button to dismiss", HL);
        in_mode = M_OK;
    }

    scr_alert_flash(phase);
    spr_put(SPR_MISC, (unsigned char) (phase ? 9 : 11), STAT_ROW * 8,
            S_BELL + 1, SP_BRAND);
}
