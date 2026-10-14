/*
 * The compose / edit form's screen, and plat_getch(): the on-screen keyboard,
 * the Famicom keyboards, and the door to the date, time and colour pickers.
 * Overlay bank 4 (OVL_FORM), beside compose.c and form.c, which are the only
 * callers of anything here.
 *
 *   rows 2-3    the mark; "New event" / "Edit event"
 *   rows 6-12   the seven fields, a row each: the label, then the value --
 *               the field being edited on the highlight ground with the
 *               cursor cell knocked out of it, so it reads as a caret
 *   row  13     the form's message ("A title is required", "Save?")
 *   row  14     a rule
 *   rows 15-24  the panel: the keyboard, or for a date, a time or a colour
 *               the picker that sets it (pickers.c), or with a Famicom
 *               keyboard attached a card saying which keys do what
 *   row  25     the buttons
 *
 * compose.c owns the editing; plat_getch() only turns presses into the
 * characters and E_* codes it already understands. A picker's choice goes in
 * the same way, as keystrokes queued ahead of the pad (in_push): to the end
 * of the field, rubbed out, the new value typed, on to the next field -- so
 * form_validate() judges a picked date exactly as it judges a typed one.
 */

#include <stdlib.h>
#include <string.h>

#include <fujinet-nes.h>

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

#define FRM_TOP     6
#define LABEL_COL   2
#define VAL_COL     9
#define HINT_COL    17
#define MSG_ROW     13
#define PANEL_TOP   15
#define PANEL_BOT   24

/* plat_getch's panel */
#define P_NONE      0
#define P_OSK       1
#define P_PICK      2
#define P_KBD       3

static uint8_t act_f;           /* the field being edited */
static uint8_t panel;
static uint8_t asking;          /* "save?" is up: A is yes, B is no */
static uint8_t typing;          /* this picker field is being typed instead */
static uint8_t osk_on;          /* a keyboard is attached but the pad asked */

static uint8_t is_pick_field(uint8_t f)
{
    return (uint8_t) (f == FRM_DATE || f == FRM_START || f == FRM_END || f == FRM_CAT);
}

static const uint8_t frm_w[FRM_NFIELDS] = { 21, 11, 6, 6, 21, 21, 16 };

static const char *const frm_label[FRM_NFIELDS] = {
    "Title", "Date", "Start", "End", "Where", "Notes", "Colour"
};

static const char LEGEND_OSK[] = "\x10" "Type \x11" "Del {Sel}Next {Start}OK";

/* ------------------------------------------------------------------ */
/* The form                                                            */
/* ------------------------------------------------------------------ */

unsigned char ui_form_width(unsigned char f)
{
    return frm_w[f];
}

void ui_form(unsigned char editing)
{
    uint8_t f;

    scr_begin();
    ui_header(editing ? "Edit event" : "New event",
              editing ? "Blank fields stay the same"
                      : "No start time = all day");
    scr_rule(RULE_ROW);
    for (f = 0; f < FRM_NFIELDS; f++)
        scr_text((uint8_t) (FRM_TOP + f), LABEL_COL, frm_label[f], 0);
    scr_rule(PANEL_TOP - 1);
    ui_legend(LEGEND_OSK);
    in_mode = M_FORM;
    scr_end();

    panel = P_NONE;
    asking = 0;
    typing = 0;
}

