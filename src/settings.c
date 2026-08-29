/*
 * Persistent settings, in a FujiNet appkey.
 *
 * Two values, packed the same way the Intellivision original packed them:
 * byte 0 is the alarm lead in minutes, and bytes 1.. are the NUL-terminated
 * calendar selector. Sixty-four bytes is not much, which is one of the reasons
 * the picker stores a calendar's *name* rather than its id -- "Work" fits
 * where "abc123@group.calendar.google.com" would be tight.
 *
 * A missing key is not an error. It means this is the first run, and the
 * defaults stand.
 */

#include <string.h>

#include <fujinet-fuji.h>

#include "gcal.h"

#define AK_CREATOR  0x4743      /* "CG" */
#define AK_APP      1
#define AK_KEY      0

#ifndef GC_FAKE_DATA
/* fuji_read_appkey memmoves a two-byte count prefix down over the payload, so
   the buffer has to be keysize + 2. A 64-byte buffer corrupts two bytes past
   its end. fuji_write_appkey reads the full 64 regardless of the count. */
static uint8_t akbuf[66];
#endif

void set_load(void)
{
    al_lead = AL_LEAD_DEFAULT;
    gc_cal[0] = '\0';

#ifndef GC_FAKE_DATA
    {
    uint16_t count = 0;

    plat_net_begin();
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    if (fuji_read_appkey(AK_KEY, &count, akbuf) && count >= 1) {
        if (akbuf[0] > 0 && akbuf[0] <= AL_LEAD_MAX)
            al_lead = akbuf[0];

        if (count > 1) {
            akbuf[65] = 0;
            copy_san(gc_cal, (const char *) akbuf + 1, CAL_SEL_LEN);
        }
    }
    plat_net_end();
    }
#endif
}

void set_save(void)
{
#ifndef GC_FAKE_DATA
    unsigned char n;

    memset(akbuf, 0, 64);
    akbuf[0] = al_lead;

    n = (unsigned char) strlen(gc_cal);
    if (n > 62)
        n = 62;
    memcpy(akbuf + 1, gc_cal, n);
    akbuf[1 + n] = 0;

    plat_net_begin();
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    fuji_write_appkey(AK_KEY, (uint16_t) (n + 2), akbuf);
    plat_net_end();
#endif
}
