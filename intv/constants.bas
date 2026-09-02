' constants.bas -- screen geometry, the scratch RAM map, colours, and the
' Google-to-Intellivision palette table.
'
' THE RAM BUDGET RULE
'
' IntyBASIC gives this program 228 bytes of 8-bit scratchpad variables and 47
' 16-bit variables, and NO string type at all. It used to be 222 of the 8-bit
' ones: CONT.KEY costs 6, and input.bas decodes the keypad from the raw byte
' itself now, so those came back. NEVER buffer tabular data -- event rows, calendar names, titles
' -- in DIM'd variables. A fetched page is parsed straight out of the mailbox
' RX window (FN_RX) as each 512-byte reply lands, and only the compact fields
' the views actually draw are kept, in the SC_* block below: ordinary cart RAM
' outside the mailbox, addressed with PEEK/POKE exactly like the mailbox is.
'
' This file is INCLUDEd first, before every other module, because IntyBASIC
' resolves a CONST at its point of use: a CONST declared in a later INCLUDE is
' silently treated as an unassigned variable reading 0.

' ---------------------------------------------------------------------------
' Screen
' ---------------------------------------------------------------------------
    CONST BACKTAB_ADDR = $0200
    CONST SCREEN_ROWS  = 12
    CONST SCREEN_COLS  = 20

    DEF FN screenpos(aColumn, aRow)  = (((aRow) * SCREEN_COLS) + (aColumn))
    DEF FN screenaddr(aColumn, aRow) = (BACKTAB_ADDR + (((aRow) * SCREEN_COLS) + (aColumn)))

' ---------------------------------------------------------------------------
' Colours, colour-stack mode.
'
' 8-15 are reachable as a FOREGROUND only on a GRAM card: a cell's foreground
' is bits 0-2 plus bit 12, and on a GROM card bit 12 is the Coloured Squares
' selector instead, so the STIC requires foreground bit 3 to be 0 there. That
' is the whole reason event colours are drawn as GRAM chips and month-grid day
' numbers as GRAM digits -- five of Google's eleven colours live above 7.
' Colour stack REGISTERS (the MODE arguments) and MOBs take the full 0-15.
' ---------------------------------------------------------------------------
    CONST CS_BLACK       = 0
    CONST CS_BLUE        = 1
    CONST CS_RED         = 2
    CONST CS_TAN         = 3
    CONST CS_DARKGREEN   = 4
    CONST CS_GREEN       = 5
    CONST CS_YELLOW      = 6
    CONST CS_WHITE       = 7
    CONST CS_GREY        = 8
    CONST CS_CYAN        = 9
    CONST CS_ORANGE      = 10
    CONST CS_BROWN       = 11
    CONST CS_PINK        = 12
    CONST CS_LIGHTBLUE   = 13
    CONST CS_YELLOWGREEN = 14
    CONST CS_PURPLE      = 15

    CONST CS_ADVANCE = $2000
    CONST GRAM_SELECT = $0800

' gramfg: a full 0-15 foreground for a GRAM cell. Bits 0-2 sit in place and
' bit 3 moves up to bit 12 ($1000), hence the *512. Only valid with
' GRAM_SELECT set -- see the note above.
    DEF FN gramfg(aColor) = (((aColor) AND 7) + (((aColor) AND 8) * 512))

' ---------------------------------------------------------------------------
' Sprites
' ---------------------------------------------------------------------------
    CONST SPR_VISIBLE = $0200
    CONST SPR_ZOOMX2  = $0400
' SPR_ZOOMY2 is bits 9-8 = 01, which is NORMAL height. Omitting it gives a
' half-height sprite with rows skipped -- the IntyBASIC manual's "0.5x".
    CONST SPR_ZOOMY2  = $0100
    CONST SPR_FLIPX   = $0400
    CONST SPR_FLIPY   = $0800
