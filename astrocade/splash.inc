; splash.inc -- the boot screen: the Google Calendar mark, full color.
;
; The four brand colors plus white and black are six colors on a machine
; that shows four; the palette split supplies them. HORCB=14H puts the
; boundary at pixel 80, down the logo's vertical midline: left palette
; {white, Google blue, Google green, black}, right {white, Google red,
; Google yellow, black}. The ring is eight filled rectangles whose pixel
; VALUES are the same on both sides -- the split does the coloring: value
; 1 is blue left of it and red right of it, value 2 green and yellow.
;
; The "31" is the font's own digits scaled 4x: each set glyph bit becomes
; one screen byte (4 pixels) four scanlines tall, so the scaler, like the
; renderer, never shifts. Black is color 3 on both palettes, so the digits
; and the title don't care which side of the split they land on.

SPLASH: SYSSUK  COLSET
        DW      SPALET
        LD      A,HORCBS
        OUT     (HORCB),A
        CALL    CLRSCR
        LD      HL,RECTS
        LD      B,8
SPR1:   PUSH    BC
        LD      A,(HL)          ; fill value
        INC     HL
        LD      E,(HL)          ; address
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)          ; scanlines
        INC     HL
        LD      B,(HL)          ; width
        INC     HL
        PUSH    HL
        EX      DE,HL
        LD      D,C
        LD      E,B
        CALL    SFILL
        POP     HL
        POP     BC
        DJNZ    SPR1
        LD      A,'3'-20H
        LD      DE,NORMEM+28*BYTEPL+15
        CALL    SGLY4
        LD      A,'1'-20H
        LD      DE,NORMEM+28*BYTEPL+21
        CALL    SGLY4
        LD      HL,STITLE
        LD      D,12
        LD      E,12
        LD      C,0CH           ; fg 3: black on both palettes
        JP      TXTAT

; value, address, scanlines, width: the ring. x bytes 12-27 (64px),
; y scanlines 8-71, stroke 8px.
RECTS:  DB      55H
        DW      NORMEM+8*BYTEPL+12
        DB      8,8             ; top left: blue
        DB      55H
        DW      NORMEM+8*BYTEPL+20
        DB      8,8             ; top right: red
        DB      0AAH
        DW      NORMEM+64*BYTEPL+12
        DB      8,8             ; bottom left: green
        DB      0AAH
        DW      NORMEM+64*BYTEPL+20
        DB      8,8             ; bottom right: yellow
        DB      55H
        DW      NORMEM+16*BYTEPL+12
        DB      24,2            ; left wall, upper: blue
        DB      0AAH
        DW      NORMEM+40*BYTEPL+12
        DB      24,2            ; left wall, lower: green
        DB      55H
        DW      NORMEM+16*BYTEPL+26
        DB      24,2            ; right wall, upper: red
        DB      0AAH
        DW      NORMEM+40*BYTEPL+26
        DB      24,2            ; right wall, lower: yellow

; SGLY4: draw glyph index A at 4x scale in black, top-left byte at DE.
SGLY4:  LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,BC           ; A*6
        LD      BC,FONT46
        ADD     HL,BC           ; HL = glyph rows
        LD      C,6
SG4R:   LD      A,(HL)          ; ink in the high nibble, bit 7 leftmost
        INC     HL
        PUSH    HL
        PUSH    DE
        LD      B,4
SG4B:   RLCA
        JR      NC,SG4N
        PUSH    AF
        PUSH    BC
        PUSH    DE
        EX      DE,HL
        LD      A,0FFH          ; pixel value 3 -- black -- four wide
        LD      D,4
        LD      E,1
        CALL    SFILL
        POP     DE
        POP     BC
        POP     AF
SG4N:   INC     DE
        DJNZ    SG4B
        POP     DE
        LD      HL,4*BYTEPL     ; down one scaled glyph row
        ADD     HL,DE
        EX      DE,HL
        POP     HL
        DEC     C
        JR      NZ,SG4R
        RET

STITLE: DB      "Google Calendar",0
