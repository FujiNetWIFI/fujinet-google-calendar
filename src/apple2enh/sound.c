/*
 * The alarm chime.
 *
 * Three rising notes, which is the whole audio surface of the program. The
 * Atari hands POKEY a divisor and walks away; here the CPU *is* the
 * oscillator, so each note is played to completion before the call returns and
 * there is nothing for plat_silence() to switch off.
 *
 * That is why the notes are short. alarm_step() asks for one every eight
 * frames over a four-second banner, so three bursts of about eighty
 * milliseconds is all the loop ever gives up.
 */

#include "../gcal.h"
#include "platform.h"

/* Shared with tone.s. */
unsigned char tone_period;
unsigned char tone_count;

void tone_play(void);

/*
 * A major triad, C-E-G, an octave above the Atari's -- 261 Hz would need a
 * delay count of 388 and the loop counter is one byte.
 *
 * period counts down a 5-cycle loop with 13 cycles of overhead around it, so
 * half a cycle is 13 + 5 * period; count is how many of those make about 80ms.
 */
static const unsigned char periods[3] = { 193, 153, 128 };  /* C5 E5 G5 */
static const unsigned char counts[3]  = {  84, 106, 125 };

void plat_tone(unsigned char note)
{
    if (note > 2)
        return;

    tone_period = periods[note];
    tone_count = counts[note];
    tone_play();
}

void plat_silence(void)
{
}
