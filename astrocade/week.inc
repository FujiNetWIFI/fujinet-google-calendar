; week.inc -- the week strip, restored with the banked cart.
;
; A density map like the month: the /WEEK listing (whose date column is
; the bare 3-letter weekday) is tallied through the shared parser into
; WKBIN/WKCNT -- leading event's colour and a count per weekday -- and
; seven rows draw from that. Row layout, 40 columns:
;
;   col 0    the leading event's colour chip
;   col 2-3  SU..SA (MWDAYS pairs, matching the wire's first two letters)
;   col 5-6  day of month, inverse when it is today
;   col 8-9  event count (blank when none)
;   col 11+  one block per event, capped at the right edge
;
; Up/Down ride the generic selection bar (NROWS=7); trigger re-anchors to
; the selected day and drills to the day view; 5 composes on the selected
; day; left/right step a whole week (views.inc reuses the month's 7-day
; walk). The rows' dates come from walking CD* back to the week's Sunday
; -- a week straddling a month boundary just works.

; ======================================================================
; WEEK
; ======================================================================
WDRAW:  LD      A,1
        LD      (PMODE),A       ; tally, the same parser walk as the month
        LD      HL,WKBIN        ; clear both tallies
        LD      B,14
        XOR     A
WGC1:   LD      (HL),A
        INC     HL
        DJNZ    WGC1
        LD      HL,SFETCH
        CALL    MSGROW
        LD      A,1             ; REQTYP: listing (/WEEK via VLITS)
        CALL    GCFETCH
        JP      C,WDNET
        CALL    CHROME
        CALL    PLTITL
        CALL    PLHDR
        CALL    PSETB
        CALL    CLKDRW
        CALL    VTAG
WTAL:   CALL    PLEVT
        JR      NC,WTAL

        CALL    DLDANC          ; CD* := the week's Sunday
        CALL    DDOW
        OR      A
        JR      Z,WRO0
        LD      B,A
WSB1:   PUSH    BC
        CALL    DSUBDAY
        POP     BC
        DJNZ    WSB1
WRO0:   XOR     A
        LD      (WTROW),A

WROW:   LD      A,(WTROW)       ; the chip, from the leading event's bin
        LD      E,A
        LD      D,0
        LD      HL,WKCNT
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      Z,WNCHIP
        LD      HL,WKBIN
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      Z,WNCHIP
        LD      B,A
        LD      A,(WTROW)
        ADD     A,CTOP
        LD      D,A
        LD      A,B
        CALL    CHIP
WNCHIP: LD      A,(WTROW)       ; the weekday pair
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,MWDAYS
        ADD     HL,DE
        LD      A,(WTROW)
        ADD     A,CTOP
        LD      D,A
        LD      E,2
        LD      C,XDONK
        LD      A,(HL)
        SUB     20H
        PUSH    HL
        CALL    GLYPH1
        POP     HL
        INC     HL
        INC     E
        LD      A,(HL)
        SUB     20H
        LD      C,XDONK
        CALL    GLYPH1
        CALL    WISTOD          ; the day number, inverse when today
        LD      C,XWONK
        JR      NZ,WDY1
        LD      C,XKONW
WDY1:   LD      A,(WTROW)
        ADD     A,CTOP
        LD      D,A
        LD      E,5
        LD      A,(CDD)
        CALL    PUT2A
        LD      A,(WTROW)       ; the count, blank when none
        LD      E,A
        LD      D,0
        LD      HL,WKCNT
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      Z,WBLKX
        PUSH    HL
        LD      C,XDONK
        LD      A,(WTROW)
        ADD     A,CTOP
        LD      D,A
        LD      E,8
        LD      A,(HL)
        CALL    PUT2A
        POP     HL
        LD      A,(HL)          ; one block per event, capped at the edge
        CP      29
        JR      C,WBLK0
        LD      A,28
WBLK0:  LD      B,A
        LD      A,(WTROW)
        ADD     A,CTOP
        LD      D,A
        LD      E,11
WBLK1:  PUSH    BC
        LD      C,XKONA
        LD      A,'#'-20H
        CALL    GLYPH1
        POP     BC
        INC     E
        DJNZ    WBLK1
WBLKX:  CALL    DADDDAY         ; the next day
        LD      HL,WTROW
        INC     (HL)
        LD      A,(HL)
        CP      7
        JP      C,WROW

        LD      A,7
        LD      (NROWS),A
        LD      HL,SHINTW
        CALL    MSGROW
        JP      VSELX
WDNET:  LD      HL,SNOCONN
        CALL    MSGROW
        CALL    INWAIT
        JP      WDRAW

; WISTOD: Z set when CD* is today. Preserves the walk's registers' spirit
; (clobbers A,HL only).
WISTOD: LD      A,(TODY)
        LD      HL,CDY
        CP      (HL)
        RET     NZ
        LD      A,(TODY+1)
        LD      HL,CDY+1
        CP      (HL)
        RET     NZ
        LD      A,(TODM)
        LD      HL,CDM
        CP      (HL)
        RET     NZ
        LD      A,(TODD)
        LD      HL,CDD
        CP      (HL)
        RET

; WSETANC: anchor := the selected week row's date (Sunday + SELROW).
WSETANC: CALL   DLDANC
        CALL    DDOW
        OR      A
        JR      Z,WSA2
        LD      B,A
WSA1:   PUSH    BC
        CALL    DSUBDAY
        POP     BC
        DJNZ    WSA1
WSA2:   LD      A,(SELROW)
        OR      A
        JR      Z,WSA4
        LD      B,A
WSA3:   PUSH    BC
        CALL    DADDDAY
        POP     BC
        DJNZ    WSA3
WSA4:   JP      DSTANC

; PLWTAL: the week tally, PLEFIN's VIEW==1 landing. C = bin. WKBIN[dow]
; keeps the leading event's bin; WKCNT[dow] saturates at 99. A line whose
; weekday pair matched nothing tallies nowhere.
PLWTAL: LD      A,(PTMP+2)
        OR      A
        RET     Z
        DEC     A
        LD      E,A
        LD      D,0
        LD      HL,WKCNT
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      NZ,PLWT1
        PUSH    HL
        LD      HL,WKBIN
        ADD     HL,DE
        LD      (HL),C
        POP     HL
PLWT1:  LD      A,(HL)
        CP      99
        RET     NC
        INC     (HL)
        RET

SHINTW: DB      "^v DAY =OPEN 5 NEW <> WEEK 0 TODAY",0
