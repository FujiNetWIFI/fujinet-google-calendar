/*
 * The alarm chime.
 *
 * The H89 has no sound of its own; the H19 has a bell, one fixed beep of
 * about 200 ms. alarm_step() asks for three notes eight frames apart, which
 * at 50 frames a second is 160 ms -- three bells that close run together into
 * one long one. So ring on the first and the last: two distinct beeps, which
 * is as much tune as a bell has.
 */

#include "../gcal.h"
#include "platform.h"

void plat_tone(unsigned char note)
{
    if (note == 0 || note == 2)
        h19_bell();
}

/* A bell stops by itself. */
void plat_silence(void)
{
}
