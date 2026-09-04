; views.inc -- the calendar views and the app's home loop.
;
; The view loop is the program's only polling loop -- the clock has to
; advance and (later) alarms have to fire while nobody touches a key;
; every other screen blocks in INWAIT. Repaint policy is main.c's two
; flags: DIRTY means refetch, and fetches happen only here, never inside
; key handling.
;
; Phase 2 renders every view through the generic list renderer; the week
; strip and month grid layouts land in phase 3.

; VIEWLP: the home loop.
VIEWLP: LD      A,(DIRTY)
        OR      A
        JR      Z,VPOLL
        CALL    VDRAW
VPOLL:  CALL    DELAY10
        CALL    CLKTIK
        CALL    INPOLR
        OR      A
        JR      Z,VPOLL
        CP      EVD0
        JR      C,VKEV
        SUB     EVD0            ; a digit
        JR      Z,VKTOD         ; 0: today
        CP      4
        JR      C,VKVIEW        ; 1-3: day, month, agenda
        CP      5
        JP      Z,VKNEW         ; 5: compose
        CP      6
        JP      Z,VKEDIT        ; 6: edit the selection
        JP      VPOLL
VKVIEW: DEC     A               ; 0,1,2 index the view table
        LD      E,A
        LD      D,0
        LD      HL,KVIEW
        ADD     HL,DE
        LD      A,(HL)
        LD      (VIEW),A
        JR      VDIRT
KVIEW:  DB      0,2,3           ; VIEW 1 (week) was dropped for ROM budget
VKTOD:  CALL    DLDTOD
        CALL    DSTANC
VDIRT:  LD      A,1
        LD      (DIRTY),A
        XOR     A
        LD      (SELROW),A
        LD      (FIRST),A
        LD      (FIRST+1),A
        JR      VIEWLP
VKEV:   CP      EVUP
        JP      Z,VKUP
        CP      EVDOWN
        JP      Z,VKDN
        CP      EVLEFT
        JP      Z,VKPRV
        CP      EVRIGHT
        JP      Z,VKNXT
        CP      EVMR
        JP      Z,VDIRT
        CP      EVTRIG
        JP      Z,VOPEN
        CP      EVEQ
        JP      Z,VOPEN
        JP      VPOLL

; VOPEN: trigger / =. Month drills to the day view on the cursor; day and
; agenda open the selected event's detail.
VOPEN:  LD      A,(VIEW)
        CP      2
        JR      Z,VODAY
        LD      A,(NROWS)
        OR      A
        JP      Z,VPOLL
        LD      A,(SELROW)      ; DEVNUM = META[SELROW].evnum
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,META
        ADD     HL,DE
        LD      A,(HL)
        LD      (DEVNUM),A
        INC     HL
        LD      A,(HL)
        LD      (DEVNUM+1),A
        CALL    DETAIL
        JR      C,VOEDIT        ; the detail asked to edit
        LD      A,1
        LD      (DIRTY),A
        JP      VIEWLP
VOEDIT: LD      A,1             ; edit the open event (phase 5 form)
        LD      (FMEDIT),A
        JP      VFORM
VODAY:  XOR     A
        LD      (VIEW),A
        JP      VDIRT

; VKNEW: compose a new event on the anchor date.
VKNEW:  XOR     A
        LD      (FMEDIT),A
        JP      VFORM
; VKEDIT: edit the selected event (day/agenda only).
VKEDIT: LD      A,(VIEW)
        CP      2
        JP      Z,VPOLL
        LD      A,(NROWS)
        OR      A
        JP      Z,VPOLL
        LD      A,(SELROW)
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,META
        ADD     HL,DE
        LD      A,(HL)
        LD      (DEVNUM),A
        INC     HL
        LD      A,(HL)
        LD      (DEVNUM+1),A
        LD      A,1
        LD      (FMEDIT),A
        JP      VFORM

; ---- left/right: every remaining view steps a day
VKPRV:  CALL    DLDANC
        CALL    DSUBDAY
VKST:   CALL    DSTANC
        JP      VDIRT
