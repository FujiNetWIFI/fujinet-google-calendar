/*
 * Screen chrome, the flat screens, and the two settings screens.
 *
 * Two kinds of screen. The banded ones -- the four views, the event detail,
 * the picker and the settings page -- run the DLI chain and are laid out to
 * its three regions: header rows 0-2, content rows 3-22, footer row 23. The
 * transient ones -- splash, busy, error -- are flat, which is deliberate
 * rather than lazy: they are exactly the screens that are up while SIO is
 * running, and SIO does not want interrupts stealing its cycles.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* Header: the mark at cols 1-4 rows 0-2, with its "31" in the clear interior
   cell at row 1 col 2, and everything else to the right of it. */
#define LOGO_ROW        0
#define LOGO_COL        1
#define HDR_TEXT_COL    6
#define TAB_COL         23
#define RIGHT_COL       38

/* Splash: LOGO_LARGE is eight cells wide, six rows tall. */
#define SPLASH_LOGO_ROW 6
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_LARGE_COLS) / 2)

static char sbuf[42];

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void num(char *dst, unsigned int v)
{
    utoa(v, dst, 10);
}

/* Two digits, zero padded, straight into dst[0..1]. */
static void num2(char *dst, unsigned char v)
{
    dst[0] = (char) ('0' + (v / 10) % 10);
    dst[1] = (char) ('0' + v % 10);
}

void ui_hhmm(char *dst, unsigned char h, unsigned char m)
{
    num2(dst, h);
    dst[2] = ':';
    num2(dst + 3, m);
    dst[5] = '\0';
}

/* ------------------------------------------------------------------ */
/* Banded chrome                                                       */
/* ------------------------------------------------------------------ */

/*
 * The view tab strip. Showing which digit selects which view in the header is
 * what keeps the footer down to a single row at 40 columns -- the
 * Intellivision needed two hint pages at 20.
 */
static void tabs(unsigned char view)
{
    static const char *const label[4] = { "1DAY", "2WK", "3MO", "4AG" };
    static const unsigned char col[4] = { 23, 28, 32, 36 };
    unsigned char i;

    scr_field(0, TAB_COL, "", (unsigned char) (SCR_COLS - TAB_COL), 0);
    for (i = 0; i < 4; i++)
        scr_text(0, col[i], label[i], (unsigned char) (i == view));
}

void ui_footer(const char *hints, const char *right)
{
    scr_row_clear(FOOT_ROW);
    if (hints)
        scr_text(FOOT_ROW, 1, hints, 0);
    if (right && *right)
        scr_right(FOOT_ROW, RIGHT_COL, right, 0);
}

/* The wall clock, five cells right-aligned on row 2. Repainting it once a
   minute is also the cheapest way to notice a wrong [General] timezone. */
void ui_clock(void)
{
    char t[6];

    if (!clk_ok) {
        scr_field(2, (unsigned char) (RIGHT_COL - 4), "", 5, 0);
        return;
    }

    ui_hhmm(t, clk_h, clk_mi);
    scr_field(2, (unsigned char) (RIGHT_COL - 4), t, 5, 0);
}

/*
 * Row 2 says how the fetch went. An error and a partial listing are not
 * mutually exclusive: whatever was parsed before the stream broke stays on
 * screen underneath the message.
 */
static const char *status_text(void)
{
    char n[6];

    if (gc_ecode) {
        switch (gc_ecode) {
        case 0:             return "No FujiNet reply";
        case GC_BADSPEC:
        case GC_NOTFOUND:   return "Bad calendar selector";
        case GC_DENIED:     return "Authorize in the web UI";
        case GC_NOAUTH:     return "Not authorized";
        case GC_NOSERVICE:  return "Service unavailable";
        default:            return "Calendar error";
        }
    }

    if (gc_count == 0)
        return "No events";

    if (gc_count == 1)
        return "1 event";

    num(n, gc_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_trunc ? " events, more" : " events");
    return sbuf;
}

