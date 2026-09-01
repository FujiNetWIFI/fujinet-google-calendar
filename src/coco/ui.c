/*
 * Screen chrome, the flat screens, and the two settings screens.
 *
 * Sixteen rows is four fewer than the Intellivision had columns to spare, and
 * every screen here is laid out to the same two-row header and one-row footer
 * so that the thirteen rows between them are the whole budget. The Atari and
 * the Apple both spend a third header row on a tab strip; this one cannot
 * afford it, so the footer carries "1234:VIEW" and the settings screen legends
 * the rest -- which is the trade the Intellivision made too.
 */

#include <cmoc.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/*
 * Row 0: mark, window title -- the whole rest of the row.
 * Row 1: mark, status, wall clock.
 *
 * The window title gets a row to itself on purpose. The user knows what they
 * launched; they do not know which day they are looking at, and at 32 columns
 * "GOOGLE CALENDAR" would be half of row 0 saying nothing. It appears on the
 * splash screen and nowhere else.
 *
 * It needs the whole row, too: "Week of Sun 23 Aug 2026" is twenty-three
 * characters off the wire, and anything narrower than that clips the year --
 * which reads as a bug rather than as a truncation. That is what pushed the
 * page indicator down to the footer, where the Atari and the Apple keep theirs
 * anyway.
 */
#define TITLE_COL       HDR_TEXT_COL            /* 5 */
#define TITLE_W         (SCR_COLS - TITLE_COL)  /* cols 5-31, 27 wide */
#define STATUS_W        21                      /* cols 5-25 */
#define CLOCK_COL       27                      /* cols 27-31 */

/* Splash: the large mark is eight cells wide, six rows tall. */
#define SPLASH_LOGO_ROW 2
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_LARGE_COLS) / 2)

static char sbuf[40];

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
/* Header and footer                                                   */
/* ------------------------------------------------------------------ */

void ui_footer(const char *hints)
{
    scr_field(FOOT_ROW, 0, hints ? hints : "", SCR_COLS, 0);
}

/* The wall clock, five cells at the right of row 1. Repainting it once a
   minute is also the cheapest way to notice a wrong [General] timezone. */
void ui_clock(void)
{
    char t[6];

    if (!clk_ok) {
        scr_field(1, CLOCK_COL, "", 5, 0);
        return;
    }

    ui_hhmm(t, clk_h, clk_mi);
    scr_field(1, CLOCK_COL, t, 5, 0);
}

/*
 * Row 1 says how the fetch went, in nineteen cells. An error and a partial
 * listing are not mutually exclusive: whatever was parsed before the stream
 * broke stays on screen underneath the message.
 *
 * The strings are shorter than the Atari's because they have to be. The full
 * sentences live on ui_error(), which has a whole screen for them.
 */
static const char *status_text(void)
{
    char n[6];

    if (gc_ecode) {
        switch (gc_ecode) {
        case 0:             return "NO FUJINET REPLY";
        case GC_BADSPEC:
        case GC_NOTFOUND:   return "BAD SELECTOR";
        case GC_DENIED:     return "NOT AUTHORIZED";
        case GC_NOAUTH:     return "NOT AUTHORIZED";
        case GC_NOSERVICE:  return "NO SERVICE";
        case GC_BADDRAFT:   return "EVENT REJECTED";
        case GC_FULL:       return "DRAFT TOO BIG";
        case GC_RDONLY:     return "READ ONLY";
        default:            return "CALENDAR ERROR";
        }
    }

    if (gc_count == 0)
        return "NO EVENTS";

    if (gc_count == 1)
        return "1 EVENT";

    num(n, gc_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_trunc ? " EVENTS, MORE" : " EVENTS");
    return sbuf;
}

void ui_status(void)
{
    scr_field(1, HDR_TEXT_COL, status_text(), STATUS_W, 0);
    ui_clock();
}

/*
 * Paint rows 0-1 and put the mark back. Callers that only move a selection do
 * not come through here.
 */
void ui_header(unsigned char view)
{
    (void) view;

    scr_rows_clear(0, 1);
    logo_small(LOGO_ROW, LOGO_COL);

    scr_field(0, TITLE_COL, gc_wtitle, TITLE_W, 0);
    ui_status();
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
    scr_center(9, "FUJINET GOOGLE CALENDAR", 0);
    scr_center(11, "LOOKING FOR FUJINET", 0);
}

void ui_notfound(void)
{
    flat_screen();
    scr_center(9, "FUJINET NOT FOUND", 0);
    scr_center(11, "CHECK THE ADAPTER", 0);
    scr_center(14, "PRESS ANY KEY", 0);
}

/*
 * No clock is fatal, not cosmetic: every device spec this client builds names
 * a date, and there is nothing sensible to put there without one. On this bus
 * the time comes back over DriveWire rather than from APETIME, but the setting
 * behind it is the same [General] timezone.
 */
