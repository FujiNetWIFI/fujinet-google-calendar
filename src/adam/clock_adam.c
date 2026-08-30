/*
 * clock_get_time() for the Adam.
 *
 * fujinet-lib has no fn_clock at all on this bus. The CoCo archive carries
 * clock_get_time and not clock_get_tz, which is what -DGC_NO_CLOCK_TZ is for;
 * the Adam archive carries neither, and a calendar client with no clock has
 * nothing to put in the date field of any device spec it builds. So the one
 * call src/clock.c needs is supplied here.
 *
 * It is not a reimplementation of anything: the firmware already answers this
 * on the Fuji device rather than on a clock device of its own.
 * lib/device/adamnet/adamFuji.cpp dispatches CMD::FUJI_GET_TIME (0xD2) to
 * adamnet_get_time(), which replies with fujiClock::get_current_time_simple()
 * -- and lib/device/fujiClock/fujiClock.cpp's build_simple() lays those bytes
 * out as
 *
 *   [ year/100 + 19, year%100, month 1-12, day 1-31, hour 0-23, min, sec ]
 *
 * which is SIMPLE_BINARY exactly, resolved through the FujiNet's [General]
 * timezone, the same setting the GCAL adapter resolves its window with. So
 * this is a seven-byte read, not a conversion.
 *
 * The transaction shape is fujinet-lib's own (adam/src/fn_fuji/*.c), minus its
 * retry loop: a fixed-length command write, then a read. `response` is the
 * library's shared 1024-byte buffer -- the firmware sets the character device's
 * length to 1024 and every fuji_* call reads the whole of it, so borrowing it
 * is both correct and a kilobyte cheaper than a second one.
 */

#include <stdint.h>
#include <eos.h>
#include <string.h>

#include <fujinet-network.h>
#include <fujinet-clock.h>

#define FUJI_DEV        0x0F    /* FUJI_DEVICEID::FUJINET on AdamNet */
#define FUJI_GET_TIME   0xD2    /* fujiCommandID.h */

#define SIMPLE_LEN      7

/* adam/src/bus/response.c. Not in the shipped headers, but it is in the
   archive and every fuji_* call in it uses this buffer. */
extern unsigned char response[1024];

uint8_t clock_get_time(uint8_t *time_data, TimeFormat format)
{
    unsigned char cmd = FUJI_GET_TIME;
    uint8_t err;

    /*
     * Only the seven-byte form is answered. The other TimeFormat values are
     * clock-device commands the Fuji device does not carry, so claiming to
     * support them would mean returning whatever 0xD2 sent under a different
     * name.
     */
    if (format != SIMPLE_BINARY)
        return FN_ERR_BAD_CMD;

    /*
     * No retry on ADAMNET_TIMEOUT, which is what fujinet-lib's own adam/ calls
     * wrap these in. It cannot happen: eos_write_character_device() restarts
     * itself internally until the device settles and only ever returns a
     * settled status, so the retry is unreachable and only reads as though a
     * missing adapter were handled. See the header comment in fuji_adam.c.
     */
    err = eos_write_character_device(FUJI_DEV, &cmd, 1);
    if (err != ADAMNET_OK)
        return FN_ERR_IO_ERROR;

    err = eos_read_character_device(FUJI_DEV, response, sizeof(response));
    if (err != ADAMNET_OK)
        return FN_ERR_IO_ERROR;

    memcpy(time_data, response, SIMPLE_LEN);

    return FN_ERR_OK;
}
