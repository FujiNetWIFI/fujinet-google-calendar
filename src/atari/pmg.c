/*
 * The Google Calendar mark, and the event colour chips, in player/missile
 * graphics.
 *
 * Four players carry the four Google colours. In the header band they draw one
 * quadrant each of the calendar page's coloured ring:
 *
 *      +------+------+          P0 blue    P1 red
 *      |      |      |
 *      |   3     1   |          the page itself is the playfield showing
 *      |      |      |          through, and "31" is ordinary text
 *      +------+------+          P2 green   P3 yellow
 *
 * -- the same quadrant-to-colour assignment as the Intellivision original's
 * four flipped MOBs in intv/gfx.bas.
 *
 * Below the header the very same players become the list's colour gutter: the
 * row-2 display list interrupt moves their HPOS to column 0, and each player
 * carries a solid block on exactly the rows whose event is its colour. Since
 * only one player ever has data on a given row they cannot overlap, so this
 * costs no extra interrupts at all -- one HPOS write per player, in an
 * interrupt that had to fire anyway to change the band colour.
 *
 * The four missiles, combined into a fifth player by GPRIOR's 5th-player bit
 * and drawing in COLPF3, supply the one colour the players cannot: Graphite.
 *
 * Sizing has to account for pixel aspect. An Atari pixel is about 0.8 as wide
 * as a scanline is tall, so a mark with as many character cells as text rows
 * comes out squat. Both variants below land within 7% of square:
 *
 *   LOGO_LARGE  double width  8 bits = 4 cells/player  8 cells x 6 rows
 *   LOGO_SMALL  normal width  8 bits = 2 cells/player  4 cells x 3 rows
 *
 * LOGO_SMALL running at normal width is also what makes the chips free: one
 * bit is then one colour clock, so the four-bit pattern 0xF0 is exactly one
 * character cell and the DLI never has to touch SIZEPn.
 */

#include <string.h>
#include <atari.h>
#include <peekpoke.h>

#include "../gcal.h"
#include "platform.h"

/* Double-line resolution: a 1K buffer, 128 bytes per player. */
#define PM_M_OFF        0x180   /* all four missiles, two bits each */
#define PM_P0_OFF       0x200
#define PM_PSTRIDE      0x80
#define PM_BUFSZ        1024

#define PM_TOP          16      /* byte holding the first scanline of row 0 */
#define PM_ROWBYTES     4       /* double-line bytes per 8-scanline text row */
#define PM_LEFT         48      /* HPOS of screen column 0, in colour clocks */
#define PM_COLCLK       4       /* colour clocks per character cell */

#define PM_ROW(r)       (PM_TOP + PM_ROWBYTES * (r))
#define PM_COL(c)       (PM_LEFT + PM_COLCLK * (c))

#define HPOSP0          0xD000
#define HPOSM0          0xD004
#define SIZEM           0xD00C

static unsigned char *pmbase;
static unsigned char  pm_ok;

/*
 * Shared with dlihw.s: the DLI parks every player and missile in the chip
 * gutter on its way into the content band, and the vertical blank puts them
 * back over the logo before row 0 is drawn again.
 */
unsigned char pm_logo_hpos[4];
unsigned char pm_logo_mhpos;
unsigned char pm_chip_hpos = PM_COL(0);

/*
 * Each quadrant occupies only its own half of the byte range. Letting the
 * top-left player carry the left edge for the full height would overlap the
 * bottom-left one, and under PRIOR_P03_PF03 the lower player index wins -- the
 * left edge would come out blue from top to bottom and the green would never
 * appear at all.
 *
 * 24 double-line bytes = 48 scanlines = 6 text rows.
 */
static const unsigned char shape_large[4][24] = {
    { 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,       /* blue   */
      0xC0, 0xC0, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,       /* red    */
      0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* green  */
      0x00, 0x00, 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xC0,
      0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* yellow */
      0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03,
      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF }
};

/* 12 double-line bytes = 24 scanlines = 3 text rows. */
static const unsigned char shape_small[4][12] = {
    { 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0,                   /* blue   */
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03,                   /* red    */
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                   /* green  */
      0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                   /* yellow */
      0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF }
};

/* Offset of each player from the mark's left edge, in colour clocks: eight
   bits span 16 at double width and 8 at normal. */
static const unsigned char hoff_large[4] = { 0, 16, 0, 16 };
static const unsigned char hoff_small[4] = { 0,  8, 0,  8 };

/*
 * A chip is one character cell wide and six of the row's eight scanlines tall.
 * The two blank scanlines matter: without them a run of same-coloured events
 * would fuse into one unbroken bar and stop reading as one chip per event.
 */
static const unsigned char chip_player[PM_ROWBYTES]  = { 0xF0, 0xF0, 0xF0, 0x00 };
static const unsigned char chip_missile[PM_ROWBYTES] = { 0x03, 0x03, 0x03, 0x00 };

