/*
 * FujiNet Google Calendar.
 *
 * No state table and no function pointers: each screen is a blocking loop, and
 * control flow is the call stack. The view loop is the outer one; the event
 * detail, the picker and the settings page are functions it calls that run
 * their own loop and return.
 *
 * Two flags carry the whole repaint policy, and keeping them distinct is the
 * single most important rule in the program:
 *
 *   shown = 0   the screen needs repainting
 *   dirty = 1   the data is stale and has to be fetched again
 *
 * Only the second costs a network round trip. Backing out of an event must not
 * re-download the day, and moving the selection must not either.
 *
 * The view loop polls rather than blocks, because the clock has to keep
 * running and alarms have to fire while nobody is touching the keyboard. Every
 * other screen blocks, which is cheaper and perfectly correct: plat_ticks()
 * keeps counting frames underneath them, so the clock is still right on the
 * way back.
 */

#include <fujinet-fuji.h>

#include "gcal.h"

static unsigned char view = VIEW_DAY;
static unsigned char sel;        /* selected list row, or WEEK day 0-6 */
static unsigned char first;      /* index of the row drawn at the top */
static unsigned char shown;
static unsigned char dirty = 1;
static unsigned char have_list;  /* a usable listing is on screen */

#ifndef GC_FAKE_DATA
static AdapterConfigExtended ace;
#endif

static void do_detail(void);
static void do_setup(void);
static void do_pick(void);

/* ------------------------------------------------------------------ */
/* Boot                                                                */
/* ------------------------------------------------------------------ */

/*
 * network_init() is a no-op on the Atari and cannot tell us whether a FujiNet
 * is present, so ask the device something only a real one can answer.
 */
static unsigned char have_fujinet(void)
{
#ifdef GC_FAKE_DATA
    return 1;                   /* there is no adapter to ask */
#else
    unsigned char ok;

    plat_net_begin();
    ok = fuji_get_adapter_config_extended(&ace) ? 1 : 0;
    plat_net_end();

    return ok;
#endif
}

/* ------------------------------------------------------------------ */
/* Selection helpers                                                   */
/* ------------------------------------------------------------------ */

/* How many selectable rows the current view has. */
static unsigned char rows_total(void)
{
    switch (view) {
    case VIEW_WEEK:   return 7;
    case VIEW_MONTH:  return 0;         /* the selection is the anchor date */
    case VIEW_AGENDA: return gc_agd_count;
    default:          return gc_count;
    }
}

static unsigned char is_sep(unsigned char row)
{
    return (unsigned char) (view == VIEW_AGENDA && row < gc_agd_count &&
                            (gc_agd[row] & AGD_SEP));
}

/*
 * Keep the window around the selection. Returns 1 when the window moved, which
 * is the only case that needs a full repaint rather than an incremental one.
 */
static unsigned char reframe(void)
{
    if (view == VIEW_WEEK || view == VIEW_MONTH)
        return 0;

    if (sel < first) {
        first = sel;
        return 1;
    }
    if (sel >= first + LIST_ROWS) {
        first = (unsigned char) (sel - LIST_ROWS + 1);
        return 1;
    }

    return 0;
}

/*
 * Move the selection by one row, skipping agenda separators. Two attempts,
 * because one press has to clear at most one separator *and* land on the event
 * beyond it.
 */
static void move_sel(signed char step)
{
    unsigned char n = rows_total();
    unsigned char old = sel;
    unsigned char tries;

    if (n == 0)
        return;

    for (tries = 0; tries < 2; tries++) {
        if (step < 0) {
            if (sel == 0)
                break;
            sel--;
        } else {
            if (sel + 1 >= n)
                break;
            sel++;
        }
        if (!is_sep(sel))
            break;
    }

    /* Nowhere to go, or we ran out of list against a separator. */
    if (is_sep(sel)) {
        sel = old;
        return;
    }

    if (sel == old)
        return;

    if (reframe())
        ui_view(view, sel, first);
    else
        ui_view_sel(view, old, sel, first);
}

/* Land on the first selectable row of a freshly fetched listing. */
static void select_first(void)
{
    sel = 0;
    first = 0;

    if (view == VIEW_WEEK) {
        /* Open on today when the week contains it, so the panel below the
           grid is showing something useful the moment it appears. */
        if (clk_ok)
            sel = date_dow(clk_y, clk_mo, clk_d);
        return;
    }

    while (is_sep(sel) && sel + 1 < gc_agd_count)
        sel++;
}

/* ------------------------------------------------------------------ */
/* Period navigation                                                   */
/* ------------------------------------------------------------------ */

