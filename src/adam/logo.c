/*
 * The Google Calendar mark, in hardware sprites.
 *
 * Every other backend draws this mark out of whatever its screen already had:
 * the Atari steers four players with a display list interrupt, the CoCo lays
 * down semigraphics bytes, the Apple settles for inverse blocks. The Adam has
 * thirty-two real sprites, so here the mark is four of them and costs the
 * character screen underneath nothing but its page colour.
 *
 * Four is not an arbitrary number. A TMS9918A displays at most four sprites on
 * any one scanline and silently drops the fifth, and a sprite occupies every
 * scanline its 16-pixel box covers whether or not it has a lit pixel there. So
 * four overlapping sprites sit exactly on the limit and a fifth colour would
 * cost the first one that shared a line with it. The quadrant assignment is
 * intv/gfx.bas's, which src/atari/pmg.c and src/coco/logo.c both kept: blue
 * top-left, red top-right, green bottom-left, yellow bottom-right.
 *
 * The "31" is deliberately not a sprite:
 *
 *   - The large mark spells it with two ordinary characters in the two cells
 *     the ring encloses, which is what the Atari and the Apple do.
 *   - The small mark's interior is twelve pixels square, which will not hold
 *     two 8x8 glyphs, so it carries a hand-drawn pair in the pattern table.
 *
 * Either way the digits are gray on the white page, they cost no sprite, and
 * the per-scanline budget stays at four.
 *
 * A 16x16 sprite's 32 pattern bytes are four 8x8 quadrants in the order
 * top-left, bottom-left, top-right, bottom-right -- not the raster order the
 * pictures below are drawn in. Re-render from the bytes if you edit one.
 */

#include <video/tms99x8.h>

#include "../gcal.h"
#include "platform.h"

/* Sprite generator handles. Each 16x16 sprite eats four 8x8 pattern slots,
   which vdp_set_sprite_16() accounts for itself. */
#define H_SMALL     0           /* 0-3 */
#define H_LARGE     4           /* 4-7 */

/* Sprite attribute slots. The four are always used together. */
#define ID_BLUE     0
#define ID_RED      1
#define ID_GREEN    2
#define ID_YELLOW   3

/*
 * The sprite attribute table, and the y value that ends it.
 *
 * A y of 208 tells the VDP to stop scanning the list there, and ending it
 * matters more than it looks. vdp_set_mode(2) clears VRAM, so slots 4 to 31
 * are left reading y=0 -- and a sprite counts against the four-per-scanline
 * budget whether or not its colour is transparent. Unterminated, twenty-eight
 * invisible sprites sit across scanlines 1 to 16 and the mark loses whichever
 * of its four colours the hardware gets to last.
 *
 * The address is z88dk's, from libsrc/classic/video/tms9918/__vdp_mode2.asm.
 * tools/adam-decode.py checks VDP reg5 against the same number and says so if
 * it ever moves.
 */
#define SPR_ATTR    0x1B00
#define SPR_END     208

/*
 * Small mark, 16x16 -- one quarter of the ring per sprite, two pixels thick.
 *
 *   BBBBBBBBRRRRRRRR      Only two sprites have a lit pixel on any given
 *   BBBBBBBBRRRRRRRR      scanline, but all four boxes cover all sixteen
 *   BB............RR      rows, so the line budget is four either way.
 *   BB............RR
 *   BB............RR      The twelve-pixel interior is transparent and shows
 *   BB............RR      the white page and gray "31" that dig_small[]
 *   BB............RR      writes into the pattern table underneath.
 *   BB............RR
 *   GG............YY
 *   GG............YY
 *   GG............YY
 *   GG............YY
 *   GG............YY
 *   GG............YY
 *   GGGGGGGGYYYYYYYY
 *   GGGGGGGGYYYYYYYY
 */
static const unsigned char sp_small_blue[32] = {
    0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,     /* TL */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* BL */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* TR */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      /* BR */
};

static const unsigned char sp_small_red[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char sp_small_green[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char sp_small_yellow[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF
};

/*
 * Large mark, 32x32 -- the same ring three pixels thick, one 16x16 sprite per
 * quadrant rather than four stacked on one spot. Two sprites cover any given
 * scanline here, so this one has the whole budget to spare.
 */
static const unsigned char sp_large_blue[32] = {
    0xFF, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,     /* TL */
    0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,     /* BL */
    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,     /* TR */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      /* BR */
};

static const unsigned char sp_large_red[32] = {
    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07
};

static const unsigned char sp_large_green[32] = {
    0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,
    0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF
};

static const unsigned char sp_large_yellow[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0xFF, 0xFF, 0xFF
};

/*
 * The small mark's "31", four cells of pattern bytes in the order
 * (0,0) (0,1) (1,0) (1,1).
 *
 *   ......###..#....      A four-by-six pair inside the twelve-pixel
 *   ........#.##....      interior. Anything wider runs into the ring; the
 *   .....###...#....      digits are drawn a pixel narrow rather than let
 *   ........#..#....      that happen.
 *   ........#..#....
 *   .....###..####..
 */
static const unsigned char dig_small[4][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x02, 0x0E },     /* row 0, col 0 */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x10 },     /* row 0, col 1 */
    { 0x02, 0x02, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00 },     /* row 1, col 0 */
    { 0x10, 0x10, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00 }      /* row 1, col 1 */
};