void ui_status(void)
{
    scr_field(2, HDR_TEXT_COL, status_text(),
              (unsigned char) (RIGHT_COL - 5 - HDR_TEXT_COL), 0);
    ui_clock();
}

/*
 * Paint rows 0-2 and put the small mark back. Callers that only move a
 * selection do not come through here.
 */
void ui_header(unsigned char view)
{
    dli_bands();

    scr_rows_clear(0, 2);
    pmg_show(LOGO_SMALL, LOGO_ROW, LOGO_COL);
    scr_text(LOGO_ROW + 1, LOGO_COL + 1, "31", 0);

    scr_text(0, HDR_TEXT_COL, "Google Calendar", 0);
    if (view < 4)
        tabs(view);

    scr_field(1, HDR_TEXT_COL, gc_wtitle,
              (unsigned char) (SCR_COLS - HDR_TEXT_COL - 2), 0);
    ui_status();
}

/* ------------------------------------------------------------------ */
/* Flat screens                                                        */
/* ------------------------------------------------------------------ */

static void flat_screen(void)
{
    dli_flat(C_FLAT_BG, C_FLAT_FG);
    scr_clear();
    pmg_chips_clear();
    pmg_show(LOGO_LARGE, SPLASH_LOGO_ROW, SPLASH_LOGO_COL);
    /* The mark's interior is six cells wide and four rows tall at this size,
       so the digits sit in its middle row. */
    scr_text(SPLASH_LOGO_ROW + 2, SPLASH_LOGO_COL + 3, "31", 0);
}

void ui_splash(void)
{
    flat_screen();
    scr_center(13, "FujiNet Google Calendar", 0);
    scr_center(15, "Looking for FujiNet", 0);
}

void ui_notfound(void)
{
    flat_screen();
    scr_center(13, "FujiNet not found", 0);
    scr_center(15, "Check the adapter", 0);
    scr_center(18, "PRESS ANY KEY", 0);
}

/*
 * No clock is fatal, not cosmetic: every device spec this client builds names
 * a date, and there is nothing sensible to put there without one.
 */
void ui_noclock(void)
{
    flat_screen();
    scr_center(13, "FujiNet clock failed", 0);
    scr_center(15, "Enable APETIME and set a", 0);
    scr_center(16, "POSIX timezone, then retry", 0);
    scr_center(18, "PRESS ANY KEY", 0);
}

void ui_busy(unsigned char reason)
{
    flat_screen();

    switch (reason) {
    case BUSY_CLOCK:
        scr_center(13, "Reading the clock...", 0);
        break;
    case BUSY_DETAIL:
        scr_center(13, "Fetching event...", 0);
        break;
    case BUSY_CALS:
        scr_center(13, "Reading calendars...", 0);
        break;
    default:
        scr_center(13, "Fetching calendar...", 0);
        scr_center(15, "up to 60 seconds", 0);
        break;
    }
}

void ui_error(unsigned char code)
{
    char n[6];

    gc_ecode = code;
    flat_screen();

    scr_center(13, "Calendar error", 0);
    scr_center(15, status_text(), 0);

    /* The raw codes underneath, because "Calendar error" on its own is not
       something anyone can act on. */
    strcpy(sbuf, gc_stage ? gc_stage : "?");
    strcat(sbuf, " code ");
    num(n, code);
    strcat(sbuf, n);
    strcat(sbuf, " dev ");
    num(n, gc_dev_ecode);
    strcat(sbuf, n);
    scr_center(17, sbuf, 0);

    scr_center(20, "PRESS ANY KEY", 0);
}

/* ------------------------------------------------------------------ */
/* Calendar picker                                                     */
/* ------------------------------------------------------------------ */

#define PICK_TOP    4
#define PICK_ROWS   12

