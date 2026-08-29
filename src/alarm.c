/*
 * Alarms.
 *
 * Entirely synthesised on this side. GCAL.cpp's field mask asks Google for
 * id, summary, location, status, colorId, recurringEventId, start, end and
 * extendedProperties -- but not reminders, so no override minutes and no
 * default reminder ever reach the console. Instead an event sounds once,
 * al_lead minutes before it starts, with the lead set on the settings screen
 * and persisted in the appkey.
 *
 * Every guard below is deliberate and inherited from the Intellivision
 * original:
 *
 *   - Only in the DAY view, and only when the loaded day is genuinely today.
 *     Browsing next Tuesday must not set an alarm off, and only a view knows
 *     how to paint the footer back afterwards, so a banner raised on the
 *     detail or settings screen would be stranded there.
 *   - Not for all-day events, which have no meaningful start minute.
 *   - Once per event, via EVF_FIRED. A refetch clears the index and therefore
 *     the flags, which is right: after a refresh an event still inside the
 *     window is genuinely news again.
 *   - The event must not have started yet. An alarm for something forty
 *     minutes underway is noise.
 */

#include "gcal.h"

#define BANNER_FRAMES   240     /* about four seconds at 60Hz */
#define CHIME_STEP      8       /* frames per note */

static unsigned int  frames;
static unsigned char note;
static unsigned char phase;

void alarm_reset(void)
{
    al_active = 0;
    al_ev = AL_NONE;
    frames = 0;
    note = 0;
    phase = 0;
}

unsigned char alarm_scan(unsigned char view)
{
    unsigned int  now;
    unsigned int  evm;
    unsigned char i;
    struct event *e;

    if (al_active || !clk_ok || gc_count == 0)
        return AL_NONE;
    if (view != VIEW_DAY)
        return AL_NONE;
    if (!clk_is_today(cur_y, cur_mo, cur_d))
        return AL_NONE;

    now = (unsigned int) clk_h * 60 + clk_mi;

    for (i = 0; i < gc_count; i++) {
        e = &gc_index[i];

        if (e->flags & (EVF_FIRED | EVF_ALLDAY))
            continue;

        evm = (unsigned int) e->sh * 60 + e->sm;
        if (evm < now)
            continue;
        if (evm - now > al_lead)
            continue;

        e->flags |= EVF_FIRED;
        al_ev = i;
        al_active = 1;
        frames = BANNER_FRAMES;
        note = 0;
        phase = 0;
        return i;
    }

    return AL_NONE;
}

/*
 * One frame of banner. Returns 1 while it is still up.
 *
 * The flash costs nothing to repaint: the footer band's colours live in two
 * bytes the display list interrupt reads on its next pass, so alternating them
 * recolours row 23 without touching a single screen cell.
 */
unsigned char alarm_step(unsigned char view)
{
    unsigned char want;

    if (!al_active)
        return 0;

    want = (unsigned char) ((frames & 16) ? 1 : 0);
    if (want != phase || frames == BANNER_FRAMES) {
        phase = want;
        ui_alarm(phase);
    }

    if (note < 3 * CHIME_STEP) {
        if ((note % CHIME_STEP) == 0)
            plat_tone((unsigned char) (note / CHIME_STEP));
        note++;
        if (note >= 3 * CHIME_STEP)
            plat_silence();
    }

    if (--frames == 0) {
        plat_silence();
        al_active = 0;
        al_ev = AL_NONE;
        ui_hints(view);
        return 0;
    }

    return 1;
}

void alarm_dismiss(void)
{
    if (!al_active)
        return;

    frames = 1;                 /* let alarm_step run its own teardown */
}
