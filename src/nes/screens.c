/*
 * The flat screens -- splash, busy, not found, no clock, errors -- and the
 * START menu with its help and about pages. Overlay bank 6 (OVL_BOOT);
 * reached only through tramp.c.
 *
 * The splash and the busy card are one screen: the large mark (its tiles
 * are the splash CHR bank, switched in with the flip), the wordmark --
 * "Google" in sprites, a letter to a colour as Google sets it, "Calendar"
 * in text -- and a line of status over the four-dot spinner the NMI turns
 * while the FujiNet works. When one follows another, only the status line
 * changes, so going from "Looking for FujiNet" to "Reading the clock" to
 * "Fetching your week" is a line of text and not three repaints.
 */

#include <string.h>

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

#define LOGO_ROW    5
#define LOGO_COL    13
#define WORD_ROW    13
#define SUB_ROW     15
#define MSG_ROW     18
#define MSG2_ROW    19
#define SPIN_ROW    22

static uint8_t flat_gen;        /* the splash family is on screen */
static uint8_t jingled;

/* "Google" a sprite a letter: tile, palette. */
static const uint8_t word[6][2] = {
    { S_WORD + 0, SP_BLUE }, { S_WORD + 1, SP_BRAND }, { S_WORD + 2, SP_BRAND },
    { S_WORD + 3, SP_BLUE }, { S_WORD + 4, SP_BRAND }, { S_WORD + 5, SP_BRAND },
};

static void flat_messages(const char *a, const char *b)
{
    scr_field(MSG_ROW, 0, "", SCR_W, 0);
    scr_field(MSG2_ROW, 0, "", SCR_W, 0);
    scr_center(MSG_ROW, a, 0);
    if (b)
        scr_center(MSG2_ROW, b, 0);
}

/* The mark and the wordmark; the caller adds its lines. Returns 1 when the
   screen was already up and only the lines need changing. */
static uint8_t flat_screen(void)
{
    uint8_t i;

    if (flat_gen == scr_gen)
        return 1;

    scr_begin();
    logo_big(LOGO_ROW, LOGO_COL);
    for (i = 0; i < 6; i++)
        spr_put((uint8_t) (SPR_CONTENT + 16 + i), (uint8_t) ((8 + i) * 8),
                WORD_ROW * 8, word[i][0], word[i][1]);
    scr_text(WORD_ROW, 15, "Calendar", 0);
    scr_center(SUB_ROW, "for FujiNet", 0);
    return 0;
}

static void flat_done(const char *legend, uint8_t mode)
{
    ui_legend(legend);
    in_mode = mode;
    if (scr_full) {
        scr_end();
        flat_gen = scr_gen;
    }
}

void b_splash(void)
{
    flat_screen();
    flat_messages("Looking for FujiNet", 0);
    spin_show(128, SPIN_ROW * 8);
    flat_done("", M_OK);
    if (!jingled) {
        jingled = 1;
        snd_play(SFX_JINGLE);
    }
}

void b_notfound(void)
{
    spin_hide();
    flat_screen();
    flat_messages("FujiNet not found", "Is the cartridge's FujiNet on?");
    flat_done("\x10" "Try again", M_OK);
    snd_play(SFX_ERROR);
}

void b_noclock(void)
{
    spin_hide();
    flat_screen();
    flat_messages("The FujiNet clock failed", "Check its time zone setting");
    flat_done("\x10" "Try again", M_OK);
    snd_play(SFX_ERROR);
}

static uint8_t prefs_loaded;

void b_busy(unsigned char reason)
{
    const char *a, *b = 0;

    switch (reason) {
    case BUSY_CLOCK:
        a = "Reading the clock";
        break;
    case BUSY_DETAIL:
        a = "Opening the event";
        break;
    case BUSY_CALS:
        a = "Reading your calendars";
        break;
    case BUSY_SAVE:
        a = "Saving the event";
        b = "Google is writing it";
        break;
    default:
        switch (cur_view) {
        case VIEW_WEEK:   a = "Fetching your week";   break;
        case VIEW_MONTH:  a = "Fetching your month";  break;
        case VIEW_AGENDA: a = "Fetching your agenda"; break;
        default:          a = "Fetching your day";    break;
        }
        b = "Up to a minute";
        break;
    }

    flat_screen();
    flat_messages(a, b);
    spin_show(128, SPIN_ROW * 8);
    flat_done("", M_OK);

    /* The first moment the FujiNet is known to be there: this backend's own
       settings (prefs.c) come with the first screen that can show them. */
    if (reason == BUSY_CLOCK && !prefs_loaded) {
        prefs_loaded = 1;
        prefs_load();
    }
}

