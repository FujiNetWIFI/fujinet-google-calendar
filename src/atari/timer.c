/*
 * Frame timing.
 *
 * Reading RTCLOK rather than counting our own frames is what keeps the clock
 * honest across the blocking screens. The event, picker and settings screens
 * sit inside KEYBDV and never come back to tick anything, but the OS vertical
 * blank goes on incrementing this the whole time, so the elapsed count is
 * still right when the view screen resumes.
 */

#include <atari.h>

#include "../gcal.h"
#include "platform.h"

#define RTCLOK  ((volatile unsigned char *) 0x12)
#define PAL     ((volatile unsigned char *) 0xD014)

void plat_vsync(void)
{
    waitvsync();
}

unsigned long plat_ticks(void)
{
    unsigned long a, b;

    /* The vertical blank can carry between the byte reads, so take it twice
       and only trust a pair that agrees. */
    do {
        a = ((unsigned long) RTCLOK[0] << 16) |
            ((unsigned long) RTCLOK[1] << 8) | RTCLOK[2];
        b = ((unsigned long) RTCLOK[0] << 16) |
            ((unsigned long) RTCLOK[1] << 8) | RTCLOK[2];
    } while (a != b);

    return a;
}

/*
 * PAL reads $01 from GTIA's PAL register and NTSC reads $0F. Getting this
 * backwards drifts the clock by 20%, which across a half-hour resync interval
 * is six minutes -- enough to fire every alarm in the wrong place.
 */
unsigned char plat_fps(void)
{
    return (unsigned char) ((*PAL & 0x0E) ? 60 : 50);
}
