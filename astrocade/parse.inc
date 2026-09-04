; parse.inc -- the listing parser: one forward pass, no line buffer.
;
; A listing line is fixed columns at width 80: marker, number (numW wide,
; recovered from the '#' in the dashed header), an optional date column
; (0/3/5/6 by view), the 11-char time, the 14-char category, the title.
; Each byte is dispatched by its line column as it streams past: drawn
; where the 40-column layout wants it, banked where the app needs it
; (event number, start time, category, flags), or swallowed. Nothing is
; buffered except the 14-byte category field, which borrows the detail
; screen's LINEBUF -- the two are never live together.
;
; Line terminators: 9BH is this transport's primary, 0AH is accepted,
; 0DH is dropped -- lines.c's triple, verified on the Intellivision.

CATBUF  EQU     LINEBUF

; EOLP (line-terminator predicate) is resident -- gcal.asm MB_SHARED --
; because the picker page needs it too.

; PSETB: the line's column boundaries for this view, once NUMW is known:
; time = 3 + numW + (dateW+1 or 0), category +12, title +15.
PSETB:  LD      A,(VIEW)
        LD      E,A
        LD      D,0
        LD      HL,DWTAB
        ADD     HL,DE
        LD      A,(NUMW)
        ADD     A,2
        LD      (PNEND),A       ; number ends at 2 + numW
        INC     A
        LD      (PDATEC),A      ; date field starts at 3 + numW
        LD      A,(NUMW)
        ADD     A,3
        ADD     A,(HL)
        LD      (PTIMEC),A
        ADD     A,12
        LD      (PCATC),A
        ADD     A,15
        LD      (PTITC),A
        RET
DWTAB:  DB      0,4,6,7

; PLTITL: line 0, the window title, streamed into the header band at
; (0,3), at most 32 cells. Carry when the stream ended first.
PLTITL: LD      D,0
        LD      E,3
        LD      C,XKONA
        LD      B,32
PLT1:   CALL    GETCH
        RET     C
        CP      0DH
        JR      Z,PLT1
        CALL    EOLP
        RET     Z
        LD      L,A
        LD      A,B
        OR      A
        JR      Z,PLT1          ; past the cap: swallow to the EOL
        LD      A,L
        SUB     20H
        CP      60H
        JR      NC,PLT2
        CALL    GLYPH1
        INC     E
PLT2:   DEC     B
        JR      PLT1

; PLHDR: line 1, the dashed header. dashed() turned every space into '-',
; so the '#' is the sole survivor at index 2 + numW - 1.
PLHDR:  LD      B,0
PLH1:   CALL    GETCH
        RET     C
        CP      0DH
        JR      Z,PLH1
        CALL    EOLP
        RET     Z
        CP      '#'
        JR      NZ,PLH2
        LD      A,B
        DEC     A
        LD      (NUMW),A
PLH2:   INC     B
        JR      PLH1

; ----------------------------------------------------------------------
; PLEVT: parse one event line and render it at row (CROW): chip in the
; gutter, marker at col 1, the time field verbatim at cols 2-12, the
; title from col 14. Banks number, flags, start time and category into
; the accumulators; PLEFIN commits them to META and advances CROW/NROWS.
; The "(no events)" line is swallowed whole. Carry set when the stream
; ended before any byte of a line.
PLEVT:  XOR     A
        LD      (PFLG),A
        LD      (PSH),A
        LD      (PSM),A
        LD      (CATLEN),A
        LD      (PSKIP),A
        LD      (PEVN),A
        LD      (PEVN+1),A
        LD      (PTMP+2),A      ; week: no weekday captured yet
        LD      A,(ANCD)        ; row date defaults to the anchor (day view)
        LD      (PDAY),A
        LD      A,(ANCM)
        LD      (PMON),A
        LD      B,0             ; line column
        LD      A,(CROW)
        LD      D,A
        LD      C,XWONK
PLE1:   CALL    GETCH
        JR      NC,PLE2
        LD      A,B             ; ended mid-line: a partial line with
        OR      A               ; content still lands; then stop
        JR      Z,PLEEOF
        CALL    PLEDON
PLEEOF: SCF
        RET
