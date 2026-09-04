; form.inc -- composing and editing an event.
;
; Seven fields on rows 3-9, the adapter's seven draft keys. Every field --
; text and the date/time fields alike -- is entered through edit.inc's grid
; keyboard (the keypad types digits straight in). Nothing touches the
; network until the save, and the form validates a required title and an
; end-without-start first, because the adapter collapses every draft
; rejection into one status code.
;
; Compose emits every non-empty field (title required); an edit emits only
; the fields the user changed -- the client holds no copy of the server's
; location, notes or category, so an untouched field must be left alone. A
; blank start time is the wire's spelling of all-day, meaningful on both.

NFLD    EQU     7
FVALCOL EQU     8

; VFORM: entry from the view loop. FMEDIT is preset (0 compose, 1 edit),
; and for an edit DEVNUM is the target. Returns to VIEWLP.
VFORM:  CALL    FCLR            ; blank every field, clear the dirty flags
        LD      A,(FMEDIT)
        OR      A
        JR      NZ,VFE
        ; compose: prefill the date from the anchor
        CALL    FSETDT
        JR      VFGO
VFE:    ; edit: date and start from the selected row's META entry
        LD      A,(SELROW)
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,META+MDAY
        ADD     HL,DE
        LD      A,(HL)          ; day
        LD      (CDD),A
        INC     HL
        LD      A,(HL)          ; month
        LD      (CDM),A
        INC     HL
        LD      A,(HL)          ; start hour
        LD      (CDN),A
        INC     HL
        LD      C,(HL)          ; start minute
        LD      HL,FDATE        ; FDATE = ANCY - month - day
        LD      BC,(ANCY)
        CALL    FP4
        LD      A,'-'
        LD      (HL),A
        INC     HL
        LD      A,(CDM)
        CALL    FP2
        LD      A,'-'
        LD      (HL),A
        INC     HL
        LD      A,(CDD)
        CALL    FP2
        LD      (HL),0
        LD      A,(SELROW)      ; reload the minute (BC was clobbered)
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,META+MSM
        ADD     HL,DE
        LD      C,(HL)
        LD      A,(CDN)         ; hour
        LD      B,A
        OR      C
        JR      Z,VFGO          ; all-day: leave start blank
        LD      HL,FSTART
        LD      A,B
        CALL    FP2
        LD      A,':'
        LD      (HL),A
        INC     HL
        LD      A,C
        CALL    FP2
        LD      (HL),0
VFGO:   XOR     A
        LD      (FSEL),A
        CALL    FDRAW
FLOOP:  CALL    INWAIT
        CP      EVUP
        JR      Z,FUP
        CP      EVDOWN
        JR      Z,FDN
        CP      EVTRIG
        JR      Z,FEDIT
        CP      EVEQ
        JP      Z,FSAVE
        CP      EVCLR
        JR      Z,FLEAVE
        JR      FLOOP
FUP:    LD      A,(FSEL)
        OR      A
        JR      NZ,FUP1
        LD      A,NFLD
FUP1:   DEC     A
        LD      (FSEL),A
        CALL    FDRAW
        JR      FLOOP
FDN:    LD      A,(FSEL)
        INC     A
        CP      NFLD
        JR      C,FDN1
        XOR     A
FDN1:   LD      (FSEL),A
        CALL    FDRAW
        JR      FLOOP
FLEAVE: CALL    FANYD           ; any field dirty?
        JR      Z,FLRET
        LD      HL,SFASK        ; "Save? (Y/N)"
        CALL    MSGROW
FLK:    CALL    INWAIT
        CALL    EVTOCH
        CP      'Y'
        JP      Z,FSAVE
        CP      'y'
        JP      Z,FSAVE
        CP      'N'
        JR      Z,FLRET
        CP      'n'
        JR      Z,FLRET
        JR      FLK
FLRET:  LD      A,1
        LD      (DIRTY),A
        JP      VIEWLP

