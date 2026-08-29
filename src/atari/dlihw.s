;
; Display list interrupt chain.
;
; GRAPHICS 0 gives one background and one text luminance for the whole frame,
; which is not enough for a Calendar-looking screen. Two DLIs split it into
; three bands: a light header (rows 0-2), a white page (rows 3-22) and a blue
; footer (row 23).
;
; Each interrupt arms the other one, so the chain cannot drift out of phase --
; whichever ran last always leaves the first one armed for the next frame. The
; header band needs no interrupt of its own: the OS vertical blank copies the
; COLOR1/COLOR2 shadows into the hardware before every frame, and dli.c keeps
; the header colors in those shadows.
;
; The first interrupt does a second job. Above it the four players draw the
; Google Calendar mark in the header; below it they are the list's colour
; gutter. Moving them is five stores of one value into HPOSP0-3 and HPOSM0, in
; an interrupt that had to fire anyway -- which is the whole reason per-event
; colour costs nothing extra on a screen whose text hue is fixed by COLPF2.
; The vertical blank puts them back over the logo before row 0 is drawn.
;

        .export         _dli_hw_on, _dli_hw_off
        .export         _dli_vbi_install, _dli_vbi_remove
        .export         _dli_list_bg, _dli_list_fg
        .export         _dli_foot_bg, _dli_foot_fg

        .import         _pm_logo_hpos, _pm_logo_mhpos, _pm_chip_hpos

VDSLST  = $0200
VVBLKI  = $0222
ATRACT  = $004D
WSYNC   = $D40A
COLPF1  = $D017
COLPF2  = $D018
NMIEN   = $D40E
SETVBV  = $E45C
SYSVBV  = $E45F

HPOSP0  = $D000
HPOSP1  = $D001
HPOSP2  = $D002
HPOSP3  = $D003
HPOSM0  = $D004

        .bss

vbi_save:       .res    2

        .data

_dli_list_bg:   .byte   $0E
_dli_list_fg:   .byte   $00
_dli_foot_bg:   .byte   $78
_dli_foot_fg:   .byte   $0E

        .code

; Fires on the last scanline of row 2; its writes land on row 3.
dli_list:
        pha
        lda     _dli_list_fg
        sta     WSYNC
        sta     COLPF1
        lda     _dli_list_bg
        sta     COLPF2

        ; Park every player and the fifth-player missile in the chip gutter.
        lda     _pm_chip_hpos
        sta     HPOSP0
        sta     HPOSP1
        sta     HPOSP2
        sta     HPOSP3
        sta     HPOSM0

        lda     #<dli_foot
        sta     VDSLST
        lda     #>dli_foot
        sta     VDSLST+1
        pla
        rti

; Fires on the last scanline of row 22; its writes land on row 23.
dli_foot:
        pha
        lda     _dli_foot_fg
        sta     WSYNC
        sta     COLPF1
        lda     _dli_foot_bg
        sta     COLPF2
        lda     #<dli_list
        sta     VDSLST
        lda     #>dli_list
        sta     VDSLST+1
        pla
        rti

; ----------------------------------------------------------------------
; void dli_hw_on (void);
;
; Arm the chain from the top and enable DLI + VBI NMIs.
; ----------------------------------------------------------------------

_dli_hw_on:
        lda     #$40                    ; VBI only while the vector moves
        sta     NMIEN
        lda     #<dli_list
        sta     VDSLST
        lda     #>dli_list
        sta     VDSLST+1
        lda     #$C0                    ; DLI + VBI
        sta     NMIEN
        rts

; ----------------------------------------------------------------------
; void dli_hw_off (void);
; ----------------------------------------------------------------------

_dli_hw_off:
        lda     #$40
        sta     NMIEN
        rts

; ----------------------------------------------------------------------
; Immediate vertical blank hook.
;
; Three jobs, all of which have to happen before the first scanline is drawn
; and none of which C can do, because the program spends its time blocked in
; the keyboard handler or inside SIO.
; ----------------------------------------------------------------------

vbi:
        ; After about nine minutes without a keypress the OS starts rotating
        ; the color shadows to protect the CRT, which would dim the header band
        ; while the DLI-written bands below it stayed put -- a screen that looks
        ; broken rather than idle.
        lda     #$00
        sta     ATRACT

        ; Re-arm the chain from the top. The two interrupts point at each
        ; other, which only stays in phase while exactly two of them fire per
        ; frame -- enabling NMIs mid-screen, as a repaint does, would otherwise
        ; leave the two swapped for good and paint the page in the footer's
        ; colour. Doing it here makes that self-healing.
        lda     #<dli_list
        sta     VDSLST
        lda     #>dli_list
        sta     VDSLST+1

        ; Put the players back over the logo. The row-2 DLI moved them into the
        ; chip gutter, and rows 0-2 are drawn before it fires again.
        lda     _pm_logo_hpos
        sta     HPOSP0
        lda     _pm_logo_hpos+1
        sta     HPOSP1
        lda     _pm_logo_hpos+2
        sta     HPOSP2
        lda     _pm_logo_hpos+3
        sta     HPOSP3
        lda     _pm_logo_mhpos
        sta     HPOSM0

        jmp     SYSVBV                  ; on into the OS vertical blank

; void dli_vbi_install (void);
_dli_vbi_install:
        lda     VVBLKI
        sta     vbi_save
        lda     VVBLKI+1
        sta     vbi_save+1
        ldy     #<vbi
        ldx     #>vbi
        lda     #6                      ; 6 = immediate VBI vector
        jmp     SETVBV

; void dli_vbi_remove (void);
_dli_vbi_remove:
        ldy     vbi_save
        ldx     vbi_save+1
        lda     #6
        jmp     SETVBV