void ui_pick(unsigned char sel, unsigned char first)
{
    unsigned char i, row;
    char n[6];

    dli_bands();
    scr_clear();
    pmg_chips_clear();

    pmg_show(LOGO_SMALL, LOGO_ROW, LOGO_COL);
    scr_text(LOGO_ROW + 1, LOGO_COL + 1, "31", 0);
    scr_text(0, HDR_TEXT_COL, "Choose calendar", 0);

    num(n, gc_cal_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_cal_count == 1 ? " calendar" : " calendars");
    scr_text(2, HDR_TEXT_COL, sbuf, 0);

    for (i = 0; i < PICK_ROWS; i++) {
        row = (unsigned char) (PICK_TOP + i);
        if (first + i >= gc_cal_count) {
            scr_row_clear(row);
            continue;
        }
        scr_field(row, 0, (first + i == sel) ? ">" : " ", 1,
                  (unsigned char) (first + i == sel));
        scr_field(row, 1, "", 2, (unsigned char) (first + i == sel));
        scr_field(row, 3, gc_cals[first + i].name, SCR_COLS - 3,
                  (unsigned char) (first + i == sel));
    }

    /* The full selector spelled out, because the picker column is 24 wide and
       a Google calendar name is routinely longer. */
    scr_rows_clear(19, 21);
    if (sel < gc_cal_count) {
        const char *s = gc_cals[sel].sel;
        scr_text(20, 0, s[0] ? s : "(every calendar Google is showing)", 0);
    }

    ui_footer("RET:SELECT  ^v:SEL  ESC:BACK", 0);
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

static char tzbuf[48];

void ui_setup(void)
{
    char n[6];

    dli_bands();
    scr_clear();
    pmg_chips_clear();

    pmg_show(LOGO_SMALL, LOGO_ROW, LOGO_COL);
    scr_text(LOGO_ROW + 1, LOGO_COL + 1, "31", 0);
    scr_text(0, HDR_TEXT_COL, "Settings", 0);

    /*
     * The timezone is the single most useful line on this screen. The GCAL
     * adapter's PosixTz parser rejects an IANA name like America/Chicago and
     * falls back to UTC without saying so, and both this clock and the window
     * events are resolved in come from that one setting -- so seeing UTC here
     * when the web UI says otherwise is the only symptom there is.
     */
    scr_text(3, 2, "Timezone", 0);
    if (!tzbuf[0])
        clk_get_tz(tzbuf, sizeof(tzbuf));
    scr_text(4, 4, tzbuf[0] ? tzbuf : "(unset -- events resolve in UTC)", 0);

    scr_text(6, 2, "Calendar", 0);
    scr_text(7, 4, gc_cal[0] ? gc_cal : "All shown calendars", 0);

    scr_text(9, 2, "Alarm lead", 0);
    num(n, al_lead);
    strcpy(sbuf, n);
    strcat(sbuf, al_lead == 1 ? " minute before" : " minutes before");
    scr_field(10, 4, sbuf, 30, 0);

    scr_text(12, 2, "Keys", 0);
    scr_text(13, 4, "1 2 3 4   day week month agenda", 0);
    scr_text(14, 4, "0         jump to today", 0);
    scr_text(15, 4, "< >       previous / next period", 0);
    scr_text(16, 4, "RETURN    open the selection", 0);
    scr_text(17, 4, "ESC       settings / back", 0);
    scr_text(18, 4, "R  Q      refresh / quit", 0);

    scr_text(20, 2, "FujiNet Google Calendar", 0);

    ui_footer("1:CALENDAR  <>:LEAD  ESC:SAVE", 0);
}

/* Redraw just the lead line, so holding left or right does not repaint the
   whole screen and the timezone is not re-read from the device each step. */
void ui_setup_lead(void)
{
    char n[6];

    num(n, al_lead);
    strcpy(sbuf, n);
    strcat(sbuf, al_lead == 1 ? " minute before" : " minutes before");
    scr_field(10, 4, sbuf, 30, 0);
}
