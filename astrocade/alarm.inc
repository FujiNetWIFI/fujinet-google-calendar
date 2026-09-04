; alarm.inc -- synthesised alarms, restored with the banked cart.
;
; The adapter's field mask never carries reminders (src/alarm.c explains
; why), so alarms are synthesised client-side over the parsed rows: in the
; day view, on today only, an un-fired timed event whose start is within
; ALLEAD minutes of now rings once -- the chime, and a banner on the
; status row showing the start time (nothing is buffered on this machine,
; so the time stands in for the title). Any key dismisses the banner and
; is swallowed (views.inc); a refetch rebuilds the rows and with them the
; fired flags -- deliberate, matching the C and Intellivision clients.

ALBANN  EQU     200             ; banner life in poll ticks (a few seconds)

; ALTICK: one poll tick from VIEWLP. Counts a live banner down, restoring
; the hint row when it lapses; otherwise scans for a due event. At most
; one alarm fires per tick.
ALTICK: LD      A,(ALSTATE)
        OR      A
        JR      Z,ALSCAN
        DEC     A
        LD      (ALSTATE),A
        RET     NZ
        LD      HL,SHINT1       ; lapsed: the hint row back (day view only)
        JP      MSGROW
ALSCAN: LD      A,(VIEW)
        OR      A
        RET     NZ              ; the day view only...
        LD      A,(CLKOK)
        OR      A
        RET     Z
        LD      A,(TODY)        ; ...and only when the anchor IS today
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
        LD      HL,ANCD
        CP      (HL)
        RET     NZ
        LD      A,(NROWS)
        OR      A
        RET     Z
        LD      B,A
        LD      A,(CLKH)        ; PTMP := now, in minutes since midnight
        CALL    ALM60
        LD      A,(CLKMI)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      (PTMP),HL
        LD      HL,META
ALS1:   CALL    ALROW
        RET     C               ; fired: one per tick is plenty
        LD      DE,MROWSZ
        ADD     HL,DE
        DJNZ    ALS1
        RET

; ALROW: is the META row at HL due? Fires it if so (carry set). Preserves
; HL and B.
ALROW:  PUSH    HL
        INC     HL
        INC     HL
        LD      A,(HL)          ; flags: all-day and already-fired skip
        AND     MFALLD+MFFIRED
        JR      NZ,ALRNO
        INC     HL
        INC     HL
        INC     HL
        INC     HL              ; +6: start hour, +7 minute
        LD      A,(HL)
        INC     HL
        LD      C,(HL)
        CALL    ALM60
        LD      E,C
        LD      D,0
        ADD     HL,DE           ; HL = start, in minutes
        LD      DE,(PTMP)
        OR      A
        SBC     HL,DE           ; dt = start - now
        JR      C,ALRNO         ; already started
        LD      A,H
        OR      A
        JR      NZ,ALRNO
        LD      A,(ALLEAD)
        CP      L
        JR      C,ALRNO         ; dt > lead
        POP     HL              ; FIRE: mark the row, bank its time
        PUSH    HL
        PUSH    BC
        INC     HL
        INC     HL
        LD      A,(HL)
        OR      MFFIRED
        LD      (HL),A
        INC     HL
        INC     HL
        INC     HL
        INC     HL
        LD      A,(HL)
        LD      (ALH),A
        INC     HL
        LD      A,(HL)
        LD      (ALMN),A
        LD      A,ALBANN        ; the banner, the time, the chime
        LD      (ALSTATE),A
        LD      HL,SALARM
        CALL    MSGROW
        LD      D,STATROW
        LD      E,10
        LD      C,XKONA
        LD      A,(ALH)
        CALL    PUT2A
        LD      A,':'-20H
        LD      C,XKONA
        CALL    GLYPH1
        INC     E
        LD      A,(ALMN)
        LD      C,XKONA
        CALL    PUT2A
        CALL    SNDAL
        POP     BC
        POP     HL
        SCF
        RET
ALRNO:  POP     HL
        OR      A
        RET

; ALM60: HL = A * 60. Clobbers A,DE.
ALM60:  LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL           ; 4A
        LD      E,L
        LD      D,H
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL           ; 64A
        OR      A
        SBC     HL,DE
        RET

SALARM: DB      "! ALARM",0