; FEDIT: edit the selected field. Every field -- text and the date/time
; fields alike -- goes through edit.inc's grid keyboard; the keypad types
; digits straight in, and '-' / ':' are picked from the grid (or '-' off
; the keypad). The dedicated digit-mask editor was dropped for ROM budget;
; see astrocade/README.md.
FEDIT:  CALL    FPTR            ; sets FMAXC for the selected field
        JP      FEDTX

; ---- up/down field selection redraw is whole-form (cheap) -------------
; FDRAW: the chrome, all seven fields, the selected label inverse, hints.
FDRAW:  CALL    CHROME
        LD      HL,SEVENT
        LD      D,1
        LD      E,3
        LD      C,XKONA
        CALL    TXTAT
        CALL    CLKDRW
        LD      B,0             ; field index (preserved locally in FDR3)
FDR1:   LD      A,B
        CALL    FROW            ; D = row
        LD      D,A
        LD      A,B             ; label at col 1, inverse if selected
        LD      HL,FSEL
        CP      (HL)
        LD      C,XWONK
        JR      NZ,FDR2
        LD      C,XKONW
FDR2:   PUSH    BC
        LD      A,B
        ADD     A,A
        ADD     A,A
        ADD     A,A             ; *8: label stride
        LD      E,A
        LD      D,0             ; clean offset -- D was the row
        LD      HL,FLABS
        ADD     HL,DE
        POP     BC
        LD      A,B             ; restore D = the field's row
        CALL    FROW
        LD      D,A
        LD      E,1
        PUSH    BC
        CALL    TXTAT
        POP     BC
FDR3:   PUSH    BC              ; the value at col 8
        LD      A,B
        CALL    FPTRA           ; HL = field B's buffer (clobbers A,D,E)
        POP     BC
        PUSH    BC
        LD      A,B
        CALL    FROW
        LD      D,A
        LD      E,FVALCOL
        LD      C,XWONK
        LD      B,32
        CALL    TXTN
        POP     BC
        INC     B
        LD      A,B
        CP      NFLD
        JR      C,FDR1
        LD      HL,SFRMHT
        JP      MSGROW

; ======================================================================
; text field: edit.inc's grid keyboard
; ======================================================================
FEDTX:  CALL    FPTRI2          ; HL = field, B = maxlen (from FPTR earlier)
        LD      DE,EDBUF        ; copy field -> EDBUF
        CALL    FSTRCP
        LD      A,(FMAXC)
        LD      (EDMAX),A
        LD      A,9
        LD      (EDTOPR),A
        LD      HL,SFEDIT
        LD      (EDHNT),HL
        CALL    EDIT
        JR      C,FEDX          ; cancel: leave the field
        LD      HL,EDBUF        ; changed? compare, set dirty, copy back
        CALL    FPTRI
        EX      DE,HL           ; DE = field
        LD      HL,EDBUF
        CALL    FCMP
        JR      Z,FEDX
        CALL    FMARKD
        LD      HL,EDBUF
        CALL    FPTRI
        EX      DE,HL
        LD      HL,EDBUF
        CALL    FSTRCP
FEDX:   CALL    FDRAW
        JP      FLOOP


; ======================================================================
; save
; ======================================================================
FSAVE:  CALL    FVALID
        LD      A,(FERR)
        OR      A
        JR      Z,FSV1
        CALL    FEMSG           ; show the validation message
        JP      FLOOP
FSV1:   LD      A,(FMEDIT)      ; edit with nothing dirty: nothing to do
        OR      A
        JR      Z,FSV2
        CALL    FANYD
        JR      NZ,FSV2
        LD      HL,SFNONE
        CALL    MSGROW
        CALL    INWAIT
        JP      FLOOP
FSV2:   LD      HL,SFSAVING
        CALL    MSGROW
        LD      A,(FMEDIT)      ; open the write channel
        OR      A
        LD      A,3             ; REQTYP compose
        JR      Z,FSV3
        LD      A,4             ; REQTYP edit
FSV3:   LD      (REQTYP),A
        CP      4
        LD      A,8
        JR      NZ,FSV4
        LD      A,45            ; the edit open resolves /N upstream
