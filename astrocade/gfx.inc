; gfx.inc -- the 4x6 renderer, fills and scrolls.
;
; Adapted from fujinet-battleship/astrocade/gfx.inc. The whole port draws on
; one alignment trick: at 2bpp, 4 pixels are one byte, and the character grid
; is 4 pixels wide -- so every glyph row is a single byte write and nothing
; here ever shifts. Text goes through the magic EXPANDER: OUT the fg/bg pair
; to XPAND, reset the expander's nibble flip-flop by writing the MAGIC
; register, and each following write through magic memory (0000H + screen
; offset) paints the HIGH nibble of the data as 4 pixels in those two colors.
; One MAGIC write per row keeps the flip-flop on the high nibble.
;
; Everything runs with interrupts off and draws opaquely (bg included), so
; redraw over live screen never flickers and never needs a clear first.
;
; netcat's copy of this file added the 96-glyph mixed-case range and a
; SCRLUP; this copy keeps the range (event titles are mixed case) and
; drops SCRLUP -- nothing here scrolls, every screen redraws opaquely.
;
; Char grid: 40 cols x 14 rows of 4x6 cells over 84 visible lines.
; Colors (right palette): 0 white page, 1 black ink, 2 Calendar blue,
; 3 gray. Column 0 sits left of the HORCB split on the LEFT palette:
; 0 white, 1 chip red, 2 chip green, 3 chip blue.

MAGEXP  EQU     08H             ; MAGIC value: expand, no shift/rotate/or/xor

; XPAND color pairs (fg*4 + bg). The NAMES are netcat's, kept so edit.inc
; drops in unchanged; the values are this palette's. Read them as: XWONK =
; body text, XKONW = its inverse, XAONK = accent, XKONA = band text,
; XDONK = dim.
XWONK   EQU     04H+0           ; ink on page -- body text
XKONW   EQU     00H+1           ; page on ink -- cursor cell, inverse text
XAONK   EQU     08H+0           ; blue on page -- accents, editor hints
XKONA   EQU     00H+2           ; page on blue -- the header/status bands
XDONK   EQU     0CH+0           ; gray on page -- dim text

; ----------------------------------------------------------------------
; TXTAT: string at HL (NUL-terminated) at char col E, row D, colors C
; (an XPAND pair). TXTN: exactly B cells of the string -- stops copying
; at the NUL and pads the rest with spaces, so fixed-width fields redraw
; opaquely. Anything outside 20H-7FH draws as '?'. Clobbers everything.
TXTAT:  LD      B,0             ; 0: no pad, stop at NUL
        JR      TXT0
TXTN:   LD      A,B
        OR      A
        RET     Z
TXT0:   LD      A,C
        OUT     (XPAND),A
        CALL    TXTPOS
TXT1:   LD      A,(HL)
        OR      A
        JR      Z,TXT3          ; NUL: pad or stop
        INC     HL
        SUB     20H
        CP      60H
        JR      C,TXT4
        LD      A,'?'-20H
TXT4:   CALL    GLYPH
        LD      A,B
        OR      A
        JR      Z,TXT1          ; unbounded (TXTAT)
        DJNZ    TXT1
        RET
TXT3:   LD      A,B
        OR      A
        RET     Z               ; TXTAT: done at NUL
TXT5:   XOR     A               ; TXTN: space-pad the remainder
        CALL    GLYPH
        DJNZ    TXT5
        RET

; GLYPH1: one glyph, index A (0-5FH), at char row D col E in colors C.
; The terminal's per-character path; TXTAT is for whole strings.
; Clobbers everything except D,E,C.
GLYPH1: PUSH    AF
        LD      A,C
        OUT     (XPAND),A
        CALL    TXTPOS
        POP     AF
        JP      GLYPH

; TXTPOS: D,E char row/col -> shadow DE = magic address; preserves the
; primary registers (HL string, B count, C colors).
TXTPOS: PUSH    HL
        PUSH    BC
        LD      A,D
        ADD     A,A
        LD      H,0
        LD      L,A
        LD      BC,ROWTAB
        ADD     HL,BC
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        LD      B,0
        LD      C,E
        ADD     HL,BC           ; HL = row*240 + col
        PUSH    HL
        EXX
        POP     DE              ; shadow DE = magic address
        EXX
        POP     BC
        POP     HL
        RET

; GLYPH: draw glyph index A (0-5FH) at the shadow-DE magic address and
; advance it one cell right. Preserves primary HL/BC (shadow DE is the
; running magic address, so the *6 scratch uses shadow BC).
GLYPH:  EXX
        LD      L,A
        LD      H,0
        ADD     HL,HL           ; A*2
        LD      B,H
        LD      C,L
        ADD     HL,HL           ; A*4
        ADD     HL,BC           ; A*6
        LD      BC,FONT46
        ADD     HL,BC           ; HL = glyph rows
        LD      B,6
GLYRW:  LD      A,MAGEXP
        OUT     (MAGIC),A       ; reset the expander nibble flip-flop
        LD      A,(HL)
        LD      (DE),A          ; one row: 4 pixels
        INC     HL
        LD      A,E             ; down one line
        ADD     A,BYTEPL
        LD      E,A
        JR      NC,GLYRW1
        INC     D
GLYRW1: DJNZ    GLYRW
        LD      A,E             ; back up 6 lines, over 1 col
        SUB     6*BYTEPL-1
        LD      E,A
        JR      NC,GLYRW2
        DEC     D
GLYRW2: EXX
        RET

; ----------------------------------------------------------------------
; CLRSCR: the whole visible screen back to color 0. Clobbers A,BC,DE,HL.
CLRSCR: LD      HL,NORMEM
        LD      DE,NORMEM+1
        LD      BC,LINES*BYTEPL-1
        LD      (HL),0
        LDIR
        RET

; CLRROW: clear char row A (all 40 columns) to color 0.
; Clobbers A,BC,DE,HL.
CLRROW: CALL    ROWADR          ; HL = NORMEM + row*240
        LD      D,H
        LD      E,L
        INC     DE
        LD      (HL),0
        LD      BC,ROWBYT-1
        LDIR
        RET

; CLRRWS: clear B char rows starting at row A. Clobbers A,BC,DE,HL.
CLRRWS: PUSH    BC
        PUSH    AF
        CALL    CLRROW
        POP     AF
        INC     A
        POP     BC
        DJNZ    CLRRWS
        RET

; ROWADR: HL = NORMEM + row A * 240 (a real screen address, not magic).
; Rides ROWTAB rather than multiplying. Clobbers A,DE.
ROWADR: ADD     A,A             ; word index into ROWTAB
        LD      H,0
        LD      L,A
        LD      DE,ROWTAB
        ADD     HL,DE
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        LD      DE,NORMEM
        ADD     HL,DE
        RET

ROWBYT  EQU     6*BYTEPL        ; 240 bytes in one char row

ROWTAB: DW      0*ROWBYT,1*ROWBYT,2*ROWBYT,3*ROWBYT,4*ROWBYT
        DW      5*ROWBYT,6*ROWBYT,7*ROWBYT,8*ROWBYT,9*ROWBYT
        DW      10*ROWBYT,11*ROWBYT,12*ROWBYT,13*ROWBYT
