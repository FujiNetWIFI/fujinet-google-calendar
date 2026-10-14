/*
 * Screen chrome, the flat screens, the picker, settings, and the form.
 *
 * Two kinds of screen, the split the Apple backend makes. The banded ones --
 * the views, the detail, the picker, settings and the form -- have the app
 * bar across row 0 with the mark and a title, and the keys on the 25th line.
 * The flat ones -- splash, error and the first busy screens -- are the large
 * mark and some centred text.
 *
 * What the H19 changes is the busy screen. A banded screen that has to wait
 * on the FujiNet says so on its own status row and on the 25th line and
 * stays where it is: blanking it for a busy screen and painting it all back
 * afterwards would be two full screens, four seconds at 9600 baud, around
 * every fetch.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* Header: the mark at cols 1-4 rows 0-2, everything else right of it. */
#define LOGO_ROW        0
#define LOGO_COL        1
#define HDR_TEXT_COL    6

/* The right-hand edge of the text area, leaving column 79 clear. */
#define RIGHT_COL       78
#define CLOCK_COL       (RIGHT_COL - 4)

/* Splash: the large mark, centred. */
#define SPLASH_LOGO_ROW 4
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_LARGE_COLS) / 2)

static char sbuf[96];

/* A banded screen is up, so a busy message can go on it. */
static unsigned char banded;

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

void ui_band(const char *title)
{
    scr_fill(0, 0, ' ' | REV, SCR_COLS);        /* the app bar */
    scr_row_clear(1);
    scr_row_clear(2);
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, title, 1);
    banded = 1;
}

/*
 * The view tabs. The bar is reverse, so an inactive tab is drawn reverse and
 * disappears into it, and the active one is a notch of normal video.
 */
static void tabs(unsigned char view)
{
    static const char *const label[4] = {
        " Day ", " Week ", " Month ", " Agenda "
    };
    static const unsigned char col[4] = { 52, 57, 63, 70 };
    unsigned char i;

    for (i = 0; i < 4; i++)
        scr_text(0, col[i], label[i], (unsigned char) (i != view));
}

/* The wall clock, five cells right-aligned on row 2. */
void ui_clock(void)
{
    char t[6];

    if (!clk_ok) {
        scr_field(2, CLOCK_COL, "", 5, 0);
        return;
    }

    ui_hhmm(t, clk_h, clk_mi);
    scr_field(2, CLOCK_COL, t, 5, 0);
}

/*
 * Row 2 says how the fetch went. An error and a partial listing are not
 * mutually exclusive: whatever was parsed before the stream broke stays on
 * screen underneath the message.
 */
