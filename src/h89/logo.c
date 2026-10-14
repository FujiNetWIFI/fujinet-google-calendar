/*
 * The Google Calendar mark.
 *
 * The Apple draws a white page with 31 on it, inverse block and inverse
 * digits. The H19 can draw the page itself: a frame of its line characters,
 * the binding rings as its bullet, the coloured strip across the top as a
 * band of reverse video. On a dark screen that is a calendar page.
 */

#include "../gcal.h"
#include "platform.h"

/*
 * Small: the header mark, four cells by three rows. Row 0 of a view is the
 * reverse app bar, so the mark's top row is a notch of normal video in it,
 * with the rings on the page's top edge.
 */
void logo_small(unsigned char row, unsigned char col)
{
    scr_field(row, col, HG_UL HG_BULLET HG_BULLET HG_UR, LOGO_SMALL_COLS, 0);
    scr_field((unsigned char) (row + 1), col, HG_VLINE "31" HG_VLINE,
              LOGO_SMALL_COLS, 0);
    scr_field((unsigned char) (row + 2), col, HG_LL HG_HLINE HG_HLINE HG_LR,
              LOGO_SMALL_COLS, 0);
}

/*
 * Large: the splash and error screens, fourteen by six, with the 31 drawn
 * three rows tall out of half blocks -- the upper half is a glyph, the lower
 * half is that glyph in reverse, and a full block is a reverse space.
 */
#define UP  HG_HALF_TOP                 /* upper half       */
#define LO  "\201"                      /* marker: lower    */
#define FU  "\202"                      /* marker: full     */

static const char *const big31[3] = {
    UP UP FU " " UP FU " ",
    UP UP FU "  " FU " ",
    LO LO FU " " LO FU LO,
};

void logo_large(unsigned char row, unsigned char col)
{
    unsigned char r, i;
    const char   *s;
    unsigned char c;

    scr_box(row, col, (unsigned char) (row + LOGO_LARGE_ROWS - 1),
            (unsigned char) (col + LOGO_LARGE_COLS - 1));
    scr_cell(row, (unsigned char) (col + 3), G_BULLET);
    scr_cell(row, (unsigned char) (col + LOGO_LARGE_COLS - 4), G_BULLET);

    /* The strip */
    scr_fill((unsigned char) (row + 1), (unsigned char) (col + 1), ' ' | REV,
             LOGO_LARGE_COLS - 2);

    for (r = 0; r < 3; r++) {
        s = big31[r];
        for (i = 0; s[i]; i++) {
            c = (unsigned char) s[i];
            if (c == 0x81)
                c = G_HALF_TOP | REV;
            else if (c == 0x82)
                c = ' ' | REV;
            scr_cell((unsigned char) (row + 2 + r),
                     (unsigned char) (col + 4 + i), c);
        }
    }
}
