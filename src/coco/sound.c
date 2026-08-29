/*
 * The alarm chime.
 *
 * Three rising notes through Color BASIC's SOUND, which is the whole audio
 * surface of the program. Like the Apple II's, the chime is *played* rather
 * than started: SOUND does not return until the note is over, so plat_tone()
 * blocks and plat_silence() has nothing left to do.
 *
 * A duration of 1 is about a sixteenth of a second, which fits inside the
 * eight frames alarm.c gives each note -- and comfortably so on a machine
 * running at 1.8 MHz, which is where DriveWire needs it. SOUND's pitch and
 * duration are both CPU delay loops, so at double speed the note comes out
 * higher and shorter than the Color BASIC manual's table says. That is
 * audible, and for a three-note chime it does not matter.
 */

#include <coco.h>

#include "../gcal.h"
#include "platform.h"

/* C, E, G from Color BASIC's own SOUND note values. */
static const unsigned char notes[3] = { 89, 125, 147 };

void plat_tone(unsigned char note)
{
    if (note > 2)
        return;

    sound(notes[note], 1);
}

void plat_silence(void)
{
}
