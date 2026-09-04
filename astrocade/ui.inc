; ui.inc -- shared chrome and drawing helpers.
;
; The color pairs live in gfx.inc; the palette in gcal.asm. The two page
; furniture facts every routine here leans on: color 0 is the white page on
; both palettes (so CLRSCR and the border agree), and byte value 0AAH is
; color 2 in all four pixels -- the Calendar-blue band fill.
;
; Column 0 is the chip gutter, left of the HORCB split on the left palette,
; and is never part of a selection bar -- the family rule, held here
; because SELINV's XOR would turn a chip into a different chip.

; ----------------------------------------------------------------------
; SFILL: fill a rectangle by direct writes: A = byte value, HL = screen
; address of the top-left byte, D = scanlines, E = width in bytes.
; Clobbers everything but E.
SFILL:  LD      C,A
SFL:    PUSH    HL
        LD      B,E
SF1:    LD      (HL),C
        INC     HL
        DJNZ    SF1
        POP     HL
        LD      A,L
        ADD     A,BYTEPL
        LD      L,A
        JR      NC,SF2
        INC     H
SF2:    DEC     D
        JR      NZ,SFL
        RET

; ----------------------------------------------------------------------
; DRAWL: draw the byte-coded list at HL. Each entry:
;   DB 0FFH                        end
;   DB 0FEH, row, value            fill the row's 240 bytes with value
;   DB row, col, pair, "text", 0   a string via TXTAT
; Leans on TXTAT returning with HL resting on the string's NUL.
DRAWL:  LD      A,(HL)
        INC     HL
        CP      0FFH
        RET     Z
        CP      0FEH
        JR      Z,DLF
        LD      D,A             ; row
        LD      E,(HL)          ; col
        INC     HL
        LD      C,(HL)          ; pair
        INC     HL
        CALL    TXTAT
        INC     HL              ; past the NUL
        JR      DRAWL
DLF:    LD      A,(HL)          ; row
        INC     HL
        LD      C,(HL)          ; value
        INC     HL
        PUSH    HL
        CALL    ROWADR
        LD      A,C
        LD      D,6
        LD      E,BYTEPL
        CALL    SFILL
        POP     HL
        JR      DRAWL

; ----------------------------------------------------------------------
; WPAL: the working palette and the column-0 gutter split. The splash set
; its own; every road out of it comes through here.
WPAL:   SYSSUK  COLSET
        DW      PALET
        LD      A,HORCBW
        OUT     (HORCB),A
        RET

; ----------------------------------------------------------------------
; CHROME: white page, Calendar-blue header band (rows 0-1) and status band
; (row 13), and the little calendar mark. Screens draw their own text.
CHROME: CALL    CLRSCR
        LD      HL,NORMEM
        LD      A,0AAH
        LD      D,HDRROWS*6
        LD      E,BYTEPL
        CALL    SFILL
        LD      A,STATROW
        CALL    ROWADR
        LD      A,0AAH
        LD      D,6
        LD      E,BYTEPL
        CALL    SFILL
        ; falls into MARK31

; MARK31: a white calendar sheet at rows 0-1, cols 0-1, "31" in blue.
; Column 0 is left of the palette split, so its digit takes the chip blue
; (left color 3); column 1's takes the Calendar blue (right color 2).
MARK31: LD      HL,NORMEM+1*BYTEPL
        LD      D,5             ; sheet top: lines 1-5 white
        LD      E,2
        XOR     A
        CALL    SFILL
        LD      A,'3'-20H
        LD      D,1
        LD      E,0
        LD      C,0CH           ; fg 3 on 0: chip blue on white, col 0
        CALL    GLYPH1
        LD      A,'1'-20H
        LD      D,1
        LD      E,1
        LD      C,XAONK
        JP      GLYPH1

; ----------------------------------------------------------------------
; CHIP: the color chip in the gutter of char row D: A = bin 0-3, 0 clears.
; Scanlines 1-4 of the row, so stacked chips read as separate blocks.
; The fill byte is bin in all four pixels: bin * 55H.
CHIP:   LD      C,A
        LD      A,D
        CALL    ROWADR
        LD      DE,BYTEPL
        ADD     HL,DE
        LD      A,C
        ADD     A,A
        ADD     A,A
        OR      C               ; bin*5
        LD      C,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,A             ; bin*50H
        OR      C               ; bin*55H
        LD      D,4
        LD      E,1
        JP      SFILL

; ----------------------------------------------------------------------
; SELINV: invert char row A as the selection bar. XOR 55H flips color
; 0<->1 -- page<->ink -- in every pixel, is its own undo, and never needs
; the row's text again. Column 0 keeps its chip.
SELINV: CALL    ROWADR
        INC     HL
        LD      D,6
SIL:    LD      B,TCOLS-1
        PUSH    HL
SI1:    LD      A,(HL)
        XOR     55H
        LD      (HL),A
        INC     HL
        DJNZ    SI1
        POP     HL
        LD      A,L
        ADD     A,BYTEPL
        LD      L,A
        JR      NC,SI2
        INC     H
SI2:    DEC     D
        JR      NZ,SIL
        RET

; ----------------------------------------------------------------------
; PUT2A: draw A (0-99) as two digits at row D col E, colors C. E advances
; past them.
PUT2A:  LD      B,0
P2A1:   CP      10
        JR      C,P2A2
        SUB     10
        INC     B
        JR      P2A1
P2A2:   PUSH    AF
        LD      A,B
        ADD     A,'0'-20H
        CALL    GLYPH1
        INC     E
        POP     AF
        ADD     A,'0'-20H
        CALL    GLYPH1
        INC     E
        RET

; ----------------------------------------------------------------------
; MSGROW: HL = message for the status band, over a fresh blue fill.
MSGROW: PUSH    HL
        LD      A,STATROW
        CALL    ROWADR
        LD      A,0AAH
        LD      D,6
        LD      E,BYTEPL
        CALL    SFILL
        POP     HL
        LD      D,STATROW
        LD      E,1
        LD      C,XKONA
        LD      B,TCOLS-1
        JP      TXTN
