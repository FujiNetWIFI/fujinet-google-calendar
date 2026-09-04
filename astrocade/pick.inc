; pick.inc -- the calendar picker and the settings screen.
;
; The picker fetches the calendar list (REQTYP 0: no title line, a dashed
; header then one calendar a line, the name in the first 53 columns) and
; renders straight from the window; picking re-scans the still-valid
; window for the chosen line's name. Index 0 is "all calendars" -- an
; empty selector. Settings edits the alarm lead and the calendar and
; persists both to the appkey; it also shows the timezone and the clock,
; because a wrong POSIX TZ is the usual reason "today" looks empty.

; ======================================================================
; PICKER -- returns with CALSEL possibly changed and the appkey saved.
; ======================================================================
PICKER: XOR     A
        LD      (SELROW),A
PKDRW:  LD      A,0             ; REQTYP: calendar list
        CALL    GCFETCH
        JP      C,PKERR
        CALL    CHROME
        CALL    CLKDRW
        LD      HL,SPKH
        LD      D,1
        LD      E,3
        LD      C,XKONA
        CALL    TXTAT
        CALL    PKGETL          ; consume the dashed header line
        LD      HL,SPKALL       ; row 3: the "all calendars" choice
        LD      D,CTOP
        LD      E,2
        LD      C,XWONK
        CALL    TXTAT
        LD      C,1             ; screen row counter / calendar index
PKD1:   CALL    PKGETL
        JR      C,PKD2
        LD      A,C
        CP      9               ; rows 4..11: up to 8 calendars
        JR      NC,PKD2
        LD      A,C
        ADD     A,CTOP
        LD      D,A
        LD      E,2
        PUSH    BC
        LD      HL,LINEBUF
        LD      C,XWONK
        LD      B,36
        CALL    TXTN
        POP     BC
        INC     C
        JR      PKD1
PKD2:   LD      A,C
        DEC     A
        LD      (PKN),A         ; number of calendars listed
        LD      HL,SPKHT
        CALL    MSGROW
        CALL    PKSEL           ; light the selection bar
PKKEY:  CALL    INWAIT
        CP      EVUP
        JR      Z,PKUP
        CP      EVDOWN
        JR      Z,PKDN
        CP      EVEQ
        JR      Z,PKPICK
        CP      EVTRIG
        JR      Z,PKPICK
        CP      EVCLR
        JR      Z,PKBK
        CP      EVCE
        JR      Z,PKBK
        JR      PKKEY
PKUP:   LD      A,(SELROW)
        OR      A
        JR      Z,PKKEY
        CALL    PKSEL
        LD      HL,SELROW
        DEC     (HL)
        CALL    PKSEL
        JR      PKKEY
PKDN:   LD      A,(PKN)
        LD      B,A
        LD      A,(SELROW)
        CP      B
        JR      NC,PKKEY
        CALL    PKSEL
        LD      HL,SELROW
        INC     (HL)
        CALL    PKSEL
        JR      PKKEY
PKBK:   RET
PKPICK: LD      A,(SELROW)
        OR      A
        JR      NZ,PKP1
        XOR     A               ; index 0: all calendars
        LD      (CALSEL),A
        JR      PKPSAVE
PKP1:   CALL    GCFETCH0Q       ; re-fetch (window may be stale after INWAIT?)
        ; the window has not seen a transaction since PKDRW, so it is still
        ; valid; walk it to the chosen calendar's name.
        CALL    PKGETL          ; header
        LD      A,(SELROW)
        DEC     A               ; calendars are 1-based on screen
        JR      Z,PKP3
        LD      B,A
PKP2:   PUSH    BC
        CALL    PKGETL
        POP     BC
        DJNZ    PKP2
PKP3:   CALL    PKGETL          ; the chosen line -> LINEBUF
        LD      HL,LINEBUF      ; copy, trimming trailing spaces, to CALSEL
        LD      DE,CALSEL
        LD      B,39
PKPC:   LD      A,(HL)
        OR      A
        JR      Z,PKPT
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    PKPC
PKPT:   XOR     A
        LD      (DE),A
PKPTR:  DEC     DE              ; trim trailing spaces
        LD      A,(DE)
        CP      ' '
        JR      NZ,PKPSAVE
        XOR     A
        LD      (DE),A
        LD      HL,CALSEL
        LD      A,E
        CP      L
        JR      NZ,PKPTR
PKPSAVE: CALL   AKSAVE
        LD      A,1
        LD      (DIRTY),A
        RET
PKERR:  LD      HL,SNOCONN
        CALL    MSGROW
        CALL    INWAIT
        RET

; GCFETCH0Q: re-open the calendar list only if a transaction has happened
; since (it has not, within the picker), so this is just a re-fetch to
; guarantee a fresh, rewound stream before the pick walk.
GCFETCH0Q: LD   A,0
        JP      GCFETCH

; PKSEL: toggle the selection bar on the current row (row 3 = ALL).
PKSEL:  LD      A,(SELROW)
        ADD     A,CTOP
        JP      SELINV

