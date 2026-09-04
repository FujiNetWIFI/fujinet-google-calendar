; month.inc -- the month grid and the week strip.
;
; Both are density maps, not event lists, so both tally the whole listing
; into a small per-day array and draw from it -- events arrive sorted by
; start, so a day's first line is its leading event and its colour. The
; month keeps 31 bytes (count in the high six bits, bin in the low two);
; the week keeps a leading bin and a count per weekday.
;
; The month cursor IS the anchor day (the Intellivision's rule), so a
; left/right step is date arithmetic in views.inc and rolling into a
; neighbour month costs nothing here. Mid-screen marks use the right
; palette (black/blue/gray) because the coloured chip gutter is column 0
; only, so a month cell shows density, not event colour.

WKBIN   EQU     META            ; 7: week, leading bin per weekday
WKCNT   EQU     META+7          ; 7: week, event count per weekday

; ======================================================================
; MONTH
; ======================================================================
MDRAW:  LD      A,1
        LD      (PMODE),A       ; walk the shared parser in tally mode
        CALL    MGCLR
        LD      HL,SFETCH
        CALL    MSGROW
        LD      A,1
        CALL    GCFETCH
        JP      C,MDNET
        CALL    CHROME
        CALL    PLTITL
        CALL    PLHDR
        CALL    PSETB
        CALL    CLKDRW
        CALL    VTAG
MTAL:   CALL    PLEVT
        JR      NC,MTAL
        CALL    MGEOM
        CALL    MHEADS
        CALL    MCELLS
        CALL    MSUM
        LD      HL,SHINTM
        JP      MSGROW
MDNET:  LD      HL,SNOCONN
        CALL    MSGROW
        CALL    INWAIT
        JR      MDRAW

MGCLR:  LD      HL,MGRID
        LD      B,31
        XOR     A
MGC1:   LD      (HL),A
        INC     HL
        DJNZ    MGC1
        RET

MGEOM:  CALL    DLDANC
        LD      A,1
        LD      (CDD),A
        CALL    DDOW
        LD      (MFIRST),A
        CALL    DDIM
        LD      (MNDAYS),A
        RET

; MHEADS: the weekday rule, pitch 5 from col 2.
MHEADS: LD      HL,MWDAYS
        LD      E,2
        LD      B,7
MHD1:   PUSH    BC
        LD      A,(HL)
        SUB     20H
        LD      D,RULEROW
        LD      C,XDONK
        PUSH    HL
        CALL    GLYPH1
        POP     HL
        INC     HL
        INC     E
        LD      A,(HL)
        SUB     20H
        LD      D,RULEROW
        LD      C,XDONK
        PUSH    HL
        CALL    GLYPH1
        POP     HL
        INC     HL
        LD      A,E
        ADD     A,4
        LD      E,A
        POP     BC
        DJNZ    MHD1
        RET

; MCELLS: draw days 1..MNDAYS. p = MFIRST + day - 1; row p/7, col p%7;
; cell top-left at (CTOP+row, 2+col*5): tens, units, then the mark cell.
MCELLS: LD      A,1
        LD      (MCDAY),A
MCL1:   LD      A,(MNDAYS)
        LD      HL,MCDAY
        CP      (HL)
        RET     C               ; past the last day
        LD      A,(MFIRST)
        ADD     A,(HL)
        DEC     A               ; p = MFIRST + day - 1
        LD      C,0
MCPR:   CP      7
        JR      C,MCPC
        SUB     7
        INC     C
        JR      MCPR
MCPC:   LD      L,A             ; col in L, row in C
        ADD     A,A
        ADD     A,A
        ADD     A,L             ; col*5
        ADD     A,2
        LD      (MCCOL),A
        LD      A,C
        ADD     A,CTOP
        LD      (MCROW),A
        LD      A,(MCDAY)       ; the digit pair: selected day inverse
        LD      HL,ANCD
        CP      (HL)
        LD      A,XWONK
        JR      NZ,MCP1
        LD      A,XKONW
MCP1:   LD      (MCPAIR),A
        LD      A,(MCDAY)       ; split into tens/units
        LD      B,0
MCTU:   CP      10
        JR      C,MCTD
        SUB     10
        INC     B
        JR      MCTU
MCTD:   LD      C,A             ; C = units, B = tens
        LD      A,(MCROW)
        LD      D,A
        LD      A,(MCCOL)
        LD      E,A
        LD      A,(MCPAIR)
        PUSH    BC
        LD      C,A             ; pair for GLYPH1
        LD      A,B
        OR      A
        LD      A,' '           ; blank tens under 10
        JR      Z,MCTG
        LD      A,B
        ADD     A,'0'
MCTG:   SUB     20H
        CALL    GLYPH1
        POP     BC
        INC     E               ; units cell; D (row) preserved by GLYPH1
        LD      A,(MCPAIR)
        LD      C,A
        LD      A,(MCDAY)
MCUU:   CP      10
        JR      C,MCUD
        SUB     10
        JR      MCUU
MCUD:   ADD     A,'0'-20H
        CALL    GLYPH1
        CALL    MMARK
        LD      HL,MCDAY
        INC     (HL)
        JP      MCL1

; MMARK: the mark cell for day (MCDAY) at (MCROW, MCCOL+2). Today gets a
; blue tile; else density -- 0 dim '.', 1-2 '+', 3+ '#'.
MMARK:  LD      A,(MCDAY)
        DEC     A
        LD      E,A
        LD      D,0
        LD      HL,MGRID
        ADD     HL,DE
        LD      A,(HL)
        SRL     A
        SRL     A               ; count
        LD      B,A
        LD      A,(MCROW)
        LD      D,A
        LD      A,(MCCOL)
        ADD     A,2
        LD      E,A
        CALL    MISTOD
        JR      NZ,MMK1
        LD      C,XKONA         ; today: blue tile
        LD      A,'#'-20H
        JP      GLYPH1
MMK1:   LD      A,B
        OR      A
        JR      NZ,MMK2
        LD      C,XDONK         ; nothing scheduled
        LD      A,'.'-20H
        JP      GLYPH1
MMK2:   LD      C,XWONK
        LD      A,B
        CP      3
        LD      A,'+'-20H
        JR      C,MMKG
        LD      A,'#'-20H
MMKG:   JP      GLYPH1

; MISTOD: Z set when day (MCDAY) in the anchor month is today.
MISTOD: LD      A,(TODY)
        LD      HL,ANCY
        CP      (HL)
        RET     NZ
        LD      A,(TODY+1)
        LD      HL,ANCY+1
        CP      (HL)
        RET     NZ
        LD      A,(TODM)
        LD      HL,ANCM
        CP      (HL)
        RET     NZ
        LD      A,(TODD)
        LD      HL,MCDAY
        CP      (HL)
        RET

; MSUM: rows 10-11, the selected day's summary: "We 16 Sep  N EVENTS".
; MSUM: the selected day's event count, "DAY NN: C EVENTS".
MSUM:   LD      HL,SSELD
        LD      D,10
        LD      E,2
        LD      C,XWONK
        CALL    TXTAT
        LD      D,10
        LD      E,6
        LD      C,XWONK
        LD      A,(ANCD)
        CALL    PUT2A
        LD      A,(ANCD)        ; the count from MGRID
        DEC     A
        LD      E,A
        LD      D,0
        LD      HL,MGRID
        ADD     HL,DE
        LD      A,(HL)
        SRL     A
        SRL     A
        LD      D,10
        LD      E,9
        LD      C,XWONK
        CALL    PUT2A
        LD      HL,SEVTS
        LD      D,10
        LD      E,12
        LD      C,XWONK
        JP      TXTAT

MWDAYS: DB      "SuMoTuWeThFrSa"
SHINTM: DB      "ARROWS 0 TODAY =OPEN 5NEW",0
SEVTS:  DB      "EVENTS",0
SSELD:  DB      "DAY",0
