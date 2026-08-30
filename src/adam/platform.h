/*
 * Coleco Adam backend -- internal interface shared by the files in this
 * directory. The portable half of the program talks to us only through the
 * plat_* / ui_* declarations in ../gcal.h.
 *
 * The screen is the TMS9918A's GRAPHICS II page, which z88dk lays out as a
 * linear bitmap: the name table at $1800 is filled with 0..255 three times, so
 * every one of the 768 cells owns its own eight pattern bytes and its own
 * eight colour bytes. That is the property this whole backend turns on.
 *
 *   pattern  $0000 + (row << 8) + (col << 3)      8 bytes, one per scanline
 *   colour   $2000 + (row << 8) + (col << 3)      8 bytes, (fg << 4) | bg
 *
 * So foreground and background are settable per cell -- per scanline within a
 * cell, in fact -- out of fifteen inks. This is the first backend that can give
 * every one of Google's eleven colours its own hue. The Atari and the CoCo both
 * quantise to five because they run out (color.c's chips[]), and the Apple has
 * no colour at all; here color_chip() is never called and ink_for_color() maps
 * e->color straight onto an ink.
 *
 * Three things about this screen bite, and all three are load-bearing:
 *
 *   - Rows 21-23 are not ours. smartkeys_clear() is vdp_vfill($1500, 0, 768)
 *     and smartkeys_attrs() writes at MODE2_ATTR + 5376, which are exactly
 *     those three rows. Nothing here ever paints below row 20, and scr_clear()
 *     stops there rather than calling clrscr(), which would wipe the legend.
 *
 *   - A cell's colour lives in a different plane from its glyph, so writing
 *     text does not set its colour and setting its colour does not disturb the
 *     text. Every scr_ call that writes glyphs repaints the attribute run
 *     afterwards, so a field's appearance never depends on what the console
 *     happened to have selected.
 *
 *   - There is no inverse video. A "selected" row is not a flipped glyph, it is
 *     the same glyph under a different attribute byte, which is why the whole
 *     interface below passes an attr rather than an `inv` flag the way the
 *     other three backends do.
 */

#ifndef ADAM_PLATFORM_H
#define ADAM_PLATFORM_H

#include <video/tms99x8.h>

#define SCR_COLS        32
#define SCR_ROWS        24

/*
 * Rows 0-20. The SmartKeys band owns the last three and smartkeyslib repaints
 * all of them every time a legend changes, so treating them as part of the
 * screen would mean fighting the library for them.
 */
#define SK_ROWS         3
#define OUR_ROWS        (SCR_ROWS - SK_ROWS)
#define OUR_BYTES       ((unsigned int) OUR_ROWS * 256)

#define PAT_BASE        0x0000
#define PAT_ADDR(r, c)  ((unsigned int) (((unsigned int) (r) << 8) + ((unsigned int) (c) << 3)))
#define ATT_ADDR(r, c)  ((unsigned int) (MODE2_ATTR + PAT_ADDR(r, c)))

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

#define ATTR(fg, bg)    ((unsigned char) (((fg) << 4) | (bg)))

/*
 * Google Calendar on the web is black on white with a blue accent, and the
 * TMS9918A happens to have a serviceable version of all three. Dark blue is
 * #5455ED against Google's #4285F4, which is as close as fifteen fixed inks
 * get.
 */
#define A_HEADER        ATTR(VDP_INK_WHITE, VDP_INK_DARK_BLUE)
/* Secondary text on the header band. Light blue on dark blue is two adjacent
   entries in the TMS9918A's ramp and is very nearly invisible; gray is the
   only ink that reads as quieter than white without disappearing. */
#define A_HDR_DIM       ATTR(VDP_INK_GRAY, VDP_INK_DARK_BLUE)
#define A_BODY          ATTR(VDP_INK_BLACK, VDP_INK_WHITE)
#define A_SEL           ATTR(VDP_INK_WHITE, VDP_INK_DARK_BLUE)
#define A_DIM           ATTR(VDP_INK_GRAY, VDP_INK_WHITE)

/*
 * Headings and date separators: black on gray. Not white on gray -- the
 * TMS9918A's gray is #CCCCCC, which is far closer to its white than to its
 * black, and white text on it is barely there.
 */
#define A_RULE          ATTR(VDP_INK_BLACK, VDP_INK_GRAY)
#define A_TODAY         ATTR(VDP_INK_DARK_BLUE, VDP_INK_WHITE)
#define A_ALARM         ATTR(VDP_INK_WHITE, VDP_INK_DARK_RED)
#define A_ALARM_ALT     ATTR(VDP_INK_DARK_RED, VDP_INK_WHITE)

/* A solid block of `ink`: no lit pixels, and the ink is the cell background. */
#define A_BLOCK(ink)    ATTR(VDP_INK_BLACK, ink)

/*
 * Google's four brand colours, for the sprite mark. #4285F4 #EA4335 #FBBC04
 * #34A853 in the quadrant assignment intv/gfx.bas established and every
 * backend since has kept: blue top-left, red top-right, green bottom-left,
 * yellow bottom-right.
 */
