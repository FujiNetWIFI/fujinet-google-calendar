;
; crt0.s -- the NES client's start-up, its two interrupts and the PPU queue.
;
; The cartridge is the FujiNet NES cartridge running this image as an MMC3
; board (mapper 4): 128K PRG, 16K CHR-RAM, 8K WRAM at $6000 (gcal-nes.cfg).
;
; The screen is 32x30 tiles of text on two name tables (vertical mirroring):
; one is shown while the other is drawn, and a flip in the queue swaps them
; in the blank, so a full repaint never shows half drawn (screen.c).
;
; Nothing but these interrupts touches the PPU while it shows a picture. C
; code puts commands in the queue (ppuq, in WRAM) and two drains empty it:
;
;   the IRQ   MMC3's scanline counter at line 215, the bottom of row 26.
;             Rendering goes off there for the rest of the frame -- rows 27
;             and below are the backdrop, which is the theme's ground, so the
;             band is invisible -- and the PPU is free from then on: ~25
;             lines that the vertical blank alone would not have.
;   the NMI   the vertical blank: the sprites' DMA, a pending palette, the
;             rest of the budget, then the display set up for the next frame
;             (name table, scroll, mask, the counter re-armed).
;
; If the IRQ's drain is ever still running when the NMI arrives (it is
; budgeted not to be), the NMI only counts the frame and leaves the PPU to
; the IRQ, which does the NMI's display work itself when it finishes.
;
; MMC3's bank select ($8000) is shared between C (the R6 window, bank.c)
; and the queue's CHR-bank command. C writes the shadow (mmc3_sel) before
; the register; the drain writes the register and then puts the shadow
; back, so a drain landing between C's two writes is harmless.
;
        .export         __STARTUP__ : absolute = 1
        .export         _exit
        .export         _bank_set, _ppu_drain, _ppu_wait_off, _ppu_wait_on
        .exportzp       _qhead, _qtail, _mmc3_sel, _bank_cur, _ppu_on
        .exportzp       _ppu_ctrl, _oam_page, _pal_dirty, _net_busy
        .exportzp       _pad_now, _pad_down, _pad_ack, _kb_busy
        .export         _frames, _oam0, _oam1, _pal_buf, _ppuq
        .import         initlib, donelib, callmain, zerobss, copydata
        .import         _snd_nmi, _spin_nmi
        .import         _chr_bg, _chr_spr, _chr_splash
        .include        "zeropage.inc"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

CTRL_ON   = $88                 ; NMI, sprites at $1000, background at $0000
MASK_ON   = $1E                 ; background and sprites, left column too

BANK_ART  = 8                   ; gcal-nes.cfg
BANK_ART2 = 9