PLE2:   CP      0DH
        JR      Z,PLE1
        CALL    EOLP
        JR      Z,PLED0
        LD      E,A             ; hold the char
        LD      A,(PSKIP)
        OR      A
        JR      NZ,PLENXT
        LD      A,B
        OR      A
        JR      Z,PLEMRK
        LD      HL,PNEND
        CP      (HL)
        JP      C,PLENUM        ; the number column only
        LD      HL,PTIMEC
        CP      (HL)
        JP      C,PLEDATE       ; the date column (agenda captures it)
        LD      HL,PCATC
        CP      (HL)
        JP      C,PLETIM
        LD      HL,PTITC
        CP      (HL)
        JP      C,PLECAT
        SUB     (HL)            ; title zone: col 14 + offset
        ADD     A,14
        CP      TCOLS
        JP      NC,PLENXT
        CALL    DRWEC
PLENXT: INC     B
        JR      PLE1
PLED0:  JP      PLEDON

; DRWEC: draw the char in E at column A of row D, pair C; out-of-range
; codepoints are skipped, and nothing is drawn at all in tally mode (the
; month walks the same parser but produces MGRID, not screen rows).
; Preserves B,C,D.
DRWEC:  LD      L,A             ; hold the column
        LD      A,(PMODE)
        OR      A
        LD      A,L
        RET     NZ              ; tally mode: draw nothing
        LD      L,E
        LD      E,A
        LD      A,L
        SUB     20H
        CP      60H
        RET     NC
        LD      C,XWONK         ; body text; MONFN may have clobbered C
        JP      GLYPH1

PLEMRK: LD      A,E             ; col 0: the marker
        CP      '*'
        JR      NZ,PLEM1
        LD      A,MFALLD
        JR      PLEM2
PLEM1:  CP      '~'
        JR      NZ,PLEM3
        LD      A,MFRECUR
PLEM2:  LD      HL,PFLG
        OR      (HL)
        LD      (HL),A
PLEM3:  LD      A,1
        CALL    DRWEC
        JR      PLENXT

; PLEDATE: the date column. Agenda ("28 Aug") banks the day and month so
; an edit prefills the right date; day view has no date column and week/
; month do not come through here.
; The date column carries the row's day. Agenda is "DD Mon" (day first);
; month is "Fr 28" (day last); the week is the bare weekday name ("Fri"),
; whose first two letters match an MWDAYS pair into PTMP+2 (dow+1, 0 =
; nothing matched). Month keeps the anchor month, agenda reads the
; 3-letter month name.
PLEDATE: LD     A,(VIEW)
        CP      1
        JR      Z,PLEDWK
        CP      2
        JR      Z,PLEDMO
        CP      3
        JP      NZ,PLENXT
        LD      A,B             ; agenda: rel within "DD Mon"
        LD      HL,PDATEC
        SUB     (HL)
        JR      Z,PLDT          ; 0: day tens
        CP      1
        JR      Z,PLDU          ; 1: day units
        CP      3
        JP      C,PLENXT        ; 2: the space
        CP      6
        JP      NC,PLENXT       ; >=6: past the name
        SUB     3               ; 3,4,5 -> month letter 0,1,2. Index via DE,
        PUSH    DE              ; never BC -- B is the live line column.
        LD      D,0             ; (E holds the char; save and restore it)
        LD      E,A
        LD      HL,PTMP
        ADD     HL,DE
        POP     DE
        LD      A,E             ; the letter
        LD      (HL),A
        LD      A,B             ; which letter? rel-3 again
        LD      HL,PDATEC
        SUB     (HL)
        CP      5               ; the 3rd letter (rel 5) completes the name
        JP      NZ,PLENXT
        PUSH    DE              ; MONFN clobbers B (line col) and D (row)
        PUSH    BC
        CALL    MONFN
        POP     BC
        POP     DE
        LD      (PMON),A
        JP      PLENXT
PLDT:   LD      A,E
        SUB     '0'
        JP      C,PLENXT
        CALL    TEN
        LD      (PDAY),A
        JP      PLENXT
PLDU:   LD      A,E
        SUB     '0'
        LD      HL,PDAY
        ADD     A,(HL)
        LD      (HL),A
        JP      PLENXT
; month "Fr 28": the day is the last two characters, rel 3 and 4.
PLEDMO: LD      A,B
        LD      HL,PDATEC
        SUB     (HL)
        CP      3
        JR      Z,PLDT
        CP      4
        JR      Z,PLDU
        JP      PLENXT
