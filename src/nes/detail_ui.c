/*
 * The event, the calendar picker and settings. Overlay bank 3 (OVL_DETAIL);
 * reached only through tramp.c.
 *
 * The event screen, after Google's own event card:
 *
 *   rows 2-3   the mark; the event's date, and its times
 *   rows 5-6   the title, two rows of it, beside a square of its colour
 *   row  7     the times again with a clock, and a repeat mark
 *   row  8     the colour's name beside a dot of it
 *   row  9     a rule
 *   rows 10-23 the description and the rest the adapter sends (DET_WIN),
 *              with a scroll bar
 *
 * Scrolling repaints the body alone when the same event is still on screen;
 * main.c asks for the whole screen on every step.
 */

#include <string.h>

#include <fujinet-nes.h>

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

#define DET_TOP     10
#define BODY_COL    2
#define SCROLL_COL  RIGHT_COL

static uint8_t det_gen, det_ev;
static uint8_t pick_gen, pick_sel, pick_first;
static uint8_t focus;                   /* the settings row under the bar */
static uint8_t prefs_changed;
static char wrapbuf[2][SCR_W + 1];

/* ------------------------------------------------------------------ */
/* The event                                                           */
/* ------------------------------------------------------------------ */

static void ev_date(const struct event *e, unsigned int *y, uint8_t *mo, uint8_t *d)
{
    *y = cur_y;
    *mo = cur_mo;
    *d = cur_d;
    if (cur_view == VIEW_AGENDA) {
        *mo = e->mon;
        *d = e->day;
        if (e->mon < cur_mo)
            (*y)++;
    }
}

static void det_body(unsigned int top)
{
    uint8_t i, t0, t1;
    unsigned int rows = gc_det_rows ? gc_det_rows : 1;
    char n[6];

    for (i = 0; i < DET_WIN; i++) {
        scr_field((uint8_t) (DET_TOP + i), BODY_COL,
                  (top + i < gc_det_rows) ? gc_det[top + i] : "",
                  DET_COLS, 0);
    }

    if (gc_det_rows > DET_WIN) {
        t0 = (uint8_t) (top * DET_WIN / rows);
        t1 = (uint8_t) (((top + DET_WIN) * DET_WIN + rows - 1) / rows);
        for (i = 0; i < DET_WIN; i++)
            scr_tile((uint8_t) (DET_TOP + i), SCROLL_COL,
                     (i >= t0 && i < t1) ? T_THUMB : T_TRACK);
    }

    if (gc_det_rows == 0) {
        ui_status_text("No description");
    } else {
        strcpy(sbuf, "Lines ");
        num(n, top + 1);
        strcat(sbuf, n);
        strcat(sbuf, "-");
        num(n, (top + DET_WIN < gc_det_rows) ? top + DET_WIN : gc_det_rows);
        strcat(sbuf, n);
        strcat(sbuf, " of ");
        num(n, gc_det_rows);
        strcat(sbuf, n);
        if (gc_det_trunc)
            strcat(sbuf, "+");
        ui_status_text(sbuf);
    }
}

void d_detail(unsigned char ev, unsigned int top)
{
    struct event *e = &gc_index[ev];
    unsigned int y;
    uint8_t mo, d, rows, i;
    const char *cn;
    char t[24];

    if (det_gen == scr_gen && det_ev == ev) {
        det_body(top);
        return;
    }

    ev_date(e, &y, &mo, &d);
    date_line(sbuf, y, mo, d);
    ui_time_range(t, e);

    scr_begin();
    ui_header(sbuf, t);

    rows = (uint8_t) wrap_text(e->title, (char *) wrapbuf, 2, 27, SCR_W + 1);
    for (i = 0; i < 2; i++)
        scr_field((uint8_t) (5 + i), 3, (i < rows) ? wrapbuf[i] : "", 27, 0);
    spr_color(SPR_CONTENT, 8, 5 * 8 + 4, S_SWATCH, e->color);

    scr_tile(7, 3, T_CLOCK);
    scr_text(7, 5, t, 0);
    if (e->flags & EVF_RECURRING) {
        scr_tile(7, (uint8_t) (6 + strlen(t)), T_REPEAT);
        scr_text(7, (uint8_t) (8 + strlen(t)), "Repeats", 0);
    }

    cn = color_name(e->color);
    spr_color(SPR_CONTENT + 1, 3 * 8, 8 * 8, S_DOT, e->color);
    if (*cn) {
        /* Google's names are one capital and the rest small. */
        strcpy(sbuf, cn);
        for (i = 1; sbuf[i]; i++)
            if (sbuf[i] >= 'A' && sbuf[i] <= 'Z')
                sbuf[i] = (char) (sbuf[i] + 32);
        scr_text(8, 5, sbuf, 0);
    } else {
        scr_text(8, 5, "Calendar colour", 0);
    }

    scr_rule(9);
    det_body(top);
    ui_legend("\x10" "Edit \x11" "Back \x06\x07" "Scroll \x04\x05" "Page");
    in_mode = M_DETAIL;
    scr_end();

    det_gen = scr_gen;
    det_ev = ev;
}

