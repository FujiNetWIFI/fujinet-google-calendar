/*
 * Frame timing.
 *
 * The Atari reads RTCLOK, which the OS vertical blank increments whatever the
 * program is doing. There is no such counter on an Apple II, so this is one we
 * keep ourselves, bumped by every plat_vsync().
 *
 * That is enough because plat_getkey() is a polling loop around plat_vsync()
 * rather than a blocking firmware call: the count goes on rising while the
 * event, picker and settings screens sit waiting for a key, which is the
 * property clk_tick() actually needs.
 *
 * What it does not cover is a SmartPort transfer. Nobody calls plat_vsync()
 * while fujinet-lib is talking to the device, so the clock loses the length of
 * every fetch -- a second or two each, a little more for a cold window open.
 * The half-hour resync in clock.c is what bounds the drift, and the wall clock
 * is redrawn from it each time.
 */

#include <apple2.h>

#include "../gcal.h"
#include "platform.h"

static unsigned long ticks;
static unsigned char fps;

void plat_vsync(void)
{
    waitvsync();
    ticks++;
}

unsigned long plat_ticks(void)
{
    return ticks;
}

/*
 * Getting this backwards drifts the clock by 20%, which across a half-hour
 * resync interval is six minutes -- enough to fire every alarm in the wrong
 * place. cc65 calibrates it against the machine at startup.
 */
unsigned char plat_fps(void)
{
    if (!fps)
        fps = (unsigned char) ((get_tv() == TV_PAL) ? 50 : 60);

    return fps;
}

/*
 * Nothing to suppress. On the Atari these switch off the display list
 * interrupts, which would otherwise steal cycles from a timing-critical SIO
 * transfer; this backend runs no interrupts of its own at all.
 */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