' SPR_BEHIND is bit 13 of the ATTRIBUTE word and means PRIORITY. The IntyBASIC
' manual calls it "change colour stack", which is the BACKTAB meaning
' copy-pasted into the SPRITE section by mistake.
    CONST SPR_BEHIND  = $2000
    CONST SPR_GRAM    = $0800

' MOB coordinates are offset by 8 from background cards: MOB (8,8) sits on
' card (0,0).
    CONST MOB_X0 = 8
    CONST MOB_Y0 = 8

' ---------------------------------------------------------------------------
' GRAM cards. 16 of the 64 are used.
' ---------------------------------------------------------------------------
    CONST GLYPH_BLOCK  = 0    ' solid 8x8: event colour chip, logo body, grid cursor
    CONST GLYPH_CORNER = 1    ' logo corner stroke, flipped into all four corners
    CONST GLYPH_DIG0   = 2    ' digits 0-9 occupy cards 2-11
    CONST GLYPH_ALLDAY = 12   ' horizontal bar marking an all-day event
    CONST GLYPH_BELL   = 13   ' alarm indicator
    CONST GLYPH_UP     = 14
    CONST GLYPH_DOWN   = 15
    CONST GRAM_CARDS   = 16   ' total uploaded by gc_define_gram

' ---------------------------------------------------------------------------
' Controller
' ---------------------------------------------------------------------------
    CONST DISC_UP    = $0004
    CONST DISC_RIGHT = $0002
    CONST DISC_DOWN  = $0001
    CONST DISC_LEFT  = $0008

' CONT.KEY-decoded values, not raw matrix bits.
    CONST KEYPAD_0 = 0
    CONST KEYPAD_1 = 1
    CONST KEYPAD_2 = 2
    CONST KEYPAD_3 = 3
    CONST KEYPAD_4 = 4
    CONST KEYPAD_5 = 5
    CONST KEYPAD_6 = 6
    CONST KEYPAD_7 = 7
    CONST KEYPAD_8 = 8
    CONST KEYPAD_9 = 9
    CONST KEYPAD_CLEAR = 10
    CONST KEYPAD_ENTER = 11
    CONST KEYPAD_NONE  = 12

' ---------------------------------------------------------------------------
' Text entry (screen.bas value window + t9.bas).
'
' The GRID_* block that used to sit here is gone. It described the 6x16
' character grid this app inherited from fujinet-config and never called; t9.bas
' is the editor now, and it needs three constants rather than fourteen.
'
' The value window is the top three rows, tail-anchored, so what is on screen is
' always where the typing is. t9.bas also owns row 3 (the candidate strip) and
' row 11 (hints + the T9/ABC indicator) while it is running, and touches nothing
' between -- which is exactly the seven rows st_form.bas puts its fields on.
' ---------------------------------------------------------------------------
    CONST VAL_ROW0        = 0
    CONST VAL_ROWS        = 3
    CONST VAL_CELLS       = 60    ' VAL_ROWS * SCREEN_COLS

' These two live here rather than in the files that own them, t9.bas and
' st_form.bas, because bar.bas reads both and comes many INCLUDEs earlier --
' and a CONST used before its definition does not fail, it silently reads as
' zero. FRM_ROW0 = VAL_ROWS + 1 is not a coincidence: the fields start on the
' first row the editor does not own.
    CONST T9_STRIP_ROW    = 3     ' t9.bas's candidate strip
    CONST FRM_ROW0        = 4     ' st_form.bas's first field row

