/*
 * Frame timing, off the VDP's own 60 Hz interrupt.
 *
 * On a Coleco the VDP raises NMI once per frame, and z88dk keeps a chain of up
 * to eight handlers on it, so counting frames here costs nothing and does not
 * displace smartkeyslib's sound handler on the same chain.
 *
 * A counter maintained by an interrupt cannot be read in one instruction on a
 * Z80, so plat_ticks() reads it twice and retries when the two disagree. That
 * matters more here than the size of the window suggests: clock.c treats a
 * backwards step as a counter wrap and throws away everything accumulated
 * since, so a single torn read would silently discard however long the user
 * had been sitting on a screen.
 *
 * Thirty-two bits at 60 Hz is 828 days, which is long enough that the wrap
 * handling the CoCo backend needs has no counterpart here.
 *
 * What the count does not cover is an AdamNet transfer. EOS masks maskable
 * interrupts around one, but the VDP's is an NMI and cannot be masked, so the
 * frame count keeps rising through a fetch -- which is the one respect in
 * which this machine's clock is better behaved than the CoCo's or the Apple's.
 */

#include <intrinsic.h>
#include <interrupt.h>

#include "../gcal.h"
#include "platform.h"

static unsigned long frames;

static void frame_isr(void)
{
    M_PRESERVE_ALL;
    frames++;
    M_RESTORE_ALL;
}

void timer_init(void)
{
    add_raster_int(frame_isr);
}

unsigned long plat_ticks(void)
{
    unsigned long a, b;

    do {
        a = frames;
        b = frames;
    } while (a != b);

    return a;
}

void plat_vsync(void)
{
    unsigned long t = plat_ticks();

    while (plat_ticks() == t)
        ;
}

/*
 * The Adam is NTSC. Unlike the CoCo, which shipped in both and has no cheap
 * way to ask a 6847 which it is, there is no PAL variant to get wrong here.
 */
unsigned char plat_fps(void)
{
    return 60;
}
