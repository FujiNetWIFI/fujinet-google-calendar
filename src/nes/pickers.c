/*
 * The form's pickers: a date off a little month, a time off a wheel, a
 * colour off Google's swatches. Overlay bank 5 (OVL_PICK); reached only
 * through tramp.c's pick_run(), from form_ui.c's plat_getch().
 *
 * Each draws itself in the form's panel (rows 15-24), runs until the user
 * decides, and says what was decided (PK_*, ovl.h). A new value goes back in
 * value[], and form_ui.c types it into the field: the picker never touches
 * the form itself, so the form's own checks still judge it.
 *
 *   pad    move                A  set           B  type it / clear it
 *   SELECT the next field      START  leave the form
 *
 * A key on a Famicom keyboard ends the picker at once (PK_KEY): whoever has
 * a keyboard is typing.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

#define PANEL_TOP   15
#define PANEL_BOT   24

static uint8_t dirs;            /* this frame's direction presses */
static uint8_t cleared;         /* the PK_SET was B's: no value */

static void panel_clear(void)
{
    uint8_t r;

    for (r = PANEL_TOP; r <= PANEL_BOT; r++)
        scr_field(r, 0, "", SCR_W, 0);
    spr_hide(SPR_CONTENT, SPR_CONTENT + 46);
    spr_hide(SPR_CURSOR, SPR_CURSOR + 3);
}

/* One frame's input: a direction (in dirs), or a decision. */
static uint8_t frame(uint8_t clear)
{
    plat_vsync();

    if (kb_ch || kb_fn)
        return PK_KEY;

    dirs = (uint8_t) (pad_new & (J_UP | J_DOWN | J_LEFT | J_RIGHT));
    cleared = 0;
    if (pad_new & J_A)      { pad_new = 0; return PK_SET; }
    if (pad_new & J_B) {
        pad_new = 0;
        cleared = clear;
        return clear ? PK_SET : PK_TYPE;
    }
    if (pad_new & J_SELECT) { pad_new = 0; return PK_NEXT; }
    if (pad_new & J_START)  { pad_new = 0; return PK_DONE; }
    pad_new = 0;
    if (dirs && pref_clicks)
        snd_play(SFX_TICK);
    return 0;
}

static unsigned int num_at(const char *s, uint8_t n)
{
    unsigned int v = 0;

    while (n--) {
        if (*s < '0' || *s > '9')
            return 0xFFFF;
        v = v * 10 + (unsigned int) (*s++ - '0');
    }
    return v;
}

static void two(char *d, uint8_t v)
{
    d[0] = (char) ('0' + v / 10);
    d[1] = (char) ('0' + v % 10);
}

/* ------------------------------------------------------------------ */
/* The date                                                            */
/* ------------------------------------------------------------------ */

