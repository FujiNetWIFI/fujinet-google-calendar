/*
 * Text and semigraphics straight into the 32x16 page at $0400, or -- on the
 * CoCo 3 build -- character/attribute pairs into the GIME's 80x24 page.
 *
 * There is no conio here to fight with: printf() and putchar() go out through
 * Color BASIC's console hook at $A002, which scrolls the screen when it runs
 * off the last row and would wreck a full-width footer -- and which costs 1.1K
 * to link. Nothing in this backend uses either.
 *
 * scr_field() is the primitive everything else is built on. scr_text(),
 * scr_right() and scr_center() are written once against it and are the same
 * code on both machines; only the primitives below the line differ.
 */

#include <string.h>
#include <coco.h>

#include "../gcal.h"
#include "platform.h"

#ifdef COCO3

/* ------------------------------------------------------------------ */
/* CoCo 3 -- the GIME text page                                        */
/* ------------------------------------------------------------------ */

/*
 * The page is not in the CPU map. Block $36 has to be banked into the $C000
 * window, written, and put back, and an interrupt taken in between would run
 * the BASIC ROM's handler with a screen where its own ROM should be. So the
 * window is opened and closed around each painter rather than each byte: the
 * longest hold is a full clear, 3,840 bytes, and this client's own timing is
 * a 60Hz tick that can afford to miss a few.
 */

static unsigned char saved_bank;

