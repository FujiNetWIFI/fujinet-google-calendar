; detail.inc -- one event, wrapped from 80 to 40 columns, paged.
;
; The adapter sends the detail as free text already wrapped at its default
; 80; we re-wrap to 40 by a hard break (each source line becomes at most
; two screen rows), which needs no stash buffer -- the source is already
; broken at word boundaries. Eleven rows a page (2-12). A page turn
; re-opens the detail and skips whole pages of wrapped lines, riding the
; adapter's 120-second window cache, so there is one code path and no
; carry state between pages. Down off the end lands on an empty page and
; backs itself up, so the last page is sticky.

DETPG   EQU     11              ; wrapped lines per page

; DETAIL: show the event numbered DEVNUM, paged with up/down. Returns when
; the user backs out; carry set if they chose to edit it.
DETAIL: XOR     A
        LD      (DPAGE),A
DETGO:  LD      A,2             ; REQTYP: detail read
        CALL    GCFETCH
        JP      C,DTERR
        CALL    CHROME
        CALL    CLKDRW
        LD      HL,SEVENT
        LD      D,1
        LD      E,3
        LD      C,XKONA
        CALL    TXTAT
        LD      A,(DPAGE)       ; skip DPAGE whole pages of wrapped lines
        OR      A
        JR      Z,DTD0
        LD      B,A
DTSK1:  PUSH    BC
        LD      B,DETPG
DTSK2:  PUSH    BC
        CALL    DWRAP
        POP     BC
        JR      C,DTSKX         ; exhausted mid-skip: past the end
        DJNZ    DTSK2
        POP     BC
        DJNZ    DTSK1
        JR      DTD0
DTSKX:  POP     BC
        LD      HL,DPAGE
        DEC     (HL)
        JR      DETGO
DTD0:   LD      C,2             ; screen row
DTD1:   PUSH    BC
        CALL    DWRAP
        POP     BC
        JR      C,DTD2
        LD      D,C
        LD      E,0
        PUSH    BC
        LD      HL,LINEBUF
        LD      C,XWONK
        CALL    TXTAT
        POP     BC
        INC     C
        LD      A,C
        CP      2+DETPG
        JR      C,DTD1
DTD2:   LD      A,C             ; an empty page past the end: back up
        CP      2
        JR      NZ,DTHINT
        LD      A,(DPAGE)
        OR      A
        JR      Z,DTHINT
        DEC     A
        LD      (DPAGE),A
        JP      DETGO
DTHINT: LD      HL,SDETHT
        CALL    MSGROW
DTKEY:  CALL    INWAIT
        CP      EVD0+6          ; '6': edit
        JR      Z,DTEDIT
        CP      EVDOWN
        JR      Z,DTPGDN
        CP      EVUP
        JR      Z,DTPGUP
        CP      EVCE
        JR      Z,DTBACK
        CP      EVCLR
        JR      Z,DTBACK
        CP      EVTRIG
        JR      Z,DTBACK
        JR      DTKEY
DTPGDN: LD      HL,DPAGE
        INC     (HL)
        JP      DETGO
DTPGUP: LD      A,(DPAGE)
        OR      A
        JR      Z,DTKEY
        DEC     A
        LD      (DPAGE),A
        JP      DETGO
DTBACK: OR      A               ; carry clear: just backed out
        RET
DTEDIT: SCF                     ; carry set: the caller opens the form
        RET
DTERR:  LD      HL,SNOCONN
        CALL    MSGROW
        CALL    INWAIT
        RET

; DWRAP: build one screen line (<=40 chars, NUL-terminated) in LINEBUF
; from the detail stream. Carry set only when the stream is exhausted and
; the line would be empty. Clobbers A,C,HL (and GETCH's).
DWRAP:  LD      HL,LINEBUF
        LD      C,0
DW1:    CALL    GETCH
        JR      C,DWEOF
        CP      0DH
        JR      Z,DW1
        CALL    EOLP
        JR      Z,DWEND
        LD      (HL),A
        INC     HL
        INC     C
        LD      A,C
        CP      40
        JR      C,DW1
DWEND:  LD      (HL),0
        LD      A,C
        OR      A
        RET
DWEOF:  LD      A,C
        OR      A
        JR      Z,DWEOF0
        LD      (HL),0
        OR      A
        RET
DWEOF0: SCF
        RET

SDETHT: DB      "6 EDIT  ^v PAGE  C BACK",0
