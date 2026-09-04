#ifdef COCO3

/*
 * Screen chrome, the flat screens, the picker and the settings screen -- the
 * MS-DOS backend's, on the GIME text page. See the note at the top of
 * views3.c for why these are that backend's painters and not views.c's.
 *
 * The banded screens run the three-region layout: header rows 0-2, content
 * rows 3-22, footer row 23. That is one content row fewer than the backend
 * this came from, whose PC text screen has 25 lines; the row constants that
 * assumed the 25th are derived from FOOT_ROW here instead.
 *
 * The header is a three-row blue band over the light-grey page, which is the
 * Atari's DLI band structure done with attribute bytes. This file does not
 * know that: it paints A_BAR and A_STAT and platform.h's roles decide.
 *
 * The settings screen legends the chip colors. All eleven of Google's names
 * are listed, as on the backend this came from, but this machine has seven
 * inks to give them -- so some names share a block, which is the honest way to
 * show a quantization rather than hiding it.
 */

#include <cmoc.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* Splash: the large mark, centered. */
#define SPLASH_LOGO_ROW 4

/*
 * The MS-DOS backend sets its geometry in ui_geom() once screen.c has probed
 * the mode. This machine is always 80x24, so platform.h has the same names as
 * constants and the 40-column arm of that function is gone with the branch.
 */
static char sbuf[96];

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

unsigned char chip_glyph(unsigned char chip)
{
    switch (chip) {
    case CHIP_BLUE:     return CHIP_GL_BLUE;
    case CHIP_RED:      return CHIP_GL_RED;
    case CHIP_GREEN:    return CHIP_GL_GREEN;
    case CHIP_YELLOW:   return CHIP_GL_YELLOW;
    case CHIP_GRAPHITE: return CHIP_GL_GRAPHITE;
    }
    return ' ';
}

/* ------------------------------------------------------------------ */
/* Banded chrome                                                       */
/* ------------------------------------------------------------------ */

/*
 * The view tab strip. Showing which digit selects which view in the header
 * is what keeps the footer down to a single row. The active tab is A_EMPH
 * -- a page-colored chip on the blue bar in color, a normal-video chip on
 * the reverse bar in monochrome, the Apple's inverted-sense trick done with
 * a role of its own because a role table has no "flip" to apply.
 */
static void tabs(unsigned char view)
{
    static const char *const label80[4] = {
        "1DAY", "2WEEK", "3MONTH", "4AGENDA"
    };
    static const unsigned char col80[4] = { 50, 56, 63, 71 };
    static const char *const label40[4] = { "1DAY", "2WK", "3MO", "4AG" };
    static const unsigned char col40[4] = { 23, 28, 32, 36 };

    const char *const *label = scr_wide ? label80 : label40;
    const unsigned char *col = scr_wide ? col80 : col40;
    unsigned char i;

    for (i = 0; i < 4; i++)
        scr_text(0, col[i], label[i],
                 (unsigned char) (i == view ? A_EMPH : A_BAR));
}

void ui_footer(const char *hints, const char *right)
{
    scr_field(FOOT_ROW, 0, "", scr_cols, A_FOOT);
    if (hints)
        scr_text(FOOT_ROW, 1, hints, A_FOOT);
    if (right && *right)
        scr_right(FOOT_ROW, right_col, right, A_FOOT);
}

/* The wall clock, five cells right-aligned on row 2. Repainting it once a
   minute is also the cheapest way to notice a wrong [General] timezone. */
void ui_clock(void)
{
    char t[6];

    if (!clk_ok) {
        scr_field(2, (unsigned char) (right_col - 4), "", 5, A_STAT);
        return;
    }

    ui_hhmm(t, clk_h, clk_mi);
    scr_field(2, (unsigned char) (right_col - 4), t, 5, A_STAT);
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
              (unsigned char) (right_col - 5 - HDR_TEXT_COL), A_STAT);
    ui_clock();
}

/*
 * Paint rows 0-2 and put the mark back. Callers that only move a selection
 * do not come through here.
 */
void ui_header(unsigned char view)
{
    scr_field(0, 0, "", scr_cols, A_BAR);
    scr_field(1, 0, "", scr_cols, A_STAT);
    scr_field(2, 0, "", scr_cols, A_STAT);

    logo_small(LOGO_ROW, LOGO_COL);

    scr_text(0, HDR_TEXT_COL, "Google Calendar", A_BAR);
    if (view < 4)
        tabs(view);

    scr_field(1, HDR_TEXT_COL, gc_wtitle,
              (unsigned char) (scr_cols - HDR_TEXT_COL - 2), A_STAT);
    ui_status();
}

/* ------------------------------------------------------------------ */
/* Flat screens                                                        */
/* ------------------------------------------------------------------ */

static void flat_screen(void)
{
    scr_clear();
    logo_large(SPLASH_LOGO_ROW,
               (unsigned char) ((scr_cols - LOGO_LARGE_COLS) / 2));
}

