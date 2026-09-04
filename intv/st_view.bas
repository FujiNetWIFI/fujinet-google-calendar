' st_view.bas -- chrome, navigation and dispatch shared by the four views.
'
' Every view has the same shape: rows 0-2 are the header run (the adapter's own
' window title, a status line, and either column headings or a rule), rows 3-9
' are seven content rows, row 10 is the blank spacer bar.bas needs, and row 11
' is the key hints.
'
' The one rule that shapes all of this: gc_fetch BLOCKS. fn_transact spins on
' WAIT for up to 900 frames per transaction, so a fetch happens exactly once --
' when a view is entered or its period changes -- and never from an input loop.
' Afterwards, moving the selection is four BACKTAB writes (bar.bas) and the
' screen is not redrawn at all.

' Every DIM ahead of the first PROCEDURE: IntyBASIC will not accept a DIM of a
' name that earlier code already referenced.
    DIM vw_shown, vw_dirty, vw_i, vw_c, vw_n
    DIM vl_len, vw_chipc, vw_chipa, vw_h, vw_m, vw_alld
    DIM vw_hintpg, vw_handled, vw_first
    DIM #vw_p

' The hint row, in two pages because twenty columns will not hold both. Keypad
' 9 toggles. Page 1 "1DAY 2WK 3MO 4AGD", page 2 "0TODAY 9KEYS CLRSET".
lit_hint:
    DATA 49,68,65,89,32,50,87,75,32,51,77,79,32,52,65,71,68,32,32,32
    CONST LEN_HINT = 20

lit_hint2:
    DATA 48,84,79,68,65,89,32,57,75,69,89,83,32,67,76,82,83,69,84,32
    CONST LEN_HINT2 = 20

' Page 3 "5NEW 6EDIT". Compose and edit could not be folded into the two pages
' above -- page 1 had three trailing spaces and page 2 one.
lit_hint3:
    DATA 53,78,69,87,32,54,69,68,73,84,32,32,32,32,32,32,32,32,32,32
    CONST LEN_HINT3 = 20
    CONST VW_HINTPGS = 3

' Error strings, one row each.
lit_e_auth:
    DATA 65,85,84,72,79,82,73,90,69,32,87,69,66,32,85,73,32,32
    CONST LEN_E_AUTH = 18
lit_e_net:
    DATA 78,79,32,70,85,74,73,78,69,84,32,82,69,80,76,89,32,32
    CONST LEN_E_NET = 18
lit_e_spec:
    DATA 66,65,68,32,67,65,76,69,78,68,65,82,32,83,80,69,67,32
    CONST LEN_E_SPEC = 18
lit_e_gen:
    DATA 67,65,76,69,78,68,65,82,32,69,82,82,79,82,32,32,32,32
    CONST LEN_E_GEN = 18
lit_busy:
    DATA 70,69,84,67,72,73,78,71,46,46,46,32,32,32,32,32,32,32,32,32
    CONST LEN_BUSY = 20
lit_none:
    DATA 78,79,32,69,86,69,78,84,83,32,32,32,32,32,32,32,32,32
    CONST LEN_NONE = 18

' ---------------------------------------------------------------------------
' vw_video: the colour stack every view runs on. Five raster runs onto four
' stack entries, wrapping 3->0 for the hint row -- see bar.bas.
'
' MODE lands on the NEXT frame and borrows the colour variable to carry its
' arguments until then, so nothing may PRINT ... COLOR until after the WAIT.
' ---------------------------------------------------------------------------
vw_video: PROCEDURE
    MODE 0, CS_BLUE, CS_BLACK, CS_DARKGREEN, CS_BLACK
    BORDER CS_BLUE
    WAIT
END