FSV4:   LD      (NTMO),A
        CALL    NCLOSE
        CALL    NOPEN
        JR      C,FSVERR
        CALL    FEMIT           ; stream the KEY: value lines
        LD      A,15            ; the commit's POST runs inside close
        LD      (NTMO),A
        CALL    NCLOSE
        CALL    NSTATUS
        JR      C,FSVERR
        LD      A,(NDEVST)      ; the verdict
        CP      NDSOK
        JR      Z,FSVOK
        CP      NDSEOF
        JR      Z,FSVOK
        CALL    FVMAP           ; map the code to a message
        JP      FLOOP
FSVOK:  LD      A,1
        LD      (DIRTY),A
        JP      VIEWLP
FSVERR: LD      HL,SNOCONN
        CALL    MSGROW
        CALL    INWAIT
        JP      FLOOP

; FEMIT: emit the fields. Compose sends every non-empty field; edit sends
; only dirty ones. START carries the date plus an optional start time; a
; date-only START is all-day.
FEMIT:  LD      A,0             ; SUMMARY <- title
        CALL    FWANT
        JR      Z,FEM1
        LD      HL,SKSUM
        LD      DE,FTITLE
        CALL    FPUTLN
FEM1:   CALL    FWSTART         ; START (date + opt start), if wanted
        JR      Z,FEM2
        CALL    FPUTST
FEM2:   LD      A,3             ; END (date + end time)
        CALL    FWANT
        JR      Z,FEM3
        LD      A,(FEND)
        OR      A
        JR      Z,FEM3
        CALL    FPUTEN
FEM3:   LD      A,4             ; LOCATION
        CALL    FWANT
        JR      Z,FEM4
        LD      HL,SKLOC
        LD      DE,FLOC
        CALL    FPUTLN
FEM4:   LD      A,5             ; DESCRIPTION
        CALL    FWANT
        JR      Z,FEM5
        LD      HL,SKDESC
        LD      DE,FDESC
        CALL    FPUTLN
FEM5:   LD      A,6             ; CATEGORY
        CALL    FWANT
        JR      Z,FEM6
        LD      HL,SKCAT
        LD      DE,FCAT
        CALL    FPUTLN
FEM6:   RET

; FWANT: Z clear when field A should be emitted. Compose: non-empty. Edit:
; dirty. (START has its own rule in FWSTART.)
FWANT:  LD      C,A
        LD      B,A
        LD      A,(FMEDIT)
        OR      A
        JR      NZ,FWED
        LD      A,C             ; compose: non-empty
        CALL    FPTRA
        LD      A,(HL)
        OR      A
        RET
FWED:   LD      A,C             ; edit: dirty
        LD      HL,FDIRTY
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        RET

; FWSTART: Z clear when START should be emitted. Compose: always (a date
; is required). Edit: date dirty OR start dirty.
FWSTART: LD     A,(FMEDIT)
        OR      A
        JR      NZ,FWS1
        LD      A,1             ; compose: always
        OR      A
        RET
FWS1:   LD      A,(FDIRTY+1)    ; date dirty
        LD      B,A
        LD      A,(FDIRTY+2)    ; start dirty
        OR      B
        RET

