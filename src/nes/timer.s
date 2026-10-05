;
; timer.s -- is this a 60 Hz console or a 50 Hz one?
;
; clock.c runs the wall clock off the frame counter between fetches, so it
; has to know the frame rate: a PAL console counted at 60 would lose ten
; minutes an hour. The answer is how many times a loop of a known length
; runs between two NMIs: 12 cycles a pass, against 29781 cycles a frame on
; an NTSC console (~2480 passes) and 33252 on a PAL one (~2770; a Dendy's is
; longer still). Run with the display off, before the first screen, when
; the NMI does almost nothing and the count is clean.
;
        .export         _fps_detect
        .import         _frames

THRESHOLD = 2620

.segment "CODE"

; unsigned char fps_detect(void): 60 or 50.
_fps_detect:
        lda     _frames
@sync:  cmp     _frames         ; to the start of a frame
        beq     @sync
        lda     _frames
        ldx     #0
        ldy     #0
@loop:  inx                     ; 2
        bne     @same           ; 3 (2 + 1 once in 256)
        iny
@same:  cmp     _frames         ; 4
        beq     @loop           ; 3
        ; YX = passes
        cpy     #>THRESHOLD
        bcc     @ntsc
        bne     @pal
        cpx     #<THRESHOLD
        bcc     @ntsc
@pal:   lda     #50
        ldx     #0
        rts
@ntsc:  lda     #60
        ldx     #0
        rts
