' st_day.bas -- the DAY view: one day's events, seven at a time.
'
' Row layout, twenty columns:
'   0      colour chip (solid block, or the bar card for an all-day event)
'   1-5    "09:00", or "ALLDY"
'   6      space
'   7-19   title, bounce-scrolled on the selected row
'
' The chip sits in column 0 because that is the one column the selection bar
' never covers (bar.bas), so it stays visually still as the cursor moves over
' it -- the same reason the file browser puts its file-type glyph there.

    DIM dy_i, dy_row, dy_n
    DIM #dy_p

' ---------------------------------------------------------------------------
' day_draw: paint rows 3-9 from SC_EVT, starting at vw_first. Sets num_rows to
' however many rows actually carry an event, which is what bar.bas and
' vw_scroll_list bound themselves by.
' ---------------------------------------------------------------------------
day_draw: PROCEDURE
    num_rows = ev_count - vw_first
    IF num_rows > ROWS_PER_PAGE THEN num_rows = ROWS_PER_PAGE
    IF num_rows < 0 THEN num_rows = 0
    IF sel_row >= num_rows THEN sel_row = 0

    ' COL_HEAD, not COL_DIM: rows 0-2 are the header run and its colour-stack
    ' background is CS_BLUE, so COL_DIM (also blue) renders invisible there.
    PRINT AT screenpos(0, ROW_COLS) COLOR COL_HEAD, "TIME  EVENT         "

    IF num_rows = 0 THEN RETURN
    FOR dy_i = 0 TO num_rows - 1
        dy_n = vw_first + dy_i
        dy_row = ROW_FIRST + dy_i
        #dy_p = SC_EVT + dy_n * EVT_STRIDE

        s_row = dy_row : s_col = CHIP_COL
        vw_chipc = PEEK(#dy_p + EVT_COLOR) AND 255
        vw_chipa = (PEEK(#dy_p + EVT_FLAGS) AND 255) AND EVF_ALLDAY
        GOSUB vw_chip

        s_col = TIME_COL : s_col_color = COL_NORMAL
        vw_h = PEEK(#dy_p + EVT_SH) AND 255
        vw_m = PEEK(#dy_p + EVT_SM) AND 255
        vw_alld = vw_chipa
        GOSUB vw_time

        s_col = TITLE_COL : s_max = TITLE_W : s_col_color = COL_NORMAL
        #s_src = SC_TITLE + dy_n * TITLE_STRIDE
        GOSUB scr_puts
    NEXT dy_i

    ' Prime the scroller on whichever row is selected.
    sc_row = ROW_FIRST + sel_row : sc_col = TITLE_COL
    sc_max = TITLE_W : sc_color = COL_NORMAL
    #sc_adv = 0
    GOSUB scroll_reset
END

' ---------------------------------------------------------------------------
' day_input: called once per frame while the DAY view is up and the shared
' keys did not claim the press.
' ---------------------------------------------------------------------------
day_input: PROCEDURE
    IF in_btn <> 0 AND num_rows > 0 THEN
        GOSUB vw_sel_event
        IF vw_n <> VW_NO_EVENT THEN
            ev_sel = vw_n
            state = ST_EVENT
            vw_shown = 0
        END IF
        RETURN
    END IF
    GOSUB vw_scroll_list
END
