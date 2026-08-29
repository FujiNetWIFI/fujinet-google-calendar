/*
 * The alarm chime.
 *
 * Three rising notes on one POKEY channel, which is the whole audio surface of
 * the program. The Intellivision original used SOUND rather than PLAY for the
 * same reason it is kept this small here: a tracker would cost far more than a
 * three-note chime is worth.
 */

#include <atari.h>
#include <peekpoke.h>

#include "../gcal.h"
#include "platform.h"

#define AUDF1   0xD200
#define AUDC1   0xD201
#define AUDCTL  0xD208

/*
 * A major triad, C-E-G. With AUDCTL clear the channel runs off the 63.9 kHz
 * clock, so the divisor is 63921 / (2 * hz) - 1.
 */
static const unsigned char notes[3] = { 60, 47, 39 };

void plat_tone(unsigned char note)
{
    if (note > 2)
        return;

    POKE(AUDCTL, 0x00);
    POKE(AUDF1, notes[note]);
    POKE(AUDC1, 0xA8);          /* pure tone, volume 8 */
}

void plat_silence(void)
{
    POKE(AUDC1, 0x00);
}
