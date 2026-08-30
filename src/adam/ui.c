/*
 * Screen chrome, the flat screens, and the two settings screens.
 *
 * Twenty-one rows and six labelled keys is the most generous shape this
 * program has ever run in, and the two facts are related: the SmartKeys carry
 * what the Atari and the Apple spend a header row on and the CoCo spends its
 * footer on, so none of rows 0-20 goes on telling the user which key does
 * what. That is what pays for a three-row header that can name the calendar as
 * well as the window, and for a MONTH grid with real cells in it.
 *
 * The band at the bottom is not ours to draw. smartkeys_display() clears and
 * repaints all three of its rows, so every screen declares its legend when it
 * paints itself and sk_bind() suppresses the repaint when the legend has not
 * actually changed.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/*
 * Row 0: mark, window title.
 * Row 1: mark, status, wall clock.
 * Row 2: the selected calendar, full width.
 *
 * Row 2 is the row no other backend has. The calendar the client is showing is
 * a persistent piece of state that changes what every other row means, and
 * until now it was visible only on the settings screen -- which is exactly
 * where nobody looks when a day comes back emptier than expected.
 *
 * The window title needs the whole of row 0: "Week of Sun 23 Aug 2026" is
 * twenty-three characters off the wire and anything narrower clips the year,
 * which reads as a bug rather than as a truncation.
 */
#define TITLE_COL       HDR_TEXT_COL            /* 3 */
#define TITLE_W         (SCR_COLS - TITLE_COL)  /* cols 3-31, 29 wide */
#define STATUS_W        23                      /* cols 3-25 */
#define CLOCK_COL       26                      /* cols 26-31 */

/* Splash: the large mark is four cells wide, four rows tall. */
#define SPLASH_LOGO_ROW 3
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_LARGE_COLS) / 2)

static char sbuf[64];

/* ------------------------------------------------------------------ */
/* SmartKey legends                                                    */
/* ------------------------------------------------------------------ */

/*
 * Labels are kept short because the slots are not equal: the six are 48, 40,
 * 40, 40, 40 and 48 pixels wide, and smartkeyslib's font is proportional and
 * does not clip -- a label that overruns its slot is drawn over its neighbour's
 * rather than truncated. Six characters is safe in the narrow four, eight in
 * the outer two.
 *
 * A NULL leaves the slot yellow, which is smartkeyslib's way of saying "there
 * is no key here", and is the honest thing to show for a screen with fewer than
 * six actions.
 */
static const struct sk_set SK_LIST = {
    { "Day", "Week", "Month", "Agenda", "Today", "More" },
    { K_VIEW1, K_VIEW2, K_VIEW3, K_VIEW4, K_TODAY, K_SKBANK }
};

/*
 * The second bank. "Setup" is K_BACK because that is the key the view loop
 * opens the settings screen with, and "Back" is the toggle home rather than
 * anything the core sees.
 *
 * The calendar picker is deliberately not here: main.c only reaches it from
 * inside the settings screen, so a key for it on this bank would be a legend
 * for something that cannot happen.
 */
static const struct sk_set SK_MORE = {
    { "Refresh", "Setup", "Quit", 0, 0, "Back" },
    { K_REFRESH, K_BACK, K_QUIT, K_NONE, K_NONE, K_SKBANK }
};

static const struct sk_set SK_DETAIL = {
    { "Pg Up", "Pg Dn", "Up", "Down", 0, "Back" },
    { K_LEFT, K_RIGHT, K_UP, K_DOWN, K_NONE, K_BACK }
};

static const struct sk_set SK_PICK = {
    { "Up", "Down", 0, 0, "Pick", "Back" },
    { K_UP, K_DOWN, K_NONE, K_NONE, K_ENTER, K_BACK }
};

/* K_BACK saves and returns, which is what "Save" has to mean here -- do_setup
   treats K_BACK and K_ENTER alike and only K_QUIT leaves without saving. */
static const struct sk_set SK_SETUP = {
    { "Less", "More", 0, "Cal", 0, "Save" },
    { K_LEFT, K_RIGHT, K_NONE, K_VIEW1, K_NONE, K_BACK }
};

static const struct sk_set SK_OK = {
    { 0, 0, 0, 0, 0, "OK" },
    { K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_ENTER }
};

