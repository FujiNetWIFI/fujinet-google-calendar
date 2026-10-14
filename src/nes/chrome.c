/*
 * What every screen shares: the header, the view tabs, the button legend,
 * the status row, the clock, and the small formatters. In the fixed bank,
 * because every overlay paints with it.
 *
 *   rows 2-3   the mark at columns 2-3; the window title and the clock on
 *              row 2, the calendar (or a screen's subtitle) on row 3
 *   row 4      the tabs -- Day Week Month Agenda -- in Google's blue, the
 *              current one on a pill
 *   row 5      a rule
 *   ...        the screen's own
 *   row 24     the status line
 *   row 25     the legend: what the buttons do on this screen, with the
 *              pad's own button shapes in it
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "nes.h"

char sbuf[64];

#define TITLE_COL   5
#define CLOCK_RCOL  RIGHT_COL
#define TITLE_W     19          /* 5-23: the clock owns 24-30 */

/* ------------------------------------------------------------------ */
/* Formatters                                                          */
/* ------------------------------------------------------------------ */

void num(char *dst, unsigned int v)
{
    utoa(v, dst, 10);
}

static void num2(char *dst, unsigned char v)
{
    dst[0] = (char) ('0' + (v / 10) % 10);
    dst[1] = (char) ('0' + v % 10);
}

/*
 * "09:00", or Google's own 12-hour spelling, "9:00am", when the 24-hour
 * clock is off in settings. Seven characters at most either way.
 */
void ui_hhmm(char *dst, unsigned char h, unsigned char m)
{
    if (pref_24h) {
        num2(dst, h);
        dst[2] = ':';
        num2(dst + 3, m);
        dst[5] = '\0';
        return;
    }

    {
        unsigned char h12 = (unsigned char) (h % 12 ? h % 12 : 12);
        char *p = dst;

        if (h12 >= 10)
            *p++ = '1';
        *p++ = (char) ('0' + h12 % 10);
        *p++ = ':';
        num2(p, m);
        p[2] = (h < 12) ? 'a' : 'p';
        p[3] = 'm';
        p[4] = '\0';
    }
}

/* "09:00-10:00", "09:00->" (ends another day) or "All day". */
void ui_time_range(char *dst, const struct event *e)
{
    char t[8];

    if (e->flags & EVF_ALLDAY) {
        strcpy(dst, "All day");
        return;
    }
    ui_hhmm(dst, e->sh, e->sm);
    if (e->flags & EVF_OPENEND) {
        strcat(dst, "->");
        return;
    }
    strcat(dst, "-");
    ui_hhmm(t, e->eh, e->em);
    strcat(dst, t);
}

/* "Fri 4 Oct 2026" */
void date_line(char *dst, unsigned int y, unsigned char mo, unsigned char d)
{
    char n[6];

    strcpy(dst, date_dow3(date_dow(y, mo, d)));
    strcat(dst, " ");
    num(n, d);
    strcat(dst, n);
    strcat(dst, " ");
    strcat(dst, date_mon3(mo));
    strcat(dst, " ");
    num(n, y);
    strcat(dst, n);
}

const char *ui_err_text(unsigned char code)
{
    switch (code) {
    case 0:             return "No FujiNet reply";
    case GC_BADSPEC:
    case GC_NOTFOUND:   return "Calendar not found";
    case GC_DENIED:     return "Authorize Google again";
    case GC_NOAUTH:     return "Google not authorized";
    case GC_NOSERVICE:  return "Google unreachable";
    case GC_BADDRAFT:   return "Rejected: check fields";
    case GC_FULL:       return "Event too large";
    case GC_RDONLY:     return "Calendar is read-only";
    default:            return "Calendar error";
    }
}

/* ------------------------------------------------------------------ */
/* Header, tabs, legend                                                */
/* ------------------------------------------------------------------ */

void clock_paint(void)
{
    char t[8];

    if (!clk_ok) {
        scr_field(HDR_ROW, 24, "", 7, 0);
        return;
    }
    ui_hhmm(t, clk_h, clk_mi);
    scr_field(HDR_ROW, 24, "", (unsigned char) (7 - strlen(t)), 0);
    scr_right(HDR_ROW, CLOCK_RCOL, t, 0);
}

void ui_header(const char *title, const char *sub)
{
    spr_logo_small(HDR_ROW, 2);
    scr_field(HDR_ROW, TITLE_COL, title, TITLE_W, 0);
    scr_field((unsigned char) (HDR_ROW + 1), TITLE_COL, sub,
              (unsigned char) (RIGHT_COL - TITLE_COL + 1), 0);
    clock_paint();
}

static const unsigned char tab_col[4] = { 2, 8, 15, 23 };
static const char *const tab_name[4] = { "Day", "Week", "Month", "Agenda" };

/* The current tab is the label on a pill: its ends are tiles, the label is
   the highlight font, and BP_CHROME makes the pill the line grey and every
   label Google's blue. */
void ui_tabs(unsigned char view)
{
    unsigned char i, c;

    scr_pal(TAB_ROW, 0, SCR_W, 2, BP_CHROME);
    for (i = 0; i < 4; i++) {
        c = tab_col[i];
        if (i == view) {
            scr_tile(TAB_ROW, (unsigned char) (c - 1), T_PILL_L);
            scr_text(TAB_ROW, c, tab_name[i], HL);
            scr_tile(TAB_ROW, (unsigned char) (c + strlen(tab_name[i])), T_PILL_R);
        } else {
            scr_text(TAB_ROW, c, tab_name[i], 0);
        }
    }
    scr_rule(RULE_ROW);
}

/*
 * The legend: a string in which a few codes stand for the pad's buttons.
 *
 *   \x10 \x11      the A and B buttons (round)
 *   {...}          a pill: SELECT, START and the keyboard's keys
 *
 * Centred on LEGEND_ROW. Everything else is text.
 */
void ui_legend(const char *s)
{
    unsigned char row[SCR_W];
    unsigned char n = 0, pill = 0, c, col;

    while (*s && n < TEXT_W) {
        c = (unsigned char) *s++;
        if (c == '{') {
            row[n++] = T_PILL_L;
            pill = HL;
        } else if (c == '}') {
            row[n++] = T_PILL_R;
            pill = 0;
        } else if (c < 0x20) {
            row[n++] = c;
        } else {
            row[n++] = (unsigned char) (c | pill);
        }
    }

    scr_fill(LEGEND_ROW, 0, T_BLANK, SCR_W);
    col = (unsigned char) ((SCR_W - n) / 2);
    memcpy(scr_buf + LEGEND_ROW * SCR_W + col, row, n);
    scr_send(LEGEND_ROW, col, n);
}

void ui_status_text(const char *s)
{
    scr_field(STAT_ROW, LEFT_COL, s, TEXT_W, 0);
}

/* The clock, a minute at a time (gcal.h). Every screen with a header shows
   it at the same place; one without simply has nothing at that spot. */
void ui_clock(void)
{
    clock_paint();
}