void ui_noclock(void)
{
    flat_screen();
    scr_center(9, "FUJINET CLOCK FAILED", 0);
    scr_center(11, "CHECK THE FUJINET AND", 0);
    scr_center(12, "ITS POSIX TIMEZONE", 0);
    scr_center(14, "PRESS ANY KEY", 0);
}

void ui_busy(unsigned char reason)
{
    flat_screen();

    switch (reason) {
    case BUSY_CLOCK:
        scr_center(9, "READING THE CLOCK...", 0);
        break;
    case BUSY_DETAIL:
        scr_center(9, "FETCHING EVENT...", 0);
        break;
    case BUSY_CALS:
        scr_center(9, "READING CALENDARS...", 0);
        break;
    case BUSY_SAVE:
        scr_center(9, "SAVING EVENT...", 0);
        break;
    default:
        scr_center(9, "FETCHING CALENDAR...", 0);
        scr_center(11, "UP TO 60 SECONDS", 0);
        break;
    }
}

void ui_error(unsigned char code)
{
    char n[6];

    gc_ecode = code;
    flat_screen();

    scr_center(9, "CALENDAR ERROR", 0);
    scr_center(11, status_text(), 0);

    /* The raw codes underneath, because "CALENDAR ERROR" on its own is not
       something anyone can act on. */
    strcpy(sbuf, gc_stage ? gc_stage : "?");
    strcat(sbuf, " CODE ");
    num(n, code);
    strcat(sbuf, n);
    strcat(sbuf, " DEV ");
    num(n, gc_dev_ecode);
    strcat(sbuf, n);
    scr_center(12, sbuf, 0);

    scr_center(14, "PRESS ANY KEY", 0);
}

/* ------------------------------------------------------------------ */
/* Calendar picker                                                     */
/* ------------------------------------------------------------------ */

#define PICK_TOP    2

/*
 * Ten rows, which is CAL_MAX, so the window never actually scrolls -- but
 * PICK_ROWS is the same number main.c bounds its scrolling by, and keeping
 * them one constant is why it lives in gcal.h now.
 *
 * The column shows gc_cals[i].sel verbatim rather than the clipped 24-column
 * name: thirty-one cells is more of a Google calendar name than the name field
 * holds, so the shorter copy would show less. The tick in column 0 marks the
 * saved one, and stays out of the selection bar for the reason every backend's
 * column 0 does -- here, because inverting a semigraphics byte recolours it.
 */
void ui_pick(unsigned char sel, unsigned char first)
{
    unsigned char i, row, on;
    char n[6];

    scr_clear();
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, "CHOOSE CALENDAR", 0);

    num(n, gc_cal_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_cal_count == 1 ? " CALENDAR" : " CALENDARS");
    scr_text(1, HDR_TEXT_COL, sbuf, 0);

    for (i = 0; i < PICK_ROWS; i++) {
        row = (unsigned char) (PICK_TOP + i);
        if (first + i >= gc_cal_count) {
            scr_row_clear(row);
            continue;
        }

        on = (unsigned char) (first + i == sel);

        /* Marks whichever calendar is saved, not whichever is under the
           cursor -- the cursor is the bar. */
        scr_cell(row, 0,
                 (unsigned char) (strcmp(gc_cals[first + i].sel, gc_cal) == 0
                                  ? SG_SOLID(SG_GREEN) : SG_BLACK));

        /* The verbatim selector, which at thirty-one cells shows more of a
           Google calendar name than the 24-column .name copy holds. Entry 0 is
           the exception: its selector is the empty string, because that is what
           "every calendar" means to the adapter, so it is the one row that has
           to fall back to the label. */
        scr_field(row, 1,
                  gc_cals[first + i].sel[0] ? gc_cals[first + i].sel
                                            : gc_cals[first + i].name,
                  (unsigned char) (SCR_COLS - 1), on);
    }

    /* The full selector spelled out over two rows, because a Google calendar
       name routinely outruns even thirty-one columns. */
    scr_rows_clear(12, 14);
    if (sel < gc_cal_count) {
        const char *s = gc_cals[sel].sel;

        scr_field(13, 0, s[0] ? s : "(EVERY CALENDAR GOOGLE SHOWS)",
                  SCR_COLS, 0);
        if (strlen(s) > SCR_COLS)
            scr_field(14, 0, s + SCR_COLS, SCR_COLS, 0);
    }

    ui_footer("ENT:PICK  BRK:BACK");
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

#define LEAD_ROW    9

static void setup_lead(void)
{
    char n[6];

    num(n, al_lead);
    strcpy(sbuf, n);
    strcat(sbuf, al_lead == 1 ? " MINUTE BEFORE" : " MINUTES BEFORE");
    scr_field(LEAD_ROW, 1, sbuf, (unsigned char) (SCR_COLS - 1), 0);
}

