/*
 * The sound effects and the alarm chime, on the APU's two pulse channels.
 *
 * Each effect is a list of steps for the NMI to play (sfx.s): frames,
 * then pulse 1's timer low, timer high and duty/volume, then pulse 2's. The
 * timer for a note of f Hz is 1789773 / (16 f) - 1. $30 is silence: the
 * constant-volume bit with a volume of 0, the length counter halted.
 *
 * The chime is three rising chords, one for each of alarm.c's three calls,
 * each with a short decay -- a bell rather than a beep, which is the whole
 * difference between an alarm and an error.
 */

#include "../gcal.h"
#include "nes.h"

void snd_start(const unsigned char *steps);
void snd_stop(void);

#define APU_STATUS (*(volatile unsigned char *) 0x4015)
#define APU_SWEEP1 (*(volatile unsigned char *) 0x4001)
#define APU_SWEEP2 (*(volatile unsigned char *) 0x4005)

/* Timers, NTSC. */
#define N_C4    0x1AB
#define N_G4    0x11C
#define N_C5    0x0D5
#define N_E5    0x0A9
#define N_G5    0x08E
#define N_A5    0x07E
#define N_C6    0x06A
#define N_E6    0x054
#define N_G6    0x046
#define N_LOW1  0x258
#define N_LOW2  0x2BC

#define LO(n)   ((n) & 0xFF)
#define HI(n)   (((n) >> 8) | 0x08)
#define D12     0x30            /* 12.5% duty, constant volume, halted */
#define D25     0x70
#define D50     0xB0
#define OFF     0, 0x08, 0x30

/* A note on pulse 1 alone. */
#define P1(f, n, duty, v)       f, LO(n), HI(n), (duty) | (v), OFF
/* A chord: pulse 1 and pulse 2. */
#define P2(f, a, b, duty, v)    f, LO(a), HI(a), (duty) | (v), LO(b), HI(b), (duty) | ((v) > 2 ? (v) - 2 : (v))
#define END                     0

static const unsigned char fx_click[] = { P1(1, N_C6, D12, 4), P1(1, N_G6, D12, 2), END };
static const unsigned char fx_tick[]  = { P1(1, N_E6, D12, 2), END };
static const unsigned char fx_error[] = {
    P1(6, N_LOW1, D50, 8), P1(2, N_LOW1, D50, 0), P1(10, N_LOW2, D50, 8), END
};

static const unsigned char fx_chime0[] = {
    P2(3, N_C5, N_E5, D25, 10), P2(4, N_C5, N_E5, D25, 7),
    P2(5, N_C5, N_E5, D25, 4), P2(6, N_C5, N_E5, D25, 2), END
};
static const unsigned char fx_chime1[] = {
    P2(3, N_E5, N_G5, D25, 11), P2(4, N_E5, N_G5, D25, 8),
    P2(5, N_E5, N_G5, D25, 5), P2(6, N_E5, N_G5, D25, 2), END
};
static const unsigned char fx_chime2[] = {
    P2(3, N_G5, N_C6, D25, 12), P2(5, N_G5, N_C6, D25, 9),
    P2(6, N_G5, N_C6, D25, 6), P2(8, N_G5, N_C6, D25, 3), END
};

/* The splash: four notes up the major chord, the last one held. */
static const unsigned char fx_jingle[] = {
    P2(5, N_G4, N_C5, D25, 8), P2(5, N_C5, N_E5, D25, 8),
    P2(5, N_E5, N_G5, D25, 8), P2(8, N_G5, N_C6, D25, 9),
    P2(8, N_G5, N_C6, D25, 5), P2(10, N_G5, N_C6, D25, 2), END
};

static const unsigned char fx_saved[] = {
    P2(4, N_E5, N_G5, D25, 8), P2(4, N_A5, N_C6, D25, 8),
    P2(8, N_A5, N_C6, D25, 4), END
};

static const unsigned char fx_open[] = { P1(2, N_G5, D12, 5), P1(2, N_C6, D12, 4), END };

static const unsigned char *const fx[] = {
    fx_click, fx_tick, fx_error, fx_chime0, fx_chime1, fx_chime2,
    fx_jingle, fx_saved, fx_open
};

void snd_init(void)
{
    APU_STATUS = 0x03;          /* the two pulses */
    APU_SWEEP1 = 0x08;          /* no sweep */
    APU_SWEEP2 = 0x08;
    snd_stop();
}

void snd_play(unsigned char id)
{
    snd_start(fx[id]);
}

void plat_tone(unsigned char note)
{
    if (note <= 2)
        snd_play((unsigned char) (SFX_CHIME0 + note));
}

void plat_silence(void)
{
    /* Each chord decays and ends by itself, well inside the frames alarm.c
       gives a note; cutting one short would only make it click. */
}
