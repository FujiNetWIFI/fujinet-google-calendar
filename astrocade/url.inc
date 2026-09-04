; url.inc -- streaming the GCAL: devicespec into the TX stream.
;
; 5cardstud's way: a character at a time, never assembled in RAM. The
; scheme carries exactly two slashes -- the view literal brings the third
; (the Intellivision client's documented near-miss) -- and /N goes BEFORE
; the query, which is repeated verbatim on detail and edit opens because
; the adapter numbers events within the ?count=&days= window.
;
; REQTYP: 0 calendar list   N:GCAL://
;         1 listing         N:GCAL://<sel>/<VIEW>/<ISO>       [+ agenda ?]
;         2 detail          N:GCAL://<sel>/<VIEW>/<ISO>/<N>   [+ agenda ?]
;         3 compose write   N:GCAL://<sel>
;         4 edit write      as detail

BLDURL: LD      HL,USCHM
        CALL    FNTXSTR
        LD      A,(REQTYP)
        OR      A
        RET     Z
        LD      HL,CALSEL
        CALL    FNTXSTR         ; empty selector = the primary calendar
        LD      A,(REQTYP)
        CP      3
        RET     Z
        LD      A,(VIEW)
        ADD     A,A
        ADD     A,A
        ADD     A,A             ; the literals sit at an 8-byte stride
        LD      E,A
        LD      D,0
        LD      HL,VLITS
        ADD     HL,DE
        CALL    FNTXSTR
        LD      A,'/'
        CALL    FNTXBYT
        LD      HL,(ANCY)
        CALL    PUTN4
        LD      A,'-'
        CALL    FNTXBYT
        LD      A,(ANCM)
        CALL    PUTN2
        LD      A,'-'
        CALL    FNTXBYT
        LD      A,(ANCD)
        CALL    PUTN2
        LD      A,(REQTYP)
        CP      1
        JR      Z,BUQ
        LD      A,'/'           ; detail and edit: /N
        CALL    FNTXBYT
        LD      HL,(DEVNUM)
        CALL    PUTNUM
BUQ:    LD      A,(VIEW)
        CP      3
        RET     NZ
        LD      HL,UAGQ
        JP      FNTXSTR

; ---- decimal into the TX stream ---------------------------------------
; PND: one digit of HL by DE (1000, 100, ...); HL keeps the remainder.
PND:    LD      B,'0'
PND1:   OR      A
        SBC     HL,DE
        JR      C,PND2
        INC     B
        JR      PND1
PND2:   ADD     HL,DE
        LD      A,B
        JP      FNTXBYT

; PUTN4: HL as four digits, zero-padded (the year).
PUTN4:  LD      DE,1000
        CALL    PND
        LD      DE,100
        CALL    PND
        LD      A,L
        ; falls into PUTN2

; PUTN2: A (0-99) as two digits, zero-padded.
PUTN2:  LD      C,'0'
PN2A:   CP      10
        JR      C,PN2B
        SUB     10
        INC     C
        JR      PN2A
PN2B:   LD      B,A
        LD      A,C
        CALL    FNTXBYT
        LD      A,B
        ADD     A,'0'
        JP      FNTXBYT

; PUTNUM: HL (1-999) with no leading zeros (the event number).
PUTNUM: LD      DE,100
        PUSH    HL
        OR      A
        SBC     HL,DE
        POP     HL
        JR      NC,PNM3
        LD      A,L
        CP      10
        JR      NC,PUTN2        ; two digits, no pad needed above 9
        ADD     A,'0'
        JP      FNTXBYT
PNM3:   CALL    PND             ; the hundreds digit; DE is still 100
        LD      A,L
        JR      PUTN2

USCHM:  DB      "N:GCAL://",0
UAGQ:   DB      "?count=96&days=90",0
VLITS:  DB      "/DAY",0,0,0,0
        DB      "/WEEK",0,0,0
        DB      "/MONTH",0,0
        DB      "/AGENDA",0
