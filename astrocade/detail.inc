; detail.inc -- one event, wrapped from 80 to 40 columns, paged.
;
; The adapter sends the detail as free text already wrapped at its default
; 80; we re-wrap to 40 by a hard break (each source line becomes at most
; two screen rows), which needs no stash buffer -- the source is already
; broken at word boundaries. Eleven rows a page (2-12). A page turn
; re-opens the detail and skips whole pages of wrapped lines, riding the
; adapter's 120-second window cache, so there is one code path and no
; carry state between pages.

DETPG   EQU     11              ; wrapped lines per page

; DETAIL: show the event numbered DEVNUM. One screenful (11 wrapped rows);
; the source is wrapped at 80, so all but the longest events fit. Returns
; when the user backs out; carry set if they chose to edit it. Paging was
; dropped for ROM budget -- see astrocade/README.md.
DETAIL: LD      A,2             ; REQTYP: detail read
        CALL    GCFETCH
        JP      C,DTERR
        CALL    CHROME
        CALL    CLKDRW
        LD      HL,SEVENT
        LD      D,1
        LD      E,3
        LD      C,XKONA
        CALL    TXTAT
        LD      C,2             ; screen row
DTD1:   PUSH    BC
        CALL    DWRAP
        POP     BC
        JR      C,DTHINT
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
DTHINT: LD      HL,SDETHT
        CALL    MSGROW
DTKEY:  CALL    INWAIT
        CP      EVD0+6          ; '6': edit
        JR      Z,DTEDIT
        CP      EVCE
        JR      Z,DTBACK
        CP      EVCLR
        JR      Z,DTBACK
        CP      EVTRIG
        JR      Z,DTBACK
        JR      DTKEY
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

SDETHT: DB      "6 EDIT   C BACK",0
