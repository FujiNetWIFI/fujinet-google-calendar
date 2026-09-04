; demo.inc -- DEMO=1: static mock screens, zero network.
;
; The palette/layout tuning vehicle, in 5cardstud's mold: every chip hue,
; every time-column shape, the month grid with today and a selection, and
; the form with the grid keyboard live over it -- all drawn from literals
; so a MAME screenshot judges layout and color with no FujiNet running.
; Any key steps splash -> day -> month -> form (the editor really runs;
; = or C leaves it) -> splash.

DEMOGO: CALL    INWAIT
        CALL    WPAL
        CALL    DDAY
        CALL    INWAIT
        CALL    DMON
        CALL    INWAIT
        CALL    DFORM
        CALL    SPLASH
        JR      DEMOGO

; ---- the day view ----
DDAY:   CALL    CHROME
        LD      HL,DDAYL
        CALL    DRAWL
        LD      HL,DCHIPS
DDC1:   LD      A,(HL)
        INC     HL
        CP      0FFH
        JR      Z,DDC2
        LD      D,A
        LD      A,(HL)
        INC     HL
        PUSH    HL
        CALL    CHIP
        POP     HL
        JR      DDC1
DDC2:   LD      A,5
        JP      SELINV

DCHIPS: DB      3,2, 4,3, 5,1, 6,2, 7,1, 8,3
        DB      0FFH

DDAYL:  DB      0,3,XKONA
        DB      "Fri 28 Aug 2026",0
        DB      0,35,XKONA
        DB      "12:45",0
        DB      1,3,XKONA
        DB      "DAY  CAL:ALL",0
        DB      2,2,XDONK
        DB      "TIME",0
        DB      2,14,XDONK
        DB      "EVENT",0
        DB      3,1,XWONK
        DB      " 09:00-10:00 Standup with Platform tea",0
        DB      4,1,XWONK
        DB      "*all day     Company holiday",0
        DB      5,1,XWONK
        DB      "~11:30-12:15 Weekly review @Room 3",0
        DB      6,1,XWONK
        DB      " 14:00->     Overnight bake",0
        DB      7,1,XWONK
        DB      " 16:00-16:30 Dentist visit",0
        DB      8,1,XWONK
        DB      " 19:00-21:00 Dinner with Sam",0
        DB      13,1,XKONA
        DB      "1-4 VIEW 0 TODAY 5 NEW 6 EDIT =OPEN",0
        DB      0FFH

; ---- the month view ----
DMON:   CALL    CHROME
        LD      HL,DMONL
        CALL    DRAWL
        LD      A,'#'-20H       ; today's tile: Fri 4, the mark cell
        LD      D,3
        LD      E,29
        LD      C,XKONA
        JP      GLYPH1

DMONL:  DB      0,3,XKONA
        DB      "September 2026",0
        DB      0,35,XKONA
        DB      "12:45",0
        DB      1,3,XKONA
        DB      "MONTH  CAL:ALL",0
        DB      2,2,XDONK
        DB      "SU   MO   TU   WE   TH   FR   SA",0
        DB      3,2,XWONK
        DB      "           1.   2.   3#   4#   5.",0
        DB      4,2,XWONK
        DB      " 6.   7#   8.   9.  10#  11.  12.",0
        DB      5,2,XWONK
        DB      "13.  14.  15#  16#  17.  18.  19.",0
        DB      6,2,XWONK
        DB      "20.  21#  22.  23.  24.  25#  26.",0
        DB      7,2,XWONK
        DB      "27.  28.  29#  30.",0
        DB      5,17,XKONW
        DB      "16",0          ; the selection: inverse digits
        DB      10,2,XWONK
        DB      "WED 16 SEP  2 EVENTS",0
        DB      11,2,XDONK
        DB      "09:00 Standup with Platform team",0
        DB      13,1,XKONA
        DB      "ARROWS MOVE  0 TODAY  =OPEN  5 NEW",0
        DB      0FFH

; ---- the form, and the grid keyboard live over it ----
DFORM:  CALL    CHROME
        LD      HL,DFRML
        CALL    DRAWL
        LD      HL,DTITLE
        LD      DE,EDBUF
        LD      BC,14
        LDIR
        LD      A,39
        LD      (EDMAX),A
        LD      A,9
        LD      (EDTOPR),A
        LD      HL,SHFRM
        LD      (EDHNT),HL
        JP      TEDIT4          ; the keyboard lives in the form page

DTITLE: DB      "Dentist visit",0
SHFRM:  DB      "= DONE  CE DEL  C CANCEL  KEYPAD TYPES",0

DFRML:  DB      0,3,XKONA
        DB      "NEW EVENT",0
        DB      0,35,XKONA
        DB      "12:45",0
        DB      1,3,XKONA
        DB      "Wed 16 Sep 2026",0
        DB      3,1,XKONW
        DB      "TITLE",0       ; the selected field's label, inverse
        DB      3,7,XWONK
        DB      "Dentist visit",0
        DB      4,1,XWONK
        DB      "DATE",0
        DB      4,7,XWONK
        DB      "2026-09-16",0
        DB      5,1,XWONK
        DB      "START",0
        DB      5,7,XWONK
        DB      "14:00",0
        DB      6,1,XWONK
        DB      "END",0
        DB      6,7,XWONK
        DB      "14:30",0
        DB      7,1,XWONK
        DB      "WHERE",0
        DB      7,7,XWONK
        DB      "12 High St",0
        DB      8,1,XWONK
        DB      "NOTES",0
        DB      8,7,XWONK
        DB      "bring the insurance card",0
        DB      9,1,XWONK
        DB      "CAT",0
        DB      9,7,XWONK
        DB      "Work",0
        DB      13,1,XKONA
        DB      "= SAVE  C DONE  ARROWS FIELD",0
        DB      0FFH