' ---------------------------------------------------------------------------
' Common view layout.
'
' Rows 0-2 are the header run, 3-9 the content run, 10 a permanent blank
' spacer, 11 the key hints. The spacer is load-bearing, not cosmetic: the
' colour-stack selection bar (bar.bas) needs a non-empty content run BELOW the
' selection as well as above it, and row 10 is what supplies it when the
' bottom entry is selected. Column 0 likewise keeps the content colour on
' every row -- it is the run above when the TOP entry is selected, and it is
' exactly where the event colour chip goes. The constraint and the feature are
' the same cell.
' ---------------------------------------------------------------------------
    CONST ROW_TITLE  = 0
    CONST ROW_SUB    = 1
    CONST ROW_COLS   = 2      ' column headings / rule
    CONST ROW_FIRST  = 3      ' first content row
    CONST ROWS_PER_PAGE = 7   ' rows 3-9
    CONST ROW_SPACER = 10
    CONST ROW_HINT   = 11

    CONST CHIP_COL = 0        ' the colour-chip gutter

' An event row: chip, "09:00" (or "ALLDY"), a space, then the title. The title
' window is narrower than a stored title, which is what the bounce-scroll in
' scroll.bas is for.
    CONST TIME_COL  = 1
    CONST TIME_W    = 5
    CONST TITLE_COL = 7
    CONST TITLE_W   = SCREEN_COLS - TITLE_COL

' ---------------------------------------------------------------------------
' Scratch RAM map, $8040-$9BFF (declared "+RWN" by fujinet.bas's one MEMATTR).
'
' Never widen that MEMATTR past $9BFF. jzIntv's --fujinet peripheral registers
' its own handler for $9C00-$9F3F and whichever peripheral registered first
' answers a given address, so declaring the mailbox range as cart RAM shadows
' the emulated FujiNet with inert RAM and the mailbox never comes up.
'
' AND THE MAP STARTS AT $8040, NOT $8000. The STIC decodes its control
' registers at $0000-$003F and MIRRORS them at $4000, $8000 and $C000 -- on
' real hardware, and in jzIntv, which registers "STIC (alias)" over
' $8000-$803F in cfg.c. A write to cart RAM down there lands in the RAM AND in
' the STIC, so the first 64 bytes of this map are really the register file:
'
'   $8000-$8017  MOB x / y / attribute     rewritten by the ISR every frame
'   $8018-$801F  collision registers       read back as phantom collisions
'   $8020-$8021  display enable / mode     rewritten by the ISR every frame
'   $8028-$802B  THE COLOUR STACK          written only by MODE
'   $802C, $8032 border colour / mask      rewritten by the ISR every frame
'   $8030-$8031  scroll x / y              never rewritten (no SCROLL here)
'
' Only the colour stack and the scroll offsets are not repaired every frame,
' and the colour stack is the one that shows. SC_EVT used to start at $8000,
' which put event record 2's EVT_NUMLO -- the adapter's event number, so 3 for
' the third event of any listing -- onto $8028, colour stack entry 0. Rows 0-2
' and the hint row run on entry 0 (bar.bas), so the header and the footer
' turned CS_TAN (3) the moment a fetch returned three events or more, and
' COL_HEAD is CS_TAN, so the column headings vanished into it. Nothing was
' wrong with the drawing code and nothing failed; the STIC had simply been
' handed an event number as a colour.
'
' `make check` fails the build if any SC_* lands below $8040.
' ---------------------------------------------------------------------------
' 96 events with 31-character titles, rather than more events with shorter
' ones: the DAY view shows a 13-column title window and bounce-scrolls the
' selected row, which only earns its keep if the stored title is meaningfully
' longer than the window. 96 covers any real day or week; the MONTH view does
' not store events at all (see SC_DAYS), and AGENDA defaults to 20.
    CONST MAX_EVENTS = 96

    CONST SC_EVT    = $8040   ' 1536  parsed event index, EVT_STRIDE x MAX_EVENTS
    CONST SC_TITLE  = $8640   ' 3072  event titles, TITLE_STRIDE x MAX_EVENTS
    CONST SC_CAL    = $9240   '   64  selected calendar selector, NUL-terminated
    CONST SC_DAYS   = $9280   '   42  month grid: per-day event count
    CONST SC_DCOL   = $92B0   '   42  month grid: per-day dominant colour
