; clock.inc -- the FujiNet is the wall clock.
;
; Interrupts are off for the program's life and VERAF is lightpen-latched,
; so there is no local time base at all: the view loop counts its own
; ~12 ms iterations and re-fetches the clock about every 30 seconds -- a
; mailbox transaction is microseconds, and the fetch is the truth.
;
; The transaction is device 45H command 49H (APETIME_GET_ISO_LOCAL),
; answering 25 ASCII bytes, YYYY-MM-DDTHH:MM:SS+HHMM, already resolved
; through the FujiNet's [General] timezone. NOT Fuji-device command D2H,
; which the RS232 dispatch does not carry (intv/clock.bas records the
; discovery). Needs [Device] enable_apetime=1.

; MUL10: HL = HL * 10. Clobbers DE.
MUL10:  ADD     HL,HL
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,DE
        RET

; D2AT: A = the value of two ASCII digits at (DE). Clobbers B,DE.
D2AT:   LD      A,(DE)
        SUB     '0'
        ADD     A,A
        LD      B,A
        ADD     A,A
        ADD     A,A
        ADD     A,B             ; *10
        LD      B,A
        INC     DE
        LD      A,(DE)
        SUB     '0'
        ADD     A,B
        RET

; CLKGET: fetch the ISO local time into TOD* / CLK*. Carry on failure.
CLKGET: LD      A,CLKDEV
        LD      E,CLKISO
        LD      L,0
        CALL    FNBEGIN
        LD      B,3
        CALL    FNCOMMIT
        RET     C
        JR      NZ,CKBAD
        LD      A,(FNREPLY)
        CP      FCACK
        JR      NZ,CKBAD
        XOR     A
        LD      (CURSLC),A      ; the cart republished slice 0
        LD      HL,0
        LD      DE,FNRDATA
        LD      B,4
CKY1:   LD      A,(DE)
        SUB     '0'
        LD      C,A
        PUSH    DE
        CALL    MUL10
        POP     DE
        PUSH    BC
        LD      B,0
        ADD     HL,BC
        POP     BC
        INC     DE
        DJNZ    CKY1
        LD      (TODY),HL
        LD      DE,FNRDATA+5
        CALL    D2AT
        LD      (TODM),A
        LD      DE,FNRDATA+8
        CALL    D2AT
        LD      (TODD),A
        LD      DE,FNRDATA+11
        CALL    D2AT
        LD      (CLKH),A
        LD      DE,FNRDATA+14
        CALL    D2AT
        LD      (CLKMI),A
        LD      A,1
        LD      (CLKOK),A
        OR      A
        RET
CKBAD:  SCF
        RET

; CLKTIK: one view-loop iteration's worth of timekeeping. Every ~2400
; iterations (~30 s), re-fetch and redraw the header clock if the minute
; moved. The fetch repaints the reply window, which is fine here: the
; view has finished rendering out of it.
CLKTIK: LD      HL,(TICKS)
        INC     HL
        LD      (TICKS),HL
        LD      DE,2400
        OR      A
        SBC     HL,DE
        RET     C
        LD      HL,0
        LD      (TICKS),HL
        CALL    CLKGET
        RET     C
        LD      A,(CLKMI)
        LD      HL,SYNCM
        CP      (HL)
        RET     Z
        ; falls into CLKDRW

; CLKDRW: the HH:MM cells in the header band.
CLKDRW: LD      A,(CLKOK)
        OR      A
        RET     Z
        LD      A,(CLKMI)
        LD      (SYNCM),A
        LD      D,0
        LD      E,35
        LD      C,XKONA
        LD      A,(CLKH)
        CALL    PUT2A
        LD      A,':'-20H
        CALL    GLYPH1
        INC     E
        LD      A,(CLKMI)
        JP      PUT2A
