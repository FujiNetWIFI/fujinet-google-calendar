/*
 * Screen chrome, the flat screens, the picker and the settings screens.
 *
 * Two kinds of screen, the same split every backend makes. The banded ones
 * -- the four views, the event detail, the picker and the settings page --
 * run the three-region layout: header rows 0-2, content rows 3-23, footer
 * row 24, one row more of content than the Apple because a PC text screen
 * has 25. The transient ones -- splash, busy, error -- are flat.
 *
 * One set of painters serves both widths. The geometry that differs is set
 * once in ui_geom() when screen.c has probed the mode; the few places the
 * *composition* differs -- tab labels, the settings key lines -- branch on
 * scr_wide. At 80 columns the layout is the Apple //e's, at 40 the Atari's.
 *
 * In colour the header is a three-row blue band over the light-grey page,
 * which is the Atari's DLI band structure done with attribute bytes; on the
 * monochrome tables row 0 is a reverse bar and rows 1-2 are plain, which is
 * the Apple's arrangement. Neither case is this file's business: it paints
 * A_BAR and A_STAT and the tables sort it out.
 */

#include <stdlib.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/* Header: the mark at cols 1-4 rows 0-2, and everything else right of it. */
#define LOGO_ROW        0
#define LOGO_COL        1
#define HDR_TEXT_COL    6

/* Splash: the large mark, centred. */
#define SPLASH_LOGO_ROW 4

/* Set by ui_geom() once screen.c knows the width; declared in platform.h. */
unsigned char right_col;
unsigned char col_cat, w_cat;
unsigned char col_title, w_title;
unsigned char wk_ncol, wk_tcol;
unsigned char mo_cellw, mo_left;
unsigned char mo_bars;

static char sbuf[96];

/*
 * The event-row and view geometry, both widths. The 80-column numbers are
 * the Apple's (chip, caret, mark, time, category, title), the 40-column
 * ones the Atari's -- which has no category column, so w_cat = 0 is also
 * the flag views.c branches on to skip it.
 */
void ui_geom(void)
{
    right_col = (unsigned char) (scr_cols - 2);

    if (scr_wide) {
        col_cat = 10;  w_cat = GC_CATW;
        col_title = 25;
        wk_ncol = 12;  wk_tcol = 14;
        mo_cellw = 11; mo_left = 1; mo_bars = 8;
    } else {
        col_cat = 0;   w_cat = 0;
        col_title = 10;
        wk_ncol = 11;  wk_tcol = 13;
        mo_cellw = 5;  mo_left = 2; mo_bars = 4;
    }
    w_title = (unsigned char) (scr_cols - col_title);
}

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
 * -- a page-coloured chip on the blue bar in colour, a normal-video chip on
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
        case GC_BADDRAFT:   return "Event rejected - check fields";
        case GC_FULL:       return "Draft too large";
        case GC_RDONLY:     return "Calendar is read-only";
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
    case BUSY_SAVE:
        scr_center(12, "Saving event...", A_TEXT);
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
 * room. In colour every one of Google's eleven names gets its own ink, so
 * the legend lists all eleven against their blocks; on the monochrome
 * tables the five ramp glyphs are legended with the names that quantise
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
        scr_text(16, 4, "N        new event                 E  edit event",
                 A_TEXT);
    } else {
        scr_text(13, 4, "1234 views    0 today", A_TEXT);
        scr_text(14, 4, "RET  open     R refresh", A_TEXT);
        scr_text(15, 4, "ESC  back     Q quit", A_TEXT);
        scr_text(16, 4, "N    new      E edit", A_TEXT);
    }

    chip_legend();

    ui_footer("1:CALENDAR  " GL_LR ":LEAD  ESC:SAVE",
              scr_wide ? "FujiNet Google Calendar" : 0);
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

/* ------------------------------------------------------------------ */
/* Compose / edit form                                                 */
/* ------------------------------------------------------------------ */

/*
 * The same flat-header arrangement as the picker and the settings page.
 * compose.c owns the cursor and the horizontal scroll; this end only knows
 * how to paint one row and where the rows are. The active field is an
 * A_SEL bar with the cursor cell knocked back to A_TEXT -- a hole in the
 * bar -- which reads on all three attribute tables, the MDA included.
 */

static const unsigned char frm_rows[FRM_NFIELDS] = { 4, 6, 7, 8, 10, 12, 14 };

#define FRM_MSG_ROW     19
#define FRM_HINT_ROW    16

static unsigned char frm_valcol(void)
{
    return (unsigned char) (scr_wide ? 15 : 8);
}

unsigned char ui_form_width(unsigned char f)
{
    switch (f) {
    case FRM_DATE:  return FRM_DATE_MAX + 1;    /* room for the cursor */
    case FRM_START:
    case FRM_END:   return FRM_TIME_MAX + 1;
    default:        return (unsigned char) (scr_cols - frm_valcol() - 2);
    }
}

void ui_form(unsigned char editing)
{
    static const char *const label80[FRM_NFIELDS] = {
        "Title", "Date", "Start time", "End time",
        "Location", "Description", "Category"
    };
    static const char *const label40[FRM_NFIELDS] = {
        "Title", "Date", "Start", "End", "Where", "Notes", "Cat"
    };
    const char *const *label = scr_wide ? label80 : label40;
    unsigned char f;

    scr_clear();

    scr_field(0, 0, "", scr_cols, A_BAR);
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, editing ? "Edit event" : "New event", A_BAR);

    for (f = 0; f < FRM_NFIELDS; f++)
        scr_text(frm_rows[f], 2, label[f], A_TEXT);

    scr_text(FRM_HINT_ROW, 2, scr_wide
             ? "Blank start time makes an all-day event"
             : "Blank start = all day", A_DIM);
    if (editing)
        scr_text((unsigned char) (FRM_HINT_ROW + 1), 2, scr_wide
                 ? "Only fields you change are sent; blank leaves one as it is"
                 : "Blank field = unchanged", A_DIM);

    ui_footer(scr_wide
              ? "TAB/RET:NEXT FIELD  " GL_UPDOWN ":FIELD  " GL_LR
                ":CURSOR  ESC:DONE"
              : "RET:NEXT  ESC:DONE", 0);
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    unsigned char row = frm_rows[f];
    unsigned char col = frm_valcol();
    unsigned char w = ui_form_width(f);

    scr_field(row, col, win, w, (unsigned char) (active ? A_SEL : A_TEXT));

    if (active)
        scr_cell(row, (unsigned char) (col + curx),
                 curx < strlen(win) ? (unsigned char) win[curx] : ' ',
                 scr_attr_byte(A_TEXT));
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    scr_row_clear(FRM_MSG_ROW);

    switch (msg) {
    case FM_ASK:       s = "Save event? (Y/N)";               break;
    case FM_NEEDTITLE: s = "A title is required";             break;
    case FM_BADDATE:   s = "Date must be YYYY-MM-DD";         break;
    case FM_BADTIME:   s = "Time must be HH:MM";              break;
    case FM_ENDALONE:  s = "An end time needs a start time";  break;
    default:           return;                  /* FM_NONE: cleared above */
    }

    scr_center(FRM_MSG_ROW, s, A_EMPH);
}