; The emit line is assembled in EDBUF (free during a save; LINEBUF must
; not be used -- it overlaps FDESC in the form's RAM overlay). Fields are
; separate buffers, so key and value never alias EDBUF.

; FPUTST: emit "START: <date>[ <start>]".
FPUTST: LD      DE,EDBUF
        LD      HL,SKSTART
        CALL    FSTRCP0
        LD      HL,FDATE
        CALL    FSTRCP0
        LD      A,(FSTART)
        OR      A
        JR      Z,FPS1
        LD      A,' '
        LD      (DE),A
        INC     DE
        LD      HL,FSTART
        CALL    FSTRCP0
FPS1:   JP      FPUTX

; FPUTEN: emit "END: <date> <end>".
FPUTEN: LD      DE,EDBUF
        LD      HL,SKEND
        CALL    FSTRCP0
        LD      HL,FDATE
        CALL    FSTRCP0
        LD      A,' '
        LD      (DE),A
        INC     DE
        LD      HL,FEND
        CALL    FSTRCP0
        JP      FPUTX

; FPUTLN: build "<key><value>\r" in EDBUF and NWRITE it. HL = key (with
; its ": "), DE = value field.
FPUTLN: PUSH    DE
        LD      DE,EDBUF
        CALL    FSTRCP0         ; key
        POP     HL              ; value field
        CALL    FSTRCP0
        ; falls into FPUTX with DE past the last char
FPUTX:  LD      A,0DH
        LD      (DE),A
        INC     DE
        LD      HL,EDBUF        ; length = DE - EDBUF
        EX      DE,HL
        OR      A
        SBC     HL,DE
        LD      B,L
        LD      HL,EDBUF
        JP      NWRITE

; ---- small helpers ----------------------------------------------------
; FSTRCP0: copy NUL-terminated (HL) to (DE); DE left past the last char
; (no NUL written). Clobbers A.
FSTRCP0: LD     A,(HL)
        OR      A
        RET     Z
        LD      (DE),A
        INC     HL
        INC     DE
        JR      FSTRCP0

; FSTRCP: copy NUL-terminated (HL) to (DE), NUL included. Clobbers A.
FSTRCP: LD      A,(HL)
        LD      (DE),A
        OR      A
        RET     Z
        INC     HL
        INC     DE
        JR      FSTRCP

; FCMP: Z when strings (HL) and (DE) are equal. Clobbers A.
FCMP:   LD      A,(DE)
        CP      (HL)
        RET     NZ
        OR      A
        RET     Z
        INC     HL
        INC     DE
        JR      FCMP

; FP2 / FP2 helpers: A (0-99) as two ASCII digits at (HL); HL advances.
FP2:    LD      B,'0'
FP2A:   CP      10
        JR      C,FP2B
        SUB     10
        INC     B
        JR      FP2A
FP2B:   LD      C,A
        LD      A,B
        LD      (HL),A
        INC     HL
        LD      A,C
        ADD     A,'0'
        LD      (HL),A
        INC     HL
        RET

; FCLR: blank every field and clear the dirty flags.
FCLR:   XOR     A               ; empty every field via its pointer
FCLR0:  PUSH    AF
        CALL    FPTRA
        LD      (HL),0
        POP     AF
        INC     A
        CP      NFLD
        JR      C,FCLR0
        LD      HL,FDIRTY
        LD      B,NFLD
        XOR     A
FCLR1:  LD      (HL),A
        INC     HL
        DJNZ    FCLR1
        RET

; FSETDT: FDATE <- the anchor date, YYYY-MM-DD.
FSETDT: LD      HL,FDATE
        LD      BC,(ANCY)
        CALL    FP4             ; year (advances HL)
        LD      A,'-'
        LD      (HL),A
        INC     HL
        LD      A,(ANCM)
        CALL    FP2
        LD      A,'-'
        LD      (HL),A
        INC     HL
        LD      A,(ANCD)
        CALL    FP2
        LD      (HL),0
        RET

; FP4: BC (a year) as four ASCII digits at (HL); HL advances. Uses the
; repeated-subtraction path, small and clear.
FP4:    PUSH    BC
        LD      D,H
        LD      E,L
        LD      H,B
        LD      L,C             ; HL = year
        LD      BC,1000
        CALL    FPD
        LD      BC,100
        CALL    FPD
        LD      BC,10
        CALL    FPD
        LD      A,L
        ADD     A,'0'
        LD      (DE),A
        INC     DE
        EX      DE,HL
        POP     BC
        RET
FPD:    LD      A,'0'-1
FPD1:   INC     A
        OR      A
        SBC     HL,BC
        JR      NC,FPD1
        ADD     HL,BC
        LD      (DE),A
        INC     DE
        RET

; FANYD: Z clear when any field is dirty.
FANYD:  LD      HL,FDIRTY
        LD      B,NFLD
FAD1:   LD      A,(HL)
        OR      A
        RET     NZ
        INC     HL
        DJNZ    FAD1
        XOR     A
        RET

; FMARKD: set FDIRTY[FSEL].
FMARKD: LD      A,(FSEL)
        LD      HL,FDIRTY
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      (HL),1
        RET

; FROW: A = the screen row of field A (3..9).
FROW:   ADD     A,CTOP
        RET

; FPTR: HL = FSEL's buffer, B/FMAXC = its max length.
FPTR:   LD      A,(FSEL)
        CALL    FPTRA
        LD      A,(FSEL)
        LD      E,A
        LD      D,0
        PUSH    HL
        LD      HL,FMAXS
        ADD     HL,DE
        LD      A,(HL)
        LD      (FMAXC),A
        LD      B,A
        POP     HL
        RET

; FPTRI: HL = FSEL's field buffer.
FPTRI:  LD      A,(FSEL)
; FPTRA: HL = field A's buffer.
FPTRA:  ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,FPTRS
        ADD     HL,DE
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        RET

; FPTRI2 / FPTRI2: HL = FSEL field, B = FMAXC (after FPTR set it).
FPTRI2: CALL    FPTRI
        LD      A,(FMAXC)
        LD      B,A
        RET

FSLEN:  LD      B,0
FSL1:   LD      A,(HL)
        OR      A
        JR      Z,FSL2
        INC     HL
        INC     B
        JR      FSL1
FSL2:   LD      A,B
        RET

; ======================================================================
; validation
; ======================================================================
FVALID: XOR     A
        LD      (FERR),A
        LD      A,(FMEDIT)      ; compose needs a title
        OR      A
        JR      NZ,FV4
        LD      A,(FTITLE)
        OR      A
        JR      NZ,FV4
        LD      A,1             ; "Title required"
        LD      (FERR),A
        RET
FV4:    LD      A,(FEND)        ; an end time needs a start time
        OR      A
        RET     Z
        LD      A,(FSTART)
        OR      A
        RET     NZ
        LD      A,2             ; "End needs a start"
        LD      (FERR),A
        RET

; ======================================================================
; messages
; ======================================================================
FEMSG:  LD      A,(FERR)
        DEC     A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,FMSGT
        ADD     HL,DE
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        JP      MSGROW

; FVMAP: map the save verdict in NDEVST to a message.
FVMAP:  LD      A,(NDEVST)
        LD      HL,SFDENY
        CP      GEDENY
        JR      Z,FVM
        CP      GENOAUTH
        JR      Z,FVM
        LD      HL,SFVANISH
        CP      GENOTFND
        JR      Z,FVM
        LD      HL,SFREJECT     ; 132/165 and anything else
FVM:    CALL    MSGROW
        CALL    INWAIT
        RET

; ---- tables and strings ----------------------------------------------
FPTRS:  DW      FTITLE,FDATE,FSTART,FEND,FLOC,FDESC,FCAT
FMAXS:  DB      39,10,5,5,32,80,15
FLABS:  DB      "Title",0,0,0
        DB      "Date",0,0,0,0
        DB      "Start",0,0,0
        DB      "End",0,0,0,0,0
        DB      "Where",0,0,0
        DB      "Notes",0,0,0
        DB      "Cat",0,0,0,0,0
SKSUM:  DB      "SUMMARY: ",0
SKSTART: DB     "START: ",0
SKEND:  DB      "END: ",0
SKLOC:  DB      "LOCATION: ",0
SKDESC: DB      "DESCRIPTION: ",0
SKCAT:  DB      "CATEGORY: ",0
SFRMHT: DB      "=SAVE C BACK ^v",0
SFEDIT: DB      "=DONE CE DEL C CANCEL",0
SFASK:  DB      "Save? (Y/N)",0
SFNONE: DB      "No changes",0
SFSAVING: DB    "Saving...",0
SFREJECT: DB    "Rejected",0
SFDENY: DB      "Not authorised",0
SFVANISH: DB    "Vanished",0
FMSGT:  DW      SFMTITLE,SFMEND
SFMTITLE: DB    "Title required",0
SFMEND: DB      "End needs a start",0