static const char *const month_name[12] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
};
static const char *const day_name[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

#define CAL_HEAD    15
#define CAL_DOW     16
#define CAL_TOP     17          /* 17-22: six weeks */
#define CAL_LEFT    6           /* a day every three columns */
#define CAL_LINE    24

static unsigned int py;
static uint8_t pmo, pd, pfirst;

static void cell(uint8_t d, uint8_t *row, uint8_t *col)
{
    uint8_t c = (uint8_t) (pfirst + d - 1);

    *row = (uint8_t) (CAL_TOP + c / 7);
    *col = (uint8_t) (CAL_LEFT + (c % 7) * 3);
}

static void day_cell(uint8_t d, uint8_t sel)
{
    uint8_t row, col;

    cell(d, &row, &col);
    scr_tile(row, (uint8_t) (col - 1), sel ? T_PILL_L : T_BLANK);
    scr_tile(row, (uint8_t) (col + 2), sel ? T_PILL_R : T_BLANK);
    if (clk_is_today(py, pmo, d) && !sel) {
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
    scr_text(row, col, sbuf, sel ? HL : 0);
}

static void date_line_full(void)
{
    char n[6];

    strcpy(sbuf, day_name[date_dow(py, pmo, pd)]);
    strcat(sbuf, " ");
    num(n, pd);
    strcat(sbuf, n);
    strcat(sbuf, " ");
    strcat(sbuf, month_name[pmo - 1]);
    strcat(sbuf, " ");
    num(n, py);
    strcat(sbuf, n);
    scr_field(CAL_LINE, 0, "", SCR_W, 0);
    scr_center(CAL_LINE, sbuf, 0);
}

static void month_draw(void)
{
    uint8_t r, d, n;
    char yy[6];

    for (r = CAL_HEAD; r < CAL_LINE; r++)
        scr_field(r, 0, "", SCR_W, 0);

    strcpy(sbuf, month_name[pmo - 1]);
    strcat(sbuf, " ");
    num(yy, py);
    strcat(sbuf, yy);
    n = (uint8_t) strlen(sbuf);
    scr_center(CAL_HEAD, sbuf, 0);
    scr_tile(CAL_HEAD, (uint8_t) ((SCR_W - n) / 2 - 2), T_PREV);
    scr_tile(CAL_HEAD, (uint8_t) ((SCR_W - n) / 2 + n + 1), T_NEXT);

    for (d = 0; d < 7; d++)
        scr_tile(CAL_DOW, (uint8_t) (CAL_LEFT + 1 + d * 3), (uint8_t) "SMTWTFS"[d]);

    pfirst = date_dow(py, pmo, 1);
    n = date_dim(py, pmo);
    for (d = 1; d <= n; d++)
        day_cell(d, (uint8_t) (d == pd));
    date_line_full();
}

static uint8_t pick_date(char *value)
{
    unsigned int y = num_at(value, 4);
    unsigned int m = num_at(value + 5, 2), d = num_at(value + 8, 2);
    uint8_t k, i, was_mo, was_d;
    unsigned int was_y;

    if (strlen(value) == 10 && y >= 1900 && m >= 1 && m <= 12 && d >= 1
        && d <= date_dim(y, (uint8_t) m)) {
        py = y;
        pmo = (uint8_t) m;
        pd = (uint8_t) d;
    } else {
        py = clk_y;
        pmo = clk_mo;
        pd = clk_d;
    }

    scr_hold();
    panel_clear();
    month_draw();
    ui_legend("\x10" "Set \x11" "Type {Sel}Next {Start}OK");
    scr_flush(PANEL_TOP, LEGEND_ROW);

    for (;;) {
        k = frame(0);
        if (k == PK_SET) {
            date_iso(value, py, pmo, pd);
            return PK_SET;
        }
        if (k)
            return k;
        if (!dirs)
            continue;

        was_y = py;
        was_mo = pmo;
        was_d = pd;
        for (i = 0; i < ((dirs & (J_UP | J_DOWN)) ? 7 : 1); i++) {
            if (dirs & (J_RIGHT | J_DOWN))
                date_addday(&py, &pmo, &pd);
            else
                date_subday(&py, &pmo, &pd);
        }
        if (pmo != was_mo || py != was_y) {
            scr_hold();
            month_draw();
            scr_flush(PANEL_TOP, PANEL_BOT);
        } else {
            day_cell(was_d, 0);
            day_cell(pd, 1);
            date_line_full();
        }
    }
}

/* ------------------------------------------------------------------ */
/* The time                                                            */
/* ------------------------------------------------------------------ */

#define TIME_ROW    18
#define LAST_MIN    (23 * 60 + 59)      /* the latest time a wheel reaches */

static unsigned int parse_time(const char *v)
{
    const char *c = strchr(v, ':');
    unsigned int h, m;

    if (!c || c - v < 1 || c - v > 2 || strlen(c + 1) != 2)
        return 0xFFFF;
    h = num_at(v, (uint8_t) (c - v));
    m = num_at(c + 1, 2);
    if (h > 23 || m > 59)
        return 0xFFFF;
    return h * 60 + m;
}

static void time_draw(uint8_t field, unsigned int t, unsigned int start)
{
    char s[8];
    uint8_t n;

    ui_hhmm(s, (uint8_t) (t / 60), (uint8_t) (t % 60));
    strcpy(sbuf, " ");
    strcat(sbuf, s);
    strcat(sbuf, " ");
    n = (uint8_t) strlen(sbuf);

    scr_field(TIME_ROW, 0, "", SCR_W, 0);
    scr_tile(TIME_ROW, (uint8_t) ((SCR_W - n) / 2 - 2), T_PREV);
    scr_tile(TIME_ROW, (uint8_t) ((SCR_W - n) / 2 - 1), T_PILL_L);
    scr_text(TIME_ROW, (uint8_t) ((SCR_W - n) / 2), sbuf, HL);
    scr_tile(TIME_ROW, (uint8_t) ((SCR_W - n) / 2 + n), T_PILL_R);
    scr_tile(TIME_ROW, (uint8_t) ((SCR_W - n) / 2 + n + 1), T_NEXT);

    scr_field(21, 0, "", SCR_W, 0);
    if (field == FRM_END && start != 0xFFFF && t > start) {
        unsigned int d = t - start;
        char n2[6];

        sbuf[0] = '\0';
        if (d >= 60) {
            num(n2, d / 60);
            strcat(sbuf, n2);
            strcat(sbuf, d >= 120 ? " hours" : " hour");
        }
        if (d % 60) {
            if (sbuf[0])
                strcat(sbuf, " ");
            num(n2, d % 60);
            strcat(sbuf, n2);
            strcat(sbuf, " min");
        }
        strcat(sbuf, " long");
        scr_center(21, sbuf, 0);
    }
}

static uint8_t pick_time(uint8_t field, char *value)
{
    unsigned int t = parse_time(value);
    unsigned int start = parse_time(pick_ref);
    uint8_t k;

    if (t == 0xFFFF) {
        if (field == FRM_END && start != 0xFFFF)
            t = start + 60;                     /* an hour, Google's default */
        else
            t = (unsigned int) (clk_h + 1) * 60; /* the next hour */
        if (t > LAST_MIN)
            t = LAST_MIN;               /* never back onto the start */
    }

    scr_hold();
    panel_clear();
    scr_center(16, field == FRM_START ? "Starts at" : "Ends at", 0);
    scr_center(20, "\x06\x07 15 minutes   \x04\x05 an hour", 0);
    scr_center(23, field == FRM_START ? "\x11 no time: all day" : "\x11 no end time", 0);
    time_draw(field, t, start);
    ui_legend("\x10" "Set \x11" "None {Sel}Next {Start}OK");
    scr_flush(PANEL_TOP, LEGEND_ROW);

    for (;;) {
        k = frame(1);
        if (k == PK_SET) {
            if (cleared) {
                value[0] = '\0';
            } else {
                two(value, (uint8_t) (t / 60));
                value[2] = ':';
                two(value + 3, (uint8_t) (t % 60));
                value[5] = '\0';
            }
            return PK_SET;
        }
        if (k)
            return k;
        if (dirs & J_UP)
            t = (t / 15 + 1) * 15;
        else if (dirs & J_DOWN)
            t = t ? ((t + 14) / 15 - 1) * 15 : 0;
        else if (dirs & J_RIGHT)
            t += 60;
        else if (dirs & J_LEFT)
            t = (t >= 60) ? t - 60 : t;
        else
            continue;
        if (t > LAST_MIN)
            t = LAST_MIN;
        time_draw(field, t, start);
    }
}

/* ------------------------------------------------------------------ */
/* The colour                                                          */
/* ------------------------------------------------------------------ */

/* Google's own order: the reds, the yellows and greens, the blues, grey.
   0 is "none" -- the calendar's own colour. */
static const uint8_t order[12] = {
    COL_NONE, COL_TOMATO, COL_FLAMINGO, COL_TANGERINE, COL_BANANA, COL_SAGE,
    COL_BASIL, COL_PEACOCK, COL_BLUEBERRY, COL_LAVENDER, COL_GRAPE, COL_GRAPHITE
};

#define SW_ROW0     17
#define SW_ROW1     20
#define SW_COL      4
#define SW_PITCH    4
#define SW_NAME     23

static void swatch_pos(uint8_t i, uint8_t *row, uint8_t *col)
{
    *row = (uint8_t) (i < 6 ? SW_ROW0 : SW_ROW1);
    *col = (uint8_t) (SW_COL + (i % 6) * SW_PITCH);
}

static void colour_name(char *dst, uint8_t c)
{
    uint8_t i;

    strcpy(dst, color_name(c));
    for (i = 1; dst[i]; i++)
        if (dst[i] >= 'A' && dst[i] <= 'Z')
            dst[i] = (char) (dst[i] + 32);
}

static void colour_sel(uint8_t i)
{
    uint8_t row, col;

    swatch_pos(i, &row, &col);
    spr_cursor(row, col, 1, 1);
    if (order[i] == COL_NONE)
        strcpy(sbuf, "The calendar's colour");
    else
        colour_name(sbuf, order[i]);
    scr_field(SW_NAME, 0, "", SCR_W, 0);
    scr_center(SW_NAME, sbuf, 0);
}

static uint8_t pick_colour(char *value)
{
    uint8_t i, row, col, sel = 0, k;
    uint8_t c = color_match(value, (uint8_t) strlen(value));

    for (i = 0; i < 12; i++)
        if (order[i] == c && *value)
            sel = i;

    scr_hold();
    panel_clear();
    scr_center(PANEL_TOP + 1, "The event's colour", 0);
    for (i = 0; i < 12; i++) {
        swatch_pos(i, &row, &col);
        if (order[i] == COL_NONE)
            scr_tile(row, col, T_CAL);
        else
            spr_color((uint8_t) (SPR_CONTENT + i), (uint8_t) (col * 8),
                      (uint8_t) (row * 8), S_SWATCH, order[i]);
    }
    colour_sel(sel);
    ui_legend("\x10" "Set \x11" "None {Sel}Next {Start}OK");
    scr_flush(PANEL_TOP, LEGEND_ROW);

    for (;;) {
        k = frame(1);
        if (k == PK_SET) {
            if (cleared || order[sel] == COL_NONE)
                value[0] = '\0';
            else
                colour_name(value, order[sel]);
            return PK_SET;
        }
        if (k)
            return k;
        if (dirs & J_RIGHT)
            sel = (uint8_t) ((sel + 1) % 12);
        else if (dirs & J_LEFT)
            sel = (uint8_t) ((sel + 11) % 12);
        else if (dirs & (J_UP | J_DOWN))
            sel = (uint8_t) ((sel + 6) % 12);
        else
            continue;
        colour_sel(sel);
    }
}

/* ------------------------------------------------------------------ */

uint8_t k_pick(uint8_t field, char *value)
{
    switch (field) {
    case FRM_DATE:  return pick_date(value);
    case FRM_CAT:   return pick_colour(value);
    default:        return pick_time(field, value);
    }
}
