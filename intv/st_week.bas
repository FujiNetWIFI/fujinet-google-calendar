' st_week.bas -- the WEEK view: seven days, one row each, events as chips.
'
' Row layout, twenty columns:
'   0      chip for the day's leading event (blank if the day is empty)
'   1-3    "SUN".."SAT"
'   4      space
'   5-6    day of month
'   7      space
'   8-19   up to twelve chips, one per event, each in its own colour
'
' Twenty columns will not hold seven days of text side by side, so the week is
' rotated: a row per day, and the day's load shown as a row of coloured chips.
' That reads as a density map at a glance and still names every event's colour.
'
' The adapter's WEEK date column is only "Fri" -- no day number -- so the dates
' down the left are computed here from the anchor, by walking back to the
' week's Sunday and stepping forward. wkst is left at the adapter's default of
' Sunday, which is what the ?wkst= query parameter would otherwise change.

    CONST WK_DOW_COL  = 1
    CONST WK_DAY_COL  = 5
    CONST WK_CHIP_COL = 8
    CONST WK_CHIPS    = SCREEN_COLS - WK_CHIP_COL

    DIM wk_i, wk_d, wk_n, wk_cnt

lit_dow3:
    DATA 83,85,78, 77,79,78, 84,85,69, 87,69,68, 84,72,85, 70,82,73, 83,65,84

' ---------------------------------------------------------------------------
' week_start: leave the shared #cd_y/cd_mo/cd_d triple on the Sunday that
' begins the anchor's week, ready for callers to step forward through with
' clk_addday. The triple IS the return value -- IntyBASIC PROCEDUREs have no
' others.
' ---------------------------------------------------------------------------
week_start: PROCEDURE
    #cd_y = #cur_y : cd_mo = cur_mo : cd_d = cur_d
    GOSUB clk_dow
    wk_n = cd_n
    IF wk_n > 0 THEN
        FOR wk_i = 1 TO wk_n
            GOSUB clk_subday
        NEXT wk_i
    END IF
END

' ---------------------------------------------------------------------------
' week_draw: paint the seven day rows.
' ---------------------------------------------------------------------------
week_draw: PROCEDURE
    num_rows = 7
    IF sel_row >= num_rows THEN sel_row = 0

    ' COL_HEAD, not COL_DIM: rows 0-2 are the header run and its colour-stack
    ' background is CS_BLUE, so COL_DIM (also blue) renders invisible there.
    PRINT AT screenpos(0, ROW_COLS) COLOR COL_HEAD, "DAY      EVENTS     "

    GOSUB week_start

    FOR wk_d = 0 TO 6
        s_row = ROW_FIRST + wk_d

        ' Day name.
        FOR wk_i = 0 TO 2
            wk_n = lit_dow3(wk_d * 3 + wk_i)
            #BACKTAB(s_row * SCREEN_COLS + WK_DOW_COL + wk_i) = (wk_n - 32) * 8 + COL_NORMAL
        NEXT wk_i

        ' Day of month, stepping forward from the week's Sunday.
        #BACKTAB(s_row * SCREEN_COLS + WK_DAY_COL) = ((cd_d / 10) + 16) * 8 + COL_VALUE
        #BACKTAB(s_row * SCREEN_COLS + WK_DAY_COL + 1) = ((cd_d % 10) + 16) * 8 + COL_VALUE

        ' Mark today by colouring its date rather than adding a glyph -- there
        ' is no spare column, and the whole row is already a selectable target.
        GOSUB clk_is_today
        IF cd_n = 1 THEN
            #BACKTAB(s_row * SCREEN_COLS + WK_DAY_COL) = ((cd_d / 10) + 16) * 8 + COL_HILIGHT
            #BACKTAB(s_row * SCREEN_COLS + WK_DAY_COL + 1) = ((cd_d % 10) + 16) * 8 + COL_HILIGHT
        END IF

        GOSUB week_chips
        GOSUB clk_addday
    NEXT wk_d
END

' ---------------------------------------------------------------------------
' week_chips: draw one chip per event whose EVT_DAY is wk_d, plus the leading
' event's colour in the gutter. Scans the whole index per row -- seven passes
' over at most 96 records, which happens once per fetch and never inside the
' input loop.
' ---------------------------------------------------------------------------
week_chips: PROCEDURE
    wk_cnt = 0
    IF ev_count = 0 THEN RETURN
    FOR wk_n = 0 TO ev_count - 1
        #evrec = SC_EVT + wk_n * EVT_STRIDE
        IF (PEEK(#evrec + EVT_DAY) AND 255) = wk_d THEN
            IF wk_cnt = 0 THEN
                ' Gutter chip: the day's leading event. The adapter sorts by
                ' start time, so the first match is the earliest.
                s_col = CHIP_COL
                vw_chipc = PEEK(#evrec + EVT_COLOR) AND 255
                vw_chipa = (PEEK(#evrec + EVT_FLAGS) AND 255) AND EVF_ALLDAY
                GOSUB vw_chip
            END IF
            IF wk_cnt < WK_CHIPS THEN
                s_col = WK_CHIP_COL + wk_cnt
                vw_chipc = PEEK(#evrec + EVT_COLOR) AND 255
                vw_chipa = (PEEK(#evrec + EVT_FLAGS) AND 255) AND EVF_ALLDAY
                GOSUB vw_chip
            END IF
            wk_cnt = wk_cnt + 1
        END IF
    NEXT wk_n
END

' ---------------------------------------------------------------------------
' week_input: the disc walks the seven days; the button drills into that day's
' DAY view, which is the only place a title is legible.
' ---------------------------------------------------------------------------
week_input: PROCEDURE
    IF in_btn <> 0 THEN
        ' Re-anchor on the selected day: walk from the week's Sunday.
        GOSUB week_start
        IF sel_row > 0 THEN
            FOR wk_i = 1 TO sel_row
                GOSUB clk_addday
            NEXT wk_i
        END IF
        #cur_y = #cd_y : cur_mo = cd_mo : cur_d = cd_d
        cur_view = VIEW_DAY
        vw_shown = 0
        vw_dirty = 1
        RETURN
    END IF

    IF in_disc = DISC_UP AND sel_row > 0 THEN
        bar_new = sel_row - 1
        GOSUB bar_move
    END IF
    IF in_disc = DISC_DOWN AND sel_row < num_rows - 1 THEN
        bar_new = sel_row + 1
        GOSUB bar_move
    END IF
END
