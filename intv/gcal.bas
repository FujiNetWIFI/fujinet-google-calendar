' gcal.bas -- Google Calendar for the Intellivision, over FujiNet.
'
' Talks to the GCAL: protocol adapter in fujinet-firmware, which speaks Google
' Calendar v3 and hands back day/week/month/agenda windows already resolved
' into local time. Day, week, month and agenda views, Google's colour scheme
' mapped onto the Intellivision palette, and alarms for upcoming events.
'
' ---------------------------------------------------------------------------
' INCLUDE ORDER IS LOAD-BEARING, TWICE OVER
'
' First: execution is sequential and INCLUDE pastes files in verbatim, so
' straight-line execution falling into a PROCEDURE or DATA body would corrupt
' the return stack. Hence the GOTO ahead of every INCLUDE, jumping clear of all
' of them before real code starts.
'
' Second: IntyBASIC resolves a CONST at its point of use, and a CONST declared
' in a LATER file is silently treated as an unassigned variable reading 0 --
' no error, just a wrong number. So the order below is a dependency order:
' constants.bas first, then the transport, then the modules that build on it.
' GOSUB targets are resolved globally and do not constrain this; only CONSTs
' and DIMs do.
' ---------------------------------------------------------------------------

    GOTO gc_start

    INCLUDE "constants.bas"     ' CONSTs everything else reads
    INCLUDE "fujinet.bas"       ' mailbox transport, N:, appkeys
    INCLUDE "clock.bas"         ' device $45, civil-date arithmetic
    INCLUDE "gcalnet.bas"       ' devicespec, fetch, column parser
    INCLUDE "screen.bas"        ' COL_*, scr_puts / scr_recolor
    INCLUDE "scroll.bas"        ' bounce-scroll for long titles
    INCLUDE "input.bas"         ' edge-detected controller poll
    INCLUDE "wrap.bas"          ' word wrap for the detail screen
    INCLUDE "bar.bas"           ' colour-stack selection bar
    INCLUDE "gfx.bas"           ' GRAM cards, MOB logo
    INCLUDE "st_view.bas"       ' shared chrome, navigation, dispatch
    INCLUDE "st_day.bas"

' ---------------------------------------------------------------------------
' The default $5000-$6FFF segment is 8K words and this app does not fit in it.
' Left alone, IntyBASIC does not report that -- it SILENTLY auto-continues into
' $7000, and then into $8000, which on this cartridge is the FujiNet RAM window
' rather than ROM at all. The result assembles, links, and produces a cart that
' fails EXEC's boot detection: the PC lands in unprogrammed GROM two
' instructions in. The observed .cfg before these directives went in was
'
'     $2000 - $2FFF = $7000
'     $3000 - $377F = $8000 RAM 8
'
' with the scratch-RAM memattr pushed from $8000 up to $8780 -- i.e. code
' placed on top of the SC_* block.
'
' So the segments are declared explicitly. $D000-$DFFF and $F000-$FFFF are the
' two areas the IntyBASIC manual sanctions for this, and the only two the sister
' FujiNet clients have actually validated on hardware. $7000 and $E000 are
' known-bad. `make check` fails the build if a fifth mapping line ever appears.
'
' Split by how cold the code is: the four views and everything the main loop
' touches every frame stay in the default segment; the screens reached by a
' deliberate keypress go out of line.
' ---------------------------------------------------------------------------
    ASM ORG $D000
    INCLUDE "st_month.bas"
    INCLUDE "st_agenda.bas"
    INCLUDE "alarm.bas"         ' AL_* CONSTs st_pick.bas reads

' ---------------------------------------------------------------------------
' $A000-$B7FF, the fourth segment, added for compose/edit.
'
' The three above were all this app used, and the Makefile still refuses any
' others -- but text entry does not fit in what they had left (about 2K words,
' against ~4K of editor and form plus a dictionary). $A000 is a normal cart ROM
' area and is served on the FujiNet cartridge.
'
' $B800-$BFFF is NOT usable and must stay empty: GRAM is aliased there on the
' Intellivision bus, so ROM placed at $B800 is simply not what reads back.
' mkdict.py takes the pool ceiling as an argument for that reason, and
' `make check` fails the build if anything lands at $B800 or above.
'
' Order inside the segment: code first at $A000, then the dictionary's letter
' pool at $B000 (T9D_CHARS). $B000 is a fixed address rather than "wherever the
' code ended" because t9.bas has to PEEK the pool through a CONST -- see its
' header -- so the boundary is declared, and overrunning it is what make check
' is for.
' ---------------------------------------------------------------------------
    ASM ORG $A000
    INCLUDE "t9.bas"            ' T9 predictive text entry
    INCLUDE "st_form.bas"       ' the compose/edit form
' st_week.bas is here rather than up in $D000 with the other three cold views,
' purely to balance the two halves of the dictionary. The pool needs ~2.5 words
' per entry and the index needs 1, so they want different-sized holes; moving
' one ~430-word module across moved the binding constraint and bought about
' seventy more dictionary entries. Its WK_* constants are read only by itself,
' so its position in the include order does not matter -- which is the only
' reason it, and not one of its neighbours, is the one that moved.
    INCLUDE "st_week.bas"