; PKGETL: one calendar-list line into LINEBUF, up to 53 chars, NUL-term.
; The char is held in D across the width test, since GETCH cannot un-read.
; Carry set when the stream is exhausted.
PKGETL: LD      HL,LINEBUF
        LD      C,0
PKL1:   CALL    GETCH
        JR      C,PKLE
        LD      D,A             ; hold it
        CP      0DH
        JR      Z,PKL1
        CALL    EOLP
        JR      Z,PKLD
        LD      A,C
        CP      53
        JR      NC,PKL1         ; past the name column: swallow to EOL
        LD      A,D
        LD      (HL),A
        INC     HL
        INC     C
        JR      PKL1
PKLD:   LD      (HL),0
        OR      A
        RET
PKLE:   LD      A,C
        OR      A
        JR      Z,PKLE0
        LD      (HL),0
        OR      A
        RET
PKLE0:  SCF
        RET

SPKH:   DB      "PICK CALENDAR",0
SPKALL: DB      "(All calendars)",0
SPKHT:  DB      "UP/DN  =PICK  C BACK",0

; ======================================================================
; SETTINGS
; ======================================================================
SETUP:  CALL    CHROME
        LD      HL,SSTH
        LD      D,1
        LD      E,3
        LD      C,XKONA
        CALL    TXTAT
        CALL    CLKDRW
STDRW:  LD      HL,SSTLEAD
        LD      D,4
        LD      E,2
        LD      C,XWONK
        CALL    TXTAT
        LD      A,(ALLEAD)
        LD      D,4
        LD      E,14
        LD      C,XWONK
        CALL    PUT2A
        LD      HL,SSTMIN
        LD      D,4
        LD      E,17
        LD      C,XWONK
        CALL    TXTAT
        LD      HL,SSTCAL
        LD      D,6
        LD      E,2
        LD      C,XWONK
        CALL    TXTAT
        LD      A,(CALSEL)
        OR      A
        LD      HL,SALL
        JR      Z,STC1
        LD      HL,CALSEL
STC1:   LD      D,6
        LD      E,12
        LD      C,XWONK
        LD      B,26
        CALL    TXTN
        LD      HL,SSTTZ
        LD      D,8
        LD      E,2
        LD      C,XWONK
        CALL    TXTAT
        CALL    TZGET
        JR      C,STTZ1
        LD      HL,LINEBUF
        LD      D,8
        LD      E,12
        LD      C,XWONK
        LD      B,26
        CALL    TXTN
STTZ1:  LD      HL,SSTHT
        CALL    MSGROW
STKEY:  CALL    INWAIT
        CP      EVUP
        JR      Z,STUP
        CP      EVDOWN
        JR      Z,STDN
        CP      EVD0+1          ; '1' -> the picker
        JR      Z,STPICK
        CP      EVTRIG
        JR      Z,STPICK
        CP      EVCLR
        JR      Z,STSAVE
        CP      EVCE
        JR      Z,STSAVE
        JR      STKEY
STUP:   LD      A,(ALLEAD)
        CP      60
        JR      NC,STKEY
        INC     A
        LD      (ALLEAD),A
        JP      STDRW
STDN:   LD      A,(ALLEAD)
        OR      A
        JR      Z,STKEY
        DEC     A
        LD      (ALLEAD),A
        JP      STDRW
STPICK: CALL    PICKER
        JP      SETUP
STSAVE: CALL    AKSAVE
        LD      A,1
        LD      (DIRTY),A
        RET

; TZGET: the adapter's timezone string into LINEBUF (the settings screen
; shows it because a rejected POSIX TZ silently falls back to UTC and this
; readout is the only symptom). CLKGET's shape, command CLKTZ. Carry on
; failure.
TZGET:  LD      A,CLKDEV
        LD      E,CLKTZ
        LD      L,0
        CALL    FNBEGIN
        LD      B,3
        CALL    FNCOMMIT
        RET     C
        JR      NZ,TZBAD
        LD      A,(FNREPLY)
        CP      FCACK
        JR      NZ,TZBAD
        XOR     A
        LD      (CURSLC),A      ; the cart republished slice 0
        LD      A,(FNRXLL)
        OR      A
        JR      Z,TZBAD
        CP      40
        JR      C,TZG1
        LD      A,39
TZG1:   LD      B,A
        LD      HL,FNRDATA
        LD      DE,LINEBUF
TZG2:   LD      A,(HL)
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    TZG2
        XOR     A               ; NUL, and the carry clear with it
        LD      (DE),A
        RET
TZBAD:  SCF
        RET

SSTH:   DB      "SETTINGS",0
SSTLEAD: DB     "Alarm lead:",0
SSTMIN: DB      "min",0
SSTCAL: DB      "Calendar:",0
SSTTZ:  DB      "Timezone:",0
SSTHT:  DB      "UP/DN LEAD  1 CAL  C SAVE",0
