; date.inc -- Gregorian arithmetic on the CDY/CDM/CDD workspace triple.
;
; Transcribed from intv/clock.bas, which every client of this app shares:
; real month lengths, the full leap rule (/4, /100, /400), Sakamoto's day
; of week, and month arithmetic that clamps the day when the target month
; is shorter (the adapter rejects an invalid date with an opaque 165).
; Division is repeated subtraction; the largest quotient here is a year
; over 7, a few hundred iterations, microseconds at this clock.

; ---- moving dates through the workspace -------------------------------
DLDANC: LD      HL,(ANCY)
        LD      (CDY),HL
        LD      A,(ANCM)
        LD      (CDM),A
        LD      A,(ANCD)
        LD      (CDD),A
        RET
DSTANC: LD      HL,(CDY)
        LD      (ANCY),HL
        LD      A,(CDM)
        LD      (ANCM),A
        LD      A,(CDD)
        LD      (ANCD),A
        RET
DLDTOD: LD      HL,(TODY)
        LD      (CDY),HL
        LD      A,(TODM)
        LD      (CDM),A
        LD      A,(TODD)
        LD      (CDD),A
        RET

; DIVA: A = HL / DE by repeated subtraction; HL = the remainder.
DIVA:   XOR     A
DVA1:   OR      A
        SBC     HL,DE
        JR      C,DVA2
        INC     A
        JR      DVA1
DVA2:   ADD     HL,DE
        RET

; DLEAP: A = 1 when CDY is a leap year, else 0.
DLEAP:  LD      A,(CDY)
        AND     3
        JR      NZ,DLP0
        LD      HL,(CDY)
        LD      DE,100
        CALL    DIVA
        LD      A,H
        OR      L
        JR      NZ,DLP1         ; /4 but not /100
        LD      HL,(CDY)
        LD      DE,400
        CALL    DIVA
        LD      A,H
        OR      L
        JR      Z,DLP1          ; /400
DLP0:   XOR     A
        RET
DLP1:   LD      A,1
        RET

; DDIM: A = days in CDM/CDY.
DDIM:   LD      A,(CDM)
        CP      2
        JR      Z,DDIMF
        LD      E,A
        LD      D,0
        LD      HL,MLENT-1
        ADD     HL,DE
        LD      A,(HL)
        RET
DDIMF:  CALL    DLEAP
        ADD     A,28
        RET
MLENT:  DB      31,28,31,30,31,30,31,31,30,31,30,31

; ---- one day either way, rolling months and years ---------------------
DADDDAY: CALL   DDIM
        LD      HL,CDD
        INC     (HL)
        CP      (HL)
        RET     NC
        LD      (HL),1
        LD      A,(CDM)
        INC     A
        CP      13
        JR      C,DAD1
        LD      A,1
        LD      HL,(CDY)
        INC     HL
        LD      (CDY),HL
DAD1:   LD      (CDM),A
        RET
DSUBDAY: LD     HL,CDD
        DEC     (HL)
        RET     NZ
        LD      A,(CDM)
        DEC     A
        JR      NZ,DSB1
        LD      A,12
        LD      HL,(CDY)
        DEC     HL
        LD      (CDY),HL
DSB1:   LD      (CDM),A
        CALL    DDIM
        LD      (CDD),A
        RET

; ---- one month either way, clamping the day ---------------------------
DADDMON: LD     A,(CDM)
        INC     A
        CP      13
        JR      C,DAM1
        LD      A,1
        LD      HL,(CDY)
        INC     HL
        LD      (CDY),HL
DAM1:   LD      (CDM),A
        JR      DCLMP
DSUBMON: LD     A,(CDM)
        DEC     A
        JR      NZ,DSM1
        LD      A,12
        LD      HL,(CDY)
        DEC     HL
        LD      (CDY),HL
DSM1:   LD      (CDM),A
DCLMP:  CALL    DDIM            ; Jan 31 plus a month is Feb 28/29
        LD      HL,CDD
        LD      B,A
        LD      A,(HL)
        CP      B
        RET     C
        LD      (HL),B
        RET

; DDOW: A = day of week for the triple, 0 = Sunday. Sakamoto's method:
; y -= (m < 3); (y + y/4 - y/100 + y/400 + t[m-1] + d) mod 7.
DDOW:   LD      HL,(CDY)
        LD      A,(CDM)
        CP      3
        JR      NC,DWK1
        DEC     HL
DWK1:   LD      (PTMP),HL
        LD      D,H
        LD      E,L
        SRL     D
        RR      E
        SRL     D
        RR      E
        ADD     HL,DE
        LD      (PTMP+2),HL     ; y + y/4
        LD      HL,(PTMP)
        LD      DE,100
        CALL    DIVA            ; A = y/100
        LD      C,A
        SRL     A
        SRL     A               ; y/400
        LD      B,A
        LD      A,C
        SUB     B
        LD      C,A             ; y/100 - y/400
        LD      B,0
        LD      HL,(PTMP+2)
        OR      A
        SBC     HL,BC
        LD      A,(CDM)
        LD      E,A
        LD      D,0
        PUSH    HL
        LD      HL,SAKT-1
        ADD     HL,DE
        LD      A,(HL)
        POP     HL
        LD      E,A
        ADD     HL,DE
        LD      A,(CDD)
        LD      E,A
        ADD     HL,DE
        LD      DE,7
        CALL    DIVA
        LD      A,L
        RET
SAKT:   DB      0,3,2,5,0,3,5,1,4,6,2,4
