/*
 * The machine: start-up, frame timing, the bus brackets.
 *
 * The frame counter is the NMI's (crt0.s), 32 bits, so it runs on through
 * every blocking bus call -- the mailbox is read by polling, not by masking
 * interrupts -- and the wall clock between fetches is exact.
 */

#include "../gcal.h"
#include "nes.h"

unsigned char fps_detect(void);

static unsigned char fps = 60;

void plat_init(void)
{
    fps = fps_detect();         /* first: the display is still off */
    scr_init();
    in_init();
    snd_init();
}

/*
 * There is nothing to return to on a cartridge, so Quit is a reset: the
 * reset vector reboots the cartridge's own image, and the FujiNet's CONFIG
 * is a power-cycle (or the cart's own boot menu) away. A soft reset rather
 * than a hang keeps "Quit" honest -- it does leave the program.
 */
void plat_shutdown(void)
{
    asm("jmp ($FFFC)");
}

void plat_vsync(void)
{
    unsigned char f = frames[0];

    while (frames[0] == f)
        ;
    in_poll();
}

unsigned long plat_ticks(void)
{
    unsigned long a, b;

    do {
        a = *(volatile unsigned long *) frames;
        b = *(volatile unsigned long *) frames;
    } while (a != b);
    return a;
}

unsigned char plat_fps(void)
{
    return fps;
}

/*
 * Every bus call blocks inside fujinet-lib until the cartridge answers, so
 * there is nothing for these to protect -- the NMI keeps drawing and the
 * spinner keeps turning underneath. What they do is keep input honest: a
 * button pressed while the program could not look is not a command for
 * whatever screen comes up next.
 */
void plat_net_begin(void)
{
    net_busy = 1;
}

void plat_net_end(void)
{
    net_busy = 0;
    in_flush();
}
