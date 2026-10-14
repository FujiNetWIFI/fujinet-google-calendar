/*
 * Heathkit H89 backend -- internal interface shared by the files in this
 * directory. The portable half of the program talks to us only through the
 * plat_* / ui_* declarations in ../gcal.h.
 *
 * The H89's screen is its H19 terminal: 80x24, a 25th line that never
 * scrolls, reverse video, and 33 graphics characters -- lines, corners and
 * tees that join cell to cell, halves, quadrants, a checkerboard, a bullet.
 * The layout is the Apple //e backend's (src/apple2enh), row for row where it
 * can be; what the H19 adds is real frames and a real month grid where the
 * Apple has rules of MouseText, and a line of its own for the keys.
 *
 * The one thing that shapes everything else: the terminal is on the far end
 * of a 9600-baud line, about a thousand characters a second, and the program
 * cannot read what is on it. So nothing here writes to the terminal. Every
 * painter writes cells into h19.c's copy of the screen, and the copy is sent
 * by difference when the program next waits for a key -- see input.c.
 */

#ifndef H89_PLATFORM_H
#define H89_PLATFORM_H

#include "h19.h"

#define SCR_COLS    80
#define SCR_ROWS    24          /* the page; the 25th line is KEY_ROW */
#define KEY_ROW     H19_STATUS_ROW

/* ------------------------------------------------------------------ */
/* Glyphs in strings                                                   */
/* ------------------------------------------------------------------ */

/*
 * A cell holds the H19's ROM index, and the graphics characters are indices
 * 0x00-0x1F and 0x7F -- so a string can carry them as those bytes, the way
 * the Apple backend carries MouseText. Nothing from the wire can collide:
 * copy_san() clamps every field to 0x20-0x7E before a painter sees it.
 *
 * Index 0x00 is the vertical line, which cannot be in a C string, so it
 * travels as 0x1E -- the pilcrow's index, which nothing here draws -- and
 * screen.c puts it back.
 *
 * Spelled as three-digit octal escapes on purpose: "\x1B" followed by a hex
 * digit is one escape, not two characters.
 */
#define HG_VLINE        "\036"
#define HG_HLINE        "\001"
#define HG_CROSS        "\002"
#define HG_UR           "\003"  /* corners: upper right... */
#define HG_LR           "\004"
#define HG_LL           "\005"
#define HG_UL           "\006"
#define HG_ARROW_R      "\010"
#define HG_CHECKER      "\011"
#define HG_ARROW_D      "\013"
#define HG_HALF_TOP     "\020"
#define HG_TEE_DOWN     "\023"  /* tees, named for the way the stem points */
#define HG_TEE_LEFT     "\024"
#define HG_TEE_UP       "\025"
#define HG_TEE_RIGHT    "\026"
#define HG_X            "\027"
#define HG_TRI_UR       "\037"
#define HG_BULLET       "\177"

#define HG_VLINE_C      0x1E    /* the same, as a byte */
#define HG_HLINE_C      0x01

/*
 * The five colour chips. The H19 has no colour at all, so -- as on the Apple
 * -- shapes stand in, chosen to differ at a glance rather than to mean
 * anything, and the category column beside them says what they are. One of
 * them is a reverse space, which no string can carry, so chips are cells.
 */
#define CHIP_CELL_BLUE      G_BULLET
#define CHIP_CELL_RED       (' ' | REV)
#define CHIP_CELL_GREEN     G_CHECKER
#define CHIP_CELL_YELLOW    G_TRI_UR
#define CHIP_CELL_GRAPHITE  G_X

unsigned char chip_cell(unsigned char chip);

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-2 header, 3-23 content, and the 25th line for the keys. The Apple
 * spends row 23 on its hints; here they have a line the page cannot scroll
 * into, so row 23 is content.
 */
#define HDR_ROWS        3
#define CONTENT_TOP     HDR_ROWS
#define LAST_ROW        (SCR_ROWS - 1)

/* ------------------------------------------------------------------ */
/* screen.c -- cells into h19.c's copy of the screen                   */
/* ------------------------------------------------------------------ */

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

/* One cell, already coded: a ROM index, | REV for reverse. */
void scr_cell(unsigned char row, unsigned char col, unsigned char cell);

/* A run of one cell -- lines, density bars, bands. */
void scr_fill(unsigned char row, unsigned char col, unsigned char cell,
              unsigned char width);

/* A horizontal line from col to col+width-1 with a label set into it,
   "--| label |-----", the label starting lcol cells in. A NULL or empty
   label is a plain line. */
void scr_rule(unsigned char row, unsigned char col, unsigned char width,
              const char *label, unsigned char lcol);

/* The four sides of a box, the corners included; the inside is untouched. */
void scr_box(unsigned char top, unsigned char left, unsigned char bottom,
             unsigned char right);

/*
 * The 25th line: a legend of the keys the screen takes. Text in [brackets]
 * is a keycap -- drawn in reverse, the brackets dropped -- so a screen names
 * a key the way it is printed on the H19's keyboard.
 */
void scr_keys(const char *legend);

/* ------------------------------------------------------------------ */
/* logo.c -- the Google Calendar mark                                  */
/* ------------------------------------------------------------------ */

#define LOGO_SMALL_COLS 4
#define LOGO_SMALL_ROWS 3
#define LOGO_LARGE_COLS 14
#define LOGO_LARGE_ROWS 6

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* ui.c -- chrome shared with views.c                                  */
/* ------------------------------------------------------------------ */

/* The app bar on row 0 with the mark and a title; the rest of rows 0-2 are
   the caller's. Marks the screen as banded, which is what ui_busy() asks. */
void ui_band(const char *title);
void ui_header(unsigned char view);     /* rows 0-2 for a view */
void ui_status(void);                   /* row 2 alone */
void ui_hhmm(char *dst, unsigned char h, unsigned char m);

/* The view on screen, or the one a detail screen was opened from. */
extern unsigned char ui_view_now;

/* Another screen is going up: what views.c knows of the list on the
   terminal, which it scrolls rather than repaints, is no longer so. */
void views_forget(void);

#endif /* H89_PLATFORM_H */
