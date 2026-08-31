/*
 * Keyboard.
 *
 * INT 16h AH=00 blocks until a key and hands back ASCII in AL with the scan
 * code in AH. The arrows and the grey navigation keys have no ASCII: the
 * 83-key board returns AL=0 for them, the 101-key returns AL=E0h, and both
 * put the scan code in AH -- so anything with an empty AL is mapped by scan
 * code and everything else by character.
 *
 * PgUp and PgDn alias K_LEFT/K_RIGHT. They are free on this keyboard, they
 * are what a DOS user's fingers already do for previous/next, and on the
 * PCjr -- where the arrows themselves need the Fn shift -- they are no
 * worse than anything else.
 *
 * The poll the view loop needs comes from _bios_keybrd(_KEYBRD_READY) --
 * INT 16h AH=01 -- rather than a hand-rolled int86: that service reports
 * "nothing waiting" in ZF, which Watcom's union REGS cannot see, and the
 * RTL wrapper folds it into a zero return. The rate is bounded by
 * plat_vsync() in the loop itself, so the poll runs 18 times a second, not
 * as fast as an 8088 can re-ask the BIOS.
 */

#include <bios.h>
#include <dos.h>
#ifdef GC_SHOT
#include <stdlib.h>
#endif

#include "../gcal.h"
#include "platform.h"

/*
 * Scripted input for headless testing. Build with, for example,
 *   -DGC_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER"
 * and the program drives itself that far, then falls through to the real
 * blocking read -- which is where a capture catches it with the screen of
 * interest already painted.
 *
 * The second spelling is this compiler's own problem: wcc cannot carry a
 * comma through -D -- everything after it is parsed as another file to
 * compile, E1139 -- so a *sequence* has to arrive without one.
 * GC_FAKE_KEYS_STR is the K_* codes as a string of letters valued
 * 'a' + code ('b' is K_UP, 'n' is K_VIEW4). Letters rather than the gmail
 * client's digits because this client has thirteen codes: '0' + 11 is ';'
 * and '0' + 12 is '<', and either loose in the compile line the flag rides
 * through would hand the shell a command separator. tools/msdos-shot.sh
 * does the translation, so nobody types the letters by hand.
 */
#ifdef GC_FAKE_KEYS
static const unsigned char fake_keys[] = { GC_FAKE_KEYS };
static unsigned char fake_idx;
#elif defined(GC_FAKE_KEYS_STR)
static const char fake_keys[] = GC_FAKE_KEYS_STR;
static unsigned char fake_idx;
#endif

#if defined(GC_FAKE_KEYS) || defined(GC_FAKE_KEYS_STR)
static unsigned char fake_next(void)
{
#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#else
    if (fake_keys[fake_idx])
        return (unsigned char) (fake_keys[fake_idx++] - 'a');
#endif
    return K_NONE;
}
#endif

#define SC_UP       0x48
#define SC_DOWN     0x50
#define SC_LEFT     0x4B
#define SC_RIGHT    0x4D
#define SC_PGUP     0x49
#define SC_PGDN     0x51

/* Blocking BIOS read: AL the character, AH the scan code. */
static unsigned int rawkey(void)
{
    union REGS r;

    r.h.ah = 0x00;
    int86(0x16, &r, &r);

    return r.x.ax;
}

static unsigned char map(unsigned int ax)
{
    unsigned char al = (unsigned char) (ax & 0xFF);

    if (al == 0x00 || al == 0xE0) {
        switch ((unsigned char) (ax >> 8)) {
        case SC_UP:                 return K_UP;
        case SC_DOWN:               return K_DOWN;
        case SC_LEFT:               return K_LEFT;
        case SC_RIGHT:              return K_RIGHT;
        case SC_PGUP:               return K_LEFT;
        case SC_PGDN:               return K_RIGHT;
        }
        return K_NONE;
    }

    switch (al) {
    case 0x0D:                      return K_ENTER;
    case 0x1B:                      return K_BACK;

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
 * Blocking. Unlike the Apple there is no counter to pump while waiting --
 * the BIOS tick advances under INT 16h on its own (see timer.c) -- so a
 * plain blocking read keeps the clock honest across the event, picker and
 * settings screens.
 */
unsigned char plat_getkey(void)
{
#if defined(GC_FAKE_KEYS) || defined(GC_FAKE_KEYS_STR)
    {
        unsigned char k = fake_next();
        if (k != K_NONE)
            return k;
    }
#endif

#ifdef GC_SHOT
    /* A capture run ends where a person would start: the screen of interest
       is painted and the program is about to block. Dump it and leave. */
    scr_snapshot();
    exit(0);
#endif

    for (;;) {
        unsigned char k = map(rawkey());
        if (k != K_NONE)
            return k;
    }
}

/*
 * The non-blocking read the view loop needs: the wall clock has to advance
 * and alarms have to fire while the user sits idle, and neither happens
 * inside a blocking read.
 */
unsigned char plat_getkey_poll(void)
{
#if defined(GC_FAKE_KEYS) || defined(GC_FAKE_KEYS_STR)
    {
        unsigned char k = fake_next();
        if (k != K_NONE)
            return k;
    }
#endif

#ifdef GC_FAKE_DATA
    /*
     * A headless run has to stop somewhere. Once the scripted keys are
     * spent, block in the real read, which leaves the screen of interest
     * painted for the capture. Polling here would spin forever and the
     * capture would time out with nothing to show.
     *
     * The cost is that the alarm banner cannot be captured this way, since
     * it needs the loop to keep turning. It is exercised on real hardware.
     */
    return plat_getkey();
#else
    if (_bios_keybrd(_KEYBRD_READY) == 0)
        return K_NONE;

    return map(rawkey());
#endif
}

void plat_anykey(void)
{
#ifdef GC_SHOT
    scr_snapshot();
    exit(0);
#endif
    rawkey();
}