static void step_period(signed char dir)
{
    unsigned char i;

    switch (view) {
    case VIEW_MONTH:
        if (dir > 0)
            date_addmonth(&cur_y, &cur_mo, &cur_d);
        else
            date_submonth(&cur_y, &cur_mo, &cur_d);
        break;

    case VIEW_DAY:
        if (dir > 0)
            date_addday(&cur_y, &cur_mo, &cur_d);
        else
            date_subday(&cur_y, &cur_mo, &cur_d);
        break;

    default:                    /* WEEK and AGENDA both step a week */
        for (i = 0; i < 7; i++) {
            if (dir > 0)
                date_addday(&cur_y, &cur_mo, &cur_d);
            else
                date_subday(&cur_y, &cur_mo, &cur_d);
        }
        break;
    }

    dirty = 1;
}

/*
 * MONTH is the odd one out: its cursor *is* the anchor date, so moving it is
 * ordinary date arithmetic and rolling into a neighbouring month comes free.
 * Only leaving the month costs a fetch.
 */
static void month_move(signed char days)
{
    unsigned char was = cur_mo;
    unsigned char i;

    for (i = 0; i < (days < 0 ? -days : days); i++) {
        if (days > 0)
            date_addday(&cur_y, &cur_mo, &cur_d);
        else
            date_subday(&cur_y, &cur_mo, &cur_d);
    }

    if (cur_mo != was)
        dirty = 1;
    else
        ui_view(VIEW_MONTH, 0, 0);
}

/* The event under the selection, or AL_NONE when it is not on one. */
static unsigned char sel_event(void)
{
    if (view == VIEW_AGENDA) {
        if (sel >= gc_agd_count || (gc_agd[sel] & AGD_SEP))
            return AL_NONE;
        return (unsigned char) (gc_agd[sel] & AGD_IDX);
    }

    if (view == VIEW_DAY)
        return (sel < gc_count) ? sel : AL_NONE;

    return AL_NONE;
}

/* ------------------------------------------------------------------ */
/* Event detail                                                        */
/* ------------------------------------------------------------------ */

static void do_detail(void)
{
    unsigned char ev = sel_event();
    unsigned int  top = 0;
    unsigned int  span;
    unsigned char k;

    if (ev == AL_NONE)
        return;

    ui_busy(BUSY_DETAIL);
    if (!gc_fetch_detail(view, gc_index[ev].num)) {
        ui_error(gc_ecode);
        plat_anykey();
        return;
    }

    ui_detail(ev, top);

    for (;;) {
        k = plat_getkey();
        span = (gc_det_rows > DET_WIN) ? gc_det_rows - DET_WIN : 0u;

        switch (k) {
        case K_UP:
            if (top) {
                top--;
                ui_detail(ev, top);
            }
            break;

        case K_DOWN:
            if (top < span) {
                top++;
                ui_detail(ev, top);
            }
            break;

        case K_LEFT:
            top = (top > DET_WIN) ? top - DET_WIN : 0u;
            ui_detail(ev, top);
            break;

        case K_RIGHT:
            top += DET_WIN;
            if (top > span)
                top = span;
            ui_detail(ev, top);
            break;

        case K_BACK:
        case K_ENTER:
        case K_QUIT:
            return;             /* and deliberately without setting dirty */
        }
    }
}

/* ------------------------------------------------------------------ */
/* Calendar picker                                                     */
/* ------------------------------------------------------------------ */

/* PICK_ROWS is gcal.h's: this scroll window and the backend's painted row
   count are the same number, and they used to disagree. */

