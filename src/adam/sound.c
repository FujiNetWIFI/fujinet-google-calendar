/*
 * The alarm chime.
 *
 * This is the one backend whose chime is not three notes it generates itself.
 * The Adam's sound effects live in the SmartWriter ROM, smartkeyslib knows
 * where they are, and a machine whose own alerts sound a particular way should
 * use them -- a synthesised triad here would be the one sound on the machine
 * that did not belong to it.
 *
 * The three calls alarm.c makes therefore pick three SmartWriter fragments
 * that rise, rather than three pitches. They are chosen to escalate: a plain
 * confirm, then a chime, then the double chime, so a banner that is missed on
 * the first note is harder to miss by the third.
 *
 * Unlike the CoCo's and the Apple's, this chime is started rather than played:
 * smartkeys_sound_play() queues a fragment and the raster interrupt
 * smartkeys_sound_init() installed advances it a frame at a time. plat_tone()
 * returns immediately, so the banner keeps flashing while the sound runs.
 */

#include <smartkeys.h>

#include "../gcal.h"
#include "platform.h"

static const unsigned char chime[3] = {
    SOUND_CONFIRM,              /* a short acknowledgement */
    SOUND_POSITIVE_CHIME,       /* the same idea, with a tail */
    SOUND_DOUBLE_CHIME          /* two notes, and the loudest of the three */
};

void plat_tone(unsigned char note)
{
    if (note > 2)
        return;

    smartkeys_sound_play(chime[note]);
}

/*
 * Deliberately empty.
 *
 * The obvious call is eos_sound_off(), but EOS $FD53 is TURN_OFF_SOUND: it
 * shuts the sound *engine* down, not just the current note, and the raster
 * handler smartkeys_sound_init() installed would go on calling
 * eos_play_sound() into a dead engine for the rest of the run. There is
 * nothing to silence anyway -- every SmartWriter fragment is a finite table
 * that ends on its own, well inside the eight frames alarm.c gives each note.
 */
void plat_silence(void)
{
}
