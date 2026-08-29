/*
 * The Google Calendar mark.
 *
 * The Atari draws it with four players, one quadrant of the coloured ring
 * each, with the white page showing through as playfield. None of that
 * survives here: no sprites, and no colour to put in them.
 *
 * What is left is the part that actually identifies the thing -- a white page
 * with 31 on it -- and on a one-bit screen a white page is an inverse block
 * and black digits on it are inverse digits. The ring above it becomes a rule
 * along the top, which is what the eye reads as the binding anyway.
 */

#include "../gcal.h"
#include "platform.h"

/*
 * Small: the header mark, four cells by three rows.
 *
 * Row 0 of a view is the inverse title bar, so the mark's top row is painted
 * in *normal* video -- a dark notch in the white bar, standing in for the
 * coloured strip. The two rows below it sit on ordinary background, where
 * inverse is the white page.
 */
void logo_small(unsigned char row, unsigned char col)
{
    scr_field(row, col, "", LOGO_SMALL_COLS, 0);
    scr_field((unsigned char) (row + 1), col, " 31 ", LOGO_SMALL_COLS, 1);
    scr_field((unsigned char) (row + 2), col, "", LOGO_SMALL_COLS, 1);
}

/*
 * Large: the splash, busy and error screens, fourteen by six.
 *
 * These have no title bar, so the strip has to draw itself -- a row of rules
 * in normal video above a white page.
 */
void logo_large(unsigned char row, unsigned char col)
{
    unsigned char i;

    for (i = 0; i < LOGO_LARGE_COLS; i++)
        scr_field(row, (unsigned char) (col + i), MT_RULE, 1, 0);

    for (i = 1; i < LOGO_LARGE_ROWS; i++)
        scr_field((unsigned char) (row + i), col, "", LOGO_LARGE_COLS, 1);

    /* Centred in the page, which is rows 1..5 of the six. */
    scr_field((unsigned char) (row + 3),
              (unsigned char) (col + (LOGO_LARGE_COLS - 2) / 2), "31", 2, 1);
}
