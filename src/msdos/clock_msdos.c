/*
 * clock_get_time() and clock_get_tz() for MS-DOS.
 *
 * fujinet-lib has no fn_clock at all on this bus. msdos/src/ has bus/,
 * fn_fuji/ and fn_network/ and nothing else, so without this file the link
 * fails on the two calls src/clock.c makes -- the same hole src/adam/
 * clock_adam.c fills for AdamNet, and the same fix: defining the symbols
 * here leaves nothing in the archive to collide with.
 *
 * The firmware does answer them. The clock is FUJI_DEVICEID::CLOCK --
 * device 0x45, the byte FUJINET.SYS passes through in AL untouched, and the
 * same device fujinet-msdos's own FUJITIME.EXE reads binary time from. The
 * command byte and reply length for each TimeFormat are the atari
 * fn_clock's own table (clock_get_time.s): SIMPLE_BINARY is index 0,
 * command 'T', seven bytes of
 *
 *      [ year/100, year%100, month 1-12, day 1-31, hour 0-23, min, sec ]
 *
 * resolved through the FujiNet's [General] timezone -- exactly the buffer
 * src/clock.c already parses and range-checks, so a NAK body fails the
 * validation there rather than setting a junk clock. aux1 is the use_alt
 * flag: zero asks for the system timezone rather than an override.
 *
 * The timezone is two plain reads on the same device, again the atari
 * shape (clock_get_tz.s): 'L' answers with a one-byte length, 'G' with
 * that many bytes of the TZ string, unterminated -- clk_get_tz() in
 * src/clock.c pre-zeroes the buffer for exactly that reason. Every caller
 * hands it a 48-byte tzbuf, so the read is capped at 47 to leave the NUL
 * alone. If the RS-232 firmware build turns out not to answer 'L'/'G',
 * delete clock_get_tz here and add -DGC_NO_CLOCK_TZ to CFLAGS_EXTRA_MSDOS
 * -- the CoCo's gate -- and ui_setup shows the clock's own reading
 * instead; see src/msdos/ui.c.
 *
 * This program does not read the DOS clock instead: FUJITIME sets DOS time
 * *from* the FujiNet, so INT 21h would hand back the same wall clock while
 * silently going stale against the adapter's window when the driver disk
 * skips it (NOTIME) -- and the FujiNet's answer is the one the GCAL
 * adapter resolves events with, which is the agreement that matters.
 *
 * Delete this file when fujinet-lib grows msdos/src/fn_clock/.
 */

#include <stdint.h>

#include <fujinet-network.h>
#include <fujinet-clock.h>

#include "platform.h"

#define CLOCK_DEV       0x45    /* FUJI_DEVICEID::CLOCK / APETIME        */
#define CMD_GETTIME     'T'     /* SIMPLE_BINARY, 7 bytes                */
#define CMD_TZ_LEN      'L'     /* one byte: strlen of the system TZ     */
#define CMD_TZ_GET      'G'     /* that many bytes of the string         */

#define SIMPLE_LEN      7
#define TZ_CAP          47      /* callers hand clk_get_tz a char[48]    */

uint8_t clock_get_time(uint8_t *time_data, TimeFormat format)
{
    /*
     * Only the seven-byte form is answered. The other TimeFormat values are
     * separate clock-device commands, and claiming to support them would
     * mean handing back 'T's reply under a different name.
     */
    if (format != SIMPLE_BINARY)
        return FN_ERR_BAD_CMD;

    if (int_f5_read(CLOCK_DEV, CMD_GETTIME, 0x00, 0x00,
                    time_data, SIMPLE_LEN) != 'C')
        return FN_ERR_IO_ERROR;

    return FN_ERR_OK;
}

uint8_t clock_get_tz(char *tz)
{
    unsigned char len = 0;

    if (int_f5_read(CLOCK_DEV, CMD_TZ_LEN, 0x00, 0x00, &len, 1) != 'C')
        return FN_ERR_IO_ERROR;

    if (len == 0 || len > TZ_CAP)
        return FN_ERR_IO_ERROR;

    if (int_f5_read(CLOCK_DEV, CMD_TZ_GET, 0x00, 0x00, tz, len) != 'C')
        return FN_ERR_IO_ERROR;

    return FN_ERR_OK;
}