/* ------------------------------------------------------------------ */
/* Errors                                                              */
/* ------------------------------------------------------------------ */

static const char *err_hint(uint8_t code)
{
    switch (code) {
    case 0:            return "Is the FujiNet on the network?";
    case GC_NOAUTH:    return "Sign in to Google in the";
    case GC_DENIED:    return "Sign in to Google again in the";
    case GC_NOTFOUND:
    case GC_BADSPEC:   return "Pick another one in Settings";
    case GC_BADDRAFT:  return "Check the date and the times";
    case GC_RDONLY:    return "Pick a calendar you can edit";
    default:           return "";
    }
}

void b_error(unsigned char code)
{
    char n[6];
    const char *h = err_hint(code);

    gc_ecode = code;
    spin_hide();

    scr_begin();
    ui_header("That didn't work", "");
    scr_rule(RULE_ROW);

    scr_center(9, ui_err_text(code), 0);
    scr_center(11, h, 0);
    if (code == GC_NOAUTH || code == GC_DENIED)
        scr_center(12, "FujiNet's web page, then retry", 0);

    strcpy(sbuf, stage_name[0] ? stage_name : "?");
    strcat(sbuf, " code ");
    num(n, code);
    strcat(sbuf, n);
    strcat(sbuf, ", device ");
    num(n, gc_dev_ecode);
    strcat(sbuf, n);
    scr_center(15, sbuf, 0);

    ui_legend("\x10" "OK");
    in_mode = M_OK;
    scr_end();
    snd_play(SFX_ERROR);
}

/* ------------------------------------------------------------------ */
/* The START menu                                                      */
/* ------------------------------------------------------------------ */

/*
 * A card over the view, and the view put back exactly as it was when it
 * closes: the whole screen is saved first (scr_save) into the detail
 * buffer, which nothing on a view screen is using -- an event's detail is
 * fetched fresh every time one is opened. The menu returns the K_* code
 * main.c would have got from a key, so everything it offers is exactly what
 * the keys already do.
 */

#define MENU_COL    9
#define MENU_W      14
#define MENU_TOP    6

/* The save goes in the detail buffer: it must be big enough (DET_ROWS in
   the Makefile). */
typedef char det_holds_a_screen[(DET_ROWS * DET_STRIDE >= SCR_SAVE_SIZE) ? 1 : -1];

#define MI_HELP     0xF0
#define MI_ABOUT    0xF1

struct mitem {
    const char *label;
    uint8_t key;
};

static const struct mitem items[] = {
    { "Today",      K_TODAY },
    { "New event",  K_NEW },
    { "Edit event", K_EDIT },
    { "Refresh",    K_REFRESH },
    { "Day",        K_VIEW1 },
    { "Week",       K_VIEW2 },
    { "Month",      K_VIEW3 },
    { "Agenda",     K_VIEW4 },
    { "Settings",   K_BACK },
    { "Help",       MI_HELP },
    { "About",      MI_ABOUT },
    { "Quit",       K_QUIT },
};
#define N_ITEMS     (sizeof(items) / sizeof(items[0]))

static uint8_t usable(uint8_t i)
{
    /* Edit needs an event under the bar, which only the lists have. */
    return (uint8_t) (items[i].key != K_EDIT
                      || cur_view == VIEW_DAY || cur_view == VIEW_AGENDA);
}

static void menu_row(uint8_t i, uint8_t sel)
{
    uint8_t row = (uint8_t) (MENU_TOP + 1 + i);
    uint8_t st = (i == sel) ? 0 : HL;

    scr_field(row, MENU_COL, "", MENU_W, st);
    scr_field(row, (uint8_t) (MENU_COL + 2), usable(i) ? items[i].label : "",
              (uint8_t) (MENU_W - 3), st);
    if (i == sel)
        spr_put(SPR_MISC + 1, (uint8_t) (MENU_COL * 8 + 4), (uint8_t) (row * 8),
                S_POINTER + 2, SP_BLUE);
}