static void do_pick(void)
{
    unsigned char psel = 0, pfirst = 0;
    unsigned char k, i;

    ui_busy(BUSY_CALS);
    if (!gc_fetch_cals()) {
        ui_error(gc_ecode);
        plat_anykey();
        return;
    }

    /* Open on whichever calendar is already chosen. */
    for (i = 0; i < gc_cal_count; i++) {
        const char *a = gc_cals[i].sel;
        const char *b = gc_cal;
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) {
            psel = i;
            break;
        }
    }
    if (psel >= PICK_ROWS)
        pfirst = (unsigned char) (psel - PICK_ROWS + 1);

    ui_pick(psel, pfirst);

    for (;;) {
        k = plat_getkey();

        switch (k) {
        case K_UP:
            if (psel) {
                psel--;
                if (psel < pfirst)
                    pfirst = psel;
                ui_pick(psel, pfirst);
            }
            break;

        case K_DOWN:
            if (psel + 1 < gc_cal_count) {
                psel++;
                if (psel >= pfirst + PICK_ROWS)
                    pfirst = (unsigned char) (psel - PICK_ROWS + 1);
                ui_pick(psel, pfirst);
            }
            break;

        case K_ENTER:
            /* Only a change is worth a refetch. */
            {
                const char *a = gc_cals[psel].sel;
                const char *b = gc_cal;
                while (*a && *a == *b) { a++; b++; }
                if (*a != *b) {
                    copy_san(gc_cal, gc_cals[psel].sel, CAL_SEL_LEN);
                    set_save();
                    dirty = 1;
                }
            }
            return;

        case K_BACK:
        case K_QUIT:
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

static void do_setup(void)
{
    unsigned char k;

    ui_setup();

    for (;;) {
        k = plat_getkey();

        switch (k) {
        case K_LEFT:
            if (al_lead > 1) {
                al_lead--;
                ui_setup_lead();
            }
            break;

        case K_RIGHT:
            if (al_lead < AL_LEAD_MAX) {
                al_lead++;
                ui_setup_lead();
            }
            break;

        case K_VIEW1:
            do_pick();
            ui_setup();
            break;

        case K_BACK:
        case K_ENTER:
            set_save();
            return;

        case K_QUIT:
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* The view loop                                                       */
/* ------------------------------------------------------------------ */

static void switch_view(unsigned char v)
{
    if (view == v)
        return;
    view = v;
    dirty = 1;
}

int main(void)
{
    unsigned char k;

    plat_init();
    ui_splash();

    while (!have_fujinet()) {
        ui_notfound();
        plat_anykey();
        ui_splash();
    }

    /* No clock is fatal, not cosmetic: every device spec names a date and
       there is nothing sensible to put there without one. */
    ui_busy(BUSY_CLOCK);
    while (!clk_fetch()) {
        ui_noclock();
        plat_anykey();
        ui_busy(BUSY_CLOCK);
    }

    set_load();
    clk_today();
    alarm_reset();

    for (;;) {
        if (dirty) {
            ui_busy(BUSY_INDEX);
            alarm_reset();

            if (gc_fetch_index(view)) {
                have_list = 1;
            } else {
                ui_error(gc_ecode);
                plat_anykey();
                /* A failed refresh keeps the listing that is still on screen;
                   only a cold failure is worth stopping for. */
                if (!have_list)
                    continue;
            }

            select_first();
            dirty = 0;
            shown = 0;
        }

        if (!shown) {
            ui_view(view, sel, first);
            shown = 1;
        }

        plat_vsync();

        if (clk_tick()) {
            ui_clock();
            if (clk_due_resync() && !al_active) {
                clk_fetch();
                ui_view(view, sel, first);
            }
        }

        if (alarm_step(view)) {
            /* A key that dismisses a banner is swallowed, so nothing moves
               under the user while they are reaching for it. */
            if (plat_getkey_poll() != K_NONE) {
                alarm_dismiss();
                continue;
            }
            continue;
        }

        alarm_scan(view);

        k = plat_getkey_poll();
        if (k == K_NONE)
            continue;

        switch (k) {
        case K_VIEW1: switch_view(VIEW_DAY); break;
        case K_VIEW2: switch_view(VIEW_WEEK); break;
        case K_VIEW3: switch_view(VIEW_MONTH); break;
        case K_VIEW4: switch_view(VIEW_AGENDA); break;

        case K_TODAY:
            clk_today();
            dirty = 1;
            break;

        case K_REFRESH:
            dirty = 1;
            break;

        case K_LEFT:
            if (view == VIEW_MONTH)
                month_move(-1);
            else
                step_period(-1);
            break;

        case K_RIGHT:
            if (view == VIEW_MONTH)
                month_move(1);
            else
                step_period(1);
            break;

        case K_UP:
            if (view == VIEW_MONTH)
                month_move(-7);
            else
                move_sel(-1);
            break;

        case K_DOWN:
            if (view == VIEW_MONTH)
                month_move(7);
            else
                move_sel(1);
            break;

        case K_ENTER:
            if (view == VIEW_MONTH || view == VIEW_WEEK) {
                /* Drill into the selected day. MONTH's cursor is already the
                   anchor; WEEK has to walk from the week's Sunday. */
                if (view == VIEW_WEEK) {
                    unsigned char n = date_dow(cur_y, cur_mo, cur_d);
                    while (n--)
                        date_subday(&cur_y, &cur_mo, &cur_d);
                    for (n = 0; n < sel; n++)
                        date_addday(&cur_y, &cur_mo, &cur_d);
                }
                view = VIEW_DAY;
                dirty = 1;
            } else {
                do_detail();
                shown = 0;
            }
            break;

        case K_BACK:
            do_setup();
            shown = 0;
            break;

        case K_QUIT:
            plat_shutdown();
            return 0;
        }
    }
}
