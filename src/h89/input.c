/*
 * Keyboard, and the screen's delivery.
 *
 * The H19 keyboard is the H89's whole interface, and it has keys no other
 * machine this program runs on has: five function keys and three coloured
 * ones along the top, and a keypad that sends arrows. So the views are on
 * f1-f4 and today on f5, and the three colour keys are the three things done
 * to events -- BLUE a new one, RED edit this one, GRAY fetch them again. The
 * 25th line says so on every screen (ui.c). The Apple's digits and letters
 * work too, for whoever learned them there.
 *
 * Every read here sends the screen first. The painters only ever write
 * h19.c's copy, so this is the one place the terminal is brought up to
 * date: once per wait, by difference, which is the whole of what makes a
 * 9600-baud screen bearable. The view loop's poll uses the flush that stops
 * between lines when a key is waiting -- the key will repaint the screen
 * anyway, so a held arrow skips the frames it would otherwise queue.
 */

#include "../gcal.h"
#include "platform.h"
#include "kbd.h"
#include "key_codes.h"

static unsigned char map(unsigned char k)
{
    switch (k) {
    case KEY_UP_ARROW:              return K_UP;
    case KEY_DOWN_ARROW:            return K_DOWN;
    case KEY_LEFT_ARROW:
    case KEY_PAGE_UP:               return K_LEFT;
    case KEY_RIGHT_ARROW:
    case KEY_PAGE_DOWN:             return K_RIGHT;

    case KEY_RETURN:                return K_ENTER;
    case KEY_ESCAPE:                return K_BACK;

    case KEY_F1: case '1':          return K_VIEW1;
    case KEY_F2: case '2':          return K_VIEW2;
    case KEY_F3: case '3':          return K_VIEW3;
    case KEY_F4: case '4':          return K_VIEW4;
    case KEY_F5: case KEY_HOME:
    case '0':                       return K_TODAY;

    case KEY_BLUE: case 'n': case 'N':  return K_NEW;
    case KEY_RED:  case 'e': case 'E':  return K_EDIT;
    case KEY_GRAY: case 'r': case 'R':  return K_REFRESH;

    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

/* Blocking, through plat_vsync(), so the frame counter goes on turning while
   a screen waits. */
unsigned char plat_getkey(void)
{
    unsigned char k;

    h19_flush();
    for (;;) {
        k = kbd_get();
        if (k)
            return map(k);
        plat_vsync();
    }
}

/*
 * The view loop's read, which must not block: the clock and the alarms run
 * between keys. Under GC_FAKE_DATA too -- the Apple's blocks there because a
 * capture has no other way to stop it, but the H89's harness types real keys
 * and can stop the machine wherever it likes.
 */
unsigned char plat_getkey_poll(void)
{
    unsigned char k;

    h19_flush_poll();
    k = kbd_get();
    return k ? map(k) : K_NONE;
}

void plat_anykey(void)
{
    h19_flush();
    while (!kbd_get())
        plat_vsync();
}

/*
 * The compose form's read. BACKSPACE and DELETE both erase before the
 * cursor; BLUE saves without the question; the keypad's arrows move. The
 * keypad is shifted, so its digits are arrows -- dates and times are typed
 * on the top row, which the form's legend says.
 */
unsigned char plat_getch(void)
{
    unsigned char k;

    for (;;) {
        h19_flush_poll();
        k = kbd_get();
        switch (k) {
        case 0:                     plat_vsync(); continue;
        case KEY_UP_ARROW:          return E_UP;
        case KEY_DOWN_ARROW:        return E_DOWN;
        case KEY_LEFT_ARROW:        return E_LEFT;
        case KEY_RIGHT_ARROW:       return E_RIGHT;
        case KEY_RETURN:
        case KEY_TAB:               return E_ENTER;
        case KEY_ESCAPE:            return E_DONE;
        case KEY_BACKSPACE:
        case KEY_DELETE:            return E_BS;
        case KEY_BLUE:              return E_SAVE;
        }
        if (k >= 0x20 && k < 0x7F)
            return k;
    }
}
