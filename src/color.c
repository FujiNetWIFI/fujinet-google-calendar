/*
 * Event colour recovery.
 *
 * Google's API exposes colorId but not the colour it names, and the Calendar
 * adapter works around that by substituting the colour *name* into the
 * category column (GCAL.cpp's category_for(), precedence extendedProperties ->
 * colour name -> calendar name). So matching the category against the eleven
 * names Google actually uses recovers the colour; anything else is a calendar
 * name and takes the default.
 *
 * The match has to be whole-token and case-insensitive. A prefix match is not
 * enough in either direction: Grape and Graphite do not diverge until index 4,
 * Banana, Basil and Blueberry not until 1-2, and a calendar called
 * "Tomato Soup" must not come out as Tomato.
 *
 * Pure: no platform, no network.
 */

#include "gcal.h"

/* In COL_* order. */
static const char names[GC_NCOLORS][10] = {
    "LAVENDER", "SAGE", "GRAPE", "FLAMINGO", "BANANA", "TANGERINE",
    "PEACOCK", "GRAPHITE", "BLUEBERRY", "BASIL", "TOMATO"
};

/*
 * Eleven Google colours onto five chips: four players plus the missiles, which
 * GPRIOR's fifth-player bit combines into one shape drawing in COLPF3.
 * Grouped by hue family, so a wall of Blueberry and Peacock events still reads
 * as one calendar and a Tomato still stands out from it.
 */
static const unsigned char chips[GC_NCOLORS + 1] = {
    CHIP_BLUE,      /* LAVENDER  #7986CB */
    CHIP_GREEN,     /* SAGE      #33B679 */
    CHIP_BLUE,      /* GRAPE     #8E24AA */
    CHIP_RED,       /* FLAMINGO  #E67C73 */
    CHIP_YELLOW,    /* BANANA    #F6BF26 */
    CHIP_RED,       /* TANGERINE #F4511E */
    CHIP_BLUE,      /* PEACOCK   #039BE5 */
    CHIP_GRAPHITE,  /* GRAPHITE  #616161 */
    CHIP_BLUE,      /* BLUEBERRY #3F51B5 */
    CHIP_GREEN,     /* BASIL     #0B8043 */
    CHIP_RED,       /* TOMATO    #D50000 */
    CHIP_BLUE       /* COL_NONE -- no colorId, so it is the calendar's own */
};

static unsigned char upper(unsigned char c)
{
    return (c >= 'a' && c <= 'z') ? (unsigned char) (c - 32) : c;
}

/*
 * Is everything from `from` to the end of the category column blank?
 *
 * This is what makes the match whole-*column* rather than whole-word, and it
 * is the difference between a calendar called "Tomato" and one called "Tomato
 * Soup". Stopping at the first space would accept both -- which is what the
 * Intellivision original does, its comment notwithstanding.
 *
 * The column is GC_CATW wide and space padded, but the last event of a listing
 * is not padded out, so running off the end of the line counts as blank.
 */
static unsigned char rest_blank(const char *p, unsigned char len,
                                unsigned char from)
{
    unsigned char i;
    unsigned char end = (len < GC_CATW) ? len : GC_CATW;

    for (i = from; i < end; i++)
        if (p[i] != ' ')
            return 0;

    return 1;
}

/*
 * p points at the start of the category column and len is what remains of the
 * line from there.
 */
unsigned char color_match(const char *p, unsigned char len)
{
    unsigned char i, j;
    unsigned char n;

    for (i = 0; i < GC_NCOLORS; i++) {
        for (j = 0; j < 10; j++) {
            n = (unsigned char) names[i][j];

            if (n == 0) {
                if (rest_blank(p, len, j))
                    return i;
                break;
            }

            if (j >= len || upper((unsigned char) p[j]) != n)
                break;
        }
    }

    return COL_NONE;
}

unsigned char color_chip(unsigned char color)
{
    return chips[(color > GC_NCOLORS) ? COL_NONE : color];
}

/*
 * COL_NONE has no name because it is not a colour: it means the category was
 * something other than one of Google's eleven, which is to say a calendar
 * name. The event's own cat[] is what holds that.
 */
const char *color_name(unsigned char color)
{
    return (color < GC_NCOLORS) ? names[color] : "";
}
