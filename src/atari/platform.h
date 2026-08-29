/*
 * Atari 8-bit backend -- internal interface shared by the files in this
 * directory. The portable half of the program talks to us only through the
 * plat_* / ui_* declarations in ../gcal.h.
 */

#ifndef ATARI_PLATFORM_H
#define ATARI_PLATFORM_H

#include <atari.h>

#define SCR_COLS    40
#define SCR_ROWS    24

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

/*
 * Google Calendar's colours on Atari hue/luma.
 *
 * The constraint that shapes everything here: in ANTIC 2 a character's pixels
 * take their HUE from COLPF2 and only their LUMINANCE from COLPF1. Text is
 * never a different colour from its band, only a different brightness. All the
 * real colour on this screen comes from the four players, which have their own
 * registers.
 *
 * The header and flat backgrounds are light greys on purpose: the logo has to
 * read as a white calendar page with a coloured ring around it.
 */
#define C_HDR_BG    _gtia_mkcolor(HUE_GREY, 6)      /* #F1F3F4 app bar   */
#define C_HDR_FG    _gtia_mkcolor(HUE_GREY, 0)
#define C_LIST_BG   _gtia_mkcolor(HUE_GREY, 7)      /* white grid page   */
#define C_LIST_FG   _gtia_mkcolor(HUE_GREY, 0)
#define C_FOOT_BG   _gtia_mkcolor(HUE_BLUE, 3)      /* #1A73E8           */
#define C_FOOT_FG   _gtia_mkcolor(HUE_GREY, 7)
#define C_BORDER    _gtia_mkcolor(HUE_BLUE, 2)

/* Flat scheme for the unbanded screens (splash, busy, error). */
#define C_FLAT_BG   _gtia_mkcolor(HUE_GREY, 7)
#define C_FLAT_FG   _gtia_mkcolor(HUE_GREY, 0)

/* The alarm banner flashes the footer band between these two. */
#define C_ALARM_A   _gtia_mkcolor(HUE_REDORANGE, 4)
#define C_ALARM_B   _gtia_mkcolor(HUE_GOLD, 6)

/*
 * Player colours. The quadrant-to-colour assignment is the Intellivision
 * original's (intv/gfx.bas): blue top-left, red top-right, green bottom-left,
 * yellow bottom-right. The same four registers colour the event chips, so the
 * chip a player draws is always the colour it drew in the logo.
 */
#define C_LOGO_BLUE     _gtia_mkcolor(HUE_BLUE, 4)      /* $78  #4285F4 */
#define C_LOGO_RED      _gtia_mkcolor(HUE_REDORANGE, 5) /* $3A  #EA4335 */
#define C_LOGO_GREEN    _gtia_mkcolor(HUE_GREEN, 5)     /* $CA  #34A853 */
#define C_LOGO_YELLOW   _gtia_mkcolor(HUE_GOLD, 7)      /* $1E  #FBBC04 */
#define C_GRAPHITE      _gtia_mkcolor(HUE_GREY, 4)      /* $08  COLPF3  */

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-2 header, 3-22 content, 23 footer. Every banded screen shares this
 * split so one DLI chain serves them all.
 */
#define HDR_ROWS    3
#define FOOT_ROW    (SCR_ROWS - 1)

/* ------------------------------------------------------------------ */
/* screen.c -- direct blitter over the OS text screen                  */
/* ------------------------------------------------------------------ */

void scr_sync(void);            /* re-read SAVMSC */
void scr_clear(void);
void scr_row_clear(unsigned char row);
void scr_rows_clear(unsigned char first, unsigned char last);

/* Write s into [col, col+width), space padded and truncated to fit. */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv);
void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv);
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char inv);
void scr_center(unsigned char row, const char *s, unsigned char inv);
void scr_row_inv(unsigned char row, unsigned char inv);

/* ------------------------------------------------------------------ */
/* dli.c / dlihw.s -- colour bands                                     */
/* ------------------------------------------------------------------ */

void dli_bands(void);           /* banded screen: shadows + DLIs on */
void dli_flat(unsigned char bg, unsigned char fg);  /* DLIs off, flat colours */
void dli_shutdown(void);
void dli_vbi_install(void);
void dli_vbi_remove(void);
void dli_foot_colors(unsigned char bg, unsigned char fg);

/* ------------------------------------------------------------------ */
/* pmg.c -- the Google Calendar mark, and the event colour chips       */
/* ------------------------------------------------------------------ */

/*
 * Sizing has to account for pixel aspect: an Atari pixel is about 0.8 as wide
 * as a scanline is tall, so the obvious "as many cells as rows" comes out
 * squat. Both variants below land within 7% of square.
 *
 * LOGO_SMALL runs at NORMAL width, which is also what makes the chips free:
 * one bit is then one colour clock, so a four-bit chip pattern is exactly one
 * character cell and the row-2 DLI never has to touch SIZEPn.
 */
#define LOGO_LARGE      0       /* double width: 8 cells x 6 text rows */
#define LOGO_SMALL      1       /* normal width: 4 cells x 3 text rows */
#define LOGO_LARGE_COLS 8
#define LOGO_LARGE_ROWS 6
#define LOGO_SMALL_COLS 4
#define LOGO_SMALL_ROWS 3

void pmg_init(void);
void pmg_show(unsigned char variant, unsigned char row, unsigned char col);
void pmg_hide(void);

/* Event chips down column 0. chips[] is one CHIP_* value per content row, or
   CHIP_NONE for a row with no chip. */
#define CHIP_NONE   0xFF

void pmg_chips(const unsigned char *chips, unsigned char first,
               unsigned char count);
void pmg_chips_clear(void);

/* ------------------------------------------------------------------ */
/* ui.c -- chrome shared with views.c                                  */
/* ------------------------------------------------------------------ */

/* The content band, rows 3..22: what the chip gutter spans. */
#define CONTENT_TOP     HDR_ROWS
#define CONTENT_ROWS    (FOOT_ROW - HDR_ROWS)

void ui_header(unsigned char view);     /* rows 0-2 plus the mark */
void ui_status(void);                   /* row 2 alone */
void ui_footer(const char *hints, const char *right);
void ui_hhmm(char *dst, unsigned char h, unsigned char m);

#endif /* ATARI_PLATFORM_H */