void ui_splash(void)
{
    flat_screen();
    scr_center(12, "FujiNet Google Calendar", A_TEXT);
    scr_center(14, "Looking for FujiNet", A_TEXT);
}

/*
 * On this platform "not found" has one overwhelmingly likely cause with an
 * actionable fix, so the screen names it: fuji_msdos.c probes the INT F5
 * vector before anything touches the bus, and a null vector means no
 * FUJINET.SYS. A loaded driver with a dead adapter lands here too, which is
 * what the first line still covers.
 */
void ui_notfound(void)
{
    flat_screen();
    scr_center(12, "FujiNet not found", A_TEXT);
    scr_center(14, "Is FUJINET.SYS loaded in CONFIG.SYS?", A_TEXT);
    scr_center(17, "PRESS ANY KEY", A_TEXT);
}

/*
 * No clock is fatal, not cosmetic: every device spec this client builds
 * names a date, and there is nothing sensible to put there without one.
 */
void ui_noclock(void)
{
    flat_screen();
    scr_center(12, "FujiNet clock failed", A_TEXT);
    scr_center(14, "Enable APETIME and set a", A_TEXT);
    scr_center(15, "POSIX timezone, then retry", A_TEXT);
    scr_center(18, "PRESS ANY KEY", A_TEXT);
}

void ui_busy(unsigned char reason)
{
    flat_screen();

    switch (reason) {
    case BUSY_CLOCK:
        scr_center(12, "Reading the clock...", A_TEXT);
        break;
    case BUSY_DETAIL:
        scr_center(12, "Fetching event...", A_TEXT);
        break;
    case BUSY_CALS:
        scr_center(12, "Reading calendars...", A_TEXT);
        break;
    default:
        scr_center(12, "Fetching calendar...", A_TEXT);
        scr_center(14, "up to 60 seconds", A_TEXT);
        break;
    }
}

void ui_error(unsigned char code)
{
    char n[6];

    gc_ecode = code;
    flat_screen();

    scr_center(12, "Calendar error", A_TEXT);
    scr_center(14, status_text(), A_TEXT);

    /* The raw codes underneath, because "Calendar error" on its own is not
       something anyone can act on. */
    strcpy(sbuf, gc_stage ? gc_stage : "?");
    strcat(sbuf, " code ");
    num(n, code);
    strcat(sbuf, n);
    strcat(sbuf, " dev ");
    num(n, gc_dev_ecode);
    strcat(sbuf, n);
    scr_center(16, sbuf, A_TEXT);

    scr_center(19, "PRESS ANY KEY", A_TEXT);
}

/* ------------------------------------------------------------------ */
/* Calendar picker                                                     */
/* ------------------------------------------------------------------ */

#define PICK_TOP    4

void ui_pick(unsigned char sel, unsigned char first)
{
    unsigned char i, row, on;
    const char   *s;
    char n[6];

    scr_clear();

    scr_field(0, 0, "", scr_cols, A_BAR);
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, "Choose calendar", A_BAR);

    num(n, gc_cal_count);
    strcpy(sbuf, n);
    strcat(sbuf, gc_cal_count == 1 ? " calendar" : " calendars");
    scr_text(2, HDR_TEXT_COL, sbuf, A_TEXT);

    for (i = 0; i < PICK_ROWS; i++) {
        row = (unsigned char) (PICK_TOP + i);
        if (first + i >= gc_cal_count) {
            scr_row_clear(row);
            continue;
        }
        on = (unsigned char) (first + i == sel ? A_SEL : A_TEXT);

        /* A tick against whichever calendar is the saved one, so the picker
           says what is in force as well as where the cursor is. It sits in
           column 0, outside the bar, the same as the chip gutter. */
        scr_field(row, 0,
                  strcmp(gc_cals[first + i].sel, gc_cal) == 0 ? GL_CHECK : " ",
                  1, A_TEXT);
        scr_field(row, 1, first + i == sel ? ">" : " ", 1, on);
        scr_field(row, 2, "", 1, on);

        /* At 80 columns the verbatim selector fits and is shown; the
           upper-cased 24-column name exists so a Google calendar name fits
           the Atari's picker column, and 40 columns has the same squeeze. */
        if (scr_wide)
            s = gc_cals[first + i].sel[0] ? gc_cals[first + i].sel
                                          : gc_cals[first + i].name;
        else
            s = gc_cals[first + i].name;
        scr_field(row, 3, s, (unsigned char) (scr_cols - 3), on);
    }

    /* The full selector spelled out, because the picker column above may
       have clipped it and this is the string the appkey actually stores. */
    scr_rows_clear(19, 21);
    if (sel < gc_cal_count) {
        s = gc_cals[sel].sel;
        scr_text(20, 2, s[0] ? s : "(every calendar Google is showing)",
                 A_TEXT);
    }

    ui_footer("RET:SELECT  " GL_UPDOWN ":MOVE  ESC:BACK", 0);
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

#ifndef GC_NO_CLOCK_TZ
static char tzbuf[48];
#endif