void pmg_init(void)
{
    unsigned int lo = (unsigned int) OS.appmhi;
    unsigned int hi = (unsigned int) OS.memtop;
    unsigned int base;

    pm_ok = 0;

    /*
     * crt0 sets both APPMHI and the C stack pointer to MEMTOP minus
     * __RESERVED_MEMORY__ (2048, from LDFLAGS_EXTRA_ATARI in the top-level
     * Makefile) and the stack grows down from there, so everything from APPMHI
     * up to MEMTOP is ours. Any 2K window contains a whole 1K-aligned 1K
     * block, which is what the P/M buffer needs.
     *
     * Reading APPMHI rather than hardcoding the reserve size means a change to
     * the Makefile cannot silently put the buffer on top of the stack.
     */
    if (hi <= lo)
        return;

    base = (lo + 0x3FF) & 0xFC00;
    if (base < lo || hi < base || (unsigned int) (hi - base) < PM_BUFSZ)
        return;

    pmbase = (unsigned char *) base;
    memset(pmbase, 0, PM_BUFSZ);

    ANTIC.pmbase = (unsigned char) (base >> 8);

    OS.pcolr0 = C_LOGO_BLUE;
    OS.pcolr1 = C_LOGO_RED;
    OS.pcolr2 = C_LOGO_GREEN;
    OS.pcolr3 = C_LOGO_YELLOW;
    OS.color3 = C_GRAPHITE;             /* COLPF3: the missiles' fifth player */

    /* A missile is two bits, so it needs double width to cover the same
       character cell a four-bit player pattern does at normal width. */
    POKE(SIZEM, 0x55);                  /* all four missiles double width */

    /*
     * GPRIOR and SDMCTL are shadowed -- the vertical blank copies them into
     * the hardware every frame, so write the shadow. GRACTL and PMBASE have no
     * shadow and are written directly.
     *
     * PRIOR_5TH_PLAYER is what turns the four missiles into one shape drawing
     * in COLPF3, which is where the Graphite chip comes from.
     */
    OS.gprior = PRIOR_P03_PF03 | PRIOR_5TH_PLAYER;
    OS.sdmctl = DMACTL_DMA_FETCH | DMACTL_PLAYFIELD_NORMAL |
                DMACTL_DMA_PLAYERS | DMACTL_DMA_MISSILES;
    GTIA_WRITE.gractl = GRACTL_PLAYERS | GRACTL_MISSLES;

    pm_ok = 1;
}

void pmg_show(unsigned char variant, unsigned char row, unsigned char col)
{
    const unsigned char *sh;
    unsigned char *p;
    unsigned char  i;
    unsigned char  rows;
    unsigned char  wide;
    unsigned char  x;

    if (!pm_ok)
        return;

    wide = (variant == LOGO_LARGE);
    rows = wide ? 24 : 12;
    x = PM_COL(col);

    for (i = 0; i < 4; i++) {
        p = pmbase + PM_P0_OFF + (unsigned int) i * PM_PSTRIDE;
        memset(p, 0, PM_PSTRIDE);

        sh = wide ? shape_large[i] : shape_small[i];
        memcpy(p + PM_ROW(row), sh, rows);

        pm_logo_hpos[i] = (unsigned char)
            (x + (wide ? hoff_large[i] : hoff_small[i]));
        POKE(HPOSP0 + i, pm_logo_hpos[i]);
    }

    /* The missiles play no part in the logo, but the vertical blank restores
       their position along with the players, so give it somewhere harmless. */
    memset(pmbase + PM_M_OFF, 0, PM_PSTRIDE);
    pm_logo_mhpos = 0;
    POKE(HPOSM0, 0);

    POKE(0xD008, wide ? PMG_SIZE_DOUBLE : PMG_SIZE_NORMAL);     /* SIZEP0 */
    POKE(0xD009, wide ? PMG_SIZE_DOUBLE : PMG_SIZE_NORMAL);
    POKE(0xD00A, wide ? PMG_SIZE_DOUBLE : PMG_SIZE_NORMAL);
    POKE(0xD00B, wide ? PMG_SIZE_DOUBLE : PMG_SIZE_NORMAL);
}

void pmg_hide(void)
{
    if (!pm_ok)
        return;

    /* Blank the shapes rather than touching GRACTL, so callers can hide
       unconditionally without worrying about the DMA state. */
    memset(pmbase + PM_P0_OFF, 0, 4 * PM_PSTRIDE);
    memset(pmbase + PM_M_OFF, 0, PM_PSTRIDE);
}

/* ------------------------------------------------------------------ */
/* Event colour chips                                                  */
/* ------------------------------------------------------------------ */

/*
 * Clear the chip gutter without disturbing the logo. The logo never reaches
 * below HDR_ROWS, so that is where the chips begin and where clearing starts.
 */
void pmg_chips_clear(void)
{
    unsigned char i;
    unsigned int  from = PM_ROW(HDR_ROWS);
    unsigned int  n = PM_PSTRIDE - from;

    if (!pm_ok)
        return;

    for (i = 0; i < 4; i++)
        memset(pmbase + PM_P0_OFF + (unsigned int) i * PM_PSTRIDE + from, 0, n);
    memset(pmbase + PM_M_OFF + from, 0, n);
}

/*
 * chips[] holds one CHIP_* value per content row, starting at screen row
 * `first`. CHIP_NONE leaves the gutter blank for that row.
 */
void pmg_chips(const unsigned char *chips, unsigned char first,
               unsigned char count)
{
    unsigned char i;
    unsigned char c;
    unsigned char *p;

    if (!pm_ok)
        return;

    pmg_chips_clear();

    for (i = 0; i < count; i++) {
        c = chips[i];
        if (c == CHIP_NONE)
            continue;

        if (c == CHIP_GRAPHITE) {
            p = pmbase + PM_M_OFF + PM_ROW(first + i);
            memcpy(p, chip_missile, PM_ROWBYTES);
        } else if (c < 4) {
            p = pmbase + PM_P0_OFF + (unsigned int) c * PM_PSTRIDE
                + PM_ROW(first + i);
            memcpy(p, chip_player, PM_ROWBYTES);
        }
    }
}
