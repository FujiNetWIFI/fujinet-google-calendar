/*
 * Tandy Color Computer backend -- internal interface shared by the files in
 * this directory. The portable half of the program talks to us only through
 * the plat_* / ui_* declarations in ../gcal.h.
 *
 * The screen is the 6847's 32x16 alpha/semigraphics page at $0400, and the
 * whole design turns on one property of it: the VDG decides per byte whether a
 * cell is a character or a 2x2 block of colour, with no mode switch and no
 * second display list.
 *
 *   $00-$3F   glyph (byte & $3F), INV asserted -- green on dark green
 *   $40-$7F   the same glyph, normal video -- dark green on green
 *   $80-$FF   SG4: bit 7 set, bits 6-4 colour, bits 3-0 quadrant mask
 *
 * So text and colour intermix freely, and this is the first backend besides
 * the Atari's that can show an event's real Google colour -- as ordinary bytes
 * in screen RAM rather than as four players steered by an interrupt.
 *
 * Three things about that byte map bite, and all three are load-bearing here:
 *
 *   - The blank byte is $60 (space $20 with bit 6 set), not $00. memset(scr, 0)
 *     paints a screen of inverse '@'.
 *
 *   - Inverse video is XOR $40 -- but only on a character. XOR $40 on an SG4
 *     byte changes its *colour* ($8F green becomes $CF buff), so every routine
 *     that flips a run leaves bytes >= $80 alone.
 *
 *   - An unlit SG4 quadrant is black, and the text background is green, so a
 *     solid green chip on a text row is invisible. Column 0 is therefore a
 *     black gutter: an empty row gets SG_BLACK and every colour reads against
 *     it, green included.
 *
 * A stock 6847 has no lowercase at all -- 64 glyphs, uppercase only -- so sc()
 * folds case and every string literal in this directory is written uppercase.
 */

#ifndef COCO_PLATFORM_H
#define COCO_PLATFORM_H

#include <coco.h>

#define SCR_COLS    32
#define SCR_ROWS    16
#define SCR_RAM     ((unsigned char *) 0x0400)

/* Space, normal video. Not zero -- see the header comment. */
#define SCR_BLANK   0x60

/* ------------------------------------------------------------------ */
/* Semigraphics-4                                                      */
/* ------------------------------------------------------------------ */

#define SG_GREEN        0
#define SG_YELLOW       1
#define SG_BLUE         2
#define SG_RED          3
#define SG_BUFF         4
#define SG_CYAN         5
#define SG_MAGENTA      6
#define SG_ORANGE       7

/* Quadrant mask bits, in the order the VDG reads them. */
#define Q_TL            0x08
#define Q_TR            0x04
#define Q_BL            0x02
#define Q_BR            0x01
#define Q_TOP           (Q_TL | Q_TR)
#define Q_BOT           (Q_BL | Q_BR)
#define Q_LEFT          (Q_TL | Q_BL)
#define Q_RIGHT         (Q_TR | Q_BR)
#define Q_ALL           0x0F

#define SG4(c, m)       ((unsigned char) (0x80 | ((c) << 4) | (m)))
#define SG_SOLID(c)     SG4(c, Q_ALL)

/* Every colour's empty cell looks the same, so the black gutter and the black
   rules are one constant. */
#define SG_BLACK        SG4(SG_GREEN, 0)

/*
 * Google's four brand colours land on four of the eight the VDG has, and
 * Graphite lands on buff. That is exactly the five chips color.c quantises its
 * eleven colour names onto, with nothing left over and nothing missing.
 */
#define CHIP_NONE       0xFF
unsigned char chip_sg(unsigned char chip);

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-1 header, 2-14 content, 15 footer.
 *
 * The Atari and the Apple both spend three rows on the header, the third being
 * a tab strip. Sixteen rows will not pay for one: the window title on row 0
 * already says which view is up, and the footer teaches the digit keys in the
 * space a static legend would have taken. The Intellivision made the same
 * trade at twelve rows and put its hints on a second page.
 */
#define HDR_ROWS        2
#define FOOT_ROW        (SCR_ROWS - 1)
#define CONTENT_TOP     HDR_ROWS
#define CONTENT_ROWS    (FOOT_ROW - HDR_ROWS)

/* Header columns. The mark occupies cols 0-3 on both header rows. */
#define LOGO_ROW        0
#define LOGO_COL        0
#define HDR_TEXT_COL    5
#define RIGHT_COL       31

/* ------------------------------------------------------------------ */
/* screen.c -- the blitter                                             */
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

/* Raw byte access, for SG4. scr_fill writes one byte across a run, which is
   what draws the chip gutter, the black rules and the density bars.
   (The obvious parameter name `byte` is a typedef in <coco.h>.) */
void scr_cell(unsigned char row, unsigned char col, unsigned char v);
void scr_fill(unsigned char row, unsigned char col, unsigned char v,
              unsigned char width);

/* Flip a run of characters between normal and inverse, leaving SG4 bytes
   alone -- on those, XOR $40 would change the colour. */
void scr_run_inv(unsigned char row, unsigned char col, unsigned char width,
                 unsigned char inv);

/* ------------------------------------------------------------------ */
/* logo.c -- the Google Calendar mark                                  */
/* ------------------------------------------------------------------ */

/*
 * An SG4 quadrant is 4 pixels wide and 6 scanlines tall, so a cell is twice as
 * tall as it is wide in block terms and both marks are wider than they are
 * high in cells.
 *
 * The "31" cannot be printed inside the mark the way the Atari and the Apple
 * print it -- text on this machine is green, never white. It is punched out of
 * the buff page as unlit (black) quadrants instead, which works because within
 * one cell SG4 gives exactly one colour plus a 2x2 on/off mask.
 */
#define LOGO_SMALL_COLS 4
#define LOGO_SMALL_ROWS 2
#define LOGO_LARGE_COLS 8
#define LOGO_LARGE_ROWS 6

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* ui.c -- chrome shared with views.c                                  */
/* ------------------------------------------------------------------ */

void ui_header(unsigned char view);     /* rows 0-1 plus the mark */
void ui_status(void);                   /* row 1 alone */
void ui_footer(const char *hints);
void ui_hhmm(char *dst, unsigned char h, unsigned char m);

#endif /* COCO_PLATFORM_H */
