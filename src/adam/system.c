/*
 * Bringing the machine up and putting it back.
 *
 * The order in plat_init() is load-bearing at both ends. smartkeys_set_mode()
 * has to come first because it calls vdp_set_mode(2), which rewrites every VDP
 * register and clears all of VRAM -- anything installed before it would be
 * wiped. The keyboard has to be armed last-ish because EOS reads it in the
 * background and the first read has to be outstanding before the first poll.
 */

#include <eos.h>
#include <smartkeys.h>
#include <video/tms99x8.h>

#include "../gcal.h"
#include "platform.h"

void timer_init(void);

void plat_init(void)
{
    /* GRAPHICS II, and the SmartKeys band's own palette. This also clears the
       screen, which is why scr_clear() below is about our twenty-one rows and
       not about starting from a known state. */
    smartkeys_set_mode();

    /* Installs a raster interrupt that runs eos_play_sound() every frame, so
       it goes in before anything that wants the machine quiet. */
    smartkeys_sound_init();

    timer_init();
    logo_init();

    eos_start_read_keyboard();

    /*
     * Google Calendar is black on white with a blue accent; the Adam comes up
     * black on cyan. The border is set to match the content ground rather than
     * the header band, because the header is three rows and the content is
     * seventeen -- a blue border would frame the wrong thing.
     */
    vdp_color(VDP_INK_BLACK, VDP_INK_WHITE, VDP_INK_WHITE);
    scr_clear();
}

/*
 * Quitting hands the machine back to SmartWriter, which is the Adam's idea of
 * where a program goes when it is finished. There is nothing else to return
 * to: this build is linked at $0000 in all-RAM mode, so the boot block that
 * loaded it is long gone.
 */
void plat_shutdown(void)
{
    scr_clear();
    logo_hide();
    sk_status("");
    eos_exit_to_smartwriter();
}

/*
 * Nothing to suppress. On the Atari these switch off display list interrupts,
 * which would otherwise steal cycles from a timing-critical SIO transfer.
 * AdamNet is a master/slave bus driven synchronously from inside EOS, and the
 * only interrupt running here is the VDP's NMI, which cannot be masked anyway.
 * They stay because they are the contract, and because they are where to put
 * the fix if a transfer ever does turn out to mind.
 */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
