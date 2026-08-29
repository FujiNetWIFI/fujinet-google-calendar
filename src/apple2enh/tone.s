;
; Square wave on the speaker.
;
; The Apple II's whole audio hardware is one bit: every touch of $C030 moves
; the cone once. A note is therefore a delay loop, and holding a note means
; staying in that loop -- there is nothing to leave running the way POKEY can
; be left running on the Atari. So plat_tone() plays its note and returns,
; rather than starting one.
;
; Half a cycle costs 13 + 5 * period cycles, which is where sound.c's numbers
; come from. They assume a 1 MHz 6502: on an accelerated machine or a IIgs at
; fast speed the chime simply comes out higher, which is a cosmetic problem
; with a three-note alarm and not worth a speed test to fix.
;
;       void tone_play (void);
;

        .export         _tone_play
        .import         _tone_period, _tone_count

SPKR    :=      $C030                   ; not in cc65's apple2.inc

        .code

_tone_play:
        lda     _tone_count
        beq     done

half:   bit     SPKR                    ; one touch, one movement of the cone
        ldx     _tone_period
delay:  dex
        bne     delay

        dec     _tone_count
        bne     half

done:   rts