/* "14:30" as the 12-hour clock shows it, beside a time field. */
static void time_hint(uint8_t row, const char *v)
{
    uint8_t h, m;
    char t[8];

    scr_field(row, HINT_COL, "", 9, 0);
    if (pref_24h || !form_time_ok(v))
        return;
    h = (uint8_t) (v[1] == ':' ? v[0] - '0' : (v[0] - '0') * 10 + v[1] - '0');
    m = (uint8_t) (atoi(strchr(v, ':') + 1));
    ui_hhmm(t, h, m);
    scr_text(row, HINT_COL, t, 0);
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    uint8_t row = (uint8_t) (FRM_TOP + f);
    uint8_t c;

    scr_field(row, VAL_COL, win, frm_w[f], active ? HL : 0);
    if (active) {
        scr_hl(row, (uint8_t) (VAL_COL + curx), 1, 0);
        if (act_f != f) {
            act_f = f;
            typing = 0;
            if (panel == P_PICK || is_pick_field(f))
                panel = P_NONE;         /* the panel follows the field */
        }
    }
    scr_text(row, LABEL_COL, frm_label[f], active ? HL : 0);

    if (f == FRM_START || f == FRM_END)
        time_hint(row, form_field_ptr(f));
    if (f == FRM_CAT) {
        const char *v = form_field_ptr(FRM_CAT);

        c = color_match(v, (uint8_t) strlen(v));
        if (*v && c != COL_NONE)
            spr_color(SPR_CONTENT + 47, (uint8_t) ((VAL_COL + frm_w[f] + 1) * 8),
                      (uint8_t) (row * 8), S_DOT, c);
        else
            spr_hide(SPR_CONTENT + 47, SPR_CONTENT + 47);
    }
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    asking = 0;
    switch (msg) {
    case FM_ASK:       s = "Save this event? \x10" "Yes \x11" "No"; asking = 1; break;
    case FM_NEEDTITLE: s = "A title is required";              break;
    case FM_BADDATE:   s = "The date is YYYY-MM-DD";           break;
    case FM_BADTIME:   s = "A time is HH:MM, 24-hour";         break;
    case FM_ENDALONE:  s = "An end time needs a start";        break;
    default:           s = "";                                 break;
    }
    scr_field(MSG_ROW, LEFT_COL, "", TEXT_W, 0);
    scr_center(MSG_ROW, s, 0);
    if (msg != FM_NONE && msg != FM_ASK)
        snd_play(SFX_ERROR);
}

/* ------------------------------------------------------------------ */
/* The on-screen keyboard                                              */
/* ------------------------------------------------------------------ */

#define KEY_TOP     16          /* rows 16, 18, 20, 22 */
#define KEY_LEFT    2           /* ten keys at a three-column pitch */
#define ACT_ROW     24