' ---------------------------------------------------------------------------
' vw_puts_lit: draw a LEN_-length ROM literal at (s_col, s_row). scr_puts
' stops at a NUL, which a DATA literal has no reason to carry, so this is the
' fixed-length counterpart.
' ---------------------------------------------------------------------------
vw_puts_lit: PROCEDURE
    FOR vw_i = 0 TO vl_len - 1
        vw_c = PEEK(#vw_p + vw_i) AND 255
        IF vw_c < 32 OR vw_c > 126 THEN vw_c = 32
        #BACKTAB(s_row * SCREEN_COLS + s_col + vw_i) = (vw_c - 32) * 8 + s_col_color
    NEXT vw_i
END

' ---------------------------------------------------------------------------
' vw_chip: draw an event colour chip at (s_col, s_row) in colour vw_chipc.
'
' A GRAM card, not a GROM one: colour-stack foreground is bits 0-2 plus bit 12,
' and bit 12 only means "colour bit 3" on a GRAM card -- on GROM it selects
' Coloured Squares instead. Five of Google's eleven colours are above 7, so a
' GROM glyph simply cannot show them.
'
' All-day events get the bar card rather than the solid block, so the two kinds
' of event stay distinguishable even for a viewer who cannot separate the
' colours.
' ---------------------------------------------------------------------------
vw_chip: PROCEDURE
    IF vw_chipa THEN
        #BACKTAB(s_row * SCREEN_COLS + s_col) = GLYPH_ALLDAY * 8 + GRAM_SELECT + gramfg(vw_chipc)
    ELSE
        #BACKTAB(s_row * SCREEN_COLS + s_col) = GLYPH_BLOCK * 8 + GRAM_SELECT + gramfg(vw_chipc)
    END IF
END

' ---------------------------------------------------------------------------
' vw_time: draw a five-character time field at (s_col, s_row) from vw_h/vw_m,
' or "ALLDY" when vw_alld is set. Zero-padded 24-hour: the Intellivision has
' twenty columns and an am/pm suffix would cost one of them for no information
' the hour does not already carry.
' ---------------------------------------------------------------------------
vw_time: PROCEDURE
    IF vw_alld THEN
        PRINT AT screenpos(s_col, s_row) COLOR s_col_color, "ALLDY"
        RETURN
    END IF
    #BACKTAB(s_row * SCREEN_COLS + s_col + 0) = ((vw_h / 10) + 16) * 8 + s_col_color
    #BACKTAB(s_row * SCREEN_COLS + s_col + 1) = ((vw_h % 10) + 16) * 8 + s_col_color
    #BACKTAB(s_row * SCREEN_COLS + s_col + 2) = (26) * 8 + s_col_color      ' ':' is card 26
    #BACKTAB(s_row * SCREEN_COLS + s_col + 3) = ((vw_m / 10) + 16) * 8 + s_col_color
    #BACKTAB(s_row * SCREEN_COLS + s_col + 4) = ((vw_m % 10) + 16) * 8 + s_col_color
END

' ---------------------------------------------------------------------------
' vw_header: rows 0-2. The title comes straight from the adapter's own line 0
' ("Fri 28 Aug 2026", "Week of Sun 23 Aug 2026", "August 2026", "Agenda from
' 28 Aug 2026"), so the console never has to format a date it did not compute.
' Clipped to 18 columns to leave the logo its two.
' ---------------------------------------------------------------------------
vw_header: PROCEDURE
    s_row = ROW_TITLE : s_col = 0 : s_max = 18 : s_col_color = COL_NORMAL
    #s_src = SC_HDR
    GOSUB scr_puts

    s_row = ROW_SUB : s_col = 0 : s_max = 18 : s_col_color = COL_VALUE
    GOSUB vw_status_line
END

' vw_status_line: row 1 -- an error, "NO EVENTS", or the event count.
'
' Blanks its own eighteen columns first. vw_clear_content starts at ROW_COLS
' and so never touches this row, and the count is variable-width: going from
' "12 EVENTS" to "5 EVENTS" would otherwise leave a stray digit behind.
vw_status_line: PROCEDURE
    FOR vw_i = 0 TO 17
        #BACKTAB(ROW_SUB * SCREEN_COLS + vw_i) = CS_BLACK
    NEXT vw_i
    s_row = ROW_SUB : s_col = 0 : s_col_color = COL_VALUE

    IF gc_err <> NS_SUCCESS THEN
        s_col_color = COL_ERROR
        vl_len = LEN_E_GEN : #vw_p = VARPTR lit_e_gen(0)
        IF gc_err = 0 THEN vl_len = LEN_E_NET : #vw_p = VARPTR lit_e_net(0)
        IF gc_err = NS_ACCESS_DENIED THEN vl_len = LEN_E_AUTH : #vw_p = VARPTR lit_e_auth(0)
        IF gc_err = NS_BAD_SPEC THEN vl_len = LEN_E_SPEC : #vw_p = VARPTR lit_e_spec(0)
        IF gc_err = NS_NOT_FOUND THEN vl_len = LEN_E_SPEC : #vw_p = VARPTR lit_e_spec(0)
        GOSUB vw_puts_lit
        RETURN
    END IF

    IF ev_count = 0 THEN
        vl_len = LEN_NONE : #vw_p = VARPTR lit_none(0)
        GOSUB vw_puts_lit
        RETURN
    END IF

    IF ev_count = 1 THEN
        PRINT AT screenpos(0, ROW_SUB) COLOR COL_VALUE, "1 EVENT"
    ELSE
        PRINT AT screenpos(0, ROW_SUB) COLOR COL_VALUE, <>ev_count, " EVENTS"
    END IF
    IF gc_trunc THEN PRINT AT screenpos(13, ROW_SUB) COLOR COL_HILIGHT, "MORE "
END

' ---------------------------------------------------------------------------
' vw_hints: row 11. Three pages, cycled by keypad 9, because twenty columns
' will not hold the key list.
' ---------------------------------------------------------------------------
vw_hints: PROCEDURE
    s_row = ROW_HINT : s_col = 0 : s_col_color = COL_NORMAL
    IF vw_hintpg = 0 THEN
        vl_len = LEN_HINT : #vw_p = VARPTR lit_hint(0)
    ELSEIF vw_hintpg = 1 THEN
        vl_len = LEN_HINT2 : #vw_p = VARPTR lit_hint2(0)
    ELSE
        vl_len = LEN_HINT3 : #vw_p = VARPTR lit_hint3(0)
    END IF
    GOSUB vw_puts_lit
    ' Highlight just the digits, so the key to press reads at a glance. One
    ' pass over the row costs far less ROM than splitting the hint into
    ' separately coloured PRINT fragments. Page 2 is exempt: "0TODAY" and
    ' "9KEYS" are key names, but so is nothing else on it -- and page 3 is
    ' all key names, so it gets the pass too.
    IF vw_hintpg <> 1 THEN
        s_col_color = COL_HILIGHT
        GOSUB scr_hilite_digits
    END IF
    GOSUB bar_rearm
END

' ---------------------------------------------------------------------------
' vw_busy: put "FETCHING..." up before a blocking gc_fetch, so the console does
' not simply freeze for the length of a mailbox round trip.
' ---------------------------------------------------------------------------
vw_busy: PROCEDURE
    s_row = ROW_HINT : s_col = 0 : s_col_color = COL_HILIGHT
    vl_len = LEN_BUSY : #vw_p = VARPTR lit_busy(0)
    GOSUB vw_puts_lit
    GOSUB bar_rearm
END

' ---------------------------------------------------------------------------
' gc_get_long_title: scroll.bas's hook. Copies the selected event's full stored
' title into SC_ENTRY and sets sc_len.
'
' Free, unlike the file browser's version of this hook, which re-read the name
' over the mailbox: titles are already resident in SC_TITLE, so nothing blocks
' inside the input loop.
' ---------------------------------------------------------------------------
gc_get_long_title: PROCEDURE
    sc_len = 0
    GOSUB vw_sel_event
    IF vw_n = VW_NO_EVENT THEN RETURN
    #vw_p = SC_TITLE + vw_n * TITLE_STRIDE
    FOR vw_i = 0 TO TITLE_STRIDE - 1
        vw_c = PEEK(#vw_p + vw_i) AND 255
        IF vw_c = 0 THEN EXIT FOR
        POKE (SC_ENTRY + vw_i), vw_c
    NEXT vw_i
    sc_len = vw_i
    POKE (SC_ENTRY + sc_len), 0
END

' ---------------------------------------------------------------------------
' vw_sel_event: vw_n = the SC_EVT index under the selection, or VW_NO_EVENT.
'
' A displayed row is not the same thing as an event index in the AGENDA view,
' where date separators are interleaved into the display list, so every place
' that needs "the selected event" goes through here rather than assuming
' vw_first + sel_row.
' ---------------------------------------------------------------------------
vw_sel_event: PROCEDURE
    vw_n = VW_NO_EVENT
    IF num_rows = 0 THEN RETURN
    vw_i = vw_first + sel_row
    IF cur_view = VIEW_AGENDA THEN
        IF (PEEK(SC_AGD + vw_i * AGD_STRIDE) AND 255) = AGD_SEP THEN RETURN
        vw_n = PEEK(SC_AGD + vw_i * AGD_STRIDE + 1) AND 255
        RETURN
    END IF
    IF vw_i >= ev_count THEN RETURN
    vw_n = vw_i
END

' ---------------------------------------------------------------------------
' Period navigation. vw_first is the index of the display row drawn on the top
' content row -- the scroll offset for lists longer than seven.
' ---------------------------------------------------------------------------

vw_next_period: PROCEDURE
    #cd_y = #cur_y : cd_mo = cur_mo : cd_d = cur_d
    IF cur_view = VIEW_MONTH THEN
        GOSUB clk_addmonth
    ELSEIF cur_view = VIEW_DAY THEN
        GOSUB clk_addday
    ELSE
        ' WEEK and AGENDA both step a week: a week view has nowhere else to
        ' go, and a 90-day agenda paged a day at a time would be tedious.
        FOR vw_i = 0 TO 6
            GOSUB clk_addday
        NEXT vw_i
    END IF
    #cur_y = #cd_y : cur_mo = cd_mo : cur_d = cd_d
    vw_dirty = 1
END

vw_prev_period: PROCEDURE
    #cd_y = #cur_y : cd_mo = cur_mo : cd_d = cur_d
    IF cur_view = VIEW_MONTH THEN
        GOSUB clk_submonth
    ELSEIF cur_view = VIEW_DAY THEN
        GOSUB clk_subday
    ELSE
        FOR vw_i = 0 TO 6
            GOSUB clk_subday
        NEXT vw_i
    END IF
    #cur_y = #cd_y : cur_mo = cd_mo : cur_d = cd_d
    vw_dirty = 1
END

' ---------------------------------------------------------------------------
' vw_load: fetch the current view's period and reset the scroll.
' ---------------------------------------------------------------------------
vw_load: PROCEDURE
    GOSUB vw_busy
    gc_view = cur_view
    GOSUB gc_fetch
    ' The agenda's display list depends only on the fetch, so it is built here
    ' rather than in agenda_draw -- which runs again on every scroll step.
    IF cur_view = VIEW_AGENDA THEN GOSUB agenda_build
    vw_first = 0
    sel_row = 0
    vw_dirty = 0
END

' ---------------------------------------------------------------------------
' vw_common_keys: the keys every view shares. Returns vw_handled = 1 if the
' press was consumed.
' ---------------------------------------------------------------------------
vw_common_keys: PROCEDURE
    vw_handled = 1
    IF in_key = KEYPAD_1 THEN
        cur_view = VIEW_DAY : vw_shown = 0 : vw_dirty = 1 : RETURN
    END IF
    IF in_key = KEYPAD_2 THEN
        cur_view = VIEW_WEEK : vw_shown = 0 : vw_dirty = 1 : RETURN
    END IF
    IF in_key = KEYPAD_3 THEN
        cur_view = VIEW_MONTH : vw_shown = 0 : vw_dirty = 1 : RETURN
    END IF
    IF in_key = KEYPAD_4 THEN
        cur_view = VIEW_AGENDA : vw_shown = 0 : vw_dirty = 1 : RETURN
    END IF
    IF in_key = KEYPAD_0 THEN
        ' Jump back to today, whichever period the user has wandered into.
        GOSUB clk_today
        vw_dirty = 1
        RETURN
    END IF
    IF in_key = KEYPAD_9 THEN
        vw_hintpg = vw_hintpg + 1
        IF vw_hintpg >= VW_HINTPGS THEN vw_hintpg = 0
        GOSUB vw_hints
        RETURN
    END IF
    IF in_key = KEYPAD_5 THEN
        GOSUB frm_new
        RETURN
    END IF
    IF in_key = KEYPAD_6 THEN
        ' Only where a single event is actually selected. In WEEK a row is a
        ' day and in MONTH a cell is a date, so vw_sel_event's row-to-index
        ' shortcut would hand back an index that means nothing here -- the
        ' same reason the C clients leave E inert in those two views.
        IF cur_view = VIEW_DAY OR cur_view = VIEW_AGENDA THEN
            GOSUB vw_sel_event
            IF vw_n <> VW_NO_EVENT THEN
                ev_sel = vw_n
                GOSUB frm_edit_event
            END IF
        END IF
        RETURN
    END IF
    IF in_key = KEYPAD_CLEAR THEN
        state = ST_SETUP
        vw_shown = 0
        RETURN
    END IF
    ' Left and right step the period everywhere EXCEPT the month grid, where
    ' they walk the cursor a day at a time and month_input rolls the period
    ' over itself when the cursor leaves the month.
    IF cur_view <> VIEW_MONTH THEN
        IF in_disc = DISC_LEFT THEN
            GOSUB vw_prev_period : RETURN
        END IF
        IF in_disc = DISC_RIGHT THEN
            GOSUB vw_next_period : RETURN
        END IF
    END IF
    vw_handled = 0
END

' ---------------------------------------------------------------------------
' do_view: the ST_VIEW state handler, called once per frame from the main
' loop. Dispatches to whichever of the four views is current.
' ---------------------------------------------------------------------------
' vw_shown and vw_dirty are deliberately separate. vw_shown = 0 means "the
' screen needs repainting" -- coming back from the detail screen, or after the
' main loop CLS'd on a video-profile change. vw_dirty = 1 means "the DATA is
' stale" -- a different view, or a different period. Only the second is worth a
' fetch, which blocks for the length of a mailbox round trip; conflating them
' would make backing out of an event detail re-download the whole day.
do_view: PROCEDURE
    IF vw_shown = 0 OR vw_dirty <> 0 THEN
        IF vw_shown = 0 THEN
            GOSUB scr_clear
            GOSUB gc_logo_show
        END IF
        GOSUB vw_hints
        IF vw_dirty <> 0 THEN GOSUB vw_load
        GOSUB vw_redraw
        vw_shown = 1
        RETURN
    END IF

    GOSUB in_poll

    ' A raised alarm banner swallows the first press that clears it, rather
    ' than also opening whatever the selection happens to be sitting on.
    IF al_active <> 0 THEN
        IF in_btn <> 0 OR in_key <> KEYPAD_NONE THEN
            GOSUB al_dismiss
            RETURN
        END IF
    END IF

    GOSUB vw_common_keys
    IF vw_handled THEN RETURN

    IF cur_view = VIEW_DAY THEN GOSUB day_input
    IF cur_view = VIEW_WEEK THEN GOSUB week_input
    IF cur_view = VIEW_MONTH THEN GOSUB month_input
    IF cur_view = VIEW_AGENDA THEN GOSUB agenda_input
END

' vw_redraw: repaint the whole content area for the current view.
vw_redraw: PROCEDURE
    GOSUB vw_clear_content
    IF cur_view = VIEW_DAY THEN GOSUB day_draw
    IF cur_view = VIEW_WEEK THEN GOSUB week_draw
    IF cur_view = VIEW_MONTH THEN GOSUB month_draw
    IF cur_view = VIEW_AGENDA THEN GOSUB agenda_draw
    GOSUB vw_header
    GOSUB vw_hints
    GOSUB bar_apply
END

' vw_clear_content: blank rows 2-10, leaving the header title and hints alone.
vw_clear_content: PROCEDURE
    FOR s_row = ROW_COLS TO ROW_SPACER
        GOSUB scr_row_clear
    NEXT s_row
END

' ---------------------------------------------------------------------------
' vw_scroll_list: shared up/down handling for the three list views. Moves the
' bar within the visible seven, and pages the window when it would leave.
' ---------------------------------------------------------------------------
vw_scroll_list: PROCEDURE
    IF in_disc = DISC_UP THEN
        IF sel_row > 0 THEN
            bar_new = sel_row - 1
            GOSUB bar_move
        ELSEIF vw_first > 0 THEN
            vw_first = vw_first - 1
            GOSUB vw_redraw
        END IF
    END IF
    IF in_disc = DISC_DOWN THEN
        IF sel_row < num_rows - 1 THEN
            bar_new = sel_row + 1
            GOSUB bar_move
        ELSEIF vw_first + num_rows < ev_count THEN
            vw_first = vw_first + 1
            GOSUB vw_redraw
        END IF
    END IF
    IF num_rows > 0 THEN GOSUB scroll_step
END