static void win_open(void)
{
    asm { orcc #$50 }
    saved_bank = *((unsigned char *) 0xFFA6);
    *((unsigned char *) 0xFFA6) = SCR_BLOCK;
}

static void win_close(void)
{
    *((unsigned char *) 0xFFA6) = saved_bank;
    asm { andcc #$AF }
}

/* Cell (row, col) inside the open window. Two bytes per cell: char then
   attribute. */
static unsigned char *cell_at(unsigned char row, unsigned char col)
{
    return SCR_WIN + ((unsigned int) row * SCR_COLS + col) * 2;
}

static void blank_run(unsigned char *p, unsigned int cells)
{
    while (cells--) {
        *p++ = SCR_BLANK;
        *p++ = A_TEXT;
    }
}


void scr_clear(void)
{
    win_open();
    blank_run(SCR_WIN, (unsigned int) SCR_COLS * SCR_ROWS);
    win_close();
}

void scr_row_clear(unsigned char row)
{
    win_open();
    blank_run(cell_at(row, 0), SCR_COLS);
    win_close();
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    win_open();
    blank_run(cell_at(first, 0),
              (unsigned int) (last - first + 1) * SCR_COLS);
    win_close();
}

/*
 * The character byte is the byte in the string. copy_san() has already clamped
 * everything from the wire to $20-$7E, which is exactly what this font draws,
 * so unlike the VDG build there is no case fold and no $3F mapping --
 * lowercase arrives and is shown.
 *
 * The last argument is an attribute, not the VDG build's inverse flag: this
 * backend's painters name a role and the role is already the byte, which is
 * what lets the layout be the MS-DOS one rather than the 32-column one with a
 * wider title column.
 */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr)
{
    unsigned char *p;
    unsigned char  n = 0;

    win_open();
    p = cell_at(row, col);

    while (n < width && *s) {
        *p++ = (unsigned char) *s++;
        *p++ = attr;
        n++;
    }
    while (n < width) {
        *p++ = SCR_BLANK;
        *p++ = attr;
        n++;
    }
    win_close();
}

/* One cell: a glyph and an explicit attribute. A color chip is a space on a
   colored ground, which is how this machine spends a cell on a color. */
void scr_cell(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char attr)
{
    unsigned char *p;

    win_open();
    p = cell_at(row, col);
    p[0] = glyph;
    p[1] = attr;
    win_close();
}

void scr_fill(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char width, unsigned char attr)
{
    unsigned char *p;

    win_open();
    p = cell_at(row, col);
    while (width--) {
        *p++ = glyph;
        *p++ = attr;
    }
    win_close();
}

/* The role tables are compile-time constants here rather than a table picked
   at init, so resolving a role is the identity. It exists because the painters
   ported from the MS-DOS backend ask for it. */
unsigned char scr_attr_byte(unsigned char attr)
{
    return attr;
}

/*
 * Palette. Slots 0-7 are the backgrounds the attribute byte's low field
 * indexes, 8-15 the foregrounds its high field does.
 *
 * paletteRGB() takes 0-3 per channel, which is the GIME's real depth -- the
 * register is six bits, two per gun.
 */
static void set_palette(void)
{
    paletteRGB(PAL_PAGE,     2, 2, 2);      /* light gray page          */
    paletteRGB(PAL_CHROME,   0, 1, 3);      /* blue: bands and the ink  */
    paletteRGB(PAL_PURPLE,   2, 0, 2);      /* Grape                    */
    paletteRGB(PAL_GREEN,    0, 2, 1);      /* Sage, Basil              */
    paletteRGB(PAL_YELLOW,   3, 3, 0);      /* Banana                   */
    paletteRGB(PAL_ORANGE,   3, 1, 0);      /* Tangerine, Flamingo      */
    paletteRGB(PAL_RED,      3, 0, 0);      /* Tomato, and the alarm    */
    paletteRGB(PAL_GRAPHITE, 1, 1, 1);      /* Graphite                 */

    paletteRGB(8 + FG_INK,    0, 0, 0);
    paletteRGB(8 + FG_PAPER,  3, 3, 3);
    paletteRGB(8 + FG_ACCENT, 0, 0, 2);
    paletteRGB(8 + FG_DIM,    1, 1, 1);
    paletteRGB(8 + FG_ALARM,  3, 0, 0);

    setBorderColor(0x00);
}

void plat_init(void)
{
    width(80);
    set_palette();
    scr_clear();
}

#else

/* ------------------------------------------------------------------ */
/* CoCo 1/2 -- the VDG page                                            */
/* ------------------------------------------------------------------ */

/*
 * ASCII to 6847 screen code.
 *
 * The glyph set is 64 entries -- '@' A-Z [ \ ] ^ _ then space through '?' --
 * indexed by (byte & $3F), with bit 6 as the INV pin. Bit 6 *set* is normal
 * video on this machine, which is the opposite of what the name suggests.
 *
 * So after folding case, ORing $40 is the whole mapping: $20-$3F becomes
 * $60-$7F and $40-$5F is already there. Everything reaching here has been
 * through copy_san(), so it is $20-$7E, and the four codes above '_' have no
 * glyph at all.
 */
static unsigned char sc(unsigned char c)
{
    if (c >= 'a' && c <= 'z')
        c = (unsigned char) (c - 0x20);         /* no lowercase in this ROM */

    if (c < 0x20 || c > 0x5F)
        c = '?';

    return (unsigned char) (c | 0x40);
}

unsigned char chip_sg(unsigned char chip)
{
    switch (chip) {
    case CHIP_BLUE:     return SG_SOLID(SG_BLUE);
    case CHIP_RED:      return SG_SOLID(SG_RED);
    case CHIP_GREEN:    return SG_SOLID(SG_GREEN);
    case CHIP_YELLOW:   return SG_SOLID(SG_YELLOW);
    case CHIP_GRAPHITE: return SG_SOLID(SG_BUFF);
    }

    return SG_BLACK;
}

void scr_clear(void)
{
    memset(SCR_RAM, SCR_BLANK, (unsigned int) SCR_COLS * SCR_ROWS);
}

void scr_row_clear(unsigned char row)
{
    memset(SCR_RAM + (unsigned int) row * SCR_COLS, SCR_BLANK, SCR_COLS);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    memset(SCR_RAM + (unsigned int) first * SCR_COLS, SCR_BLANK,
           (unsigned int) (last - first + 1) * SCR_COLS);
}

void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv)
{
    unsigned char *p = SCR_RAM + (unsigned int) row * SCR_COLS + col;
    unsigned char  v = inv ? 0x40 : 0x00;
    unsigned char  n = 0;

    while (n < width && *s) {
        *p++ = (unsigned char) (sc((unsigned char) *s++) ^ v);
        n++;
    }
    while (n < width) {
        *p++ = (unsigned char) (SCR_BLANK ^ v);
        n++;
    }
}

void scr_cell(unsigned char row, unsigned char col, unsigned char v)
{
    SCR_RAM[(unsigned int) row * SCR_COLS + col] = v;
}

