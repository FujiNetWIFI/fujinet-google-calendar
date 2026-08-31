/*
 * The Google Calendar mark.
 *
 * A white page with "31" on it, ringed in the brand colours -- blue top
 * left, red top right, green bottom left, yellow bottom right, the quadrant
 * assignment every backend inherits from intv/gfx.bas. The ring is CP437
 * full-block cells, one stroke per colour, the gmail client's technique;
 * the page inside it is the one extension: 'W' cells are *painted* as page
 * rather than skipped, because on the black-and-white tables the page has
 * to be laid down as reverse video or the mark would be a ring around a
 * hole. Digits paint as themselves on the page.
 *
 * Blank cells are still not painted at all, so the small mark sits across
 * the app bar (row 0) and the status band (rows 1-2) without carrying
 * either's background around.
 *
 * On the two tables with no colour, the ring is intensity-white blocks
 * around the reverse-video page -- "an inverse block with digits inside
 * it", the Apple's one-bit rendering, arrived at from the other direction.
 *
 * Two sizes, the Apple's dimensions: the small one in the header, the large
 * one on the splash, busy and error screens.
 */

#include "../gcal.h"
#include "platform.h"

/*
 * 'B'lue, 'R'ed, 'Y'ellow, 'G'reen strokes; 'W' the page; a digit is
 * itself; space is not painted. The small mark has no room for a page --
 * the digits are its interior.
 */
static const char * const small_rows[LOGO_SMALL_ROWS] = {
    "BBRR",
    "B31Y",
    "GGYY",
};

static const char * const large_rows[LOGO_LARGE_ROWS] = {
    "BBBBBBBRRRRRRR",
    "BWWWWWWWWWWWWR",
    "BWWWWW31WWWWWR",
    "GWWWWWWWWWWWWY",
    "GWWWWWWWWWWWWY",
    "GGGGGGGYYYYYYY",
};

/* Black on light grey: the page, in every table. On colour it is the same
   byte the body text uses; on B&W and MDA it is reverse video, which is
   what makes the page read as a solid white card. */
#define PAGE_ATTR   0x70

static unsigned char stroke_attr(char c)
{
    unsigned char bg;

    if (!scr_color)
        return scr_attr_byte(A_EMPH);

    /* The page's background nibble under the stroke's own foreground. The
       block glyph hides the background anyway; carrying it keeps the cell
       honest if the glyph ever changes. */
    bg = (unsigned char) (scr_attr_byte(A_TEXT) & 0x70);

    switch (c) {
    case 'B':   return (unsigned char) (bg | 0x09);
    case 'R':   return (unsigned char) (bg | 0x0C);
    case 'Y':   return (unsigned char) (bg | 0x0E);
    case 'G':   return (unsigned char) (bg | 0x0A);
    }

    return scr_attr_byte(A_TEXT);
}

static void draw(const char * const *rows, unsigned char nrows,
                 unsigned char row, unsigned char col)
{
    unsigned char r, x;
    unsigned char c;
    const char   *s;

    for (r = 0; r < nrows; r++) {
        s = rows[r];
        for (x = 0; s[x]; x++) {
            c = (unsigned char) s[x];

            if (c == ' ')
                continue;

            if (c == 'W')
                scr_cell((unsigned char) (row + r), (unsigned char) (col + x),
                         ' ', PAGE_ATTR);
            else if (c >= '0' && c <= '9')
                scr_cell((unsigned char) (row + r), (unsigned char) (col + x),
                         c, PAGE_ATTR);
            else
                scr_cell((unsigned char) (row + r), (unsigned char) (col + x),
                         GL_BLOCK, stroke_attr((char) c));
        }
    }
}

void logo_small(unsigned char row, unsigned char col)
{
    draw(small_rows, LOGO_SMALL_ROWS, row, col);
}

void logo_large(unsigned char row, unsigned char col)
{
    draw(large_rows, LOGO_LARGE_ROWS, row, col);
}