/* views.c paints the detail screen, so it needs this one legend; the rest are
   declared by the screens in this file. */
void ui_keys_detail(void)
{
    sk_bind(&SK_DETAIL);
}

/* Which of the two list banks is up. Reset by ui_hints(), so any full repaint
   puts the views back within reach without the user having to press Back. */
static unsigned char bank;

void ui_bank_toggle(void)
{
    bank ^= 1;
    sk_bind(bank ? &SK_MORE : &SK_LIST);
}

/*
 * Called by every view, and by the alarm teardown to give the status row back.
 * It is the single place the list screens' keys are declared, which is why it
 * has to be reachable from the view number alone.
 */
void ui_hints(unsigned char view)
{
    (void) view;

    bank = 0;
    sk_bind(&SK_LIST);
    scr_row_clear(STAT_ROW);
}

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void num(char *dst, unsigned int v)
{
    utoa(v, dst, 10);
}

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
/* Header                                                              */
/* ------------------------------------------------------------------ */

/* The wall clock, at the right of row 1. Repainting it once a minute is also
   the cheapest way to notice a wrong [General] timezone. */
void ui_clock(void)
{
    char t[6];

    if (!clk_ok) {
        scr_field(1, CLOCK_COL, "", 6, A_HEADER);
        return;
    }

    ui_hhmm(t, clk_h, clk_mi);
    scr_field(1, CLOCK_COL, t, 6, A_HEADER);
}

/*
 * Row 1 says how the fetch went. An error and a partial listing are not
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
    scr_field(1, HDR_TEXT_COL, status_text(), STATUS_W, A_HEADER);
    ui_clock();
}

/*
 * Paint the whole band and put the mark back. Callers that only move a
 * selection do not come through here.
 *
 * The band is filled before anything is written into it: the header is the one
 * region whose ground is not the body colour, and a field that only paints its
 * own cells would leave the gaps between them white.
 */
void ui_header(unsigned char view)
{
    (void) view;

    scr_rows_clear(0, HDR_ROWS - 1);
    scr_attr(0, 0, SCR_COLS, A_HEADER);
    scr_attr(1, 0, SCR_COLS, A_HEADER);
    scr_attr(2, 0, SCR_COLS, A_HDR_DIM);

    logo_small(LOGO_ROW, LOGO_COL);

    scr_field(0, TITLE_COL, gc_wtitle, TITLE_W, A_HEADER);
    ui_status();

    scr_field(2, 0, gc_cal[0] ? (const char *) gc_cal : "All shown calendars",
              SCR_COLS, A_HDR_DIM);
}

/* ------------------------------------------------------------------ */
/* Flat screens                                                        */
/* ------------------------------------------------------------------ */

static void flat_screen(void)
{
    scr_clear();
    logo_large(SPLASH_LOGO_ROW, SPLASH_LOGO_COL);
}

void ui_splash(void)
{
    flat_screen();
    sk_status("");
    scr_center(9, "FujiNet Google Calendar", A_BODY);
    scr_center(11, "Looking for FujiNet", A_DIM);
}

void ui_notfound(void)
{
    flat_screen();
    sk_bind(&SK_OK);
    scr_center(9, "FujiNet not found", A_BODY);
    scr_center(11, "Check the adapter", A_DIM);
}

/*
 * No clock is fatal, not cosmetic: every device spec this client builds names a
 * date, and there is nothing sensible to put there without one.
 *
 * On this bus the time does not come from a clock device at all -- fujinet-lib
 * ships no fn_clock for the Adam, and src/adam/clock_adam.c asks the Fuji
 * device itself. So the thing to check is the FujiNet's own [General]
 * timezone, which is also what the GCAL adapter resolves its windows with.
 */
void ui_noclock(void)
{
    flat_screen();
    sk_bind(&SK_OK);
    scr_center(9,  "FujiNet clock failed", A_BODY);
    scr_center(11, "Check the FujiNet and", A_DIM);
    scr_center(12, "its POSIX timezone", A_DIM);
}

void ui_busy(unsigned char reason)
{
    flat_screen();
    sk_status("");

    switch (reason) {
    case BUSY_CLOCK:
        scr_center(9, "Reading the clock...", A_BODY);
        break;
    case BUSY_DETAIL:
        scr_center(9, "Fetching event...", A_BODY);
        break;
    case BUSY_CALS:
        scr_center(9, "Reading calendars...", A_BODY);
        break;
    default:
        scr_center(9,  "Fetching calendar...", A_BODY);
        scr_center(11, "Up to 60 seconds", A_DIM);
        break;
    }
}