' Text entry's own scratch. SC_T9_* belong to t9.bas and are declared here
' rather than in it because SC_INJECT has to be visible to input.bas, which is
' included first -- and a CONST referenced before the file that declares it is
' silently read as zero rather than diagnosed. Keeping all three together also
' keeps the map below honest.
'
' SC_INJECT is the scripted-input hook (see input.bas): two magic bytes, then a
' keypad value, a disc value and a button flag that override what in_poll
' DECODED, then a sixth byte that replaces the raw controller byte BEFORE it
' decodes anything. The sixth is the one worth having -- overriding decoded
' values skips in_poll's own logic, which is how the keypad/disc aliasing bug
' got past a green test suite.
'
' It is not t9-specific any more: moving it out of t9_poll and into in_poll is
' what makes every screen scriptable, the form and the views included.
    CONST SC_T9_STACK  = $92E0   ' range stack, 16 levels x 4 bytes
    CONST SC_T9_DIGITS = $9320   ' typed digit sequence, 16 bytes
    CONST SC_INJECT    = $9390   ' test-harness input injection, 6 bytes
    CONST SC_EDIT   = $93A0   '  256  flattened detail text / timezone readout
    CONST SC_ENTRY  = $94A0   '  128  bounce-scroll source (scroll.bas hardcodes this)
' NOT SC_HOLD. IntyBASIC folds names to upper case, so a constant called
' SC_HOLD would be the same identifier as scroll.bas's sc_hold pause counter --
' which compiles, silently, into one name that is both a $9520 address and a
' frame counter. Nothing in the SC_* block may collide with scroll.bas's sc_*
' variables: sc_row, sc_col, sc_max, sc_color, sc_off, sc_len, sc_dir, sc_tick,
' sc_hold, sc_idle, sc_active.
    CONST SC_LINEBUF = $9520  '  128  partial line carried across a 512-byte read
    CONST SC_DETAIL = $95A0   '  512  event detail, wrapped to 21-byte rows
' The compose/edit form parks on top of SC_DETAIL. The two are never live at
' once -- opening the form leaves the detail screen, and returning from it
' redraws -- and this is the same overlay the C clients use on their tight
' targets (GC_FORM_OVERLAY, src/gcal.h). 313 of the 512 bytes are used:
'     +0    209  the seven fields, at st_form.bas's frm_off offsets
'     +209    7  one dirty flag per field
'     +216   97  a snapshot of the field being edited, for change detection
    CONST SC_FORM   = SC_DETAIL
    CONST SC_FDIRTY = SC_DETAIL + 209
    CONST SC_FSNAP  = SC_DETAIL + 216
    CONST SC_LIST   = $97A0   '  640  calendar picker: names + selectors
    CONST SC_AGD    = $9A20   '  384  AGENDA display list, 2 B x 192
    CONST SC_HDR    = $9BA0   '   32  window title from the adapter's line 0
    '     $9BC0-$9BFF free

