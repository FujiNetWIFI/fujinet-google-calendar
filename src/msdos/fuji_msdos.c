/*
 * fujinet-lib overrides for the fuji device: the null-vector guard and the
 * two appkey calls settings.c depends on. Defining the symbols here leaves
 * the library's members unreferenced, the src/adam/ pattern.
 *
 * fuji_get_adapter_config_extended(): every INT F5 in this program assumes
 * FUJINET.SYS is resident. When it is not, the interrupt vector is null, and
 * int86x through a null vector jumps to 0000:0000 -- not an error return, a
 * crash. This is the one call main.c's have_fujinet() gates everything
 * behind, so checking the vector here protects every later bus call in the
 * program: a missing driver becomes the "FujiNet not found" screen instead
 * of a hang, and ui_notfound() can name CONFIG.SYS with a straight face.
 * Past the guard it is the library member verbatim -- device 0x70, FUJICMD
 * 0xC4, the struct read back whole.
 *
 * fuji_read_appkey() / fuji_write_appkey(): the archive's members
 * (msdos/src/fn_fuji/fuji_{read,write}_appkey.c) mistake the bus reply for
 * a boolean. int_f5_read/int_f5_write return raw AL -- 'C' complete, 'E'
 * error, 'N' NAK -- and all three are nonzero, so the library's `if (...)`
 * and `> 0` treat a NAK as success: a *missing* key, which is every first
 * run, reads back as 66 bytes of whatever was in the buffer, and
 * set_load() would adopt garbage settings. The read's failure path also
 * does `count=0`, assigning the pointer rather than the count. Both are
 * rewritten with the transaction shape kept -- open (mode 0/1), then the
 * 66-byte read with its two-byte count prefix memmoved down, or the flat
 * 64-byte write with the count in aux1 -- and every leg checked == 'C'.
 * ak_creator_id/ak_app_id stay the library's own: they live in the
 * fuji_ak_data member that fuji_set_appkey_details() pulls in, so
 * set_load()'s call sequence is unchanged.
 *
 * Delete this file when fujinet-lib checks the vector itself and its appkey
 * members check for 'C'.
 */

#include <dos.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <fujinet-fuji.h>

#include "platform.h"

bool fuji_get_adapter_config_extended(AdapterConfigExtended *ac)
{
    if (_dos_getvect(0xF5) == 0)
        return false;

    return int_f5_read(0x70, 0xC4, 0x00, 0x00,
                       ac, sizeof(AdapterConfigExtended)) == 'C';
}

/* The bus form of the open payload -- fujinet-lib's own layout. */
typedef struct {
    uint16_t creator_id;
    uint8_t  app_id;
    uint8_t  key_id;
    uint8_t  open_mode;
    uint8_t  reserved;
} Appkey;

/* common/src/fn_fuji/fuji_ak_data.c, set by fuji_set_appkey_details(). */
extern uint16_t ak_creator_id;
extern uint8_t  ak_app_id;

static bool appkey_open(uint8_t key_id, uint8_t mode)
{
    Appkey ak;

    ak.creator_id = ak_creator_id;
    ak.app_id     = ak_app_id;
    ak.key_id     = key_id;
    ak.open_mode  = mode;
    ak.reserved   = 0;

    return int_f5_write(0x70, FUJICMD_OPEN_APPKEY, 0, 0,
                        &ak, sizeof(ak)) == 'C';
}

bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data)
{
    if (!appkey_open(key_id, 0))
        return false;

    if (int_f5_read(0x70, FUJICMD_READ_APPKEY, 0, 0, data, 66) != 'C') {
        *count = 0;
        return false;
    }

    *count = *(uint16_t *) data;
    memmove(data, data + 2, 64);
    return true;
}

bool fuji_write_appkey(uint8_t key_id, uint16_t count, uint8_t *data)
{
    (void) count;               /* the bus writes the whole 64 regardless */

    if (!appkey_open(key_id, 1))
        return false;

    return int_f5_write(0x70, FUJICMD_WRITE_APPKEY, 64, 0, data, 64) == 'C';
}