void ui_setup(void)
{
    char t[6];

    scr_clear();
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, "SETTINGS", 0);

    /*
     * The Atari shows the POSIX timezone string here, which is the single most
     * useful line on its settings screen: the GCAL adapter's parser rejects an
     * IANA name like America/Chicago and falls back to UTC without saying so,
     * and both this clock and the window events are resolved in come from that
     * one setting.
     *
     * clock_get_tz() does not exist on this bus -- fujinet-lib declares it for
     * every platform and builds it for some -- so the string is unavailable.
     * Printing "(unset)" would be a lie, since the timezone may be perfectly
     * set and we simply cannot read it back. What is shown instead is the
     * clock's own reading, which is the observable *consequence* of the same
     * setting: if this says a date and time the user does not recognise, the
     * timezone is what to go and look at.
     */
    scr_text(2, 1, "FUJINET CLOCK", 0);
    if (clk_ok) {
        date_iso(sbuf, clk_y, clk_mo, clk_d);
        strcat(sbuf, " ");
        ui_hhmm(t, clk_h, clk_mi);
        strcat(sbuf, t);
    } else {
        strcpy(sbuf, "(NOT READ)");
    }
    scr_text(3, 1, sbuf, 0);

    scr_text(5, 1, "CALENDAR", 0);
    scr_field(6, 1, gc_cal[0] ? (const char *) gc_cal : "ALL SHOWN CALENDARS",
              (unsigned char) (SCR_COLS - 1), 0);

    scr_text(8, 1, "ALARM LEAD", 0);
    setup_lead();

    /* The keys the footer has no room to name. */
    scr_text(11, 1, "KEYS", 0);
    scr_text(12, 1, "1234 VIEWS   0 TODAY", 0);
    scr_text(13, 1, "<>  PERIOD   ENT OPEN", 0);
    scr_text(14, 1, "R RFRSH Q QUIT N NEW E EDIT", 0);

    ui_footer("1:CAL  <>:LEAD  BRK:SAVE");
}

/* Redraw just the lead line, so holding left or right does not repaint the
   whole screen. */
void ui_setup_lead(void)
{
    setup_lead();
}

/* ------------------------------------------------------------------ */
/* Compose / edit form                                                 */
/* ------------------------------------------------------------------ */

/*
 * Sixteen rows carry seven fields, one hint and a message row, so the
 * spacing is tighter than anyone else's. The active field is a full
 * inverse bar -- the list's selection language -- with the cursor cell
 * knocked back to normal video. compose.c owns the cursor and the
 * horizontal scroll; the editor here is append-and-backspace (see
 * input.c) and the echo is the 6847's uppercase set, which is also
 * everything the wire will get from this keyboard.
 */

static const unsigned char frm_rows[FRM_NFIELDS] = { 2, 4, 5, 6, 8, 10, 12 };

#define FRM_VAL_COL     7
#define FRM_HINT_ROW    13
#define FRM_MSG_ROW     14

static const unsigned char frm_w[FRM_NFIELDS] = {
    24, FRM_DATE_MAX + 1, FRM_TIME_MAX + 1, FRM_TIME_MAX + 1, 24, 24, 16
};

unsigned char ui_form_width(unsigned char f)
{
    return frm_w[f];
}

void ui_form(unsigned char editing)
{
    unsigned char f;

    scr_clear();
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, editing ? "EDIT EVENT" : "NEW EVENT", 0);

    /* One literal at a six-byte stride, not an array of pointers -- CMOC
       initialises a static pointer array with run-time code per entry. */
    for (f = 0; f < FRM_NFIELDS; f++)
        scr_text(frm_rows[f], 1,
                 "TITLE\0DATE\0\0START\0END\0\0\0WHERE\0NOTES\0CAT" + f * 6, 0);

    scr_text(FRM_HINT_ROW, 1, "BLANK START = ALL DAY", 0);

    ui_footer("ENT:NEXT  BRK:DONE");
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    char b[2];

    scr_field(frm_rows[f], FRM_VAL_COL, win, frm_w[f], active);

    if (active) {
        b[0] = curx < strlen(win) ? win[curx] : ' ';
        b[1] = '\0';
        scr_field(frm_rows[f], (unsigned char) (FRM_VAL_COL + curx), b, 1, 0);
    }
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    scr_row_clear(FRM_MSG_ROW);

    /* A switch of literals, which CMOC stores as plain strings -- a static
       table of pointers would cost run-time init code per entry. */
    switch (msg) {
    case FM_ASK:       s = "SAVE? (Y/N)";        break;
    case FM_NEEDTITLE: s = "TITLE REQUIRED";     break;
    case FM_BADDATE:   s = "DATE: YYYY-MM-DD";   break;
    case FM_BADTIME:   s = "TIME: HH:MM";        break;
    case FM_ENDALONE:  s = "END NEEDS A START";  break;
    default:           return;                  /* FM_NONE: cleared above */
    }

    scr_center(FRM_MSG_ROW, s, 1);
}
