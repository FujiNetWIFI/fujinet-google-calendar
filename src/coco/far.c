/*
 * Far storage in the CoCo 3's second 64K.
 *
 * The 6809 sees 64K at a time and this build fills nearly all of it, but a
 * 128K machine has another sixteen 8K blocks and BASIC does not use all of
 * them. cocoroms/coco3.asm names the second bank's allocation:
 *
 *   $30-$33  hi-res graphics screen        $35  hi-res command stack
 *   $34      GET/PUT buffer                $36  the 80-column text screen
 *   $37      "UNUSED BY BASIC"
 *
 * $37 is the one this takes, and it is banked into the $C000 window -- the
 * same window screen.c borrows for the text page. That window costs nothing,
 * because $C000-$DFFF is not program space: it is where Disk BASIC and
 * HDB-DOS live, so the program never had it to begin with.
 *
 * Which is also the hazard, and the reason for the shape of this file.
 * fujinet-lib's dwread() and dwwrite() reach the FujiNet by jumping through
 * the vectors at [$D93F] and [$D941] -- inside that window. Every network
 * read, every appkey, every clock query goes through them. If the window were
 * open across one of those calls the jump would land in whatever was banked
 * there instead of in HDB-DOS.
 *
 * So the window is never open across a call. Both routines below mask
 * interrupts, bank, copy with an inline loop, unbank and unmask, and they call
 * nothing in between -- not even memcpy. By the time either returns, HDB-DOS
 * is back at $C000. Nothing else in the program has to know this exists or
 * observe any discipline about it, which matters because the FujiNet calls are
 * spread across net.c, clock.c, settings.c, model.c and ui3.c.
 */

#ifdef COCO3

#include <coco.h>

#include "../gcal.h"
#include "platform.h"

#define FAR_BLOCK   0x37
#define FAR_WIN     ((unsigned char *) 0xC000)

void far_get(void *dst, unsigned int off, unsigned int len)
{
    unsigned char *d = (unsigned char *) dst;
    unsigned char *s;
    unsigned char  saved;

    asm { orcc #$50 }
    saved = *((unsigned char *) 0xFFA6);
    *((unsigned char *) 0xFFA6) = FAR_BLOCK;

    s = FAR_WIN + off;
    while (len--)
        *d++ = *s++;

    *((unsigned char *) 0xFFA6) = saved;
    asm { andcc #$AF }
}

void far_put(unsigned int off, const void *src, unsigned int len)
{
    const unsigned char *s = (const unsigned char *) src;
    unsigned char       *d;
    unsigned char        saved;

    asm { orcc #$50 }
    saved = *((unsigned char *) 0xFFA6);
    *((unsigned char *) 0xFFA6) = FAR_BLOCK;

    d = FAR_WIN + off;
    while (len--)
        *d++ = *s++;

    *((unsigned char *) 0xFFA6) = saved;
    asm { andcc #$AF }
}

/* ------------------------------------------------------------------ */
/* Event titles                                                        */
/* ------------------------------------------------------------------ */

static char titlebuf[TITLE_LEN];

const char *ev_title(unsigned char ev)
{
    far_get(titlebuf, (unsigned int) (FAR_TITLE + ev * TITLE_LEN),
            TITLE_LEN);
    titlebuf[TITLE_LEN - 1] = '\0';
    return titlebuf;
}

void ev_set_title(unsigned char ev, const char *src)
{
    far_put((unsigned int) (FAR_TITLE + ev * TITLE_LEN), src, TITLE_LEN);
}

#endif /* COCO3 */