' The adapter's window title ("Fri 28 Aug 2026", "Week of Sun 23 Aug 2026",
' "August 2026", "Agenda from 28 Aug 2026") is displayed verbatim on the title
' row, so 32 is comfortably more than the 20 columns can show.
    CONST SC_HDR_MAX = 32

' SC_EVT record. 16 bytes; the stride is a power of two so indexing is a
' shift rather than a multiply.
'
' EVT_DAY means something different per view, because that is what each view
' actually needs and the adapter's date column already differs per view:
'   DAY    always 0 -- every event is on the anchor day
'   WEEK   0-6, day of week, from the "Sun".."Sat" column
'   MONTH  1-31, day of month, from the "Fr 28" column
'   AGENDA 1-31, day of month; EVT_MON carries the month so the date
'          separator can still be rendered across a month boundary
    CONST EVT_STRIDE   = 16
    CONST EVT_DAY      = 0
    CONST EVT_FLAGS    = 1
    CONST EVT_COLOR    = 2
    CONST EVT_SH       = 3
    CONST EVT_SM       = 4
    CONST EVT_EH       = 5
    CONST EVT_EM       = 6
    CONST EVT_TLEN     = 7
    CONST EVT_NUMLO    = 8    ' eventNum, little-endian: the /N a detail open needs
    CONST EVT_NUMHI    = 9
    CONST EVT_MON      = 10   ' 1-12, AGENDA only
    '     11-15 reserved

    CONST EVF_ALLDAY    = 1
    CONST EVF_RECURRING = 2
    CONST EVF_FIRED     = 4   ' alarm already sounded for this event
    CONST EVF_OPENEND   = 8   ' "HH:MM->" -- ends on a later day

    CONST TITLE_STRIDE = 32

' Calendar picker: SC_LIST holds CAL_MAX entries of CAL_STRIDE bytes, each a
' NUL-terminated display name followed at CAL_SELOFF by a NUL-terminated
' selector to put in the devicespec.
'
' Ten is not arbitrary: the adapter itself resolves at most GCAL_MAX_CALENDARS
' = 8 for a merged selector, so eight real calendars plus the "all shown" entry
' covers everything it will ever act on, with one spare.
    CONST CAL_MAX    = 10
    CONST CAL_STRIDE = 64
    CONST CAL_SELOFF = 24

' AGENDA display list. An agenda mixes date separators into the event rows, and
' scrolling needs random access to that combined sequence, so it is
' materialised once after a fetch instead of being recomputed per row.
'   byte 0: 0 = event row, 1 = date separator
'   byte 1: event index -- for a separator, the first event of the group, whose
'           EVT_DAY/EVT_MON supply the date to print
    CONST AGD_MAX    = 192
    CONST AGD_STRIDE = 2
    CONST AGD_SEP    = 1

' ---------------------------------------------------------------------------
' Views and top-level states. Both 0-based: IntyBASIC's ON ... GOSUB uses the
' value directly as the jump-table index, not value-1 the way classic BASIC's
' 1-based ON ... GOTO does.
' ---------------------------------------------------------------------------
    CONST VIEW_DAY    = 0
    CONST VIEW_WEEK   = 1
    CONST VIEW_MONTH  = 2
    CONST VIEW_AGENDA = 3

' Returned by vw_sel_event when the selected row is not an event -- an agenda
' date separator, or an empty list. 255 can never be a real index: MAX_EVENTS
' is 96.
    CONST VW_NO_EVENT = 255

    CONST ST_BOOT   = 0
    CONST ST_PICK   = 1       ' calendar picker
    CONST ST_VIEW   = 2       ' whichever of the four views cur_view names
    CONST ST_EVENT  = 3       ' event detail
    CONST ST_SETUP  = 4       ' alarm lead / timezone readout
    CONST ST_FORM   = 5       ' compose / edit an event

' #evrec: a pointer to one SC_EVT record, shared by every screen that walks
' the event array -- st_day, st_week, st_agenda, st_event and alarm.bas each
' had their own (#dy_p/#wk_p/#ag_p/#ev_p/#al_p), which cost five of the 47
' 16-bit slots to say the same thing five times. t9.bas needs eleven of them,
' so they were merged.
'
' The rule that makes this safe, and the rule to keep: #evrec belongs to
' whichever screen is currently drawing, and is never live across a state
' change. The four views are mutually exclusive states, and al_scan runs from
' the main loop AFTER `ON state GOSUB` has returned, never nested inside a
' draw -- so no two of the old five were ever live at once. st_pick's #pk_p is
' deliberately NOT folded in: it points into SC_LIST, not SC_EVT, and it stays
' live across the picker's fetch loop.
    DIM #evrec

' ---------------------------------------------------------------------------
' Google's eleven event colours, mapped onto the Intellivision's sixteen.
'
' The GCAL adapter does not expose the numeric colorId. When an event has one
' it substitutes the colour NAME into the category field (Calendar.cpp's
' category_for(), whose precedence is extendedProperties, then colour, then the
' calendar's own name), so matching the category against these eleven names is
' how a client recovers the colour. An unmatched category is a calendar name,
' not a colour, and falls back to COLOR_DEFAULT.
'
' Eleven Google colours land on eleven distinct Intellivision colours, which is
' a better fit than it has any right to be. Palette values checked against
' jzIntv src/gfx/gfx_n900.c.
'
'   Lavender  #7986CB -> 13 light blue     Graphite  #616161 ->  8 grey
'   Sage      #33B679 ->  5 green          Blueberry #3F51B5 ->  1 blue
'   Grape     #8E24AA -> 15 purple         Basil     #0B8043 ->  4 dark green
'   Flamingo  #E67C73 -> 12 pink           Tomato    #D50000 ->  2 red
'   Banana    #F6BF26 ->  6 yellow
'   Tangerine #F4511E -> 10 orange
'   Peacock   #039BE5 ->  9 cyan
'
' Names are compared whole, not by prefix: Grape and Graphite first differ at
' index 4, and Banana / Basil / Blueberry only separate at index 1-2.
' ---------------------------------------------------------------------------
    CONST GC_NCOLORS    = 11
    CONST COLOR_DEFAULT = CS_CYAN

