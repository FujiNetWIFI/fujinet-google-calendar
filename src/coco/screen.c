/*
 * Text and semigraphics straight into the 32x16 page at $0400.
 *
 * There is no conio here to fight with: printf() and putchar() go out through
 * Color BASIC's console hook at $A002, which scrolls the screen when it runs
 * off row 15 and would wreck a full-width footer -- and which costs 1.1K to
 * link. Nothing in this backend uses either.
 *
 * scr_field() is the primitive everything else is built on. The raw pair,
 * scr_cell() and scr_fill(), is what SG4 goes through: the chip gutter, the
 * black rules, the WEEK chip strip and the MONTH density bars are all one byte
 * repeated, and none of them may be routed through sc().
 */

#include <string.h>
#include <coco.h>

#include "../gcal.h"
#include "platform.h"

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

/* ------------------------------------------------------------------ */
/* Clearing                                                            */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Raw bytes                                                           */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Platform lifecycle                                                  */
/* ------------------------------------------------------------------ */

void plat_init(void)
{
    /* A CoCo 3 may have come up in a 40- or 80-column GIME text mode, which
       has attribute colour but no VDG semigraphics at all. Ask for 32 and we
       are back on the page this backend knows how to draw. */
    width(32);
    scr_clear();
}

/*
 * Quitting cold-starts the machine rather than returning.
 *
 * There is nothing to return to: this program is linked at $0E00 and loading
 * it overwrote the BASIC program that launched it, along with the loader
 * itself. coldStart() is the only honest exit.
 */
void plat_shutdown(void)
{
    scr_clear();
    coldStart();
}
