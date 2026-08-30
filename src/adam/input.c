/*
 * Keyboard and SmartKeys.
 *
 * The Adam has six labelled keys above the keyboard whose captions are drawn
 * on the screen, which is a better answer to "how do I switch view" than any
 * of the other three backends could give: the Atari and the Apple spend a
 * header row on a tab strip, the CoCo spends its footer on "1234:VIEW". Here
 * the machine has a place to put it and the whole of rows 0-20 stays content.
 *
 * The core only knows K_* codes, so a SmartKey means whatever the screen
 * currently on display says it means. Legend and meaning are therefore set
 * together, by sk_bind(), and never separately -- a legend that has drifted
 * from its map is a key that lies about what it does.
 *
 * K_SKBANK is the one exception that never reaches main.c. The list screens
 * have more actions than six keys, so SmartKey VI flips them between two
 * legends; that flip happens here, inside the read, and returns K_NONE so the
 * core never sees a keypress at all.
 *
 * The digits and letters are kept as well, and match the other backends
 * exactly: 1-4 switch view, 0 is today, R refreshes, Q quits. A SmartKey is
 * discoverable and a letter is fast, and there is no reason to make anyone
 * choose.
 */

#include <eos.h>
#include <smartkeys.h>

#include "../gcal.h"
#include "platform.h"

/* EOS keyboard codes. eos_read_keyboard_special_keys.md is the reference; the
   arrows are a contiguous run in the order up, right, down, left. */
#define KEY_RETURN      0x0D
#define KEY_ESCAPE      0x1B
#define KEY_UNDO        0x91
#define KEY_CLEAR       0x96
#define KEY_UP          0xA0
#define KEY_RIGHT       0xA1
#define KEY_DOWN        0xA2
#define KEY_LEFT        0xA3

/* What the six keys currently mean. Zeroed rather than left stale, so a screen
   that binds fewer than six does not inherit the previous screen's. */
static unsigned char sk_key[6];

/* The set on display. smartkeys_display() clears and repaints the whole band,
   which is far too much work to do on every ui_view(), and nothing in this
   backend ever paints over rows 21-23 -- so if the pointer has not changed,
   the legend on screen is still right. */
static const struct sk_set *sk_cur;

#ifdef GC_FAKE_KEYS
/*
 * Scripted input for headless testing, the same mechanism the CoCo backend
 * carries. Build with -DGC_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER" and the program
 * drives itself that far, then falls through to the real blocking read.
 */
static const unsigned char fake_keys[] = { GC_FAKE_KEYS };
static unsigned char fake_idx;
#endif

/* ------------------------------------------------------------------ */
/* SmartKeys                                                           */
/* ------------------------------------------------------------------ */

void sk_bind(const struct sk_set *s)
{
    unsigned char i;

    for (i = 0; i < 6; i++)
        sk_key[i] = s->key[i];

    if (sk_cur == s)
        return;
    sk_cur = s;

    smartkeys_display(s->label[0], s->label[1], s->label[2],
                      s->label[3], s->label[4], s->label[5]);
}

/*
 * No keys, one line of yellow status -- what smartkeyslib does with a slot
 * whose label is NULL. This is the band's state on every screen that is
 * waiting rather than offering a choice.
 *
 * sk_cur is invalidated rather than tracked: the message changes even when the
 * set does not, so there is nothing to compare against.
 */
void sk_status(const char *msg)
{
    unsigned char i;

    for (i = 0; i < 6; i++)
        sk_key[i] = K_NONE;

    sk_cur = 0;
    smartkeys_display(0, 0, 0, 0, 0, 0);
    smartkeys_status(msg);
}

/* ------------------------------------------------------------------ */
/* Translation                                                         */
/* ------------------------------------------------------------------ */

static unsigned char map(unsigned char c)
{
    if (c >= SMARTKEY_I && c <= SMARTKEY_VI)
        return sk_key[c - SMARTKEY_I];

    switch (c) {
    case KEY_UP:                    return K_UP;
    case KEY_DOWN:                  return K_DOWN;
    case KEY_LEFT:                  return K_LEFT;
    case KEY_RIGHT:                 return K_RIGHT;

    case KEY_RETURN:                return K_ENTER;
    case KEY_ESCAPE:
    case KEY_UNDO:                  return K_BACK;

    case '0':                       return K_TODAY;
    case '1':                       return K_VIEW1;
    case '2':                       return K_VIEW2;
    case '3':                       return K_VIEW3;
    case '4':                       return K_VIEW4;

    case 'r': case 'R': case KEY_CLEAR: return K_REFRESH;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

/*
 * One raw key, or 0 if none is waiting.
 *
 * EOS reads the keyboard in the background: eos_start_read_keyboard() arms a
 * read and eos_end_read_keyboard() answers 0 or 1 until one completes, then
 * hands back the code. Every completed read has to be re-armed or the keyboard
 * goes quiet, which is why that call is here rather than at init only.
 */
static unsigned char raw(void)
{
    unsigned char c = eos_end_read_keyboard();

    if (c <= 1)
        return 0;

    eos_start_read_keyboard();
    return c;
}

/*
 * A SmartKey the backend handles itself. Returns 1 when the key has been
 * consumed and nothing should reach the core.
 */
static unsigned char consumed(unsigned char k)
{
    if (k != K_SKBANK)
        return 0;

    ui_bank_toggle();
    return 1;
}

/* ------------------------------------------------------------------ */
/* The plat_ contract                                                  */
/* ------------------------------------------------------------------ */

unsigned char plat_getkey(void)
{
    unsigned char c, k;

#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    for (;;) {
        c = raw();
        if (c) {
            k = map(c);
            if (!consumed(k) && k != K_NONE)
                return k;
        }
        plat_vsync();
    }
}

/*
 * The non-blocking read the view loop needs: the clock has to advance and
 * alarms have to fire while nobody is touching the keyboard, and neither
 * happens inside a blocking read. Every other screen blocks, which is cheaper
 * and still correct.
 */
unsigned char plat_getkey_poll(void)
{
#ifndef GC_FAKE_DATA
    unsigned char c, k;
#endif

#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

#ifdef GC_FAKE_DATA
    /*
     * A headless run has to stop somewhere. Once the scripted keys are spent,
     * block in the real read so the screen under test stays painted for the
     * capture; polling here would spin forever with nothing to show.
     */
    return plat_getkey();
#else
    c = raw();
    if (!c)
        return K_NONE;

    k = map(c);
    if (consumed(k))
        return K_NONE;

    return k;
#endif
}

/*
 * Any key at all, including the ones map() throws away -- this is the "press
 * any key" of the error and splash screens, and a user who presses SmartKey
 * III there means to continue.
 */
void plat_anykey(void)
{
#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys)) {
        fake_idx++;
        return;
    }
#endif

    while (!raw())
        plat_vsync();
}
