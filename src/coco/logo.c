/*
 * The Google Calendar mark, in semigraphics.
 *
 * Both marks are tables of SG4 bytes copied straight into screen RAM. The
 * constraint that shapes them is that all four quadrants of one cell share one
 * colour, so every boundary between two brand colours has to fall on a cell
 * edge -- which is why the ring is a whole cell thick rather than the two
 * pixels the Intellivision used.
 *
 * The other constraint is that the "31" cannot be text. The Atari and the
 * Apple both print it as two ordinary characters in the middle of the mark;
 * here text is dark green on green and never white, so the digits are punched
 * out of the buff page as *unlit* quadrants instead. Unlit is black, and one
 * colour plus a 2x2 on/off mask per cell is exactly what that needs.
 *
 * The tables were generated from the pictures below and re-rendered from the
 * bytes to check them, which is worth doing again if you edit one: quadrant
 * bit order is TL TR BL BR from bit 3 down, and getting it wrong produces
 * something that still looks deliberate.
 *
 * A quadrant is 4 pixels wide and 6 scanlines tall, so the digits are 4
 * quadrants wide and 5 tall -- 16 x 30 pixels, which is about the proportion
 * of a real digit. Three-wide would be legible and far too narrow.
 */

#include <string.h>

#include "../gcal.h"
#include "platform.h"

/*
 *   BBWWWWRR      A page with a brand-coloured post at each corner, in the
 *   BBWWWWRR      quadrant assignment intv/gfx.bas established and
 *   GGWWWWYY      src/atari/pmg.c kept: blue top-left, red top-right, green
 *   GGWWWWYY      bottom-left, yellow bottom-right.
 *
 * Four cells on two rows, which is 32 x 24 pixels. It costs the header no
 * rows -- the header is two rows whatever goes in it.
 */
static const unsigned char mark_small[LOGO_SMALL_ROWS][LOGO_SMALL_COLS] = {
    { 0xAF, 0xCF, 0xCF, 0xBF },
    { 0x8F, 0xCF, 0xCF, 0x9F }
};

/*
 *   BBBBBBBBRRRRRRRR      A cell-thick four-colour ring around a buff page,
 *   BBBBBBBBRRRRRRRR      with 31 punched out of it in black.
 *   BBWWWWWWWWWWWWRR
 *   BBW....WWW..WWRR      "3"  ####      "1"  .##.
 *   BBWWWW.WWWW.WWRR           ...#           ..#.
 *   BBWW...WWWW.WWRR           .###           ..#.
 *   GGWWWW.WWWW.WWYY           ...#           ..#.
 *   GGW....WW....WYY           ####           ####
 *   GGWWWWWWWWWWWWYY
 *   GGWWWWWWWWWWWWYY      The blue/red half meets the green/yellow half
 *   GGGGGGGGYYYYYYYY      between quadrant rows 5 and 6, which is a cell
 *   GGGGGGGGYYYYYYYY      boundary -- it has to be.
 *
 * Eight cells on six rows, 64 x 72 pixels.
 */
static const unsigned char mark_large[LOGO_LARGE_ROWS][LOGO_LARGE_COLS] = {
    { 0xAF, 0xAF, 0xAF, 0xAF, 0xBF, 0xBF, 0xBF, 0xBF },
    { 0xAF, 0xCE, 0xCC, 0xCD, 0xCF, 0xCC, 0xCF, 0xBF },
    { 0xAF, 0xCF, 0xCC, 0xC5, 0xCF, 0xCA, 0xCF, 0xBF },
    { 0x8F, 0xCE, 0xCC, 0xC5, 0xCE, 0xC8, 0xCD, 0x9F },
    { 0x8F, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0x9F },
    { 0x8F, 0x8F, 0x8F, 0x8F, 0x9F, 0x9F, 0x9F, 0x9F }
};

void logo_small(unsigned char row, unsigned char col)
{
    unsigned char i;

    for (i = 0; i < LOGO_SMALL_ROWS; i++)
        memcpy(SCR_RAM + (unsigned int) (row + i) * SCR_COLS + col,
               mark_small[i], LOGO_SMALL_COLS);
}

void logo_large(unsigned char row, unsigned char col)
{
    unsigned char i;

    for (i = 0; i < LOGO_LARGE_ROWS; i++)
        memcpy(SCR_RAM + (unsigned int) (row + i) * SCR_COLS + col,
               mark_large[i], LOGO_LARGE_COLS);
}
