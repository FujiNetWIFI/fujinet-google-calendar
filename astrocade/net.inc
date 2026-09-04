; net.inc -- the GCAL: round trip, one-shot.
;
; 5cardstud's shape, not netcat's: a calendar fetch is request/response,
; so the CLOSE belongs at the START of the next request (every screen
; renders straight out of the reply window between transactions), and a
; bounded settle loop waits for two consecutive avail readings to agree.
; Two adaptations for this adapter:
;
;  * NSDEVST 136 (END_OF_FILE) is SUCCESS. Calendar.cpp builds the whole
;    reply at open and reports EOF the moment its buffer drains -- so an
;    empty day reports EOF with avail 0 at the first STATUS, and every
;    complete fetch ends in it. The card ports' "non-1 is failure" rule
;    would abort both.
;  * Replies can exceed the 1KB window: GETCH streams bytes and refills
;    the window with further READs behind the caller's back. Nothing
;    behind the current byte is ever re-read, so the repaint is invisible.
;
; All routines return carry SET on failure, clear on success.

; NCMD: begin a network-device transaction. E = command, L = nparam.
NCMD:   LD      A,NETDEV
        JP      FNBEGIN

; SLC0: every transaction makes the cart republish slice 0.
SLC0:   XOR     A
        LD      (CURSLC),A
        RET

; NCLOSE: retire the previous request's channel. A first call has nothing
; to close and earns a NAK; both results are ignored.
NCLOSE: LD      E,NCCLOSE
        LD      L,0
        CALL    NCMD
        LD      B,3             ; ~6 s
        CALL    FNCOMMIT
        JP      SLC0

; NOPEN: OPEN the spec for (REQTYP): aux1 DIR/READ/WRITE, aux2 the listing
; width or 0. BLDURL streams the devicespec; NTMO is the commit leash.
NOPEN:  LD      E,NCOPEN
        LD      L,2
        CALL    NCMD
        LD      A,(REQTYP)
        CP      2
        JR      C,NOP1          ; 0 cals, 1 listing: directory
        JR      Z,NOP2          ; 2 detail: read
        LD      A,GMWRITE       ; 3 compose, 4 edit: write
        JR      NOP3
NOP1:   LD      A,GMDIR
        JR      NOP3
NOP2:   LD      A,GMREAD
NOP3:   CALL    FNPARB
        LD      A,(REQTYP)
        CP      2
        LD      A,GCWIDTH
        JR      C,NOP4
        XOR     A               ; detail and writes: aux2 0
NOP4:   CALL    FNPARB
        CALL    BLDURL
        LD      A,(NTMO)
        LD      B,A
        CALL    FNCOMMIT
        RET     C
        JR      NZ,NOBAD
        CALL    SLC0
        LD      A,(FNREPLY)
        CP      FCACK
        RET     Z
NOBAD:  SCF
        RET

; NSTATUS: one poll. On success AVAIL, NCONN and NDEVST are fresh.
NSTATUS: LD     E,NCSTAT
        LD      L,2
        CALL    NCMD
        XOR     A
        CALL    FNPARB
        XOR     A
        CALL    FNPARB
        LD      B,4
        CALL    FNCOMMIT
        RET     C
        JR      NZ,NSBAD
        LD      A,(FNREPLY)
        CP      FCACK
        JR      NZ,NSBAD
        LD      A,(FNRDATA+NSAVLO)
        LD      (AVAIL),A
        LD      A,(FNRDATA+NSAVHI)
        LD      (AVAIL+1),A
        LD      A,(FNRDATA+NSCONN)
        LD      (NCONN),A
        LD      A,(FNRDATA+NSDEVST)
        LD      (NDEVST),A
        CALL    SLC0
        XOR     A
        RET
NSBAD:  SCF
        RET

; NREAD: READ min(AVAIL, RXMAX) bytes into the reply window, then capture
; the reply length -- the next transaction's would replace it -- and
; re-home the slice cursor. The bytes stay in the window.
NREAD:  LD      HL,(AVAIL)
        LD      DE,RXMAX
        OR      A
        SBC     HL,DE
        JR      C,NRD1          ; avail < clamp: take avail
        LD      H,D
        LD      L,E
        JR      NRD2
NRD1:   ADD     HL,DE
NRD2:   PUSH    HL
        LD      E,NCREAD
        LD      L,1
        CALL    NCMD
        LD      A,2             ; one 2-byte parameter: the length, LE
        CALL    FNTXBYT
        POP     HL
        PUSH    HL
        LD      A,L
        CALL    FNTXBYT
        POP     HL
        LD      A,H
        CALL    FNTXBYT
        LD      B,8
        CALL    FNCOMMIT
        RET     C
        JR      NZ,NRBAD
        LD      A,(FNREPLY)
        CP      FCACK
        JR      NZ,NRBAD
        LD      A,(FNRXLL)      ; capture NOW
        LD      (RXLEN),A
        LD      A,(FNRXLH)
        LD      (RXLEN+1),A
        JP      SLC0            ; XOR A inside: carry clear