lit_colnames:
    DATA 76,65,86,69,78,68,69,82,0,0        ' LAVENDER
    DATA 83,65,71,69,0,0,0,0,0,0            ' SAGE
    DATA 71,82,65,80,69,0,0,0,0,0           ' GRAPE
    DATA 70,76,65,77,73,78,71,79,0,0        ' FLAMINGO
    DATA 66,65,78,65,78,65,0,0,0,0          ' BANANA
    DATA 84,65,78,71,69,82,73,78,69,0       ' TANGERINE
    DATA 80,69,65,67,79,67,75,0,0,0         ' PEACOCK
    DATA 71,82,65,80,72,73,84,69,0,0        ' GRAPHITE
    DATA 66,76,85,69,66,69,82,82,89,0       ' BLUEBERRY
    DATA 66,65,83,73,76,0,0,0,0,0           ' BASIL
    DATA 84,79,77,65,84,79,0,0,0,0          ' TOMATO
    CONST COLNAME_STRIDE = 10

lit_colvals:
    DATA CS_LIGHTBLUE, CS_GREEN, CS_PURPLE, CS_PINK, CS_YELLOW, CS_ORANGE
    DATA CS_CYAN, CS_GREY, CS_BLUE, CS_DARKGREEN, CS_RED

' ---------------------------------------------------------------------------
' Cross-module globals. DIM'd here, ahead of every INCLUDE, because IntyBASIC
' fails to compile a DIM of a name another INCLUDE already referenced.
' ---------------------------------------------------------------------------
    DIM state, cur_view, sel_row, vid_now, vid_want
' fm_sel is st_form.bas's selection, but bar.bas's frm_bar_* routines key off
' it and bar.bas is INCLUDEd first -- a name used ahead of its DIM is
' auto-created and the real DIM then fails, so it is declared here.
    DIM fm_sel
    DIM num_rows, ev_count, gc_trunc, ev_sel
    ' The date the current view is anchored on. The year needs 16 bits; month
    ' and day do not.
    DIM #cur_y
    DIM cur_mo, cur_d
' al_active belongs to alarm.bas and #ev_num to st_event.bas, but both are read
' by files that come EARLIER in the include order -- st_view.bas tests
' al_active to decide whether the banner is up, and st_form.bas needs the
' adapter's event number to build an edit URL. A name used before its own file
' DIMs it is auto-created at first use, and the real DIM then fails with
' "already defined". The build carried that error for al_active before compose
' was added; it is fixed here rather than reproduced.
    DIM al_active
    DIM #ev_num
