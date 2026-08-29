/*
 * Keyboard.
 *
 * The Atari needs Ctrl held down for its cursor keys, so that backend accepts
 * the bare keycaps too. An enhanced //e has real arrow keys and this one does
 * not need the workaround.
 *
 * Digits 1-4 switch view and 0 jumps to today, exactly as on the Intellivision
 * keypad -- which is why the header carries a tab strip showing which digit is
 * which.
 */

#include <apple2.h>
#include <peekpoke.h>

#include "../gcal.h"
#include "platform.h"

#define KBD         0xC000      /* bit 7 set while a key is waiting */
#define KBDSTRB     0xC010

#ifdef GC_FAKE_KEYS
/*
 * Scripted input for headless testing. Build with, for example,
 *   -DGC_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER"
 * and the program drives itself that far, then falls through to the real
 * blocking read -- which is where a capture catches it with the screen of
 * interest already painted.
 */
static const unsigned char fake_keys[] = { GC_FAKE_KEYS };
static unsigned char fake_idx;
#endif

/* The waiting key, stripped of the strobe bit, or 0 for nothing. */
static unsigned char rawkey(void)
{
    unsigned char c = PEEK(KBD);

    if (!(c & 0x80))
        return 0;

    POKE(KBDSTRB, 0);

    return (unsigned char) (c & 0x7F);
}

static unsigned char map(unsigned char c)
{
    switch (c) {
    case CH_CURS_UP:                return K_UP;
    case CH_CURS_DOWN:              return K_DOWN;
    case CH_CURS_LEFT:              return K_LEFT;
    case CH_CURS_RIGHT:             return K_RIGHT;

    case CH_ENTER:                  return K_ENTER;
    case CH_ESC:                    return K_BACK;

    case '0':                       return K_TODAY;
    case '1':                       return K_VIEW1;
    case '2':                       return K_VIEW2;
    case '3':                       return K_VIEW3;
    case '4':                       return K_VIEW4;

    case 'r': case 'R':             return K_REFRESH;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

/*
 * Blocking, but through plat_vsync() rather than a firmware read.
 *
 * That is the whole reason the clock survives a screen that blocks: there is
 * no RTCLOK here, so the frame counter only advances when somebody turns it,
 * and this is what turns it while the event, picker and settings screens wait.
 */
unsigned char plat_getkey(void)
{
    unsigned char c;

#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    for (;;) {
        c = rawkey();
        if (c)
            return map(c);
        plat_vsync();
    }
}

/*
 * The non-blocking read the view loop needs.
 *
 * The Gmail client blocks throughout, which is fine for a mailbox. A calendar
 * cannot: the wall clock has to advance and alarms have to fire while the user
 * sits idle, and neither happens inside a blocking read.
 */
unsigned char plat_getkey_poll(void)
{
#ifndef GC_FAKE_DATA
    unsigned char c;
#endif

#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

#ifdef GC_FAKE_DATA
    /*
     * A headless run has to stop somewhere. Once the scripted keys are spent,
     * block in the real read, which leaves the screen of interest painted for
     * the capture. Polling here would spin forever and the capture would time
     * out with nothing to show.
     *
     * The cost is that the alarm banner cannot be captured this way, since it
     * needs the loop to keep turning. It is exercised on real hardware.
     */
    return plat_getkey();
#else
    c = rawkey();
    if (!c)
        return K_NONE;

    return map(c);
#endif
}

void plat_anykey(void)
{
    while (!rawkey())
        plat_vsync();
}
