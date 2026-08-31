/*
 * Frame timing off the BIOS tick.
 *
 * The BDA dword at 0040:006C counts timer-tick interrupts, 18.2 per second,
 * from power-on -- the PC's RTCLOK. It advances in the background whatever
 * this program is doing, which is the property clock.c actually needs: a
 * blocking keyboard read parks the CPU inside INT 16h and the count keeps
 * moving, so the clock stays honest across every waiting screen with nobody
 * pumping a counter the way the Apple backend must.
 *
 * plat_fps() says 18, not 60. clock.c divides elapsed ticks by this to step
 * seconds, so the honest answer matters: calling it 60 would run the clock
 * at a third speed. The 0.2 left over is ~1% drift, absorbed by the
 * half-hour resync, and the wrap -- the BIOS zeroes the count at midnight,
 * read back as now < last -- is the same "wrapped" case clk_tick() already
 * treats as costing at most one second.
 *
 * The consequence for alarm.c's frame constants, tuned at 60: the banner
 * flashes and the chime steps at a third of the Atari's rate, ~13 seconds
 * of banner and ~1.3 of chime. Acceptable for an alarm, and preferred over
 * a second timing path: a vertical-retrace wait on 0x3DA could give colour
 * adapters a real 60, but the MDA has no such bit and one clock everyone
 * shares is easier to trust.
 */

#include <dos.h>

#include "../gcal.h"
#include "platform.h"

/*
 * INT 08 can fire between two halves of a 16-bit read of a 32-bit value, so
 * read until two reads agree -- the classic torn-read guard, two
 * instructions cheaper than cli/sti and safe under any interrupt handler.
 */
static unsigned long peek_ticks(void)
{
    unsigned long far *p = MK_FP(0x0040, 0x006C);
    unsigned long a, b;

    do {
        a = *p;
        b = *p;
    } while (a != b);

    return a;
}

/* One frame here is one BIOS tick: the view loop turns 18 times a second,
   which bounds the poll rate without a busy spin. */
void plat_vsync(void)
{
    unsigned long t = peek_ticks();

    while (peek_ticks() == t)
        ;
}

unsigned long plat_ticks(void)
{
    return peek_ticks();
}

unsigned char plat_fps(void)
{
    return 18;
}
