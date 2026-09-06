/*
 * Keyboard.
 *
 * There is no ESC key on a Color Computer, so BREAK is the back-out key -- the
 * same choice fujinet-config and fujinet-fujirkle both made, and what the
 * footer spells as BRK. CLEAR joins R as refresh, mirroring the Atari's use of
 * its own CLEAR.
 *
 * Digits 1-4 switch view and 0 jumps to today, exactly as on the Intellivision
 * keypad. The Atari and the Apple advertise that with a tab strip in the
 * header; sixteen rows will not pay for one, so the footer carries "1234:VIEW"
 * instead and the settings screen legends the rest.
 */

#include <coco.h>

#include "../gcal.h"
#include "platform.h"

#define KEY_LEFT    0x08
#define KEY_RIGHT   0x09
#define KEY_DOWN    0x0A
#define KEY_UP      0x5E
#define KEY_ENTER   0x0D
#define KEY_BREAK   0x03
#define KEY_CLEAR   0x0C

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

static unsigned char map(unsigned char c)
{
    switch (c) {
    case KEY_UP:                    return K_UP;
    case KEY_DOWN:                  return K_DOWN;
    case KEY_LEFT:                  return K_LEFT;
    case KEY_RIGHT:                 return K_RIGHT;

    case KEY_ENTER:                 return K_ENTER;
    case KEY_BREAK:                 return K_BACK;

    case '0':                       return K_TODAY;
    case '1':                       return K_VIEW1;
    case '2':                       return K_VIEW2;
    case '3':                       return K_VIEW3;
    case '4':                       return K_VIEW4;

    case 'r': case 'R': case KEY_CLEAR: return K_REFRESH;
    case 'q': case 'Q':             return K_QUIT;

    case 'n': case 'N':             return K_NEW;
    case 'e': case 'E':             return K_EDIT;
    }

    return K_NONE;
}

/*
 * The blocking wait, kept in a function of its own on purpose.
 *
 * It is where tools/coco-shot.sh sets its breakpoint: the scripted keys are
 * consumed before anything reaches here, so the first entry is exactly the
 * moment the script runs out and the screen under test is painted.
 *
 * It polls inkey() around plat_vsync() rather than calling waitkey(), because
 * plat_vsync() is what folds the 16-bit TIMER into the 32-bit count
 * plat_ticks() returns. Blocking in the ROM instead would stop the fold and
 * lose eighteen minutes of wall clock at every wrap.
 */
unsigned char plat_key_block(void)
{
    unsigned char c;

    for (;;) {
        c = inkey();
        if (c)
            return c;
        plat_vsync();
    }
}

unsigned char plat_getkey(void)
{
#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    return map(plat_key_block());
}

/*
 * The non-blocking read the view loop needs.
 *
 * The view loop cannot block: the wall clock has to advance and alarms have to
 * fire while the user sits idle, and neither happens inside a blocking read.
 * Every other screen does block, which is cheaper and still correct.
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
    c = inkey();
    if (!c)
        return K_NONE;

    return map(c);
#endif
}

void plat_anykey(void)
{
    plat_key_block();
}

/*
 * The compose form's read. The left arrow is E_BS rather than a cursor
 * move, because on this keyboard the left arrow *is* the erase key --
 * BASIC's own convention -- which leaves the editor append-and-backspace:
 * there is no key left to walk the cursor with, and a 24-cell window does
 * not miss it. inkey() yields the 6847's uppercase-only set, which is also
 * all the screen could echo.
 */
unsigned char plat_getch(void)
{
#ifdef GC_FAKE_KEYS
    /* Spent verbatim: the scripted K_* codes' low values land on E_* inside
       the form, which is what lets a capture drive the field cursor. */
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    for (;;) {
        unsigned char c = plat_key_block();

        switch (c) {
        case KEY_UP:    return E_UP;
        case KEY_DOWN:  return E_DOWN;
        case KEY_ENTER: return E_ENTER;
        case KEY_BREAK: return E_DONE;
        case KEY_LEFT:  return E_BS;
        }

        if (c >= 0x20 && c < 0x7F) {
#ifdef COCO3
            /*
             * inkey() hands back the ROM's uppercase-only set, which is all
             * the VDG page can echo -- but the GIME's character generator has
             * lowercase, and a calendar full of SHOUTED event titles is not
             * what anyone wants on the wire.
             *
             * So invert the machine's convention the way fujinet-config's
             * src/coco/input.c does: a letter arrives lowercase unless SHIFT
             * is actually down. isKeyPressed() reads the physical row, which
             * is the only way to know -- the ROM has already folded the case
             * out of the character by the time we see it.
             */
            if (c >= 'A' && c <= 'Z' &&
                !isKeyPressed(KEY_PROBE_SHIFT, KEY_BIT_SHIFT))
                c = (unsigned char) (c + 0x20);
#endif
            return c;
        }
    }
}
