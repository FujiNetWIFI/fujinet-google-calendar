' st_month.bas -- the MONTH view: a real 7x6 calendar grid.
'
' Layout:
'   row 2     "SU MO TU WE TH FR SA"  -- seven cells of two columns, at 0,3,..,18
'   rows 3-8  six week rows of day numbers
'   row 9     a one-line summary of the selected day
'
' Day numbers are drawn from the GRAM digit cards, not the GROM font, because
' the whole point of colouring them is to show each day's leading event colour
' and five of Google's eleven colours are above 7 -- which a GROM cell cannot
' express in colour-stack mode (see gfx.bas).
'
' The month view is the one view that does not store events: gc_fetch tallies
' straight into SC_DAYS/SC_DCOL while streaming, so a 300-event month costs 42
' bytes and never touches MAX_EVENTS.
'
' THE SELECTION IS cur_d
'
' There is no separate "selected day" variable. The view anchor's day IS the
' selection, so moving the cursor is ordinary date arithmetic through
' clock.bas's helpers, and walking off the top or bottom of the grid rolls into
' the neighbouring month for free -- with the leap-year and month-length rules
' applied exactly once, in one place. A second variable would have to be kept
' in step with cur_d through every one of those transitions.
'
' Selection is a cell rather than a row, so the colour-stack bar is parked
' (bar_apply does that when num_rows is 0) and the cursor is a MOB instead. A
' sprite with SPR_BEHIND is drawn everywhere it is visible EXCEPT where the
' card underneath has a foreground pixel, so a solid block parked behind a cell
' fills it and the digits are punched out of it in their own colour -- real
' inverse video, and it costs nothing per frame.

    CONST MO_CELL_W = 3       ' two digit columns plus a gap
    CONST MO_SUMROW = 9

    DIM mo_i, mo_d, mo_c, mo_r, mo_first, mo_ndays, mo_cnt
    DIM mo_col, mo_row, mo_step, mo_dir, mo_omo

lit_dow2:
    DATA 83,85, 77,79, 84,85, 87,69, 84,72, 70,82, 83,65

' ---------------------------------------------------------------------------
' month_geom: mo_first = weekday of the 1st (0 = Sunday), mo_ndays = length of
' the month. Both derived from the anchor, not from the adapter -- the MONTH
' window title carries only "August 2026".
' ---------------------------------------------------------------------------
month_geom: PROCEDURE
    #cd_y = #cur_y : cd_mo = cur_mo : cd_d = 1
    GOSUB clk_dow
    mo_first = cd_n
    GOSUB clk_dim
    mo_ndays = cd_n
END

' ---------------------------------------------------------------------------
' month_digit: draw digit mo_c (0-9) at (mo_col, mo_row) in colour mo_i, from
' the GRAM digit cards.
' ---------------------------------------------------------------------------
month_digit: PROCEDURE
    #BACKTAB(mo_row * SCREEN_COLS + mo_col) = (GLYPH_DIG0 + mo_c) * 8 + GRAM_SELECT + gramfg(mo_i)
END

' ---------------------------------------------------------------------------
' month_draw
' ---------------------------------------------------------------------------
month_draw: PROCEDURE
    ' A cell cursor, not a row bar, so bar.bas parks its advance bits.
    num_rows = 0

    GOSUB month_geom
    IF cur_d > mo_ndays THEN cur_d = mo_ndays
    IF cur_d < 1 THEN cur_d = 1

    ' Column headings. COL_HEAD, not COL_DIM -- see screen.bas.
    FOR mo_i = 0 TO 6
        #BACKTAB(ROW_COLS * SCREEN_COLS + mo_i * MO_CELL_W) = \
            (lit_dow2(mo_i * 2) - 32) * 8 + COL_HEAD
        #BACKTAB(ROW_COLS * SCREEN_COLS + mo_i * MO_CELL_W + 1) = \
            (lit_dow2(mo_i * 2 + 1) - 32) * 8 + COL_HEAD
    NEXT mo_i

    ' Day cells.
    FOR mo_d = 1 TO mo_ndays
        mo_i = mo_first + mo_d - 1
        mo_r = mo_i / 7
        mo_c = mo_i % 7
        IF mo_r > 5 THEN GOTO md_next

        mo_row = ROW_FIRST + mo_r
        mo_col = mo_c * MO_CELL_W

        ' Grey for a day with nothing on it, the leading event's colour
        ' otherwise. SC_DCOL is only meaningful where SC_DAYS is non-zero.
        mo_cnt = PEEK(SC_DAYS + mo_d) AND 255
        IF mo_cnt > 0 THEN
            mo_i = PEEK(SC_DCOL + mo_d) AND 255
        ELSE
            mo_i = CS_GREY
        END IF

        ' The tens digit is suppressed below 10 rather than zero-padded: a
        ' calendar prints "3", not "03".
        IF mo_d >= 10 THEN
            mo_c = mo_d / 10
            GOSUB month_digit
        END IF
        mo_col = mo_col + 1
        mo_c = mo_d % 10
        GOSUB month_digit