; week "Fri": hold the first letter; the second completes the pair and
; matches it against MWDAYS (whose pairs equal the wire's first two
; letters byte for byte).
PLEDWK: LD      A,B
        LD      HL,PDATEC
        SUB     (HL)
        JR      Z,PLDW0
        CP      1
        JP      NZ,PLENXT
        LD      A,E
        LD      (PTMP+1),A
        PUSH    BC
        PUSH    DE
        LD      HL,MWDAYS
        LD      C,0
PLDW1:  LD      A,(PTMP)
        CP      (HL)
        INC     HL
        JR      NZ,PLDW2
        LD      A,(PTMP+1)
        CP      (HL)
        JR      NZ,PLDW2
        LD      A,C
        INC     A
        LD      (PTMP+2),A
        JR      PLDW3
PLDW2:  INC     HL
        INC     C
        LD      A,C
        CP      7
        JR      C,PLDW1
PLDW3:  POP     DE
        POP     BC
        JP      PLENXT
PLDW0:  LD      A,E
        LD      (PTMP),A
        JP      PLENXT

; MONFN: A = month 1-12 from the three letters in PTMP, matched against
; MMON3; the anchor month if no match. Clobbers B,C,D,E,H,L.
MONFN:  LD      HL,MMON3
        LD      C,0
MONF1:  LD      A,C
        CP      12
        JR      NC,MONFBD
        LD      DE,PTMP
        PUSH    HL
        LD      B,3
MONF2:  LD      A,(DE)
        CP      (HL)
        JR      NZ,MONF3
        INC     HL
        INC     DE
        DJNZ    MONF2
        POP     HL
        LD      A,C
        INC     A
        RET
MONF3:  POP     HL
        LD      DE,3
        ADD     HL,DE
        INC     C
        JR      MONF1
MONFBD: LD      A,(ANCM)
        RET
MMON3:  DB      "JanFebMarAprMayJunJulAugSepOctNovDec"

PLENUM: LD      A,E             ; the number column
        CP      '('
        JR      NZ,PLEN1
        LD      A,B
        CP      2
        JR      NZ,PLEN1
        LD      A,1             ; "  (no events)"
        LD      (PSKIP),A
        JP      PLENXT
PLEN1:  LD      A,E
        SUB     '0'
        CP      10
        JP      NC,PLENXT
        PUSH    BC
        PUSH    DE
        LD      C,A
        LD      B,0
        LD      HL,(PEVN)
        CALL    MUL10           ; clobbers DE only
        ADD     HL,BC
        LD      (PEVN),HL
        POP     DE
        POP     BC
        JP      PLENXT

PLETIM: LD      A,B             ; the 11-char time field
        LD      HL,PTIMEC
        SUB     (HL)
        LD      L,A             ; rel 0-10
        LD      A,E
        SUB     '0'
        CP      10
        JR      NC,PLET5
        LD      H,A             ; a digit: bank by rel
        LD      A,L
        OR      A
        JR      NZ,PLET1
        LD      A,H
        CALL    TEN
        LD      (PSH),A
        JR      PLET8
PLET1:  CP      1
        JR      NZ,PLET2
        LD      A,(PSH)
        ADD     A,H
        LD      (PSH),A
        JR      PLET8
PLET2:  CP      3
        JR      NZ,PLET3
        LD      A,H
        CALL    TEN
        LD      (PSM),A
        JR      PLET8
PLET3:  CP      4
        JR      NZ,PLET8
        LD      A,(PSM)
        ADD     A,H
        LD      (PSM),A
        JR      PLET8
PLET5:  LD      A,E
        CP      '>'
        JR      NZ,PLET8
        LD      A,(PFLG)
        OR      MFOPEN
        LD      (PFLG),A
PLET8:  LD      A,L
        ADD     A,2             ; time draws at cols 2-12
        CALL    DRWEC
        JP      PLENXT

PLECAT: PUSH    DE              ; the 14-char category: banked, not drawn
        LD      A,B
        LD      HL,PCATC
        SUB     (HL)
        LD      E,A
        LD      D,0
        LD      HL,CATBUF
        ADD     HL,DE
        POP     DE
        LD      (HL),E
        LD      A,E
        CP      ' '
        JP      Z,PLENXT
        LD      A,B
        LD      HL,PCATC
        SUB     (HL)
        INC     A
        LD      (CATLEN),A
        JP      PLENXT

TEN:    PUSH    BC              ; A = A * 10
        LD      C,A
        ADD     A,A
        ADD     A,A
        ADD     A,C
        ADD     A,A
        POP     BC
        RET

