/*
 * The pad, the Famicom keyboards, and what they mean on each screen.
 *
 * The controller in port 1 is the whole of the input on a stock NES:
 *
 *   views    pad: move    A: open    B: today    SELECT: next view
 *            START: the menu (new, edit, refresh, the views, calendars,
 *            settings, help, quit)
 *   event    pad: scroll / page    A: edit    B: back
 *   lists    pad: move    A: choose    B: back
 *   form     osk.c: the on-screen keyboard
 *
 * A Family BASIC keyboard or a Subor in the expansion port works alongside
 * it (fujinet-lib's fuji_nes_kbd_*): the arrows, RETURN, ESC/STOP for back,
 * 1-4 for the views, 0 today, R refresh, N new, E edit, S settings, Q quit,
 * M the menu -- and on the Family BASIC keyboard F1-F4 the views, F5
 * refresh, F6 today, F7 new, F8 the menu. The keyboard library reports no
 * function keys, so those are read off the raw matrix here.
 *
 * The pad is read by the NMI and its presses latched (crt0.s), so none is
 * lost while the program is busy drawing or waiting on the FujiNet. Reading
 * the keyboard writes $4016, which is also the pad's strobe, so while it is
 * read (once a frame, in_poll from plat_vsync) kb_busy keeps the NMI off it.
 *
 * Held directions repeat: after 20 frames, then every 6, then -- after a
 * second of holding -- every 3, which is what makes the date and time
 * pickers' wheels quick to spin.
 */

#include <string.h>

#include <fujinet-nes.h>

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

#define REPEAT_FIRST    20
#define REPEAT_NEXT     6
#define REPEAT_FAST     3
#define FAST_AFTER      60

uint8_t in_mode;
uint8_t pad, pad_new;
uint8_t kb_type;
char    kb_ch;
uint8_t kb_fn;

static uint8_t hold, held;
static uint8_t fn_prev;

#define INQ 48              /* a picked date is 31 keystrokes (form_ui.c) */
static uint8_t inq[INQ];
static uint8_t inq_head, inq_tail;

#ifdef GC_FAKE_KEYS
/* Scripted input for headless captures, as on the other backends: build
   with -DGC_FAKE_KEYS="K_VIEW3,K_RIGHT" and the program drives itself that
   far, then goes over to the real pad. */
static const unsigned char fake_keys[] = { GC_FAKE_KEYS };
static unsigned char fake_idx;
#endif

/*
 * F1-F8 and STOP off the Family BASIC keyboard's matrix: row r, half h,
 * bit b is key r * 8 + h * 4 + b, and F8 is key 0, F7 key 8 ... F1 key 56;
 * STOP is key 7. fujinet-lib's tables follow MesenCE's matrix, as this does.
 */
static uint8_t read_fn(void)
{
    uint8_t rows[26];
    uint8_t n;

    kb_busy = 1;
    fuji_nes_kbd_scan(rows);
    kb_busy = 0;
    if (rows[1] & 0x08)
        return 9;                               /* STOP */
    for (n = 1; n <= 8; n++)
        if (rows[(8 - n) * 2] & 0x01)
            return n;
    return 0;
}

void in_init(void)
{
    kb_busy = 1;
    kb_type = fuji_nes_kbd_detect();
    kb_busy = 0;
}

/* What the NMI has latched since last time (crt0.s pad_nmi). A take waits
   for the NMI to have cleared the one before, or the same press would be
   taken twice. */
static uint8_t pad_take(void)
{
    uint8_t d;

    if (pad_ack)
        return 0;
    d = pad_down;
    pad_ack = d;
    return d;
}

void in_poll(void)
{
    uint8_t dir, down, f;

    pad = pad_now;
    down = pad_take();

    dir = (uint8_t) (pad & (J_UP | J_DOWN | J_LEFT | J_RIGHT));
    if (dir && !(down & dir)) {
        if (held < 255)
            held++;
        if (--hold == 0) {
            hold = (held > FAST_AFTER) ? REPEAT_FAST : REPEAT_NEXT;
            down |= dir;
        }
    } else if (dir) {
        hold = REPEAT_FIRST;
        held = 0;
    }
    pad_new |= down;

    if (kb_type != FUJI_NES_KBD_NONE) {
        char c;

        kb_busy = 1;
        c = fuji_nes_kbd_getc();
        kb_busy = 0;

        if (c)
            kb_ch = c;
        if (kb_type == FUJI_NES_KBD_FAMILY_BASIC) {
            f = read_fn();
            if (f && f != fn_prev)
                kb_fn = f;
            fn_prev = f;
        }
    }
}

void in_flush(void)
{
    pad_take();
    pad_new = 0;
    kb_ch = 0;
    kb_fn = 0;
}

void in_push(uint8_t k)
{
    uint8_t n = (uint8_t) ((inq_head + 1) % INQ);

    if (n != inq_tail) {
        inq[inq_head] = k;
        inq_head = n;
    }
}