md_next:
    NEXT mo_d

    GOSUB month_summary
    GOSUB month_cursors
END

' ---------------------------------------------------------------------------
' month_cell_xy: grid position of day mo_d, for the MOB cursors.
' ---------------------------------------------------------------------------
month_cell_xy: PROCEDURE
    mo_i = mo_first + mo_d - 1
    mo_row = ROW_FIRST + mo_i / 7
    mo_col = (mo_i % 7) * MO_CELL_W
END

' ---------------------------------------------------------------------------
' month_cursors: MOB 0 marks the selected day, MOB 7 today.
'
' SPR_ZOOMX2 stretches the one 8-pixel card across the sixteen pixels a
' two-column cell occupies.
' ---------------------------------------------------------------------------
month_cursors: PROCEDURE
    mo_d = cur_d
    GOSUB month_cell_xy
    SPRITE MOB_CURSOR, \
        SPR_VISIBLE + SPR_ZOOMX2 + MOB_X0 + mo_col * 8, \
        SPR_ZOOMY2 + MOB_Y0 + mo_row * 8, \
        SPR_BEHIND + SPR_GRAM + GLYPH_BLOCK * 8 + CS_YELLOW

    ' Today only gets a marker when the grid is actually showing its month.
    IF clk_ok = 1 AND #clk_y = #cur_y AND clk_mo = cur_mo THEN
        mo_d = clk_d
        GOSUB month_cell_xy
        SPRITE MOB_TODAY, \
            SPR_VISIBLE + SPR_ZOOMX2 + MOB_X0 + mo_col * 8, \
            SPR_ZOOMY2 + MOB_Y0 + mo_row * 8, \
            SPR_BEHIND + SPR_GRAM + GLYPH_BLOCK * 8 + CS_BROWN
    ELSE
        SPRITE MOB_TODAY, 0, 0, 0
    END IF
END

' ---------------------------------------------------------------------------
' month_summary: row 9 -- "28: 3 EVENTS" for the selected day. Blanks its own
' row first, because the count is variable width.
' ---------------------------------------------------------------------------
month_summary: PROCEDURE
    FOR mo_i = 0 TO SCREEN_COLS - 1
        #BACKTAB(MO_SUMROW * SCREEN_COLS + mo_i) = CS_BLACK
    NEXT mo_i
    mo_cnt = PEEK(SC_DAYS + cur_d) AND 255
    IF mo_cnt = 0 THEN
        PRINT AT screenpos(0, MO_SUMROW) COLOR COL_DIM, <.2>cur_d, ": NO EVENTS"
    ELSE
        PRINT AT screenpos(0, MO_SUMROW) COLOR COL_VALUE, <.2>cur_d, ": ", <>mo_cnt, " EVENTS"
    END IF
END

' ---------------------------------------------------------------------------
' month_input: the disc walks the grid a day at a time in every direction,
' which is why the MONTH view takes left and right for itself instead of
' letting vw_common_keys use them to step the period.
'
' Stepping is done by clk_addday/clk_subday rather than by adding to cur_d, so
' a move that leaves the month rolls the year over correctly and lands on a
' real date. Only when the month actually changed is a re-fetch needed.
' ---------------------------------------------------------------------------
month_input: PROCEDURE
    IF in_btn <> 0 THEN
        cur_view = VIEW_DAY
        vw_shown = 0
        vw_dirty = 1
        RETURN
    END IF

    mo_step = 0
    mo_dir = 0
    IF in_disc = DISC_LEFT THEN mo_step = 1 : mo_dir = 0
    IF in_disc = DISC_RIGHT THEN mo_step = 1 : mo_dir = 1
    IF in_disc = DISC_UP THEN mo_step = 7 : mo_dir = 0
    IF in_disc = DISC_DOWN THEN mo_step = 7 : mo_dir = 1
    IF mo_step = 0 THEN RETURN

    mo_omo = cur_mo
    #cd_y = #cur_y : cd_mo = cur_mo : cd_d = cur_d
    FOR mo_i = 1 TO mo_step
        IF mo_dir THEN
            GOSUB clk_addday
        ELSE
            GOSUB clk_subday
        END IF
    NEXT mo_i
    #cur_y = #cd_y : cur_mo = cd_mo : cur_d = cd_d

    IF cur_mo <> mo_omo THEN
        ' New month: the tallies are stale, so this needs a real fetch.
        vw_dirty = 1
    ELSE
        GOSUB month_summary
        GOSUB month_cursors
    END IF
END
