/*
 * Frame timing, off Color BASIC's 60 Hz TIMER at $0112.
 *
 * The counter is sixteen bits and wraps every 65536 frames -- about eighteen
 * minutes. clock.c treats a backwards step as a wrap and resets its baseline,
 * throwing away everything accumulated since, so handing it the raw word would
 * silently lose whatever time passed while the user sat on the settings screen
 * across a rollover. plat_ticks() extends it to a genuinely monotonic 32 bits
 * instead.
 *
 * That extension is only correct if somebody folds it at least once per wrap,
 * which is why plat_getkey() is a polling loop around plat_vsync() rather than
 * CMOC's waitkey(): plat_vsync() folds, so the count keeps rising while the
 * event, picker and settings screens wait for a key -- the property clk_tick()
 * actually needs, and the same reason the Apple II backend polls.
 *
 * What none of it covers is a DriveWire transfer. fujinet-lib's dwread jumps
 * through [$D93F], which masks interrupts for the duration, so TIMER itself
 * stops and the clock loses the length of every fetch. That is the Apple's
 * SmartPort situation exactly, and the half-hour resync in clock.c is what
 * bounds it.
 */

#include "../gcal.h"
#include "platform.h"

#define TIMER   (*(unsigned int *) 0x0112)

static unsigned int lo;         /* last TIMER value folded */
static unsigned int hi;         /* wraps so far */

unsigned long plat_ticks(void)
{
    unsigned int now = TIMER;

    if (now < lo)
        hi++;
    lo = now;

    return ((unsigned long) hi << 16) | now;
}

/*
 * CMOC accepts `volatile` and then ignores it, so this reads TIMER through the
 * macro on every pass rather than hoisting it into a register. The compiler
 * does no cross-statement elimination of pointer loads, which is what makes
 * that safe -- but it is the reason the loop is written this way and not with
 * a cached copy.
 */
void plat_vsync(void)
{
    unsigned int t = TIMER;

    while (TIMER == t)
        ;

    plat_ticks();               /* fold the wrap while we are here */
}

/*
 * A PAL CoCo drives the same interrupt off a 50 Hz field rate, and getting
 * this wrong drifts the clock by 20% -- six minutes across a half-hour resync,
 * enough to fire every alarm in the wrong place. There is no cheap way to ask
 * a 6847 which it is, so this is the NTSC answer and a PAL machine resyncs
 * more visibly.
 */
unsigned char plat_fps(void)
{
    return 60;
}

/*
 * Nothing to suppress. On the Atari these switch off the display list
 * interrupts, which would otherwise steal cycles from a timing-critical SIO
 * transfer; this backend runs no interrupts of its own, and fujinet-lib masks
 * them itself for the length of a DriveWire transfer.
 */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