uint8_t in_pop(void)
{
    uint8_t k;

#ifdef GC_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif
    if (inq_head == inq_tail)
        return K_NONE;
    k = inq[inq_tail];
    inq_tail = (uint8_t) ((inq_tail + 1) % INQ);
    return k;
}

/* ------------------------------------------------------------------ */
/* The meaning of a press                                              */
/* ------------------------------------------------------------------ */

static uint8_t take_bit(uint8_t mask)
{
    if (pad_new & mask) {
        pad_new &= (uint8_t) ~mask;
        return 1;
    }
    return 0;
}

static uint8_t dir_key(void)
{
    if (take_bit(J_UP))    return K_UP;
    if (take_bit(J_DOWN))  return K_DOWN;
    if (take_bit(J_LEFT))  return K_LEFT;
    if (take_bit(J_RIGHT)) return K_RIGHT;
    return K_NONE;
}

/* The keyboard, the same on every screen. */
static uint8_t kb_key(void)
{
    char c = kb_ch;
    uint8_t f = kb_fn;

    kb_ch = 0;
    kb_fn = 0;

    switch (f) {
    case 1: case 2: case 3: case 4:
        return (uint8_t) (K_VIEW1 + f - 1);
    case 5: return K_REFRESH;
    case 6: return K_TODAY;
    case 7: return K_NEW;
    case 8: return (in_mode == M_VIEW) ? menu_run() : K_NONE;
    case 9: return K_BACK;                      /* STOP */
    }

    switch (c) {
    case 0:                     return K_NONE;
    case FUJI_NES_KEY_UP:       return K_UP;
    case FUJI_NES_KEY_DOWN:     return K_DOWN;
    case FUJI_NES_KEY_LEFT:     return K_LEFT;
    case FUJI_NES_KEY_RIGHT:    return K_RIGHT;
    case FUJI_NES_KEY_ENTER:    return K_ENTER;
    case FUJI_NES_KEY_ESC:      return K_BACK;
    case FUJI_NES_KEY_HOME:     return K_TODAY;
    case '1': case '2': case '3': case '4':
        return (uint8_t) (K_VIEW1 + c - '1');
    case '0':                   return K_TODAY;
    case 'r': case 'R':         return K_REFRESH;
    case 'n': case 'N':         return K_NEW;
    case 'e': case 'E':         return K_EDIT;
    case 's': case 'S':         return K_BACK;
    case 'q': case 'Q':         return K_QUIT;
    case 'm': case 'M':
        return (in_mode == M_VIEW) ? menu_run() : K_NONE;
    }
    return K_NONE;
}

static void click(uint8_t k)
{
    if (k != K_NONE && pref_clicks)
        snd_play((k >= K_UP && k <= K_RIGHT) ? SFX_TICK : SFX_CLICK);
}

static uint8_t pad_key(void)
{
    uint8_t k = dir_key();

    if (k != K_NONE)
        return k;

    switch (in_mode) {
    case M_VIEW:
        if (take_bit(J_A))      return K_ENTER;
        if (take_bit(J_B))      return K_TODAY;
        if (take_bit(J_SELECT)) return (uint8_t) (K_VIEW1 + ((cur_view + 1) & 3));
        if (take_bit(J_START))  return menu_run();
        break;

    case M_DETAIL:
        if (take_bit(J_A) || take_bit(J_SELECT)) return K_EDIT;
        if (take_bit(J_B) || take_bit(J_START))  return K_BACK;
        break;

    case M_PICK:
        if (take_bit(J_A))                       return K_ENTER;
        if (take_bit(J_B) || take_bit(J_START))  return K_BACK;
        break;

    case M_SETUP:
        if (take_bit(J_A))                       return K_ENTER;
        if (take_bit(J_B) || take_bit(J_START))  return K_BACK;
        break;

    default:                                    /* M_OK */
        if (pad_new) {
            pad_new = 0;
            return K_ENTER;
        }
        break;
    }
    pad_new = 0;
    return K_NONE;
}

unsigned char plat_getkey_poll(void)
{
    uint8_t k = in_pop();

    if (k == K_NONE)
        k = pad_key();
    if (k == K_NONE)
        k = kb_key();
    if (k == K_NONE)
        return K_NONE;

    click(k);

    /* Settings has rows of its own the core knows nothing of (the theme,
       the clock, the clicks): the screen's own handler gets every key first
       and says what, if anything, the core should see. */
    if (in_mode == M_SETUP)
        k = setup_key(k);
    return k;
}

unsigned char plat_getkey(void)
{
    uint8_t k;

    for (;;) {
        k = plat_getkey_poll();
        if (k != K_NONE)
            return k;
        plat_vsync();
    }
}

void plat_anykey(void)
{
    uint8_t m = in_mode;

    in_flush();
    in_mode = M_OK;
    while (plat_getkey_poll() == K_NONE)
        plat_vsync();
    in_mode = m;
}