#define G_BLUE          VDP_INK_DARK_BLUE
#define G_RED           VDP_INK_MEDIUM_RED
#define G_YELLOW        VDP_INK_LIGHT_YELLOW
#define G_GREEN         VDP_INK_MEDIUM_GREEN

/*
 * Google's eleven event colours, one ink each, in COL_* order. This is the
 * table color.c's chips[] exists to avoid needing -- and the reason this
 * backend does not call color_chip().
 *
 *   LAVENDER  #7986CB -> light blue      GRAPHITE  #616161 -> black
 *   SAGE      #33B679 -> light green     BLUEBERRY #3F51B5 -> dark blue
 *   GRAPE     #8E24AA -> magenta         BASIL     #0B8043 -> dark green
 *   FLAMINGO  #E67C73 -> light red       TOMATO    #D50000 -> dark red
 *   BANANA    #F6BF26 -> light yellow    PEACOCK   #039BE5 -> cyan
 *   TANGERINE #F4511E -> medium red
 *
 * Nothing collides: eleven names onto eleven distinct inks, and gray is left
 * over for COL_NONE, which is not a colour but "the category was a calendar
 * name". See the table in screen.c for why Graphite is the awkward one.
 */
unsigned char ink_for_color(unsigned char color);

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-2 header, 3 column rule, 4-19 content, 20 status. The SmartKeys carry
 * what the Atari puts in a tab strip and the CoCo in a footer, so neither of
 * those costs a row here -- which is what pays for a real MONTH grid.
 */
#define HDR_ROWS        3
#define RULE_ROW        3
#define CONTENT_TOP     4
#define CONTENT_BOT     19
#define STAT_ROW        20

#define LOGO_ROW        0
#define LOGO_COL        0
#define HDR_TEXT_COL    3
#define RIGHT_COL       (SCR_COLS - 1)

/* ------------------------------------------------------------------ */
/* screen.c                                                            */
/* ------------------------------------------------------------------ */

void scr_clear(void);
void scr_row_clear(unsigned char row);
void scr_rows_clear(unsigned char first, unsigned char last);

/* Write s into [col, col+width), space padded and truncated to fit, then paint
   the attribute run. */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr);
void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char attr);
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char attr);
void scr_center(unsigned char row, const char *s, unsigned char attr);

/* Colour without glyphs: scr_cell paints one solid block, scr_fill a run.
   This is what draws the event chips, the rules and the MONTH density bars. */
void scr_cell(unsigned char row, unsigned char col, unsigned char ink);
void scr_fill(unsigned char row, unsigned char col, unsigned char ink,
              unsigned char width);

/* Repaint an attribute run without touching the glyphs under it. */
void scr_attr(unsigned char row, unsigned char col, unsigned char width,
              unsigned char attr);

/* ------------------------------------------------------------------ */
/* logo.c -- the Google Calendar mark, in hardware sprites              */
/* ------------------------------------------------------------------ */

/*
 * Four 16x16 sprites, one per Google brand colour. A TMS9918A shows at most
 * four sprites on any one scanline and silently drops the fifth, so four is the
 * ceiling and the mark is drawn to sit exactly on it: the four sprites carry
 * disjoint pixels of the same shape and are laid out so no scanline needs a
 * fifth.
 *
 * The "31" is not a sprite. It is punched into the pattern table underneath in
 * gray on white, which sidesteps the per-scanline limit altogether and gives
 * sharper numerals than a 16x16 cell can.
 */
#define LOGO_SMALL_COLS 2
#define LOGO_SMALL_ROWS 2
#define LOGO_LARGE_COLS 4
#define LOGO_LARGE_ROWS 4

void logo_init(void);                           /* patterns into VRAM, once */
void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);
void logo_hide(void);

/* ------------------------------------------------------------------ */
/* input.c -- SmartKeys                                                */
/* ------------------------------------------------------------------ */

/*
 * SmartKey I..VI arrive as 0x81..0x86 and mean whatever the screen currently
 * on display says they mean, so the legend and the key map are set together.
 * K_SKBANK is private to this backend: it never reaches main.c, it flips the
 * list screens between their two legends inside plat_getkey_poll().
 */
#define K_SKBANK    0xFE

struct sk_set {
    const char   *label[6];     /* NULL leaves the slot as yellow status */
    unsigned char key[6];
};

void sk_bind(const struct sk_set *s);           /* legend + map, together */
void sk_status(const char *msg);                /* no keys, yellow status */

/* ------------------------------------------------------------------ */
/* ui.c -- chrome shared with views.c                                  */
/* ------------------------------------------------------------------ */

void ui_header(unsigned char view);     /* rows 0-2 plus the mark */
void ui_keys_detail(void);              /* the detail screen's legend */
void ui_status(void);                   /* row 2 alone */
void ui_hhmm(char *dst, unsigned char h, unsigned char m);
void ui_bank_toggle(void);              /* input.c, on K_SKBANK */

#endif /* ADAM_PLATFORM_H */