void scr_fill(unsigned char row, unsigned char col, unsigned char v,
              unsigned char width)
{
    memset(SCR_RAM + (unsigned int) row * SCR_COLS + col, v, width);
}

/*
 * Flip a run between normal and inverse.
 *
 * Bytes >= $80 are semigraphics and are skipped: there, bit 6 is part of the
 * colour field, so XOR $40 would silently recolour a chip rather than
 * highlight it. Nothing in this backend actually paints a chip inside a
 * selection bar -- the gutter is deliberately outside it -- but a run that
 * crosses one must not corrupt it either.
 */
void scr_run_inv(unsigned char row, unsigned char col, unsigned char width,
                 unsigned char inv)
{
    unsigned char *p = SCR_RAM + (unsigned int) row * SCR_COLS + col;
    unsigned char  i;

    for (i = 0; i < width; i++, p++) {
        if (*p >= 0x80)
            continue;
        if (inv)
            *p = (unsigned char) (*p & 0xBF);
        else
            *p = (unsigned char) (*p | 0x40);
    }
}

void plat_init(void)
{
    /* A CoCo 3 may have come up in a 40- or 80-column GIME text mode, which
       has attribute colour but no VDG semigraphics at all. Ask for 32 and we
       are back on the page this backend knows how to draw. */
    width(32);
    scr_clear();
}

#endif /* COCO3 */

/* ------------------------------------------------------------------ */
/* Text, on both machines                                              */
/* ------------------------------------------------------------------ */

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (col + len > SCR_COLS)
        len = (unsigned char) (SCR_COLS - col);
    scr_field(row, col, s, len, inv);
}

/* Right-align s so that its last character lands on column rcol. */
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len > rcol + 1)
        len = (unsigned char) (rcol + 1);
    scr_field(row, (unsigned char) (rcol + 1 - len), s, len, inv);
}

void scr_center(unsigned char row, const char *s, unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len >= SCR_COLS)
        scr_field(row, 0, s, SCR_COLS, inv);
    else
        scr_text(row, (unsigned char) ((SCR_COLS - len) / 2), s, inv);
}

/*
 * Quitting cold-starts the machine rather than returning.
 *
 * There is nothing to return to: loading this program overwrote the BASIC
 * program that launched it, along with the loader itself. coldStart() is the
 * only honest exit.
 */
void plat_shutdown(void)
{
    scr_clear();
    coldStart();
}

#ifdef COCO3

/* ------------------------------------------------------------------ */
/* Event color                                                        */
/* ------------------------------------------------------------------ */

/*
 * Google's eleven names onto the seven backgrounds the GIME can spare, in
 * COL_* order. Grouped by hue so a wall of Blueberry and Peacock still reads
 * as one calendar, but Grape keeps its purple and Tangerine its orange, which
 * the VDG build's five chips both lose.
 *
 * PAL_CHROME doubles as the blue: the bands and a blue chip are the same blue
 * and never meet, and spending a ninth slot on the distinction is not
 * available.
 */
static const unsigned char inks[GC_NCOLORS + 1] = {
    PAL_CHROME,     /* LAVENDER  #7986CB */
    PAL_GREEN,      /* SAGE      #33B679 */
    PAL_PURPLE,     /* GRAPE     #8E24AA */
    PAL_ORANGE,     /* FLAMINGO  #E67C73 */
    PAL_YELLOW,     /* BANANA    #F6BF26 */
    PAL_ORANGE,     /* TANGERINE #F4511E */
    PAL_CHROME,     /* PEACOCK   #039BE5 */
    PAL_GRAPHITE,   /* GRAPHITE  #616161 */
    PAL_CHROME,     /* BLUEBERRY #3F51B5 */
    PAL_GREEN,      /* BASIL     #0B8043 */
    PAL_RED,        /* TOMATO    #D50000 */
    PAL_GRAPHITE    /* COL_NONE -- no colorId of its own */
};

unsigned char ink_attr(unsigned char color)
{
    if (color > GC_NCOLORS)
        color = GC_NCOLORS;

    return ATTR(FG_PAPER, inks[color]);
}

#endif /* COCO3 */
