/*
 * Keyboard mapping.
 *
 * The Atari's cursor keys need Ctrl held down, which is a lot to ask of
 * someone paging through a calendar, so the bare keycaps those arrows live on
 * are accepted too: - = + * read the same as up, down, left, right.
 *
 * Digits 1-4 switch view and 0 jumps to today, exactly as on the
 * Intellivision keypad -- which is why the header carries a tab strip showing
 * which digit is which.
 */

#include <atari.h>

#include "../gcal.h"
#include "platform.h"

unsigned char plat_key(void);           /* key.s */

#ifdef GC_FAKE_KEYS
/*
 * Scripted input for headless testing. Build with, for example,
 *   -DGC_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER"
 * and the program drives itself that far, then falls through to the real
 * blocking read -- which is where a debugger breakpoint on plat_key catches it
 * with the screen of interest already painted.
 */
static const unsigned char fake_keys[] = { GC_FAKE_KEYS };
static unsigned char fake_idx;
#endif

static unsigned char map(unsigned char c)
{
    switch (c) {
    case CH_CURS_UP:    case '-':   return K_UP;
    case CH_CURS_DOWN:  case '=':   return K_DOWN;
    case CH_CURS_LEFT:  case '+':   return K_LEFT;
    case CH_CURS_RIGHT: case '*':   return K_RIGHT;

    case CH_ENTER:                  return K_ENTER;
    case CH_ESC:                    return K_BACK;

    case '0':                       return K_TODAY;
    case '1':                       return K_VIEW1;
    case '2':                       return K_VIEW2;
    case '3':                       return K_VIEW3;
    case '4':                       return K_VIEW4;

    case 'r': case 'R': case CH_CLR: return K_REFRESH;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

unsigned char plat_getkey(void)
{
#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    return map(plat_key());
}

/*
 * The non-blocking read the view loop needs.
 *
 * The Gmail client blocks in KEYBDV throughout, which is fine for a mailbox.
 * A calendar cannot: the wall clock has to advance and alarms have to fire
 * while the user sits idle, and neither happens inside a blocking read. CH
 * ($02FC) is where the keyboard IRQ leaves the raw code, so testing it for
 * "nothing pending" costs one load and lets the loop run a frame at a time.
 * Every other screen keeps the blocking read.
 */
unsigned char plat_getkey_poll(void)
{
#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

#ifdef GC_FAKE_DATA
    /*
     * A headless run has to stop somewhere. Once the scripted keys are spent,
     * block in the real read: that is where tools/atari-shot.sh has its
     * breakpoint, and it catches the screen of interest already painted. A
     * polling loop would spin forever and the capture would time out with
     * nothing to show.
     *
     * The cost is that the alarm banner cannot be captured this way, since it
     * needs the loop to keep turning. It is exercised on real hardware.
     */
    return map(plat_key());
#else
    if (OS.ch == 0xFF)
        return K_NONE;

    return map(plat_key());
#endif
}

void plat_anykey(void)
{
    plat_key();
}
