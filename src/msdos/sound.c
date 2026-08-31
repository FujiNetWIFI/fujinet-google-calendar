/*
 * The alarm chime, on the PC speaker.
 *
 * Channel 2 of the 8253 drives the speaker cone through the gate bits in
 * port 61h: program a square-wave divisor, open the gate, and the note
 * sounds until the gate closes -- no CPU in the loop at all, unlike the
 * Apple's click-the-diaphragm oscillator. That inversion is why
 * plat_silence() genuinely matters here: alarm.c calls it between notes and
 * after the chime, and without it the last note would sound forever.
 * plat_shutdown() calls it too, so Q during a chime does not exit to DOS
 * with the speaker keyed.
 *
 * The three notes are C5, E5, G5 -- the same rising major triad every other
 * backend plays -- as divisors of the PIT's 1.19318 MHz input clock.
 */

#include <conio.h>

#include "../gcal.h"
#include "platform.h"

/* 1193182 / frequency: C5 523 Hz, E5 659 Hz, G5 784 Hz. */
static const unsigned int divisors[3] = { 2280, 1810, 1522 };

void plat_tone(unsigned char note)
{
    unsigned int d;

    if (note > 2)
        return;
    d = divisors[note];

    outp(0x43, 0xB6);                   /* channel 2, lo/hi, square wave */
    outp(0x42, d & 0xFF);
    outp(0x42, d >> 8);
    outp(0x61, inp(0x61) | 0x03);       /* timer gate + speaker enable   */
}

void plat_silence(void)
{
    outp(0x61, inp(0x61) & 0xFC);
}