static const char *status_text(void)
{
    char n[6];

    switch (gc_ecode) {
    case 0:             break;
    case GC_BADSPEC:
    case GC_NOTFOUND:   return "Bad calendar selector";
    case GC_DENIED:     return "Authorize Google in the FujiNet web UI";
    case GC_NOAUTH:     return "Not authorized";
    case GC_NOSERVICE:  return "Service unavailable";
    case GC_BADDRAFT:   return "Event rejected - check the fields";
    case GC_FULL:       return "Draft too large";
    case GC_RDONLY:     return "Calendar is read-only";
    default:            return "Calendar error";
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

static void status_field(const char *s, unsigned char inv)
{
    scr_field(2, HDR_TEXT_COL, s, (unsigned char) (CLOCK_COL - 1 - HDR_TEXT_COL),
              inv);
}

void ui_status(void)
{
    status_field(status_text(), 0);
    ui_clock();
}

void ui_header(unsigned char view)
{
    ui_band("Google Calendar");
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
    views_forget();
    scr_clear();
    scr_keys(0);
    logo_large(SPLASH_LOGO_ROW, SPLASH_LOGO_COL);
    banded = 0;
}

/* A flat screen is up just before a bus call that may take a while, so it
   goes to the terminal now rather than at the next key read. */
void ui_splash(void)
{
    flat_screen();
    scr_center(12, "FujiNet Google Calendar", 0);
    scr_center(14, "Looking for FujiNet", 0);
    h19_flush();
}

void ui_notfound(void)
{
    flat_screen();
    scr_center(12, "FujiNet not found", 0);
    scr_center(14, "Check the card and that the FujiNet is running", 0);
    scr_center(17, "Press any key to try again", 0);
}

/*
 * No clock is fatal, not cosmetic: every device spec this client builds names
 * a date, and there is nothing sensible to put there without one.
 */
void ui_noclock(void)
{
    flat_screen();
    scr_center(12, "FujiNet clock failed", 0);
    scr_center(14, "Enable APETIME and set a POSIX timezone, then retry", 0);
    scr_center(17, "Press any key", 0);
}

void ui_busy(unsigned char reason)
{
    const char *s;

    switch (reason) {
    case BUSY_CLOCK:    s = "Reading the clock...";     break;
    case BUSY_DETAIL:   s = "Fetching the event...";    break;
    case BUSY_CALS:     s = "Reading calendars...";     break;
    case BUSY_SAVE:     s = "Saving the event...";      break;
    default:            s = "Fetching the calendar..."; break;
    }

    if (banded) {
        h19_cursor(H19_NONE, 0);        /* the form's, while it saves */
        status_field(s, 1);
        scr_keys("Waiting for the FujiNet -- up to a minute for a calendar");
    } else {
        flat_screen();
        scr_center(12, s, 0);
        if (reason == BUSY_INDEX)
            scr_center(14, "up to a minute", 0);
    }

    h19_flush();
}

void ui_error(unsigned char code)
{
    char n[6];

    gc_ecode = code;
    flat_screen();

    scr_center(12, "Calendar error", 0);
    /* 0 is no reply at all: the calls failed, so there is no device status
       to name. */
    scr_center(14, code ? status_text() : "No reply from the FujiNet", 0);

    /* The raw codes underneath, because "Calendar error" on its own is not
       something anyone can act on. */
    strcpy(sbuf, gc_stage ? gc_stage : "?");
    strcat(sbuf, " code ");
    num(n, code);
    strcat(sbuf, n);
    strcat(sbuf, " dev ");
    num(n, gc_dev_ecode);
    strcat(sbuf, n);
    scr_center(16, sbuf, 0);

    scr_center(19, "Press any key", 0);
}

/* ------------------------------------------------------------------ */
/* Calendar picker                                                     */
/* ------------------------------------------------------------------ */

#define PICK_BOX_TOP    3
#define PICK_TOP        (PICK_BOX_TOP + 1)
#define PICK_BOX_BOT    (PICK_TOP + PICK_ROWS)
#define PICK_SEL_ROW    (PICK_BOX_BOT + 2)

void ui_pick(unsigned char sel, unsigned char first)
{
    unsigned char i, row, on;
    char n[6];

    views_forget();
    scr_clear();
    ui_band("Choose calendar");

    num(n, gc_cal_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_cal_count == 1 ? " calendar" : " calendars");
    scr_text(2, HDR_TEXT_COL, sbuf, 0);

    scr_box(PICK_BOX_TOP, 0, PICK_BOX_BOT, SCR_COLS - 1);

    for (i = 0; i < PICK_ROWS && first + i < gc_cal_count; i++) {
        row = (unsigned char) (PICK_TOP + i);
        on = (unsigned char) (first + i == sel);

        /* A bullet against whichever calendar is the saved one, outside the
           bar, so the picker says what is in force as well as where the
           cursor is. */
        if (strcmp(gc_cals[first + i].sel, gc_cal) == 0)
            scr_cell(row, 2, G_BULLET);
        scr_cell(row, 3, on ? (G_ARROW_R | REV) : ' ');
        scr_cell(row, 4, on ? (' ' | REV) : ' ');
        scr_field(row, 5,
                  gc_cals[first + i].sel[0] ? gc_cals[first + i].sel
                                            : gc_cals[first + i].name,
                  SCR_COLS - 6, on);
    }

    /* The full selector spelled out. */
    if (sel < gc_cal_count) {
        const char *s = gc_cals[sel].sel;
        scr_text(PICK_SEL_ROW, 2, s[0] ? s : "(every calendar Google is showing)",
                 0);
    }

    scr_keys("[RETURN] Select  [^v] Move  [ESC] Back");
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

static char tzbuf[48];

static void section(unsigned char row, const char *label)
{
    scr_rule(row, 0, SCR_COLS, label, 1);
}

static void chip_line(unsigned char row, unsigned char chip, const char *s)
{
    scr_cell(row, 4, chip_cell(chip));
    scr_text(row, 6, s, 0);
}

void ui_setup(void)
{
    views_forget();
    scr_clear();
    ui_band("Settings");
    scr_text(1, HDR_TEXT_COL, "FujiNet Google Calendar for the Heathkit H89", 0);

    /*
     * The timezone is the single most useful line on this screen. The GCAL
     * adapter's PosixTz parser rejects an IANA name like America/Chicago and
     * falls back to UTC without saying so, and both this clock and the window
     * events are resolved in come from that one setting -- so seeing UTC here
     * when the web UI says otherwise is the only symptom there is.
     */
    section(3, "Timezone");
    if (!tzbuf[0])
        clk_get_tz(tzbuf, sizeof(tzbuf));
    scr_text(4, 4, tzbuf[0] ? tzbuf : "(unset -- events resolve in UTC)", 0);

    section(6, "Calendar");
    scr_text(7, 4, gc_cal[0] ? gc_cal : "All shown calendars", 0);

    section(9, "Alarm lead");
    ui_setup_lead();

    section(12, "Keys");
    scr_text(13, 4, "f1-f4    day week month agenda      f5 HOME  today", 0);
    scr_text(14, 4, "RETURN   open the selection         GRAY     fetch again", 0);
    scr_text(15, 4, "BLUE     new event                  RED      edit the event", 0);
    scr_text(16, 4, "ESC      settings / back            Q        quit", 0);

    section(17, "Chips");
    chip_line(18, CHIP_BLUE,     "Lavender Grape Peacock Blueberry");
    chip_line(19, CHIP_RED,      "Flamingo Tangerine Tomato");
    chip_line(20, CHIP_GREEN,    "Sage Basil");
    chip_line(21, CHIP_YELLOW,   "Banana");
    chip_line(22, CHIP_GRAPHITE, "Graphite");

    scr_keys("[f1] Calendar  [<] [>] Alarm lead  [ESC] Save and back  "
             "[Q] Back unsaved");
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

/* ------------------------------------------------------------------ */
/* Compose / edit form                                                 */
/* ------------------------------------------------------------------ */

/*
 * The Apple's arrangement: full-word labels and wide field windows. An
 * inactive field shows its extent as an underline; the active one is a
 * reverse bar with the H19's own block cursor at the edit point, which the
 * Apple has to fake with a hole in the bar. compose.c owns the cursor and
 * the horizontal scroll; this end paints what it is given.
 */

static const unsigned char frm_rows[FRM_NFIELDS] = { 4, 6, 7, 8, 10, 12, 14 };

#define FRM_VAL_COL     14
#define FRM_HINT_ROW    17
#define FRM_MSG_ROW     20

static const unsigned char frm_w[FRM_NFIELDS] = {
    62, FRM_DATE_MAX + 1, FRM_TIME_MAX + 1, FRM_TIME_MAX + 1, 62, 62, 17
};

unsigned char ui_form_width(unsigned char f)
{
    return frm_w[f];
}

void ui_form(unsigned char editing)
{
    static const char *const label[FRM_NFIELDS] = {
        "Title", "Date", "Start time", "End time",
        "Location", "Description", "Category"
    };
    unsigned char f;

    views_forget();
    scr_clear();
    ui_band(editing ? "Edit event" : "New event");

    for (f = 0; f < FRM_NFIELDS; f++)
        scr_text(frm_rows[f], 2, label[f], 0);

    scr_text(FRM_HINT_ROW, 2,
             "Blank start time makes an all-day event.  Digits are on the "
             "top row.", 0);
    if (editing)
        scr_text(FRM_HINT_ROW + 1, 2,
                 "Only fields you change are sent; blank leaves one as it is.",
                 0);

    scr_keys("[RETURN] Next  [^v] Field  [<>] Cursor  [DEL] Erase  "
             "[BLUE] Save  [ESC] Done");
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    unsigned char w = ui_form_width(f);
    unsigned char row = frm_rows[f];
    unsigned char n;

    scr_field(row, FRM_VAL_COL, win, w, active);

    if (active) {
        h19_cursor((unsigned char) (FRM_VAL_COL + curx), row);
        return;
    }

    n = (unsigned char) strlen(win);
    if (n < w)
        scr_fill(row, (unsigned char) (FRM_VAL_COL + n), G_BAR_BOTTOM,
                 (unsigned char) (w - n));
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    scr_row_clear(FRM_MSG_ROW);

    switch (msg) {
    case FM_ASK:       s = " Save event? (Y/N) ";              break;
    case FM_NEEDTITLE: s = " A title is required ";            break;
    case FM_BADDATE:   s = " Date must be YYYY-MM-DD ";        break;
    case FM_BADTIME:   s = " Time must be HH:MM ";             break;
    case FM_ENDALONE:  s = " An end time needs a start time "; break;
    default:           return;                  /* FM_NONE: cleared above */
    }

    scr_center(FRM_MSG_ROW, s, 1);
}
