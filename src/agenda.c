/*
 * Not built into GCALED3, the CoCo 3's compose form: that program draws no
 * calendar and fetches no listing. Every object on the link line is included
 * whether it is referenced or not, so the only way to keep it out is for the
 * file to compile to nothing. GC_EDITOR is defined by that build alone.
 */
#ifndef GC_EDITOR

/*
 * The agenda display list.
 *
 * The agenda view interleaves date separators with events, so a display row
 * and an event index are no longer the same thing. Building the combined
 * sequence once, after a fetch, is what lets scrolling do random access into
 * it -- recomputing "which event is on display row 9" on every keypress would
 * mean walking the whole index each time.
 *
 * One byte per row: AGD_SEP marks a separator, the low seven bits hold the
 * event index. A separator carries the index of the *first* event of its
 * group, whose day and month supply the date to print.
 *
 * Pure: no platform, no network.
 */

#include "gcal.h"

void agenda_build(void)
{
    unsigned char i;
    unsigned char pd = 0;       /* 0 is never a real day of month, so the */
    unsigned char pm = 0;       /* first event always opens a group */
    struct event *e;

    gc_agd_count = 0;

    for (i = 0; i < gc_count; i++) {
        e = &gc_index[i];

        if (e->day != pd || e->mon != pm) {
            if (gc_agd_count >= AGD_MAX)
                return;
            gc_agd[gc_agd_count++] = (unsigned char) (AGD_SEP | i);
            pd = e->day;
            pm = e->mon;
        }

        if (gc_agd_count >= AGD_MAX)
            return;
        gc_agd[gc_agd_count++] = i;
    }
}

#endif /* !GC_EDITOR */