/* ------------------------------------------------------------------ */
/* The calendar picker                                                 */
/* ------------------------------------------------------------------ */

#define PICK_TOP    LIST_TOP

static void pick_row(uint8_t i, uint8_t first, uint8_t sel)
{
    uint8_t row = (uint8_t) (PICK_TOP + i);
    uint8_t n = (uint8_t) (first + i);
    const struct cal *c;

    if (n >= gc_cal_count) {
        scr_field(row, 2, "", 28, 0);
        return;
    }
    c = &gc_cals[n];
    scr_field(row, 2, "", 28, n == sel ? HL : 0);
    if (strcmp(c->sel, gc_cal) == 0)
        scr_tile(row, 2, T_TICK);
    scr_field(row, 4, c->sel[0] ? c->sel : "All calendars", 26, n == sel ? HL : 0);
}

static void pick_panel(uint8_t sel)
{
    const char *s;

    scr_rows_clear(PANEL_ROW, PANEL_ROW + 1);
    if (sel >= gc_cal_count)
        return;
    s = gc_cals[sel].sel;
    if (!s[0]) {
        scr_text(PANEL_ROW, 2, "Every calendar Google shows", 0);
        return;
    }
    scr_field(PANEL_ROW, 2, s, 28, 0);
    if (strlen(s) > 28)
        scr_field(PANEL_ROW + 1, 2, s + 28, 28, 0);
}

