/*
 * The wall clock.
 *
 * Fetched from the FujiNet at boot and re-fetched every half hour; between
 * those it runs off the OS frame counter. Boot fails terminally without it,
 * because every device spec this client builds names a date and there is no
 * sensible fallback for "today".
 *
 * The [General] timezone setting drives both this clock and the window the
 * GCAL adapter resolves events in, which is why the settings screen shows the
 * timezone: if it is unset, or set to an IANA name the adapter's POSIX parser
 * rejects, both silently become UTC and the day view looks empty after
 * teatime.
 */

#include <fujinet-clock.h>

#include "gcal.h"

#define RESYNC_MINUTES  30

static unsigned long last;      /* RTCLOK when the current second started */
static unsigned char since;     /* minutes since the last successful fetch */
static unsigned char fps;

unsigned char clk_fetch(void)
{
    clk_ok = 0;

    if (!fps)
        fps = plat_fps();

#ifdef GC_FAKE_DATA
    /* The same day the canned listings are dated, so "today" lines up with
       the anchor and the alarm path is reachable in a headless run. */
    clk_y = 2026; clk_mo = 8; clk_d = 28;
    clk_h = 14; clk_mi = 32; clk_s = 0;
    last = plat_ticks();
    since = 0;
    clk_ok = 1;
    return 1;
#else
    {
    unsigned char buf[8];

    plat_net_begin();
    if (clock_get_time(buf, SIMPLE_BINARY) != FN_ERR_OK) {
        plat_net_end();
        return 0;
    }
    plat_net_end();

    clk_y  = (unsigned int) buf[0] * 100 + buf[1];
    clk_mo = buf[2];
    clk_d  = buf[3];
    clk_h  = buf[4];
    clk_mi = buf[5];
    clk_s  = buf[6];

    /* A NAK body or an unregistered clock device reads back as plausible
       bytes; this is the cheapest thing that tells the two apart. */
    if (clk_mo < 1 || clk_mo > 12 || clk_d < 1 || clk_d > 31 ||
        clk_h > 23 || clk_mi > 59 || clk_y < 2000 || clk_y > 2199)
        return 0;

    last = plat_ticks();
    since = 0;
    clk_ok = 1;
    return 1;
    }
#endif
}

unsigned char clk_get_tz(char *dst, unsigned char dstsize)
{
    unsigned char i;
    unsigned char ok;

    /* clock_get_tz reads a length byte and copies exactly that many bytes --
       it does not terminate what it writes. */
    for (i = 0; i < dstsize; i++)
        dst[i] = '\0';

#ifdef GC_FAKE_DATA
    dst[0] = 'C'; dst[1] = 'S'; dst[2] = 'T'; dst[3] = '6';
    dst[4] = 'C'; dst[5] = 'D'; dst[6] = 'T';
    ok = 1;
#elif defined(GC_NO_CLOCK_TZ)
    /*
     * fujinet-lib declares clock_get_tz for every platform but only builds it
     * for some: the CoCo archive carries fn_clock/clock_get_time.o and nothing
     * else, so calling it is an undefined symbol at link.
     *
     * A backend that cannot ask must not then print "(unset)" -- the timezone
     * may be perfectly set and we simply have no way to read it back. Show the
     * clock's own reading instead, which is the observable consequence of the
     * same [General] setting and the actual symptom anyone would check. See
     * src/coco/ui.c.
     */
    (void) dst;
    ok = 0;
#else
    plat_net_begin();
    ok = (clock_get_tz(dst) == FN_ERR_OK);
    plat_net_end();
#endif

    dst[dstsize - 1] = '\0';
    return ok;
}

/*
 * Returns 1 when the displayed minute changed, which is the only thing worth
 * repainting and also the granularity the alarm scan works at.
 */
unsigned char clk_tick(void)
{
    unsigned long now;
    unsigned int  secs;
    unsigned char bumped = 0;

    if (!clk_ok)
        return 0;

    now = plat_ticks();
    if (now < last) {           /* RTCLOK wrapped after ~77 hours */
        last = now;
        return 0;
    }

    secs = (unsigned int) ((now - last) / fps);
    if (secs == 0)
        return 0;

    /* Away long enough that stepping second by second is silly -- and long
       enough that the FujiNet's own clock is the better answer anyway. */
    if (secs > 3600) {
        since = RESYNC_MINUTES;
        return 1;
    }

    last += (unsigned long) secs * fps;

    while (secs--) {
        if (++clk_s < 60)
            continue;
        clk_s = 0;
        bumped = 1;

        if (since < 255)
            since++;

        if (++clk_mi < 60)
            continue;
        clk_mi = 0;

        if (++clk_h < 24)
            continue;
        clk_h = 0;
        date_addday(&clk_y, &clk_mo, &clk_d);
    }

    return bumped;
}

unsigned char clk_due_resync(void)
{
    return (unsigned char) (since >= RESYNC_MINUTES);
}

void clk_today(void)
{
    cur_y = clk_y;
    cur_mo = clk_mo;
    cur_d = clk_d;
}

unsigned char clk_is_today(unsigned int y, unsigned char mo, unsigned char d)
{
    return (unsigned char) (clk_ok && y == clk_y && mo == clk_mo && d == clk_d);
}
