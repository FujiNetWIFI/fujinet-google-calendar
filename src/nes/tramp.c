/*
 * The trampolines: the fixed bank's door into each overlay.
 *
 * The program is bigger than the 24K the NES can see without switching
 * (R7's 8K and the fixed 16K), so the screens and the network each live in
 * an 8K bank that takes turns in the R6 window at $8000 (gcal-nes.cfg). A
 * call into one goes through a function here, in the fixed bank: switch the
 * window, call, switch it back to whatever the caller was running from --
 * which is what makes a call from one overlay into another (the form's
 * busy screen, say) come home to the right bank.
 *
 * The portable core is not touched to make this work. The Makefile compiles
 * the core files that live in an overlay with their entry points renamed
 * (-Dgc_fetch_index=bk_gc_fetch_index and so on, the prototype in gcal.h
 * renamed with them), and the names main.c and the rest call are defined
 * here. The backend's own overlay functions simply have their own names.
 *
 * The rule the whole arrangement rests on: nothing in an overlay calls into
 * another overlay except through here, and no pointer into an overlay's
 * ROM is handed across one. Every pointer that crosses -- a line of the
 * draft, an event number -- is into WRAM, with one exception the core makes
 * and ui_error() below deals with. tools/nes/xbank.py checks the first half
 * of that rule against the objects after every link; the second half no
 * linker can see.
 */

#include "../gcal.h"
#include "nes.h"
#include "ovl.h"

uint8_t cur_view;
char    pick_ref[FRM_TIME_MAX + 1];     /* the START time, for END's picker */

uint8_t bank_enter(uint8_t b)
{
    uint8_t p = bank_cur;

    bank_set(b);
    return p;
}

/* ------------------------------------------------------------------ */
/* The portable core's overlays                                        */
/* ------------------------------------------------------------------ */

unsigned char gc_fetch_index(unsigned char view)
{
    uint8_t p = bank_enter(BANK_NET), r = bk_gc_fetch_index(view);

    bank_leave(p);
    return r;
}

unsigned char gc_fetch_detail(unsigned char view, const char *evnum)
{
    uint8_t p = bank_enter(BANK_NET), r = bk_gc_fetch_detail(view, evnum);

    bank_leave(p);
    return r;
}

unsigned char gc_fetch_cals(void)
{
    uint8_t p = bank_enter(BANK_NET), r = bk_gc_fetch_cals();

    bank_leave(p);
    return r;
}

unsigned char gc_save_begin(unsigned char editing, unsigned char view,
                            const char *evnum)
{
    uint8_t p = bank_enter(BANK_NET), r = bk_gc_save_begin(editing, view, evnum);

    bank_leave(p);
    return r;
}

unsigned char gc_save_end(void)
{
    uint8_t p = bank_enter(BANK_NET), r = bk_gc_save_end();

    bank_leave(p);
    return r;
}

void form_put(const char *line)
{
    uint8_t p = bank_enter(BANK_NET);

    bk_form_put(line);
    bank_leave(p);
}

unsigned char compose_new(unsigned int y, unsigned char mo, unsigned char d)
{
    uint8_t p = bank_enter(BANK_FORM), r = bk_compose_new(y, mo, d);

    bank_leave(p);
    return r;
}

unsigned char compose_edit(unsigned char view, unsigned char ev)
{
    uint8_t p = bank_enter(BANK_FORM), r = bk_compose_edit(view, ev);

    bank_leave(p);
    return r;
}

/* ------------------------------------------------------------------ */
/* The views                                                           */
/* ------------------------------------------------------------------ */

static uint8_t view_bank(uint8_t view)
{
    return (view == VIEW_WEEK || view == VIEW_MONTH) ? BANK_GRID : BANK_VIEWS;
}

void ui_view(unsigned char view, unsigned char sel, unsigned char first)
{
    uint8_t p = bank_enter(view_bank(view));

    cur_view = view;
    if (view == VIEW_WEEK || view == VIEW_MONTH)
        g_view(view, sel, first);
    else
        v_view(view, sel, first);
    bank_leave(p);
}

void ui_view_sel(unsigned char view, unsigned char from, unsigned char to,
                 unsigned char first)
{
    uint8_t p = bank_enter(view_bank(view));

    if (view == VIEW_WEEK || view == VIEW_MONTH)
        g_view_sel(view, from, to, first);
    else
        v_view_sel(view, from, to, first);
    bank_leave(p);
}

void ui_hints(unsigned char view)
{
    uint8_t p = bank_enter(BANK_VIEWS);

    v_hints(view);
    bank_leave(p);
}

void ui_alarm(unsigned char phase)
{
    uint8_t p = bank_enter(BANK_VIEWS);

    v_alarm(phase);
    bank_leave(p);
}

/* ------------------------------------------------------------------ */
/* Detail, picker, settings                                            */
/* ------------------------------------------------------------------ */

void ui_detail(unsigned char ev, unsigned int top)
{
    uint8_t p = bank_enter(BANK_DETAIL);

    d_detail(ev, top);
    bank_leave(p);
}

void ui_pick(unsigned char sel, unsigned char first)
{
    uint8_t p = bank_enter(BANK_DETAIL);

    d_pick(sel, first);
    bank_leave(p);
}

void ui_setup(void)
{
    uint8_t p = bank_enter(BANK_DETAIL);

    d_setup();
    bank_leave(p);
}

void ui_setup_lead(void)
{
    uint8_t p = bank_enter(BANK_DETAIL);

    d_setup_lead();
    bank_leave(p);
}

uint8_t setup_key(uint8_t k)
{
    uint8_t p = bank_enter(BANK_DETAIL), r = d_setup_key(k);

    bank_leave(p);
    return r;
}

/* ------------------------------------------------------------------ */
/* The flat screens and the START menu                                 */
/* ------------------------------------------------------------------ */

void ui_splash(void)
{
    uint8_t p = bank_enter(BANK_BOOT);

    b_splash();
    bank_leave(p);
}

void ui_notfound(void)
{
    uint8_t p = bank_enter(BANK_BOOT);

    b_notfound();
    bank_leave(p);
}

void ui_noclock(void)
{
    uint8_t p = bank_enter(BANK_BOOT);

    b_noclock();
    bank_leave(p);
}

void ui_busy(unsigned char reason)
{
    uint8_t p = bank_enter(BANK_BOOT);

    b_busy(reason);
    bank_leave(p);
}

/*
 * gc_stage is the one pointer the core hands across a bank: it points at a
 * string literal in net.c, which is in bank 0, and the error screen is in
 * bank 6 -- read from there it is whatever bank 6 has at that address, and
 * the strcpy() that formats it runs off the end of its buffer. So the name is
 * copied out here, with bank 0 in the window, into RAM the screen can read.
 */
char stage_name[8];

void ui_error(unsigned char code)
{
    uint8_t p = bank_enter(BANK_NET), i = 0;

    if (gc_stage)
        for (; i < sizeof(stage_name) - 1 && gc_stage[i]; i++)
            stage_name[i] = gc_stage[i];
    stage_name[i] = '\0';

    bank_set(BANK_BOOT);
    b_error(code);
    bank_leave(p);
}

uint8_t menu_run(void)
{
    uint8_t p = bank_enter(BANK_BOOT), r = b_menu();

    bank_leave(p);
    return r;
}

/* ------------------------------------------------------------------ */
/* The form's pickers                                                  */
/* ------------------------------------------------------------------ */

uint8_t pick_run(uint8_t field, char *value)
{
    uint8_t p = bank_enter(BANK_PICK), r = k_pick(field, value);

    bank_leave(p);
    return r;
}