void ui_error(unsigned char code)
{
    char n[6];

    gc_ecode = code;
    flat_screen();
    sk_bind(&SK_OK);

    scr_center(9,  "Calendar error", A_BODY);
    scr_center(11, status_text(), A_BODY);

    /* The raw codes underneath, because "Calendar error" on its own is not
       something anyone can act on. */
    strcpy(sbuf, gc_stage ? gc_stage : "?");
    strcat(sbuf, " code ");
    num(n, code);
    strcat(sbuf, n);
    strcat(sbuf, " dev ");
    num(n, gc_dev_ecode);
    strcat(sbuf, n);
    scr_center(13, sbuf, A_DIM);
}

/* ------------------------------------------------------------------ */
/* Calendar picker                                                     */
/* ------------------------------------------------------------------ */

#define PICK_TOP    CONTENT_TOP                 /* 4 */
#define PICK_DETAIL (PICK_TOP + PICK_ROWS + 1)  /* 17 */

/*
 * PICK_ROWS is twelve and CAL_MAX is ten, so the window never actually
 * scrolls -- but it is the same number main.c bounds its scrolling by, and
 * keeping them one constant is why it lives in gcal.h.
 *
 * The column shows gc_cals[i].sel verbatim rather than the clipped 24-column
 * name: thirty-one cells is more of a Google calendar name than the name field
 * holds, so the shorter copy would show less. The tick in column 0 marks the
 * saved one and stays out of the selection bar, the rule every backend's
 * column 0 follows -- here because that cell's attribute is a colour and the
 * bar's is not.
 */