/* ------------------------------------------------------------------ */

/*
 * Patterns go into VRAM once. Only the attribute table changes afterwards, so
 * moving the mark between the header and the splash is four four-byte writes.
 */
void logo_init(void)
{
    vdp_set_sprite_mode(sprite_large);          /* 16x16, unmagnified */

    vdp_set_sprite_16(H_SMALL + 0, (void *) sp_small_blue);
    vdp_set_sprite_16(H_SMALL + 1, (void *) sp_small_red);
    vdp_set_sprite_16(H_SMALL + 2, (void *) sp_small_green);
    vdp_set_sprite_16(H_SMALL + 3, (void *) sp_small_yellow);

    vdp_set_sprite_16(H_LARGE + 0, (void *) sp_large_blue);
    vdp_set_sprite_16(H_LARGE + 1, (void *) sp_large_red);
    vdp_set_sprite_16(H_LARGE + 2, (void *) sp_large_green);
    vdp_set_sprite_16(H_LARGE + 3, (void *) sp_large_yellow);

    /* Four sprites and no more, for the whole run. */
    vdp_vpoke(SPR_ATTR + 4 * 4, SPR_END);

    logo_hide();
}

/*
 * Hiding is ending the list one slot earlier rather than moving four sprites
 * off the bottom: one write instead of sixteen, and it cannot be undone by
 * accident. logo_small() and logo_large() both write slot 0's real y, which
 * puts the list back.
 */
void logo_hide(void)
{
    vdp_vpoke(SPR_ATTR, SPR_END);
}

/*
 * Two cells by two, which is what lets the header keep the mark without
 * spending a row on it -- the header is three rows whatever goes in it.
 *
 * The page has to be painted as well as the ring: the header band is dark blue
 * and the ring encloses whatever the cells underneath already were.
 */
void logo_small(unsigned char row, unsigned char col)
{
    unsigned char x = (unsigned char) (col << 3);
    unsigned char y = (unsigned char) (row << 3);
    unsigned char r, c;

    for (r = 0; r < LOGO_SMALL_ROWS; r++) {
        for (c = 0; c < LOGO_SMALL_COLS; c++)
            vdp_vwrite((void *) dig_small[r * LOGO_SMALL_COLS + c],
                       PAT_ADDR(row + r, col + c), 8);
        scr_attr((unsigned char) (row + r), col, LOGO_SMALL_COLS, A_DIM);
    }

    vdp_put_sprite_16(ID_BLUE,   x, y, H_SMALL + 0, G_BLUE);
    vdp_put_sprite_16(ID_RED,    x, y, H_SMALL + 1, G_RED);
    vdp_put_sprite_16(ID_GREEN,  x, y, H_SMALL + 2, G_GREEN);
    vdp_put_sprite_16(ID_YELLOW, x, y, H_SMALL + 3, G_YELLOW);
}

/*
 * Four cells by four, with the digits printed as text in the two the ring
 * encloses -- the same trick the Atari and the Apple use, and the reason this
 * mark needs no hand-drawn glyphs of its own.
 */
void logo_large(unsigned char row, unsigned char col)
{
    unsigned char x = (unsigned char) (col << 3);
    unsigned char y = (unsigned char) (row << 3);
    unsigned char i;

    for (i = 0; i < LOGO_LARGE_ROWS; i++)
        scr_field((unsigned char) (row + i), col, "", LOGO_LARGE_COLS, A_DIM);

    scr_text((unsigned char) (row + 1), (unsigned char) (col + 1), "31", A_DIM);

    vdp_put_sprite_16(ID_BLUE,   x,      y,      H_LARGE + 0, G_BLUE);
    vdp_put_sprite_16(ID_RED,    x + 16, y,      H_LARGE + 1, G_RED);
    vdp_put_sprite_16(ID_GREEN,  x,      y + 16, H_LARGE + 2, G_GREEN);
    vdp_put_sprite_16(ID_YELLOW, x + 16, y + 16, H_LARGE + 3, G_YELLOW);
}