NRBAD:  SCF
        RET

; NWRITE: send B bytes from (HL) to the open channel. B <= TXMAX, so the
; length's high byte is always zero. The count rides twice -- once as the
; parameter, once as the payload the cart measures -- exactly as the
; Intellivision client does it. Carry set on failure. Clobbers everything.
NWRITE: LD      A,B
        OR      A
        RET     Z               ; nothing to send, carry clear
        PUSH    HL
        PUSH    BC
        LD      E,NCWRITE
        LD      L,1
        CALL    NCMD
        LD      A,2             ; parameter size byte: a 2-byte length
        CALL    FNTXBYT
        POP     BC
        PUSH    BC
        LD      A,B
        CALL    FNTXBYT         ; length, low
        XOR     A
        CALL    FNTXBYT         ; length, high
        POP     BC
        POP     HL
NWR1:   LD      A,(HL)          ; FNTXBYT touches only A,D,E
        CALL    FNTXBYT
        INC     HL
        DJNZ    NWR1
        LD      B,4
        CALL    FNCOMMIT
        RET     C
        JR      NZ,NWBAD
        CALL    SLC0
        LD      A,(FNREPLY)
        CP      FCACK
        RET     Z
NWBAD:  SCF
        RET

; ----------------------------------------------------------------------
; GCFETCH: A = request type (REQTYP). One full round trip: close the
; previous channel, open, settle, first READ. On success the stream
; cursor is primed for GETCH. An empty reply succeeds with RXLEN 0.
GCFETCH: LD    (REQTYP),A
        LD      A,15            ; ~30 s: the first fetch reaches Google
        LD      (NTMO),A
        CALL    NCLOSE
        CALL    NOPEN
        RET     C
        XOR     A
        LD      H,A
        LD      L,A
        LD      (PRVAVL),HL
        LD      B,20            ; settle-loop bound
GFS1:   PUSH    BC
        CALL    NSTATUS
        POP     BC
        JR      C,GFCFL
        LD      A,(NDEVST)
        CP      NDSEOF
        JR      NZ,GFS2
        LD      HL,(AVAIL)      ; EOF: drained. With data waiting, read
        LD      A,H             ; it; with none, an empty success.
        OR      L
        JR      NZ,GFRD
        LD      HL,0
        LD      (RXLEN),HL
        LD      (RXOFF),HL
        LD      A,1
        LD      (RXEOF),A
        OR      A
        RET
GFS2:   CP      NDSOK
        JR      NZ,GFCFL        ; a hard error; the code stays in NDEVST
        LD      HL,(AVAIL)
        LD      A,H
        OR      L
        JR      Z,GFAGN
        LD      DE,(PRVAVL)
        OR      A
        SBC     HL,DE
        JR      Z,GFRD          ; two readings agree: it has landed
GFAGN:  LD      HL,(AVAIL)
        LD      (PRVAVL),HL
        LD      DE,4000         ; ~3 frames between polls
GFDLY:  DEC     DE
        LD      A,D
        OR      E
        JR      NZ,GFDLY
        DJNZ    GFS1
GFCFL:  CALL    NCLOSE
        SCF
        RET
GFRD:   CALL    NREAD
        JR      C,GFCFL
        XOR     A
        LD      (RXEOF),A
        LD      HL,0
        LD      (RXOFF),HL
        RET

; ----------------------------------------------------------------------
; GETCH: A = the next reply byte; carry set at end of stream. Refills the
; window with further READs when the cursor reaches RXLEN -- the caller
; never notices the repaint because nothing behind the cursor is re-read.
; Preserves BC and DE.
GETCH:  LD      HL,(RXLEN)
        PUSH    DE
        LD      DE,(RXOFF)
        OR      A
        SBC     HL,DE
        JR      Z,GCREF
        EX      DE,HL           ; HL = the cursor
        POP     DE
        CALL    RXGETB
        INC     HL
        LD      (RXOFF),HL
        OR      A
        RET
GCREF:  POP     DE
        LD      A,(RXEOF)
        OR      A
        JR      NZ,GCEOF
        PUSH    BC
        PUSH    DE
        CALL    NSTATUS
        JR      C,GCRF1
        LD      HL,(AVAIL)
        LD      A,H
        OR      L
        JR      Z,GCRF1         ; nothing more: the stream is done
        CALL    NREAD
        JR      C,GCRF1
        LD      HL,0
        LD      (RXOFF),HL
        POP     DE
        POP     BC
        JR      GETCH
GCRF1:  POP     DE
        POP     BC
GCEOF:  LD      A,1
        LD      (RXEOF),A
        SCF
        RET

; UPCASE: fold a-z in A to A-Z. Clobbers A only.
UPCASE: CP      'a'
        RET     C
        CP      'z'+1
        RET     NC
        SUB     20H
        RET
