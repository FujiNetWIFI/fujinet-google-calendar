; gcal.asm -- Google Calendar for the Bally Astrocade, over FujiNet.
;
; A third standalone implementation beside the C core (src/) and the
; Intellivision client (intv/), because the Astrocade has 4KB of RAM total,
; all of it screen RAM, and a cartridge window whose usable half is 6,912
; bytes. The infrastructure -- the 4x6 renderer, the 96-glyph mixed-case
; font, the keypad/stick event space, the grid keyboard, the mailbox
; transport -- is netcat/astrocade's, copied file for file; the network
; round trip is 5cardstud's one-shot shape, because a GCAL: fetch is a
; request/response, not a held stream.
;
; Nothing is ever buffered from the network: every screen renders straight
; out of the cart's repainted 1KB reply window (state.inc), and paging is a
; re-open plus a skip count, which the adapter's 120-second window cache
; makes nearly free.
;
; ROM budget: 0000H-1AFFH of the 8K window (6,912 bytes); 1B00H+ belongs to
; the mailbox and build.sh stamps the "FUJI" claim at 1CFCH. checksize.py
; itemises the MB_* fences on every build.
;
; Interrupts stay off for the program's whole life (fujilib.inc's contract);
; pacing is DELAY10 spins, and the wall clock is the FujiNet's, re-fetched.

        INCLUDE "HVGLIB.H"
        INCLUDE "fujinet.inc"
        INCLUDE "build/flags.inc"

; ---- geometry ---------------------------------------------------------
LINES   EQU     84              ; visible scanlines: 14 rows of 4x6
TCOLS   EQU     40              ; columns
HDRROWS EQU     2               ; rows 0-1: the blue header band
RULEROW EQU     2               ; column heads / rule
CTOP    EQU     3               ; first content row
CROWS   EQU     10              ; content rows 3-12
STATROW EQU     13              ; hints / messages / alarm banner
LISTPG  EQU     10              ; events on one list page
HORCBW  EQU     1               ; working screens: column 0 is the chip
                                ; gutter, on the left palette
HORCBS  EQU     14H             ; splash: split at pixel 80, down the
                                ; middle of the logo

; ---- RAM map ----------------------------------------------------------
; 4000H-4D1FH is the visible screen (84 * 40). Everything above is ours:
; 672 bytes to 4FC0H, where the stack tops out under the BIOS cells.
;
; 4D20H-4DEFH is an overlay with two mutually exclusive layouts. A view
; screen keeps per-row metadata for the rendered page there; the form keeps
; its seven field buffers there. Returning from the form always refetches
; (DIRTY=1), so the view half is dead the whole time a form is open.

; -- overlay, view mode --
META    EQU     4D20H           ; 10 rows x 8 bytes, offsets below
MROWSZ  EQU     8
MEVNLO  EQU     0               ; event number within the window, LE
MEVNHI  EQU     1
MFLAGS  EQU     2               ; MF_* bits
MBIN    EQU     3               ; chip color bin 0-3
MDAY    EQU     4               ; the row's date (agenda/month rows)
MMON    EQU     5
MSH     EQU     6               ; start hour/minute, for alarms and edit
MSM     EQU     7
WKX     EQU     4D70H           ; WEEK: per-day next-chip column, 7 bytes
MGRID   EQU     4D70H           ; MONTH: 31 bytes, count(6b)<<2 | bin(2b)
LINEBUF EQU     4D90H           ; DETAIL: 44 bytes of 40-col wrap staging

; -- overlay, form mode --
FTITLE  EQU     4D20H           ; 40
FDATE   EQU     4D48H           ; 11
FSTART  EQU     4D53H           ; 6
FEND    EQU     4D59H           ; 6
FLOC    EQU     4D5FH           ; 33
FDESC   EQU     4D80H           ; 81 (the one RAM-forced narrowing: the C
                                ;    clients allow 96)
FCAT    EQU     4DD1H           ; 16

EDBUF   EQU     4DF0H           ; 96: edit.inc's fixed buffer; big enough
                                ; for the largest field (FDESC) plus NUL