; PLEDON: the line ended. Commit a real row; swallow the rest.
PLEDON: LD      A,(PSKIP)
        OR      A
        JR      NZ,PLERT
        LD      HL,(PEVN)
        LD      A,H
        OR      L
        JR      Z,PLERT
        CALL    PLEFIN
PLERT:  OR      A
        RET

; PLEFIN: commit the parsed line. In render mode (day/agenda) that means a
; META entry, a colour chip and a row advance; in tally mode (month) it
; folds the day's count and leading colour into MGRID.
PLEFIN: CALL    CATMAT          ; A = bin
        LD      C,A
        LD      A,(PMODE)
        OR      A
        JR      Z,PLEREN
        LD      A,(VIEW)        ; tally: the week and the month differ
        CP      1
        JP      Z,PLWTAL
        JR      PLETAL
PLEREN:
        LD      A,(CROW)        ; render: META[row]
        SUB     CTOP
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,META
        ADD     HL,DE
        LD      A,(PEVN)
        LD      (HL),A
        INC     HL
        LD      A,(PEVN+1)
        LD      (HL),A
        INC     HL
        LD      A,(PFLG)
        LD      (HL),A
        INC     HL
        LD      A,C
        LD      (HL),A          ; bin
        INC     HL
        LD      A,(PDAY)        ; the row's date (agenda captured; else anchor)
        LD      (HL),A
        INC     HL
        LD      A,(PMON)
        LD      (HL),A
        INC     HL
        LD      A,(PSH)
        LD      (HL),A
        INC     HL
        LD      A,(PSM)
        LD      (HL),A
        LD      A,C
        OR      A
        JR      Z,PLEF1         ; bin 0 (Graphite): no chip
        LD      B,A
        LD      A,(CROW)
        LD      D,A
        LD      A,B
        CALL    CHIP
PLEF1:  LD      HL,CROW
        INC     (HL)
        LD      HL,NROWS
        INC     (HL)
        RET

; PLETAL: month tally. C = bin. MGRID[day-1] = count(6b)<<2 | bin(2b),
; the leading event's bin kept, the count saturating at 63.
PLETAL: LD      A,(PDAY)
        OR      A
        RET     Z
        CP      32
        RET     NC
        DEC     A
        LD      E,A
        LD      D,0
        LD      HL,MGRID
        ADD     HL,DE
        LD      A,(HL)
        AND     3
        JR      NZ,PLTKC        ; a prior event keeps the bin
        LD      A,(HL)
        AND     0FCH
        OR      C
        LD      (HL),A
PLTKC:  LD      A,(HL)
        AND     0FCH
        CP      0FCH
        RET     Z
        LD      A,(HL)
        ADD     A,4
        LD      (HL),A
        RET

; ----------------------------------------------------------------------
; CATMAT: A = the color bin for CATBUF/CATLEN. The category is one of the
; eleven Google color names or it is a calendar name; the match is the
; full name, case-folded, never a prefix -- "Tomato Soup" is a calendar.
; Bins: 1 red, 2 green, 3 blue (and the unmatched default); Graphite has
; no hue in the gutter and gets no chip.
CATMAT: LD      HL,CATTAB
CM1:    LD      A,(HL)
        OR      A
        JR      Z,CMDEF
        LD      C,A             ; entry length
        PUSH    HL
        INC     HL
        LD      B,(HL)          ; bin
        INC     HL
        LD      A,(CATLEN)
        CP      C
        JR      NZ,CMNXT
        LD      DE,CATBUF
CM2:    LD      A,(DE)
        CALL    UPCASE
        CP      (HL)
        JR      NZ,CMNXT
        INC     HL
        INC     DE
        DEC     C
        JR      NZ,CM2
        POP     HL
        LD      A,B
        RET
CMNXT:  POP     HL
        LD      A,(HL)
        ADD     A,2
        LD      E,A
        LD      D,0
        ADD     HL,DE
        JR      CM1
CMDEF:  LD      A,3
        RET
CATTAB: DB      6,1,"TOMATO"
        DB      8,1,"FLAMINGO"
        DB      9,1,"TANGERINE"
        DB      6,1,"BANANA"
        DB      4,2,"SAGE"
        DB      5,2,"BASIL"
        DB      7,3,"PEACOCK"
        DB      9,3,"BLUEBERRY"
        DB      8,3,"LAVENDER"
        DB      5,3,"GRAPE"
        DB      8,0,"GRAPHITE"
        DB      0
