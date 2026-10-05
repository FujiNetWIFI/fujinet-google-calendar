;
; sfx.s -- the NMI's two small jobs besides the display: the sound
; effects, and the busy spinner.
;
; Sound. An effect is a list of steps (sound.c), each one so many frames of
; both pulse channels -- timer low, timer high, then duty and volume -- and a
; step of 0 frames ends it. The NMI starts each step on time however long the
; program is busy, which is what lets the alarm chime ring under a flashing
; banner and the splash jingle play while the FujiNet is being looked for.
;
; The spinner. Four dots in Google's four colours walk round a circle while a
; bus call blocks the program (screens.c's busy card): the NMI moves them,
; because the program itself is stuck inside fujinet-lib until the FujiNet
; answers. They are sprites 12-15 in both sprite pages, so a page flip never
; loses them.
;
        .export         _snd_nmi, _snd_start, _snd_stop
        .export         _spin_nmi, _spin_show, _spin_hide
        .import         _oam0, _oam1, popa

.segment "ZEROPAGE"
sfx_ptr:    .res 2
sfx_wait:   .res 1
spin_on:    .res 1
spin_vis:   .res 1              ; the dots are in the pages
spin_x:     .res 1
spin_y:     .res 1
spin_t:     .res 1              ; frames to the next step
spin_ph:    .res 1              ; 0-7

SPIN_SPR  = 12 * 4
SPIN_RATE = 5

.segment "CODE"

; ---- sound -----------------------------------------------------------------

; snd_start(steps): play an effect from its first step at the next frame.
_snd_start:
        ldy     #0
        sty     sfx_wait        ; the NMI leaves the pointer alone meanwhile
        sta     sfx_ptr
        stx     sfx_ptr+1
        iny
        sty     sfx_wait
        rts

_snd_stop:
        lda     #0
        sta     sfx_wait
        lda     #$30
        sta     $4000
        sta     $4004
        rts

_snd_nmi:
        lda     sfx_wait
        beq     @r
        dec     sfx_wait
        bne     @r
        ldy     #0
        lda     (sfx_ptr),y
        beq     @end
        sta     sfx_wait
        iny
        lda     (sfx_ptr),y
        sta     $4002
        iny
        lda     (sfx_ptr),y
        sta     $4003
        iny
        lda     (sfx_ptr),y
        sta     $4000
        iny
        lda     (sfx_ptr),y
        sta     $4006
        iny
        lda     (sfx_ptr),y
        sta     $4007
        iny
        lda     (sfx_ptr),y
        sta     $4004
        lda     sfx_ptr
        clc
        adc     #7
        sta     sfx_ptr
        bcc     @r
        inc     sfx_ptr+1
@r:     rts
@end:   lda     #$30
        sta     $4000
        sta     $4004
        rts

; ---- the spinner ------------------------------------------------------------

; spin_show(x, y): the dots round (x, y), from the next frame.
_spin_show:
        sta     spin_y
        jsr     popa
        sta     spin_x
        lda     #1
        sta     spin_t
        sta     spin_on
        rts

_spin_hide:
        lda     #0
        sta     spin_on
        rts

; Offsets of the eight positions round the circle, and each dot's tile and
; palette: blue, red, yellow, green (S_SPIN + slot; SP_BLUE / SP_BRAND).
spin_dx:    .byte   0, 6, 8, 6, 0, <-6, <-8, <-6
spin_dy:    .byte   <-8, <-6, 0, 6, 8, 6, 0, <-6
dot_tile:   .byte   $1A, $19, $1A, $1B
dot_attr:   .byte   1, 0, 0, 0

_spin_nmi:
        lda     spin_on
        bne     @on
        lda     spin_vis
        beq     @r
        lda     #0
        sta     spin_vis
        lda     #$F8
        ldx     #0
@hide:  sta     _oam0+SPIN_SPR,x
        sta     _oam1+SPIN_SPR,x
        inx
        inx
        inx
        inx
        cpx     #16
        bne     @hide
@r:     rts
@on:    dec     spin_t
        bne     @r
        lda     #SPIN_RATE
        sta     spin_t
        lda     #1
        sta     spin_vis
        lda     spin_ph
        clc
        adc     #1
        and     #7
        sta     spin_ph
        ldx     #0              ; OAM offset
        ldy     #0              ; dot
@dot:   tya                     ; position: phase + 2 * dot
        asl     a
        clc
        adc     spin_ph
        and     #7
        sty     tmpd
        tay
        lda     spin_y
        clc
        adc     spin_dy,y
        sec
        sbc     #3              ; the dot's centre, a line early
        sta     _oam0+SPIN_SPR,x
        sta     _oam1+SPIN_SPR,x
        lda     spin_x
        clc
        adc     spin_dx,y
        sec
        sbc     #3
        sta     _oam0+SPIN_SPR+3,x
        sta     _oam1+SPIN_SPR+3,x
        ldy     tmpd
        lda     dot_tile,y
        sta     _oam0+SPIN_SPR+1,x
        sta     _oam1+SPIN_SPR+1,x
        lda     dot_attr,y
        sta     _oam0+SPIN_SPR+2,x
        sta     _oam1+SPIN_SPR+2,x
        inx
        inx
        inx
        inx
        iny
        cpy     #4
        bne     @dot
        rts

.segment "BSS"
tmpd:       .res 1