void ui_pick(unsigned char sel, unsigned char first)
{
    unsigned char i, row, on;
    char n[6];

    scr_clear();
    sk_bind(&SK_PICK);

    scr_attr(0, 0, SCR_COLS, A_HEADER);
    scr_attr(1, 0, SCR_COLS, A_HEADER);
    scr_attr(2, 0, SCR_COLS, A_HDR_DIM);
    logo_small(LOGO_ROW, LOGO_COL);

    scr_field(0, HDR_TEXT_COL, "Choose calendar", TITLE_W, A_HEADER);

    num(n, gc_cal_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_cal_count == 1 ? " calendar" : " calendars");
    scr_field(1, HDR_TEXT_COL, sbuf, TITLE_W, A_HEADER);
    scr_field(2, 0, "", SCR_COLS, A_HDR_DIM);

    for (i = 0; i < PICK_ROWS; i++) {
        row = (unsigned char) (PICK_TOP + i);
        if (first + i >= gc_cal_count) {
            scr_row_clear(row);
            continue;
        }

        on = (unsigned char) (first + i == sel);

        /* Marks whichever calendar is saved, not whichever is under the
           cursor -- the cursor is the bar. Basil green, because a tick is
           the one thing on this screen that is not a Google event colour. */
        if (strcmp(gc_cals[first + i].sel, gc_cal) == 0)
            scr_cell(row, 0, VDP_INK_DARK_GREEN);
        else
            scr_fill(row, 0, VDP_INK_WHITE, 1);

        /*
         * The verbatim selector, which at thirty-one cells shows more of a
         * Google calendar name than the 24-column .name copy holds. Entry 0 is
         * the exception: its selector is the empty string, because that is what
         * "every calendar" means to the adapter, so it is the one row that has
         * to fall back to the label.
         */
        scr_field(row, 1,
                  gc_cals[first + i].sel[0] ? gc_cals[first + i].sel
                                            : gc_cals[first + i].name,
                  (unsigned char) (SCR_COLS - 1), on ? A_SEL : A_BODY);
    }

    /* The full selector spelled out over two rows, because a Google calendar
       name routinely outruns even thirty-one columns. */
    scr_fill(PICK_DETAIL - 1, 0, VDP_INK_GRAY, SCR_COLS);
    scr_rows_clear(PICK_DETAIL, CONTENT_BOT);
    if (sel < gc_cal_count) {
        const char *s = gc_cals[sel].sel;

        scr_field(PICK_DETAIL, 0, s[0] ? s : "(every calendar Google shows)",
                  SCR_COLS, A_BODY);
        if (strlen(s) > SCR_COLS)
            scr_field((unsigned char) (PICK_DETAIL + 1), 0, s + SCR_COLS,
                      SCR_COLS, A_BODY);
    }
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

#define SET_TOP     CONTENT_TOP     /* 4 */
#define LEAD_ROW    13

static void setup_lead(void)
{
    char n[6];

    num(n, al_lead);
    strcpy(sbuf, n);
    strcat(sbuf, al_lead == 1 ? " minute before" : " minutes before");
    scr_field(LEAD_ROW, 1, sbuf, (unsigned char) (SCR_COLS - 1), A_BODY);
}

void ui_setup(void)
{
    char t[6];

    scr_clear();
    sk_bind(&SK_SETUP);

    scr_attr(0, 0, SCR_COLS, A_HEADER);
    scr_attr(1, 0, SCR_COLS, A_HEADER);
    scr_attr(2, 0, SCR_COLS, A_HDR_DIM);
    logo_small(LOGO_ROW, LOGO_COL);
    scr_field(0, HDR_TEXT_COL, "Settings", TITLE_W, A_HEADER);
    scr_field(1, HDR_TEXT_COL, "", TITLE_W, A_HEADER);
    scr_field(2, 0, "", SCR_COLS, A_HDR_DIM);

    /*
     * The Atari shows the POSIX timezone string here, which is the single most
     * useful line on its settings screen: the GCAL adapter's parser rejects an
     * IANA name like America/Chicago and falls back to UTC without saying so,
     * and both this clock and the window events are resolved in come from that
     * one setting.
     *
     * There is no clock device on this bus to ask -- see clock_adam.c -- so the
     * string is unavailable and printing "(unset)" would be a lie: the timezone
     * may be perfectly set and we simply cannot read it back. What is shown
     * instead is the clock's own reading, which is the observable consequence
     * of the same setting. If this says a date and time the user does not
     * recognise, the timezone is what to go and look at.
     */
    scr_text(SET_TOP, 1, "FujiNet clock", A_DIM);
    if (clk_ok) {
        date_iso(sbuf, clk_y, clk_mo, clk_d);
        strcat(sbuf, " ");
        ui_hhmm(t, clk_h, clk_mi);
        strcat(sbuf, t);
    } else {
        strcpy(sbuf, "(not read)");
    }
    scr_field((unsigned char) (SET_TOP + 1), 1, sbuf,
              (unsigned char) (SCR_COLS - 1), A_BODY);

    scr_text((unsigned char) (SET_TOP + 3), 1, "Calendar", A_DIM);
    scr_field((unsigned char) (SET_TOP + 4), 1,
              gc_cal[0] ? (const char *) gc_cal : "All shown calendars",
              (unsigned char) (SCR_COLS - 1), A_BODY);
    if (strlen(gc_cal) > SCR_COLS - 1)
        scr_field((unsigned char) (SET_TOP + 5), 1, gc_cal + (SCR_COLS - 1),
                  (unsigned char) (SCR_COLS - 1), A_BODY);

    scr_text((unsigned char) (LEAD_ROW - 1), 1, "Alarm lead", A_DIM);
    setup_lead();

    /*
     * The keys the SmartKeys have no slot for. Everything on this list works on
     * every screen; the six labelled keys are the discoverable subset, not the
     * whole of it.
     *
     * In black rather than the gray the field labels use. Gray above a black
     * value reads as a caption, which is what a field label is; a reference
     * list is the thing on the screen someone is actually trying to read, and
     * #CCCCCC on white is not enough contrast to read anything by.
     */
    scr_fill((unsigned char) (LEAD_ROW + 2), 0, VDP_INK_GRAY, SCR_COLS);
    scr_text((unsigned char) (LEAD_ROW + 3), 1, "1-4 views      0 today",
             A_BODY);
    scr_text((unsigned char) (LEAD_ROW + 4), 1, "<> period      R refresh",
             A_BODY);
    scr_text((unsigned char) (LEAD_ROW + 5), 1, "ENTER open     Q quit",
             A_BODY);
}

/* Redraw just the lead line, so holding Less or More does not repaint the
   whole screen. */
void ui_setup_lead(void)
{
    setup_lead();
}