static void menu_card(uint8_t sel)
{
    uint8_t i;

    spr_hide(SPR_CONTENT, 63);
    scr_hold();
    scr_field(MENU_TOP, MENU_COL, "", MENU_W, HL);
    scr_text(MENU_TOP, (uint8_t) (MENU_COL + 2), "Menu", HL);
    for (i = 0; i < N_ITEMS; i++)
        menu_row(i, sel);
    scr_field((uint8_t) (MENU_TOP + 1 + N_ITEMS), MENU_COL, "", MENU_W, HL);
    scr_flush(MENU_TOP, (uint8_t) (MENU_TOP + 1 + N_ITEMS));
}

static void page_help(void)
{
    scr_begin();
    ui_header("Help", "The pad, and the keyboard");
    scr_rule(RULE_ROW);
    scr_text(6, 2, "\x06\x07\x04\x05  move; \x04\x05 next day/week", 0);
    scr_text(7, 2, "\x10  open the event or the day", 0);
    scr_text(8, 2, "\x11  back to today", 0);
    scr_text(9, 2, "Select  the next view", 0);
    scr_text(10, 2, "Start   this menu", 0);
    scr_text(12, 2, "In an event: \x10 edit, \x11 back", 0);
    scr_text(13, 2, "In a form: the on-screen keys;", 0);
    scr_text(14, 2, "  \x10 type, \x11 rub out,", 0);
    scr_text(15, 2, "  Select next field, Start done", 0);
    scr_rule(17);
    scr_text(18, 2, "Keyboard: 1-4 views, 0 today", 0);
    scr_text(19, 2, "N new, E edit, R refresh,", 0);
    scr_text(20, 2, "S settings, M menu, Q quit,", 0);
    scr_text(21, 2, "ESC back, F8 save a form", 0);
    ui_legend("\x10" "OK");
    in_mode = M_OK;
    scr_end();
}

static void page_about(void)
{
    uint8_t i;

    scr_begin();
    logo_big(4, LOGO_COL);
    for (i = 0; i < 6; i++)
        spr_put((uint8_t) (SPR_CONTENT + 16 + i), (uint8_t) ((8 + i) * 8),
                12 * 8, word[i][0], word[i][1]);
    scr_text(12, 15, "Calendar", 0);
    scr_center(14, "for the FujiNet NES cartridge", 0);
    scr_center(16, "Your Google Calendar on an NES:", 0);
    scr_center(17, "day, week, month and agenda,", 0);
    scr_center(18, "events opened, made and edited.", 0);
    scr_center(20, "github.com/FujiNetWIFI", 0);
    strcpy(sbuf, "Version ");
    strcat(sbuf, GIT_VERSION);
    scr_center(22, sbuf, 0);
    ui_legend("\x10" "OK");
    in_mode = M_OK;
    scr_end();
}

uint8_t b_menu(void)
{
    uint8_t *save = (uint8_t *) gc_det;
    uint8_t sel = 0, k = K_NONE, old = in_mode;

    scr_save(save);
    in_mode = M_MENU;
    menu_card(sel);
    snd_play(SFX_OPEN);
    in_flush();

    for (;;) {
        plat_vsync();
        if (pad_new & (J_UP | J_DOWN)) {
            uint8_t was = sel;

            do {
                sel = (uint8_t) ((pad_new & J_UP) ? (sel + N_ITEMS - 1) % N_ITEMS
                                                  : (sel + 1) % N_ITEMS);
            } while (!usable(sel));
            pad_new = 0;
            menu_row(was, sel);
            menu_row(sel, sel);
            if (pref_clicks)
                snd_play(SFX_TICK);
            continue;
        }
        if ((pad_new & (J_B | J_START)) || kb_ch == 0x1B) {
            k = K_NONE;
            break;
        }
        if ((pad_new & J_A) || kb_ch == 0x0D) {
            k = items[sel].key;
            if (k == MI_HELP || k == MI_ABOUT) {
                if (k == MI_HELP)
                    page_help();
                else
                    page_about();
                plat_anykey();
                k = K_NONE;
            }
            break;
        }
        pad_new = 0;
        kb_ch = 0;
        kb_fn = 0;
    }

    in_flush();
    spr_hide(SPR_MISC + 1, SPR_MISC + 1);
    scr_restore(save);
    in_mode = old;
    return k;
}
