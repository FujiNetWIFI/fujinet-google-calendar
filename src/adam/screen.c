/*
 * Glyphs and colour on the GRAPHICS II page.
 *
 * The two planes are written by different means and that split runs through
 * the whole file. Glyphs go out through z88dk's console, which knows how to
 * blit a font cell into eight pattern bytes at an arbitrary position and is
 * what every other Adam FujiNet client uses. Colour is written straight into
 * the attribute plane with vdp_vfill, one call per run.
 *
 * Every glyph-writing call repaints the attribute run behind it. That is not
 * belt and braces: the console keeps its own notion of the current colour and
 * changes it whenever anything else prints, so a field whose colour came from
 * whatever vdp_color() was last called with is a field whose colour is a
 * function of paint order. Repainting makes each field's appearance depend on
 * its own arguments and nothing else.
 *
 * Nothing here touches rows 21-23. scr_clear() clears twenty-one rows rather
 * than calling clrscr(), because clrscr() would take the SmartKeys legend with
 * it and smartkeyslib would have to be asked to paint it again.
 */

#include <conio.h>
#include <string.h>
#include <video/tms99x8.h>

#include "../gcal.h"
#include "platform.h"

/* One row, padded, plus the terminator. */
static char pad[SCR_COLS + 1];

/*
 * In COL_* order, with COL_NONE on the end.
 *
 * Ten of the eleven pick themselves. Graphite is the one that does not: it is
 * #616161, a *dark* gray, and the TMS9918A's only gray is #CCCCCC, which
 * against the white page is barely a chip at all. Black is further from the
 * hex and much closer to the intent.
 *
 * That also frees gray for COL_NONE, which is not a colour but "the category
 * was a calendar name" -- so an uncoloured event now reads as a faint chip
 * rather than as an explicitly graphite one, and the two are told apart.
 */
static const unsigned char inks[GC_NCOLORS + 1] = {
    VDP_INK_LIGHT_BLUE,         /* LAVENDER  #7986CB */
    VDP_INK_LIGHT_GREEN,        /* SAGE      #33B679 */
    VDP_INK_MAGENTA,            /* GRAPE     #8E24AA */
    VDP_INK_LIGHT_RED,          /* FLAMINGO  #E67C73 */
    VDP_INK_LIGHT_YELLOW,       /* BANANA    #F6BF26 */
    VDP_INK_MEDIUM_RED,         /* TANGERINE #F4511E */
    VDP_INK_CYAN,               /* PEACOCK   #039BE5 */
    VDP_INK_BLACK,              /* GRAPHITE  #616161 */
    VDP_INK_DARK_BLUE,          /* BLUEBERRY #3F51B5 */
    VDP_INK_DARK_GREEN,         /* BASIL     #0B8043 */
    VDP_INK_DARK_RED,           /* TOMATO    #D50000 */
    VDP_INK_GRAY                /* COL_NONE  -- no colorId */
};

unsigned char ink_for_color(unsigned char color)
{
    return inks[(color > GC_NCOLORS) ? COL_NONE : color];
}

/* ------------------------------------------------------------------ */
/* Clearing                                                            */
/* ------------------------------------------------------------------ */

void scr_clear(void)
{
    vdp_vfill(PAT_BASE, 0x00, OUR_BYTES);
    vdp_vfill(MODE2_ATTR, A_BODY, OUR_BYTES);
}

void scr_row_clear(unsigned char row)
{
    vdp_vfill(PAT_ADDR(row, 0), 0x00, 256);
    vdp_vfill(ATT_ADDR(row, 0), A_BODY, 256);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    unsigned int n = (unsigned int) (last - first + 1) << 8;

    vdp_vfill(PAT_ADDR(first, 0), 0x00, n);
    vdp_vfill(ATT_ADDR(first, 0), A_BODY, n);
}

/* ------------------------------------------------------------------ */
/* Colour                                                              */
/* ------------------------------------------------------------------ */

void scr_attr(unsigned char row, unsigned char col, unsigned char width,
              unsigned char attr)
{
    if (col >= SCR_COLS || width == 0)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    vdp_vfill(ATT_ADDR(row, col), attr, (unsigned int) width << 3);
}

/*
 * A solid block of colour: no lit pixels, and an attribute whose background is
 * the ink. Blanking the pattern matters -- a chip is often painted over a cell
 * that used to hold a glyph, and leaving the glyph would show it in black
 * against the chip.
 */
void scr_fill(unsigned char row, unsigned char col, unsigned char ink,
              unsigned char width)
{
    if (col >= SCR_COLS || width == 0)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    vdp_vfill(PAT_ADDR(row, col), 0x00, (unsigned int) width << 3);
    vdp_vfill(ATT_ADDR(row, col), A_BLOCK(ink), (unsigned int) width << 3);
}

void scr_cell(unsigned char row, unsigned char col, unsigned char ink)
{
    scr_fill(row, col, ink, 1);
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

/*
 * The console's cursor is left wherever the last character landed, so every
 * write positions it first. Writing the full width of row 20 leaves it at the
 * start of row 21, which is inside the SmartKeys band -- harmless only because
 * nothing in this backend ever prints without a gotoxy() in front of it.
 */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr)
{
    unsigned char n = 0;
    unsigned char c;

    if (col >= SCR_COLS || width == 0)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    while (n < width && *s) {
        c = (unsigned char) *s++;
        pad[n++] = (char) ((c < 0x20 || c > 0x7E) ? '?' : c);
    }
    while (n < width)
        pad[n++] = ' ';
    pad[n] = '\0';

    gotoxy(col, row);
    cputs(pad);

    scr_attr(row, col, width, attr);
}

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char attr)
{
    unsigned char len = (unsigned char) strlen(s);

    if (col + len > SCR_COLS)
        len = (unsigned char) (SCR_COLS - col);
    scr_field(row, col, s, len, attr);
}

/* Right-align s so that its last character lands on column rcol. */
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char attr)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len > rcol + 1)
        len = (unsigned char) (rcol + 1);
    scr_field(row, (unsigned char) (rcol + 1 - len), s, len, attr);
}

void scr_center(unsigned char row, const char *s, unsigned char attr)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len >= SCR_COLS)
        scr_field(row, 0, s, SCR_COLS, attr);
    else
        scr_text(row, (unsigned char) ((SCR_COLS - len) / 2), s, attr);
}