VKNXT:  CALL    DLDANC
        CALL    DADDDAY
        JR      VKST

; ---- up/down: day/agenda move the selection bar; month moves a week;
; week has no per-row selection yet.
VKUP:   LD      A,(VIEW)
        CP      2
        JR      Z,VKUPM
        LD      A,(SELROW)
        OR      A
        JP      Z,VPOLL
        CALL    VSELX
        LD      HL,SELROW
        DEC     (HL)
        CALL    VSELX
        JP      VPOLL
VKUPM:  CALL    DLDANC
        LD      B,7
VKUM1:  PUSH    BC
        CALL    DSUBDAY
        POP     BC
        DJNZ    VKUM1
        JP      VKST
VKDN:   LD      A,(VIEW)
        CP      2
        JR      Z,VKDNM
        LD      A,(NROWS)
        LD      B,A
        LD      A,(SELROW)
        INC     A
        CP      B
        JP      NC,VPOLL
        CALL    VSELX
        LD      HL,SELROW
        INC     (HL)
        CALL    VSELX
        JP      VPOLL
VKDNM:  CALL    DLDANC
        LD      B,7
VKDM1:  PUSH    BC
        CALL    DADDDAY
        POP     BC
        DJNZ    VKDM1
        JP      VKST

; VSELX: toggle the bar on the selected row (SELINV is its own undo).
VSELX:  LD      A,(NROWS)
        OR      A
        RET     Z
        LD      A,(SELROW)
        ADD     A,CTOP
        JP      SELINV

; ----------------------------------------------------------------------
; VDRAW: render the current view. Week and month are density maps with
; their own renderers; day and agenda share the event-list renderer.
VDRAW:  XOR     A
        LD      (DIRTY),A
        LD      A,(VIEW)
        CP      2
        JP      Z,MDRAW
        ; day / agenda: the list renderer
VDLIST: XOR     A
        LD      (PMODE),A       ; render rows, don't tally
        LD      HL,SFETCH
        CALL    MSGROW
        LD      A,1             ; REQTYP: listing
        CALL    GCFETCH
        JR      NC,VDOK
        LD      HL,SNOCONN
        CALL    MSGROW
        CALL    INWAIT          ; any key retries
        JR      VDLIST
VDOK:   NOP
        CALL    CHROME
        CALL    PLTITL          ; the window title, as it streams
        CALL    PLHDR
        CALL    PSETB
        CALL    CLKDRW
        CALL    VTAG
        CALL    VHEADS
        LD      A,CTOP
        LD      (CROW),A
        XOR     A
        LD      (NROWS),A
VDR1:   LD      A,(CROW)
        CP      CTOP+CROWS
        JR      NC,VDR2
        CALL    PLEVT
        JR      NC,VDR1
VDR2:   LD      HL,SHINT1
        CALL    MSGROW
        LD      A,(NROWS)
        OR      A
        RET     Z
        LD      B,A             ; clamp the selection to the page
        LD      A,(SELROW)
        CP      B
        JR      C,VDR3
        LD      A,B
        DEC     A
        LD      (SELROW),A
VDR3:   JP      VSELX

; VTAG: row 1: the view's name (the calendar picker was dropped, so there
; is only ever the one calendar to name).
VTAG:   LD      A,(VIEW)
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,VLITS+1      ; past the '/'
        ADD     HL,DE
        LD      D,1
        LD      E,3
        LD      C,XKONA
        JP      TXTAT

; VHEADS: row 2 column heads for the list shape.
VHEADS: LD      HL,STIME
        LD      D,RULEROW
        LD      E,2
        LD      C,XDONK
        CALL    TXTAT
        LD      HL,SEVENT
        LD      D,RULEROW
        LD      E,14
        LD      C,XDONK
        JP      TXTAT

SFETCH: DB      "Fetching...",0
SNOCONN: DB     "No connection",0
STIME:  DB      "TIME",0
SEVENT: DB      "EVENT",0
SHINT1: DB      "1-3 VIEW 0 TODAY 5 NEW 6 EDIT",0