; The bottom of row 26. The counter is clocked once a line by A12 (the
; sprites' fetches from $1000); MAME's cart clocks it from line 0, the
; hardware from the pre-render line, so on hardware this lands a line early
; -- still under row 26's last line, which is blank on every screen.
IRQ_LINE  = 215

; Queue commands (screen.c writes them)
Q_COPY    = 1                   ; hi, lo, n, n bytes
Q_FILL    = 2                   ; hi, lo, n, value
Q_FLIP    = 3                   ; show the other name table (and sprite page)
Q_CHR     = 4                   ; reg, value: an MMC3 CHR bank

; Drain budgets in bytes moved; a command costs its bytes plus 4. No command
; may cost more than the smaller budget (screen.c sends at most 64 bytes).
NMI_BUDGET = 76
IRQ_BUDGET = 110

; ---------------------------------------------------------------------------
; NES 2.0: mapper 4, 128K PRG, 16K CHR-RAM, 8K PRG-RAM, vertical mirroring.
.segment "HEADER"
        .byte   "NES", $1A
        .byte   8               ; PRG: 8 x 16K
        .byte   0               ; CHR-ROM: none
        .byte   $41             ; mapper 4 (low nibble), vertical mirroring
        .byte   $08             ; NES 2.0
        .byte   0, 0            ; mapper high, submapper; ROM size high
        .byte   $07             ; PRG-RAM 64 << 7 = 8K
        .byte   $08             ; CHR-RAM 64 << 8 = 16K
        .byte   0, 0, 0, 0

; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
_qhead:     .res 1              ; the queue: where C writes next
_qtail:     .res 1              ; where the drain reads next
_mmc3_sel:  .res 1              ; C's bank select (always 6: the R6 window)
_bank_cur:  .res 1              ; the bank in R6
_ppu_on:    .res 1              ; 0: rendering off, the PPU is C's (boot)
_ppu_ctrl:  .res 1              ; PPUCTRL for the next frame (bit 0: name table)
_oam_page:  .res 1              ; the sprite page the DMA sends ($02 / $03)
_pal_dirty: .res 1              ; pal_buf waits for the blank
_net_busy:  .res 1              ; a blocking bus call is under way (spinner)
budget:     .res 1
tmp:        .res 1
in_drain:   .res 1
nmi_late:   .res 1
is_off:     .res 1              ; the NMI has turned rendering off
_pad_now:   .res 1              ; the pad as the last NMI read it
_pad_down:  .res 1              ; buttons pressed since input.c last took them
_pad_ack:   .res 1              ; what input.c took, for the NMI to clear
_kb_busy:   .res 1              ; input.c is scanning the keyboard: hands off $4016
pad_last:   .res 1
padtmp:     .res 1              ; the NMI can land inside the IRQ's drain: own temp

.segment "BSS"
_frames:    .res 4
_pal_buf:   .res 32
_ppuq:      .res 256

.segment "OAM0"
_oam0:      .res 256
.segment "OAM1"
_oam1:      .res 256

; ---------------------------------------------------------------------------
.segment "STARTUP"

start:
        sei
        cld
        ldx     #$FF
        txs
        inx
        stx     PPUCTRL         ; NMI off
        stx     PPUMASK
        stx     $4010           ; no DMC interrupt
        lda     #$40
        sta     $4017           ; no APU frame interrupt
        stx     $E000           ; no MMC3 interrupt
        bit     PPUSTATUS
@v1:    bit     PPUSTATUS
        bpl     @v1

        ; MMC3: PRG mode 0 ($8000 R6, $A000 R7, $C000 fixed), CHR mode 0
        ; ($0000 R0, $0800 R1, $1000-$1FFF R2-R5)
        ldy     #0
@mm:    sty     $8000
        lda     mmc3_init,y
        sta     $8001
        iny
        cpy     #8
        bne     @mm
        lda     #$80
        sta     $A001           ; WRAM on, writable
        lda     #0
        sta     $A000           ; vertical mirroring

        ; internal RAM all zero; the sprites off the screen
        lda     #0
        tax
@clr:   sta     $00,x
        sta     $0100,x
        sta     $0400,x
        sta     $0500,x
        sta     $0600,x
        sta     $0700,x
        inx
        bne     @clr
        lda     #$F8
@ofs:   sta     _oam0,x
        sta     _oam1,x
        inx
        bne     @ofs
        lda     #6
        sta     _mmc3_sel
        sta     $8000
        lda     #>_oam0
        sta     _oam_page

@v2:    bit     PPUSTATUS
        bpl     @v2

        jsr     zerobss
        jsr     copydata
        lda     #<$0800         ; the C stack: $0400-$07FF
        ldx     #>$0800
        sta     c_sp
        stx     c_sp+1

        ; the pattern tables, with the display still off
        lda     #BANK_ART
        jsr     _bank_set
        lda     #<_chr_bg
        ldx     #>_chr_bg
        ldy     #$00
        jsr     chr_copy4k
        lda     #<_chr_spr
        ldx     #>_chr_spr
        ldy     #$10
        jsr     chr_copy4k
        ; the large mark's tiles into CHR-RAM 1K banks 8-9, through R1
        lda     #1
        sta     $8000
        lda     #8
        sta     $8001
        lda     #BANK_ART2
        jsr     _bank_set
        lda     #<_chr_splash
        ldx     #>_chr_splash
        ldy     #$08
        jsr     chr_copy2k
        lda     #1
        sta     $8000
        lda     #2
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
        lda     #0
        jsr     _bank_set

        lda     #CTRL_ON
        sta     _ppu_ctrl
        sta     PPUCTRL         ; the frame clock (rendering is still off)
        cli                     ; (the scanline interrupt)
        jsr     initlib
        jsr     callmain
_exit:  jsr     donelib
        jmp     start

mmc3_init:
        .byte   0, 2, 4, 5, 6, 7        ; CHR: background 0-3K, sprites 4-7K
        .byte   0, 7                    ; PRG: R6 bank 0; R7 bank 7, for good

; chr_copy4k / chr_copy2k: (A,X) -> PPU address Y*256, rendering off.
chr_copy4k:
        sta     ptr1
        stx     ptr1+1
        lda     #16
        bne     chr_pages
chr_copy2k:
        sta     ptr1
        stx     ptr1+1
        lda     #8
chr_pages:
        sta     tmp
        bit     PPUSTATUS
        sty     PPUADDR
        lda     #0
        sta     PPUADDR
        ldy     #0
@pg:    lda     (ptr1),y
        sta     PPUDATA
        iny
        bne     @pg
        inc     ptr1+1
        dec     tmp
        bne     @pg
        rts

; ---------------------------------------------------------------------------
.segment "CODE"

; bank_set(bank): the R6 window ($8000-$9FFF) gets PRG bank A.
_bank_set:
        ldx     #6
        stx     _mmc3_sel
        stx     $8000
        sta     $8001
        sta     _bank_cur
        rts

; ppu_wait_off(): turn rendering off at the next blank and wait until it is.
; The queue is then C's to drain directly (ppu_drain(255)).
_ppu_wait_off:
        lda     #0
        sta     is_off
        sta     _ppu_on
@w:     lda     is_off
        beq     @w
        rts

; ppu_wait_on(): rendering back on from the next blank, and wait for it.
_ppu_wait_on:
        lda     #1
        sta     _ppu_on
        lda     _frames
@w:     cmp     _frames
        beq     @w
        rts

; The display for the next frame: name table, scroll 0, rendering on and the
; counter armed for the bottom band.
show:
        bit     PPUSTATUS
        lda     #0
        sta     PPUSCROLL
        sta     PPUSCROLL
        lda     _ppu_ctrl
        sta     PPUCTRL
        lda     #MASK_ON
        sta     PPUMASK
        sta     $E000           ; acknowledge, and off while it is set up
        lda     #IRQ_LINE
        sta     $C000
        sta     $C001           ; reload at the next line clocked
        sta     $E001
        rts

; The blank's PPU work: sprites, the palette, the queue, the display.
blank_work:
        lda     #0
        sta     OAMADDR
        lda     _oam_page
        sta     OAMDMA
        lda     _pal_dirty
        beq     @np
        bit     PPUSTATUS
        lda     #$3F
        sta     PPUADDR
        ldx     #0
        stx     PPUADDR
@pl:    lda     _pal_buf,x
        sta     PPUDATA
        inx
        cpx     #32
        bne     @pl
        lda     #0
        sta     _pal_dirty
        ; The PPU's address is left in the palette: put it back to the
        ; name table, or the ground of the next drained line could show the
        ; colour it points at for a moment (a harmless but visible blip).
        lda     #$20
        sta     PPUADDR
        lda     #0
        sta     PPUADDR
@np:    lda     #NMI_BUDGET
        jsr     drain
        jmp     show

nmi:
        pha
        txa
        pha
        tya
        pha
        inc     _frames
        bne     @c
        inc     _frames+1
        bne     @c
        inc     _frames+2
        bne     @c
        inc     _frames+3
@c:     lda     _ppu_on
        beq     @off
        lda     in_drain
        bne     @late
        jsr     blank_work
        jmp     @tail
@late:  lda     #1
        sta     nmi_late
        jmp     @tail
@off:   lda     #0              ; rendering off: the PPU is C's
        sta     PPUMASK
        sta     $E000
        lda     #1
        sta     is_off
@tail:  jsr     pad_nmi
        jsr     _spin_nmi
        jsr     _snd_nmi
        pla
        tay
        pla
        tax
        pla
        rti

irq:
        pha
        txa
        pha
        tya
        pha
        sta     $E000           ; acknowledge; the NMI re-arms it
        lda     #0
        sta     PPUMASK         ; the bottom band: the backdrop
        lda     #1
        sta     in_drain
        lda     #IRQ_BUDGET
        jsr     drain
        lda     #0
        sta     in_drain
        lda     nmi_late
        beq     @ret
        lda     #0
        sta     nmi_late
        jsr     blank_work
@ret:   pla
        tay
        pla
        tax
        pla
        rti

; ---------------------------------------------------------------------------
; The pad, every frame, from the NMI -- so a press is latched even while the
; program is busy drawing a screen or waiting on the FujiNet, and nothing is
; lost to a long redraw. input.c takes the latched presses and says which it
; took (pad_ack); the NMI clears exactly those. While input.c reads a Famicom
; keyboard through the same $4016 the pad's strobe is on, kb_busy keeps the
; NMI's hands off it (the press is still down next frame).
pad_nmi:
        lda     _kb_busy
        bne     @r
        lda     #1
        sta     $4016
        lda     #0
        sta     $4016
        ldx     #8
@bit:   lda     $4016
        and     #3              ; bit 1: a Famicom's hard-wired pad
        cmp     #1              ; carry = the button
        rol     padtmp
        dex
        bne     @bit
        lda     _pad_ack        ; what input.c took last time
        beq     @new
        eor     #$FF
        and     _pad_down
        sta     _pad_down
        lda     #0
        sta     _pad_ack
@new:   lda     pad_last        ; newly down: now and not before
        eor     #$FF
        and     padtmp
        ora     _pad_down
        sta     _pad_down
        lda     padtmp
        sta     pad_last
        sta     _pad_now
@r:     rts

; ---------------------------------------------------------------------------
; ppu_drain(budget): run queued commands while the budget lasts (255: until
; the queue is empty). Only with the PPU free: from the interrupts, or from C
; between ppu_wait_off and ppu_wait_on.
_ppu_drain:
drain:
        sta     budget
@next:  ldx     _qtail
        cpx     _qhead
        bne     @have
        rts
@have:  lda     _ppuq,x
        cmp     #Q_FLIP
        beq     @flip
        cmp     #Q_CHR
        beq     @chr
        ; COPY or FILL: the cost is n + 4
        txa
        clc
        adc     #3
        tay
        lda     _ppuq,y
        clc
        adc     #4
        ldy     budget
        cpy     #255
        beq     @go
        cmp     budget
        beq     @take
        bcc     @take
        rts                     ; not this time
@take:  sta     tmp
        lda     budget
        sec
        sbc     tmp
        sta     budget
@go:    lda     _ppuq,x         ; the op
        sta     tmp
        inx
        bit     PPUSTATUS
        lda     _ppuq,x
        sta     PPUADDR
        inx
        lda     _ppuq,x
        sta     PPUADDR
        inx
        ldy     _ppuq,x         ; n
        inx
        lda     tmp
        cmp     #Q_FILL
        beq     @fill
@cl:    lda     _ppuq,x
        sta     PPUDATA
        inx
        dey
        bne     @cl
        stx     _qtail
        jmp     @next
@fill:  lda     _ppuq,x
        inx
@fl:    sta     PPUDATA
        dey
        bne     @fl
        stx     _qtail
        jmp     @next

@flip:  inx
        stx     _qtail
        lda     _ppu_ctrl
        eor     #1
        sta     _ppu_ctrl
        lda     _oam_page
        eor     #1              ; $02 <-> $03
        sta     _oam_page
        rts                     ; one flip a frame: the rest waits

@chr:   inx
        lda     _ppuq,x
        sta     $8000
        inx
        lda     _ppuq,x
        sta     $8001
        inx
        lda     _mmc3_sel
        sta     $8000
        stx     _qtail
        jmp     @next

; ---------------------------------------------------------------------------
.segment "VECTORS"
        .word   nmi
        .word   start
        .word   irq

.segment "CLAIM"
        .byte   "FUJI"
        .res    6, $FF