void d_pick(unsigned char sel, unsigned char first)
{
    uint8_t i;
    char n[6];

    if (pick_gen == scr_gen && pick_first == first) {
        pick_row((uint8_t) (pick_sel - first), first, sel);
        pick_row((uint8_t) (sel - first), first, sel);
        pick_panel(sel);
        pick_sel = sel;
        return;
    }

    num(n, gc_cal_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_cal_count == 1 ? " calendar" : " calendars");

    scr_begin();
    ui_header("Choose a calendar", sbuf);
    scr_rule(RULE_ROW);
    for (i = 0; i < PICK_ROWS; i++)
        pick_row(i, first, sel);
    scr_rule(PANEL_RULE);
    pick_panel(sel);
    ui_legend("\x10" "Choose \x11" "Back");
    in_mode = M_PICK;
    scr_end();

    pick_gen = scr_gen;
    pick_sel = sel;
    pick_first = first;
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

/*
 * The core's settings are the alarm lead and the calendar (settings.c, key
 * 0); this backend adds the theme, the clock and the clicks (prefs.c, key
 * 1). The bar picks a row, left and right change it, A opens the calendar
 * picker, B saves both keys and goes back. A change to the theme shows at
 * once -- it is a palette, so a whole new look costs 32 bytes in the blank.
 */

#define F_LEAD      0
#define F_CAL       1
#define F_THEME     2
#define F_CLOCK     3
#define F_CLICKS    4
#define F_COUNT     5

#define SET_TOP     6
#define VAL_COL     11
#define VAL_W       19

static const char *const labels[F_COUNT] = {
    "Alarm", "Calendar", "Theme", "Clock", "Clicks"
};

static void setting_value(uint8_t f, char *dst)
{
    char n[6];

    switch (f) {
    case F_LEAD:
        num(n, al_lead);
        strcpy(dst, n);
        strcat(dst, al_lead == 1 ? " min before" : " mins before");
        break;
    case F_CAL:
        strcpy(dst, gc_cal[0] ? gc_cal : "All calendars");
        break;
    case F_THEME:
        strcpy(dst, pref_dark ? "Dark" : "Light");
        break;
    case F_CLOCK:
        strcpy(dst, pref_24h ? "24-hour" : "12-hour");
        break;
    default:
        strcpy(dst, pref_clicks ? "On" : "Off");
        break;
    }
}

static void setting_row(uint8_t f)
{
    uint8_t row = (uint8_t) (SET_TOP + f * 2);
    uint8_t st = (f == focus) ? HL : 0;

    scr_field(row, 2, labels[f], 9, st);
    setting_value(f, sbuf);
    if (f == F_CAL) {
        scr_field(row, VAL_COL, sbuf, VAL_W, st);
    } else {
        scr_tile(row, VAL_COL, st ? T_HL_PREV : T_PREV);
        scr_field(row, (uint8_t) (VAL_COL + 1), sbuf, (uint8_t) (VAL_W - 2), st);
        scr_tile(row, (uint8_t) (VAL_COL + VAL_W - 1), st ? T_HL_NEXT : T_NEXT);
    }
}

void d_setup_lead(void)
{
    setting_row(F_LEAD);
}

static const char *kb_name(void)
{
    switch (kb_type) {
    case FUJI_NES_KBD_FAMILY_BASIC: return "Family BASIC";
    case FUJI_NES_KBD_SUBOR:        return "Subor";
    default:                        return "None: pad only";
    }
}

void d_setup(void)
{
    uint8_t f;
    char t[8];

    scr_begin();
    ui_header("Settings", "FujiNet Google Calendar");
    scr_rule(RULE_ROW);
    for (f = 0; f < F_COUNT; f++)
        setting_row(f);

    scr_rule(17);
    scr_text(18, 2, "Keyboard", 0);
    scr_field(18, VAL_COL, kb_name(), VAL_W, 0);

    scr_text(19, 2, "Zone", 0);
    if (!clk_get_tz(sbuf, 20))
        strcpy(sbuf, "(not set)");
    scr_field(19, VAL_COL, sbuf, VAL_W, 0);

    scr_text(20, 2, "Clock", 0);
    if (clk_ok) {
        date_iso(sbuf, clk_y, clk_mo, clk_d);
        strcat(sbuf, " ");
        ui_hhmm(t, clk_h, clk_mi);
        strcat(sbuf, t);
    } else {
        strcpy(sbuf, "(not read)");
    }
    scr_field(20, VAL_COL, sbuf, VAL_W, 0);

    scr_text(21, 2, "Version", 0);
    scr_field(21, VAL_COL, GIT_VERSION, VAL_W, 0);

    ui_legend("\x06\x07" "Pick \x04\x05" "Change \x10" "Cal \x11" "Save");
    in_mode = M_SETUP;
    prefs_changed = 0;
    scr_end();
}

static void move_focus(int8_t step)
{
    uint8_t old = focus;

    focus = (uint8_t) ((focus + F_COUNT + step) % F_COUNT);
    setting_row(old);
    setting_row(focus);
}

static void toggle(uint8_t f)
{
    switch (f) {
    case F_THEME:
        pref_dark ^= 1;
        theme_dark = pref_dark;
        scr_theme();
        break;
    case F_CLOCK:
        pref_24h ^= 1;
        clock_paint();
        break;
    case F_CLICKS:
        pref_clicks ^= 1;
        break;
    default:
        return;
    }
    prefs_changed = 1;
    setting_row(f);
}

/* Every key on the settings screen comes here first (input.c); what this
   returns is what main.c's do_setup() sees. */
uint8_t d_setup_key(uint8_t k)
{
    switch (k) {
    case K_UP:
        move_focus(-1);
        return K_NONE;
    case K_DOWN:
        move_focus(1);
        return K_NONE;
    case K_LEFT:
    case K_RIGHT:
        if (focus == F_LEAD)
            return k;
        if (focus != F_CAL)
            toggle(focus);
        return K_NONE;
    case K_ENTER:
        if (focus == F_CAL)
            return K_VIEW1;             /* do_pick() */
        toggle(focus);
        return K_NONE;
    case K_VIEW1:
        return K_VIEW1;
    case K_BACK:
        if (prefs_changed)
            prefs_save();
        return K_BACK;                  /* set_save(), and out */
    case K_QUIT:
        return K_QUIT;
    }
    return K_NONE;
}