/*
 * The chip column has to be legended somewhere and this is the screen with
 * room. In color every one of Google's eleven names gets its own ink, so
 * the legend lists all eleven against their blocks; on the monochrome
 * tables the five ramp glyphs are legended with the names that quantize
 * onto them, the Apple's arrangement.
 */
static void chip_legend(void)
{
    unsigned char i, row, col, percol, slotw;

    if (scr_color) {
        percol = scr_wide ? 4 : 3;
        slotw  = scr_wide ? 19 : 13;

        scr_text(17, 2, "Colors", A_TEXT);
        for (i = 0; i < GC_NCOLORS; i++) {
            row = (unsigned char) (18 + i / percol);
            col = (unsigned char) (4 + (i % percol) * slotw);
            scr_cell(row, col, GL_BLOCK, ink_attr(i));
            scr_text(row, (unsigned char) (col + 2), color_name(i), A_TEXT);
        }
        return;
    }

    scr_text(17, 2, "Chips", A_TEXT);
    scr_cell(18, 4, CHIP_GL_BLUE, scr_attr_byte(A_TEXT));
    scr_text(18, 6, "Lavender Grape Peacock Blueberry", A_TEXT);
    scr_cell(19, 4, CHIP_GL_RED, scr_attr_byte(A_TEXT));
    scr_text(19, 6, "Flamingo Tangerine Tomato", A_TEXT);
    scr_cell(20, 4, CHIP_GL_GREEN, scr_attr_byte(A_TEXT));
    scr_text(20, 6, "Sage Basil", A_TEXT);
    scr_cell(21, 4, CHIP_GL_YELLOW, scr_attr_byte(A_TEXT));
    scr_text(21, 6, "Banana", A_TEXT);
    scr_cell(22, 4, CHIP_GL_GRAPHITE, scr_attr_byte(A_TEXT));
    scr_text(22, 6, "Graphite", A_TEXT);
}

void ui_setup(void)
{
    scr_clear();

    scr_field(0, 0, "", scr_cols, A_BAR);
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, "Settings", A_BAR);

#ifndef GC_NO_CLOCK_TZ
    /*
     * The timezone is the single most useful line on this screen. The GCAL
     * adapter's PosixTz parser rejects an IANA name like America/Chicago
     * and falls back to UTC without saying so, and both this clock and the
     * window events are resolved in come from that one setting -- so seeing
     * UTC here when the web UI says otherwise is the only symptom there is.
     */
    scr_text(3, 2, "Timezone", A_TEXT);
    if (!tzbuf[0])
        clk_get_tz(tzbuf, sizeof(tzbuf));
    scr_text(4, 4, tzbuf[0] ? tzbuf : "(unset -- events resolve in UTC)",
             A_TEXT);
#else
    /*
     * The 'L'/'G' timezone reads turned out unanswered on this bus, so the
     * string is unavailable. Printing "(unset)" would be a lie -- the
     * timezone may be perfectly set and we simply cannot read it back --
     * so what is shown is the clock's own reading, the observable
     * consequence of the same setting. The CoCo's arrangement; see
     * src/coco/ui.c.
     */
    scr_text(3, 2, "FujiNet clock", A_TEXT);
    if (clk_ok) {
        char t[6];

        date_iso(sbuf, clk_y, clk_mo, clk_d);
        strcat(sbuf, " ");
        ui_hhmm(t, clk_h, clk_mi);
        strcat(sbuf, t);
    } else {
        strcpy(sbuf, "(not read)");
    }
    scr_text(4, 4, sbuf, A_TEXT);
#endif

    scr_text(6, 2, "Calendar", A_TEXT);
    scr_field(7, 4, gc_cal[0] ? gc_cal : "All shown calendars",
              (unsigned char) (scr_cols - 4), A_TEXT);

    scr_text(9, 2, "Alarm lead", A_TEXT);
    ui_setup_lead();

    scr_text(12, 2, "Keys", A_TEXT);
    if (scr_wide) {
        scr_text(13, 4, "1 2 3 4  day week month agenda     0  jump to today",
                 A_TEXT);
        scr_text(14, 4, "RETURN   open the selection        R  refresh",
                 A_TEXT);
        scr_text(15, 4, "ESC      settings / back           Q  quit",
                 A_TEXT);
    } else {
        scr_text(13, 4, "1234 views    0 today", A_TEXT);
        scr_text(14, 4, "RET  open     R refresh", A_TEXT);
        scr_text(15, 4, "ESC  back     Q quit", A_TEXT);
    }

    chip_legend();

    ui_footer("1:CALENDAR  " GL_LR ":LEAD  ESC:SAVE",
              "FujiNet Google Calendar");
}

/* Redraw just the lead line, so holding left or right does not repaint the
   whole screen and the timezone is not re-read from the device each step. */
void ui_setup_lead(void)
{
    char n[6];

    num(n, al_lead);
    strcpy(sbuf, n);
    strcat(sbuf, al_lead == 1 ? " minute before" : " minutes before");
    scr_field(10, 4, sbuf, 30, A_TEXT);
}

#endif /* COCO3 */