static const char pages[3][4][11] = {
    { "1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.-" },
    { "1234567890", "QWERTYUIOP", "ASDFGHJKL\"", "ZXCVBNM;:?" },
    { "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",./<>?", "`~ @#&*/+=" },
};

/* The action row: where each key is, how wide, and what it is. */
#define A_SHIFT     0
#define A_SYM       1
#define A_SPACE     2
#define A_LEFT      3
#define A_RIGHT     4
#define A_DEL       5
#define A_NEXT      6
#define N_ACT       7
static const uint8_t act_col[N_ACT] = { 2, 5, 9, 18, 21, 24, 27 };
static const uint8_t act_w[N_ACT]   = { 1, 2, 7, 1, 1, 1, 3 };

static uint8_t page, shift_once, kx, ky;

static void osk_keys(void)
{
    uint8_t r, i;

    for (r = 0; r < 4; r++)
        for (i = 0; i < 10; i++)
            scr_tile((uint8_t) (KEY_TOP + r * 2), (uint8_t) (KEY_LEFT + i * 3),
                     (uint8_t) (pages[page][r][i] | HL));

    scr_tile(ACT_ROW, act_col[A_SHIFT],
             page == 1 ? (shift_once ? T_K_SHIFT : T_K_LOCK) : T_K_SHIFT);
    scr_text(ACT_ROW, act_col[A_SYM], page == 2 ? "ab" : "#+", HL);
}

static void osk_draw(void)
{
    uint8_t r;

    scr_hold();
    for (r = PANEL_TOP; r <= PANEL_BOT; r++)
        scr_field(r, 0, "", SCR_W, 0);
    spr_hide(SPR_CONTENT, SPR_CONTENT + 46);
    osk_keys();
    scr_tile(ACT_ROW, act_col[A_SPACE], T_K_SPACE);
    scr_field(ACT_ROW, (uint8_t) (act_col[A_SPACE] + 1), "", 6, HL);
    scr_tile(ACT_ROW, act_col[A_LEFT], T_K_LEFT);
    scr_tile(ACT_ROW, act_col[A_RIGHT], T_K_RIGHT);
    scr_tile(ACT_ROW, act_col[A_DEL], T_K_DEL);
    scr_text(ACT_ROW, act_col[A_NEXT], "Nxt", HL);
    ui_legend(LEGEND_OSK);
    scr_flush(PANEL_TOP, LEGEND_ROW);
    panel = P_OSK;
}

static void osk_cursor(void)
{
    if (ky < 4)
        spr_cursor((uint8_t) (KEY_TOP + ky * 2), (uint8_t) (KEY_LEFT + kx * 3), 1, 1);
    else
        spr_cursor(ACT_ROW, act_col[kx], act_w[kx], 1);
}

/* A Famicom keyboard is attached: the panel says how to use it, and the pad
   can still ask for the on-screen keys. */
static void kbd_card(void)
{
    uint8_t r;

    scr_hold();
    for (r = PANEL_TOP; r <= PANEL_BOT; r++)
        scr_field(r, 0, "", SCR_W, 0);
    spr_hide(SPR_CONTENT, SPR_CONTENT + 46);
    spr_hide(SPR_CURSOR, SPR_CURSOR + 3);
    scr_center(16, "Type with the keyboard", 0);
    scr_center(18, "RETURN or TAB: the next field", 0);
    scr_center(19, "ESC: done      F8: save now", 0);
    scr_center(20, "\x06\x07: fields    \x04\x05: move", 0);
    scr_center(22, "\x10 on the pad: on-screen keys", 0);
    ui_legend("\x10" "Keys {Select}Next {Start}Done");
    scr_flush(PANEL_TOP, LEGEND_ROW);
    panel = P_KBD;
}

/* The nearest action-row key to a column, for moving down into the row. */
static uint8_t act_at(uint8_t col)
{
    uint8_t i, best = 0, d, bd = 255;

    for (i = 0; i < N_ACT; i++) {
        d = (uint8_t) (col > act_col[i] ? col - act_col[i] : act_col[i] - col);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

static void osk_move(uint8_t dir)
{
    uint8_t col;

    switch (dir) {
    case J_LEFT:
        kx = (uint8_t) (kx ? kx - 1 : (ky < 4 ? 9 : N_ACT - 1));
        break;
    case J_RIGHT:
        kx = (uint8_t) ((ky < 4) ? (kx + 1) % 10 : (kx + 1) % N_ACT);
        break;
    case J_UP:
    case J_DOWN:
        col = (uint8_t) ((ky < 4) ? KEY_LEFT + kx * 3 : act_col[kx]);
        ky = (uint8_t) ((dir == J_UP) ? (ky + 4) % 5 : (ky + 1) % 5);
        if (ky == 4)
            kx = act_at(col);
        else
            kx = (uint8_t) ((col >= KEY_LEFT ? col - KEY_LEFT + 1 : 0) / 3);
        if (kx > 9)
            kx = 9;
        break;
    }
    osk_cursor();
    if (pref_clicks)
        snd_play(SFX_TICK);
}

/* A on the key under the cursor: a character, an E_* code, or 0 when the
   key only changed the keyboard. */
static uint8_t osk_press(void)
{
    char c;

    if (pref_clicks)
        snd_play(SFX_CLICK);

    if (ky < 4) {
        c = pages[page][ky][kx];
        if (page == 1 && shift_once) {
            page = 0;
            osk_keys();
        }
        return (uint8_t) c;
    }

    switch (kx) {
    case A_SHIFT:
        if (page == 1 && shift_once) {
            shift_once = 0;             /* a second press locks it */
        } else if (page == 1) {
            page = 0;
        } else {
            page = 1;
            shift_once = 1;
        }
        osk_keys();
        return 0;
    case A_SYM:
        page = (uint8_t) (page == 2 ? 0 : 2);
        shift_once = 0;
        osk_keys();
        return 0;
    case A_SPACE:   return ' ';
    case A_LEFT:    return E_LEFT;
    case A_RIGHT:   return E_RIGHT;
    case A_DEL:     return E_BS;
    default:        return E_ENTER;
    }
}

/* ------------------------------------------------------------------ */
/* plat_getch                                                          */
/* ------------------------------------------------------------------ */

static uint8_t kb_char(void)
{
    char c = kb_ch;
    uint8_t f = kb_fn;

    kb_ch = 0;
    kb_fn = 0;

    if (f == 8)
        return E_SAVE;
    if (f == 9)
        return E_DONE;                  /* STOP */
    switch (c) {
    case 0:                     return 0;
    case FUJI_NES_KEY_UP:       return E_UP;
    case FUJI_NES_KEY_DOWN:     return E_DOWN;
    case FUJI_NES_KEY_LEFT:     return E_LEFT;
    case FUJI_NES_KEY_RIGHT:    return E_RIGHT;
    case FUJI_NES_KEY_ENTER:    return E_ENTER;
    case FUJI_NES_KEY_TAB:      return E_DOWN;
    case FUJI_NES_KEY_ESC:      return E_DONE;
    case FUJI_NES_KEY_BS:       return E_BS;
    case 0x7F:                  return E_BS;
    }
    if (c >= 0x20 && c < 0x7F)
        return (uint8_t) c;
    return 0;
}

/* Replace the field's value by typing: to its end, rub it out, type the new
   one, then on to the next field. */
static void inject_value(const char *old, const char *v)
{
    uint8_t n = (uint8_t) strlen(old), i;

    for (i = 0; i < n; i++)
        in_push(E_RIGHT);
    for (i = 0; i < n; i++)
        in_push(E_BS);
    while (*v)
        in_push((uint8_t) *v++);
    in_push(E_ENTER);
}

unsigned char plat_getch(void)
{
    static char picked[FRM_DATE_MAX + 6];
    uint8_t k;

    for (;;) {
        k = in_pop();
        if (k)
            return k;

        if (asking) {
            plat_vsync();
            k = kb_char();
            if (k)
                return k;
            if (pad_new & J_A)     { pad_new = 0; return 'y'; }
            if (pad_new & J_B)     { pad_new = 0; return 'n'; }
            if (pad_new & J_START) { pad_new = 0; return E_SAVE; }
            pad_new = 0;
            continue;
        }

        /* A date, a time or a colour: its picker, unless asked to type. */
        if (is_pick_field(act_f) && !typing) {
            const char *cur = form_field_ptr(act_f);

            spr_hide(SPR_CURSOR, SPR_CURSOR + 3);
            strcpy(picked, cur);
            strcpy(pick_ref, form_field_ptr(FRM_START));
            panel = P_PICK;
            switch (pick_run(act_f, picked)) {
            case PK_SET:
                inject_value(cur, picked);
                break;
            case PK_NEXT:   return E_ENTER;
            case PK_PREV:   return E_UP;
            case PK_DONE:   return E_DONE;
            case PK_TYPE:
                typing = 1;
                panel = P_NONE;
                break;
            case PK_KEY:
                typing = 1;
                panel = P_NONE;
                k = kb_char();
                if (k)
                    return k;
                break;
            }
            continue;
        }

        if (kb_type != FUJI_NES_KBD_NONE && !osk_on) {
            if (panel != P_KBD)
                kbd_card();
            plat_vsync();
            k = kb_char();
            if (k)
                return k;
            if (pad_new & J_A)      { osk_on = 1; panel = P_NONE; }
            else if (pad_new & J_SELECT) k = E_ENTER;
            else if (pad_new & J_START)  k = E_DONE;
            else if (pad_new & J_UP)     k = E_UP;
            else if (pad_new & J_DOWN)   k = E_DOWN;
            pad_new = 0;
            if (k)
                return k;
            continue;
        }

        if (panel != P_OSK) {
            osk_draw();
            osk_cursor();
        }
        plat_vsync();

        k = kb_char();
        if (k)
            return k;

        if (pad_new & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
            uint8_t d = pad_new;

            pad_new = 0;
            osk_move((uint8_t) (d & J_UP ? J_UP : d & J_DOWN ? J_DOWN
                                : d & J_LEFT ? J_LEFT : J_RIGHT));
            continue;
        }
        if (pad_new & J_A) {
            pad_new = 0;
            k = osk_press();
            if (k)
                return k;
            continue;
        }
        k = 0;
        if (pad_new & J_B)
            k = E_BS;
        else if (pad_new & J_SELECT)
            k = E_ENTER;
        else if (pad_new & J_START)
            k = E_DONE;
        pad_new = 0;
        if (k) {
            if (pref_clicks)
                snd_play(SFX_CLICK);
            return k;
        }
    }
}