; -- fixed block --
V_KEY   EQU     4E50H           ; last raw key, for edge detection
V_RPT   EQU     4E51H           ; auto-repeat countdown
V_GX    EQU     4E52H           ; grid keyboard cursor
V_GY    EQU     4E53H
EDTOPR  EQU     4E54H           ; the editor's top row
EDMAX   EQU     4E55H           ; the editor's length cap
EDVN    EQU     4E56H           ; value characters currently shown
EDCIX   EQU     4E57H           ; grid cell being drawn
EDHNT   EQU     4E58H           ; word: the editor's hint string
TXONE   EQU     4E5AH           ; a one-byte write's staging cell
CURSLC  EQU     4E5BH           ; reply slice the cart is publishing
AVAIL   EQU     4E5CH           ; word: NET_STATUS bytes waiting
NCONN   EQU     4E5EH           ; NET_STATUS connected flag
NDEVST  EQU     4E5FH           ; NET_STATUS nDevStatus_t
RXLEN   EQU     4E60H           ; word: reply length, captured after a READ
PRVAVL  EQU     4E62H           ; word: the settle loop's previous reading
RXOFF   EQU     4E64H           ; word: GETCH's cursor into the reply
RXEOF   EQU     4E66H           ; nonzero once the channel has drained
NUMW    EQU     4E67H           ; width of the listing's number column
PCOL    EQU     4E68H           ; parser: current line column
PSTATE  EQU     4E69H           ; parser: line state
PTMP    EQU     4E6AH           ; parser scratch, 4 bytes
FIRST   EQU     4E6EH           ; word: event lines to skip (paging)
NROWS   EQU     4E70H           ; rows on the rendered page
SELROW  EQU     4E71H           ; selected row 0..NROWS-1
MORE    EQU     4E72H           ; a full page ended with lines left
VIEW    EQU     4E73H           ; 0 day, 1 week, 2 month, 3 agenda
ANCY    EQU     4E74H           ; word: anchor year
ANCM    EQU     4E76H           ; anchor month
ANCD    EQU     4E77H           ; anchor day (the month view's cursor)
TODY    EQU     4E78H           ; word: today, from the clock
TODM    EQU     4E7AH
TODD    EQU     4E7BH
CLKH    EQU     4E7CH           ; the header clock
CLKMI   EQU     4E7DH
CLKOK   EQU     4E7EH           ; nonzero once a clock fetch has succeeded
TICKS   EQU     4E7FH           ; word: poll iterations since last sync
SYNCM   EQU     4E81H           ; minutes shown when last drawn
DEVNUM  EQU     4E82H           ; word: detail/edit target event number
DPAGE   EQU     4E84H           ; detail page index
ALLEAD  EQU     4E85H           ; alarm lead minutes (settings byte 0)
ALSTATE EQU     4E86H           ; banner up?
ALH     EQU     4E87H           ; next alarm hh:mm (scratch)
ALMN    EQU     4E88H
FSEL    EQU     4E89H           ; form: selected field
FMEDIT  EQU     4E8AH           ; form: 0 compose, 1 edit
FDIRTY  EQU     4E8BH           ; form: 7 dirty flags
FERR    EQU     4E92H           ; form: last validation error
SHOWN   EQU     4E93H           ; repaint policy: chrome drawn?
DIRTY   EQU     4E94H           ; repaint policy: refetch needed?
MFIRST  EQU     4E95H           ; month: weekday of the 1st
MNDAYS  EQU     4E96H           ; month: days in the anchor month
CDY     EQU     4E97H           ; word: date-math workspace
CDM     EQU     4E99H
CDD     EQU     4E9AH
CDN     EQU     4E9BH           ; scratch
PTIMEC  EQU     4E9CH           ; parser: the line's time column
PCATC   EQU     4E9DH           ; parser: the category column
PTITC   EQU     4E9EH           ; parser: the title column
CROW    EQU     4E9FH           ; renderer: current screen row
CALSEL  EQU     4EA0H           ; 40: calendar selector, NUL-terminated
PFLG    EQU     4EC8H           ; parser accumulators: flags
PSH     EQU     4EC9H           ;   start hour
PSM     EQU     4ECAH           ;   start minute
CATLEN  EQU     4ECBH           ;   category length
PEVN    EQU     4ECCH           ;   word: event number
PSKIP   EQU     4ECEH           ;   swallowing "(no events)"
REQTYP  EQU     4ECFH           ; the request being built (url.inc)
NTMO    EQU     4ED0H           ; the open's FNCOMMIT leash
MTDAY   EQU     4ED1H           ; month/week tally: the line's day
MCDAY   EQU     4ED2H           ; month grid: the cell being drawn
MCROW   EQU     4ED3H           ; month grid: its row
MCCOL   EQU     4ED4H           ; month grid: its base column
MCPAIR  EQU     4ED5H           ; month grid: its digit colour pair
WTROW   EQU     4ED6H           ; week: the row being drawn
WTC1    EQU     4ED7H           ; week tally: the weekday's two letters
WTC2    EQU     4ED8H
WTX     EQU     4ED9H           ; week: the next block column
PKN     EQU     4EDAH           ; picker: number of calendars listed
PNEND   EQU     4EDBH           ; parser: end of the number column
PDATEC  EQU     4EDCH           ; parser: start of the date column
PDAY    EQU     4EDDH           ; parser: the row's day (agenda)
PMON    EQU     4EDEH           ; parser: the row's month (agenda)
FTYPEC  EQU     4EDFH           ; form: the selected field's type
FMAXC   EQU     4EE0H           ; form: the selected field's max length
PMODE   EQU     4EE1H           ; parser: 0 render list rows, 1 tally MGRID
; 4EE2H-4FBFH: free, and the stack's room
STACK   EQU     4FC0H           ; grows down; 4FC0H+ left to the BIOS cells

; META flag bits
MFALLD  EQU     1               ; all-day
MFRECUR EQU     2               ; recurring
MFOPEN  EQU     4               ; open-ended
MFFIRED EQU     80H             ; alarm already sounded

; MB_* labels are module fences for tools/checksize.py's budget table.
        ORG     FIRSTC
MB_MAIN:
        DB      55H
        DW      MENUST
        DW      PRGNAM
        DW      PRGSTR
PRGNAM: DB      "GCAL"
        DB      0

PRGSTR: DI
        LD      SP,STACK
        SYSTEM  INTPC
        DO      SETOUT
        DB      LINES*2         ; blank below the text
        DB      HORCBS          ; the splash's palette split
        DB      8
        DO      COLSET
        DW      SPALET
        DO      FILL
        DW      NORMEM
        DW      LINES*BYTEPL
        DB      0               ; color 0 everywhere: the white page
        EXIT

        CALL    SPLASH

        CALL    FNCHECK
        JP      NZ,NOCARD

        ; The keypress that picked us off the on-board menu is still down.
KWAIT:  CALL    KEYRAW
        OR      A
        JR      NZ,KWAIT

        XOR     A
        LD      (V_KEY),A
        LD      (CURSLC),A
        LD      A,1             ; start the grid cursor on 'A', not space
        LD      (V_GX),A
        LD      (V_GY),A

        IF      DEMO
        JP      DEMOGO
        ELSE
        CALL    WPAL
        LD      A,10            ; defaults until appkeys land (phase 4)
        LD      (ALLEAD),A
        XOR     A
        LD      (CALSEL),A
        LD      (VIEW),A
        LD      (SELROW),A
        LD      (CLKOK),A
        LD      H,A
        LD      L,A
        LD      (FIRST),HL
        LD      (TICKS),HL
        LD      A,0FFH
        LD      (SYNCM),A
        CALL    CLKGO           ; the clock is mandatory; blocks until it
        CALL    DLDTOD          ; anchor := today
        CALL    DSTANC
        LD      A,1
        LD      (DIRTY),A
        JP      VIEWLP

; CLKGO: three tries; then a message and a key per retry round. The rest
; of the program assumes a believable "today".
CLKGO:  LD      B,3
CKG1:   PUSH    BC
        CALL    CLKGET
        POP     BC
        RET     NC
        DJNZ    CKG1
        LD      HL,SNOCLK
        CALL    MSGROW
        CALL    INWAIT
        JR      CLKGO
        ENDIF

; ---- Errors -----------------------------------------------------------
NOCARD: LD      HL,SNOCART
        LD      D,STATROW
        LD      E,3
        LD      C,XWONK
        CALL    TXTAT
HALTE:  JR      HALTE

; ---- Data -------------------------------------------------------------
MB_DATA:
; COLSET stores descending, ports 7 down to 0: left palette colors 3,2,1,0,
; then right palette colors 3,2,1,0. Byte = (hue << 3) | luminance; hue 0
; is the grayscale column. Values computed against MAME's palette math for
; the Google colors -- see astrocade/README.md.
;
; Working screens (HORCB=1: column 0 is the chip gutter on the left):
;   left  3 chip blue (D3), 2 chip green (C3), 1 chip red (51), 0 white
;   right 3 gray (03), 2 Calendar blue (F4), 1 black ink (00), 0 white
PALET:  DB      0D3H,0C3H,051H,007H
        DB      003H,0F4H,000H,007H

; Splash (HORCB=14H: split down the logo's midline):
;   left  3 black, 2 Google green (C2), 1 Google blue (F4), 0 white
;   right 3 black, 2 Google yellow (75), 1 Google red (4C), 0 white
SPALET: DB      000H,0C2H,0F4H,007H
        DB      000H,075H,04CH,007H

SNOCART: DB     "NO FUJINET CART",0
        IF      DEMO
        ELSE
SNOCLK: DB      "No clock",0
        ENDIF

MB_SPLASH:
        INCLUDE "splash.inc"
MB_UI:
        INCLUDE "ui.inc"
        IF      DEMO
MB_DEMO:
        INCLUDE "demo.inc"
        ELSE
MB_VIEWS:
        INCLUDE "views.inc"
MB_MONTH:
        INCLUDE "month.inc"
MB_DETAIL:
        INCLUDE "detail.inc"
MB_FORM:
        INCLUDE "form.inc"
        ENDIF
MB_PARSE:
        INCLUDE "parse.inc"
MB_NET:
        INCLUDE "net.inc"
MB_URL:
        INCLUDE "url.inc"
MB_DATE:
        INCLUDE "date.inc"
MB_CLOCK:
        INCLUDE "clock.inc"
MB_EDIT:
        INCLUDE "edit.inc"
MB_INPUT:
        INCLUDE "input.inc"
MB_SOUND:
        INCLUDE "sound.inc"
MB_STATE:
        INCLUDE "state.inc"
MB_GFX:
        INCLUDE "gfx.inc"
MB_FONT:
        INCLUDE "assets/font.inc"
MB_FUJILIB:
        INCLUDE "fujilib.inc"
MB_END:
