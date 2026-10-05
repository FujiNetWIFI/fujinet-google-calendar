/*
 * This backend's own settings: the theme (Google's light or dark scheme),
 * the clock (12- or 24-hour) and the key clicks.
 *
 * They live in appkey 1 of the same creator and app as settings.c's key 0,
 * which every client of this family reads with the same layout -- alarm
 * lead, then the calendar selector -- and which this backend therefore
 * leaves exactly as it is. Key 1 is the NES's alone:
 *
 *   'N' 1   then the theme, the clock and the clicks, a byte each
 *
 * A missing key, or one written by something else, leaves the defaults:
 * light, 12-hour like Google's own US default, clicks on.
 */

#include <string.h>

#include <fujinet-fuji.h>

#include "../gcal.h"
#include "nes.h"

#define AK_CREATOR  0x4743
#define AK_APP      1
#define AK_KEY      1

uint8_t pref_dark, pref_24h, pref_clicks = 1;

#ifndef GC_FAKE_DATA
static uint8_t pkbuf[66];       /* keysize + 2: see settings.c */
#endif

static void apply(void)
{
    theme_dark = pref_dark;
    scr_theme();
}

void prefs_load(void)
{
#ifndef GC_FAKE_DATA
    uint16_t count = 0;

    plat_net_begin();
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    if (fuji_read_appkey(AK_KEY, &count, pkbuf) && count >= 5
        && pkbuf[0] == 'N' && pkbuf[1] == 1) {
        pref_dark = (uint8_t) (pkbuf[2] != 0);
        pref_24h = (uint8_t) (pkbuf[3] != 0);
        pref_clicks = (uint8_t) (pkbuf[4] != 0);
    }
    plat_net_end();
#endif
    apply();
}

void prefs_save(void)
{
#ifndef GC_FAKE_DATA
    memset(pkbuf, 0, 64);
    pkbuf[0] = 'N';
    pkbuf[1] = 1;
    pkbuf[2] = pref_dark;
    pkbuf[3] = pref_24h;
    pkbuf[4] = pref_clicks;

    plat_net_begin();
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    fuji_write_appkey(AK_KEY, 5, pkbuf);
    plat_net_end();
#endif
    apply();
}