' The generated dictionary. It carries its own ASM ORGs -- pool at $B000,
' meta+index into the tail of $D000 -- so it must be followed by an explicit
' ORG, never by bare code. That is the whole of the experiment's "INCLUDE it
' LAST" rule: what actually matters is that nothing is assembled after it at
' an address it chose.
'
' It is NOT last here, and deliberately so. IntyBASIC appends its runtime
' epilogue after the whole program, wherever the final ORG left the assembler
' -- and that epilogue is 1345 words, not the few hundred one might assume.
' Left at the end, it followed the index into $D000 and pushed the segment
' 432 words into $E000, which this cartridge does not boot from. Putting the
' last code block after the dictionary parks the epilogue back in $F000's
' tail, where it has always lived.
    INCLUDE "t9dict.bas"

    ASM ORG $F000
    INCLUDE "st_event.bas"
    INCLUDE "st_pick.bas"

' ---------------------------------------------------------------------------
' Boot.
' ---------------------------------------------------------------------------
gc_start:
    ' Cart RAM comes up undefined, so nothing may be read before it is written.
    POKE SC_CAL, 0
    POKE SC_HDR, 0
    ' in_poll reads SC_INJECT every frame looking for a two-byte magic that arms
    ' its scripted-input hook. Undefined cart RAM spelling it is a 1-in-65536
    ' shot; clearing it once costs nothing and removes the question.
    POKE SC_INJECT, 0
    #fn_tmo = FN_TMO_DEF

    MODE 0, CS_BLACK, CS_BLACK, CS_BLACK, CS_BLACK
    BORDER CS_BLACK
    WAIT
    GOSUB scr_clear
    GOSUB gc_define_gram

    PRINT AT screenpos(0, 0) COLOR COL_NORMAL, "GOOGLE CALENDAR"
    PRINT AT screenpos(0, 2) COLOR COL_DIM, "CONNECTING TO FUJI  "
    GOSUB gc_logo_show

    GOSUB fn_wait_mailbox
    IF fn_ok = 0 THEN
        PRINT AT screenpos(0, 2) COLOR COL_ERROR, "NO CARTRIDGE MAILBOX"
        PRINT AT screenpos(0, 4) COLOR COL_DIM, "CHECK THE CARTRIDGE "
        GOTO gc_halt
    END IF

    PRINT AT screenpos(0, 2) COLOR COL_DIM, "READING CLOCK...    "
    GOSUB clk_fetch
    IF clk_ok = 0 THEN
        ' Without the clock there is no "today" to anchor a view on, and no
        ' wall time to compare an alarm against. The adapter would still serve
        ' a date typed into a devicespec, but nothing here can supply one.
        PRINT AT screenpos(0, 2) COLOR COL_ERROR, "FUJINET CLOCK FAILED"
        PRINT AT screenpos(0, 4) COLOR COL_DIM, "ENABLE APETIME AND  "
        PRINT AT screenpos(0, 5) COLOR COL_DIM, "SET A POSIX TIMEZONE"
        GOTO gc_halt
    END IF
    GOSUB clk_today

    PRINT AT screenpos(0, 2) COLOR COL_DIM, "READING SETTINGS... "
    GOSUB pk_settings_load
    GOSUB al_init

    cur_view = VIEW_DAY
    state = ST_VIEW
    vw_shown = 0
    vw_dirty = 1
    vid_now = 255               ' force the first profile switch to fire

' ---------------------------------------------------------------------------
' Main loop.
' ---------------------------------------------------------------------------
gc_main:
    WAIT

    GOSUB clk_tick
    IF clk_resync <> 0 THEN
        clk_resync = 0
        ' Re-syncing costs a blocking transaction, so it only happens on the
        ' half hour and never inside a screen's own input handling.
        GOSUB clk_fetch
    END IF

    ' Video profile, by id rather than by a mode boolean: two screens can share
    ' MODE 0 and differ only in their stack, which a boolean would never catch.
    ' CLS on every transition, or the outgoing BACKTAB is reinterpreted under
    ' the new palette for a frame. MODE lands on the NEXT frame and borrows the
    ' colour variable to carry its arguments until then, so no PRINT ... COLOR
    ' may run until after the WAIT inside these.
    ' ST_FORM shares the views' stack, not the other screens' all-black one:
    ' its selected-field bar IS colour-stack entry 2, so it cannot exist on a
    ' profile whose four entries are all black. That also makes ST_VIEW <->
    ' ST_FORM stop being a profile change -- nothing depended on the scr_clear
    ' and the shown-flag reset below, because frm_new/frm_edit_event already
    ' zero fm_shown, frm_draw does its own scr_clear, and do_form zeroes
    ' vw_shown on the way out.
    vid_want = 1
    IF state = ST_VIEW THEN vid_want = 0
    IF state = ST_FORM THEN vid_want = 0
    IF vid_want <> vid_now THEN
        vid_now = vid_want
        GOSUB scr_clear
        IF vid_now = 0 THEN
            GOSUB vw_video
        ELSE
            MODE 0, CS_BLACK, CS_BLACK, CS_BLACK, CS_BLACK
            BORDER CS_BLACK
            WAIT
        END IF
        ' Whatever screen is up must redraw itself: the CLS above wiped it.
        vw_shown = 0
        ev_shown = 0
        pk_shown = 0
        su_shown = 0
        fm_shown = 0
    END IF

    ON state GOSUB do_idle, do_pick, do_view, do_event, do_setup, do_form

    GOSUB al_scan

    GOTO gc_main

' do_idle: ST_BOOT's slot in the dispatch table. Boot finishes before the loop
' starts, so this state is never current -- but ON ... GOSUB indexes the table
' directly, so index 0 has to exist.
do_idle: PROCEDURE
    state = ST_VIEW
END

gc_halt:
    WAIT
    GOTO gc_halt
