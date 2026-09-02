' st_agenda.bas -- the AGENDA view: a rolling list across days.
'
' Row layout matches the DAY view (chip, time, title) so the two read the same,
' with date separators interleaved:
'
'   28 AUG              <- separator, COL_HILIGHT, and no chip in the gutter
'   * ALLDY COMPANY HOL
'     09:00 STANDUP
'   29 AUG
'     14:00 REVIEW
'
' The separators mean a displayed row is not the same thing as an event index,
' and scrolling needs random access to the combined sequence -- so the display
' list is materialised once into SC_AGD after each fetch rather than being
' recomputed for every row. Two bytes an entry; at most one separator per event
' plus the events themselves, which AGD_MAX covers for the whole 96-event cap.
'
' The window is fetched with ?count=96&days=90 (gcalnet.bas), because the
' adapter's own default is 20 events over 365 days.

' ag_try is separate from ag_i deliberately: the skip loops below call
' vw_redraw, which reaches agenda_draw and agenda_sep -- both of which use
' ag_i. A shared counter would be reset underneath the loop that owns it.
    DIM ag_i, ag_n, ag_row, ag_d, ag_m, ag_pd, ag_pm
    DIM ag_count, ag_try

lit_mon3:
    DATA 74,65,78, 70,69,66, 77,65,82, 65,80,82, 77,65,89, 74,85,78
    DATA 74,85,76, 65,85,71, 83,69,80, 79,67,84, 78,79,86, 68,69,67

' ---------------------------------------------------------------------------
' agenda_build: walk the event index once and write the display list.
'
' ag_pd/ag_pm hold the previous row's date; 0 can never be a real day of month,
' so it is a safe "no previous" sentinel and the first event always opens a
' group.
' ---------------------------------------------------------------------------
agenda_build: PROCEDURE
    ag_count = 0
    ag_pd = 0
    ag_pm = 0
    IF ev_count = 0 THEN RETURN

    FOR ag_i = 0 TO ev_count - 1
        #evrec = SC_EVT + ag_i * EVT_STRIDE
        ag_d = PEEK(#evrec + EVT_DAY) AND 255
        ag_m = PEEK(#evrec + EVT_MON) AND 255

        IF ag_d <> ag_pd OR ag_m <> ag_pm THEN
            IF ag_count < AGD_MAX THEN
                POKE (SC_AGD + ag_count * AGD_STRIDE), AGD_SEP
                POKE (SC_AGD + ag_count * AGD_STRIDE + 1), ag_i
                ag_count = ag_count + 1
            END IF
            ag_pd = ag_d
            ag_pm = ag_m
        END IF

        IF ag_count < AGD_MAX THEN
            POKE (SC_AGD + ag_count * AGD_STRIDE), 0
            POKE (SC_AGD + ag_count * AGD_STRIDE + 1), ag_i
            ag_count = ag_count + 1
        END IF
    NEXT ag_i
END

' ---------------------------------------------------------------------------
' agenda_draw
' ---------------------------------------------------------------------------
agenda_draw: PROCEDURE
    num_rows = ag_count - vw_first
    IF num_rows > ROWS_PER_PAGE THEN num_rows = ROWS_PER_PAGE
    IF num_rows < 0 THEN num_rows = 0
    IF sel_row >= num_rows THEN sel_row = 0

    ' A separator is not a selectable thing; if the window opens on one, step
    ' the bar past it so the button always has an event under it.
    IF num_rows > 0 THEN
        IF (PEEK(SC_AGD + (vw_first + sel_row) * AGD_STRIDE) AND 255) = AGD_SEP THEN
            IF sel_row + 1 < num_rows THEN sel_row = sel_row + 1
        END IF
    END IF

    ' COL_HEAD, not COL_DIM: rows 0-2 are the header run and its colour-stack
    ' background is CS_BLUE, so COL_DIM (also blue) renders invisible there.
    PRINT AT screenpos(0, ROW_COLS) COLOR COL_HEAD, "UPCOMING            "

    IF num_rows = 0 THEN RETURN
    FOR ag_row = 0 TO num_rows - 1
        ag_n = vw_first + ag_row
        s_row = ROW_FIRST + ag_row
        ag_i = PEEK(SC_AGD + ag_n * AGD_STRIDE + 1) AND 255
        #evrec = SC_EVT + ag_i * EVT_STRIDE

        IF (PEEK(SC_AGD + ag_n * AGD_STRIDE) AND 255) = AGD_SEP THEN
            GOSUB agenda_sep
        ELSE
            GOSUB agenda_event
        END IF
    NEXT ag_row

    sc_row = ROW_FIRST + sel_row : sc_col = TITLE_COL
    sc_max = TITLE_W : sc_color = COL_NORMAL
    #sc_adv = 0
    GOSUB scroll_reset
END

' agenda_sep: "28 AUG" in the dim colour, at the left edge.
agenda_sep: PROCEDURE
    ag_d = PEEK(#evrec + EVT_DAY) AND 255
    ag_m = PEEK(#evrec + EVT_MON) AND 255
    IF ag_m < 1 OR ag_m > 12 THEN ag_m = 1
    #BACKTAB(s_row * SCREEN_COLS + 1) = ((ag_d / 10) + 16) * 8 + COL_HILIGHT
    #BACKTAB(s_row * SCREEN_COLS + 2) = ((ag_d % 10) + 16) * 8 + COL_HILIGHT
    FOR ag_i = 0 TO 2
        ag_n = lit_mon3((ag_m - 1) * 3 + ag_i)
        #BACKTAB(s_row * SCREEN_COLS + 4 + ag_i) = (ag_n - 32) * 8 + COL_HILIGHT
    NEXT ag_i
END

' agenda_event: the same chip / time / title row the DAY view draws.
agenda_event: PROCEDURE
    s_col = CHIP_COL
    vw_chipc = PEEK(#evrec + EVT_COLOR) AND 255
    vw_chipa = (PEEK(#evrec + EVT_FLAGS) AND 255) AND EVF_ALLDAY
    GOSUB vw_chip

    s_col = TIME_COL : s_col_color = COL_NORMAL
    vw_h = PEEK(#evrec + EVT_SH) AND 255
    vw_m = PEEK(#evrec + EVT_SM) AND 255
    vw_alld = vw_chipa
    GOSUB vw_time

    s_col = TITLE_COL : s_max = TITLE_W : s_col_color = COL_NORMAL
    #s_src = SC_TITLE + ag_i * TITLE_STRIDE
    GOSUB scr_puts
END

' ---------------------------------------------------------------------------
' agenda_input
' ---------------------------------------------------------------------------
agenda_input: PROCEDURE
    IF in_btn <> 0 AND num_rows > 0 THEN
        GOSUB vw_sel_event
        IF vw_n <> VW_NO_EVENT THEN
            ev_sel = vw_n
            state = ST_EVENT
            vw_shown = 0
        END IF
        RETURN
    END IF

    ' Skip separators as the bar passes over them, so the selection always
    ' rests on something the button can open.
    IF in_disc = DISC_UP THEN
        GOSUB agenda_up
    ELSEIF in_disc = DISC_DOWN THEN
        GOSUB agenda_down
    END IF
    IF num_rows > 0 THEN GOSUB scroll_step
END

' Two attempts, not one: a single press must clear at most one separator plus
' land on the event beyond it.
agenda_up: PROCEDURE
    FOR ag_try = 0 TO 1
        IF sel_row > 0 THEN
            bar_new = sel_row - 1
            GOSUB bar_move
        ELSEIF vw_first > 0 THEN
            vw_first = vw_first - 1
            GOSUB vw_redraw
        ELSE
            RETURN
        END IF
        IF (PEEK(SC_AGD + (vw_first + sel_row) * AGD_STRIDE) AND 255) <> AGD_SEP THEN RETURN
    NEXT ag_try
END

agenda_down: PROCEDURE
    FOR ag_try = 0 TO 1
        IF sel_row < num_rows - 1 THEN
            bar_new = sel_row + 1
            GOSUB bar_move
        ELSEIF vw_first + num_rows < ag_count THEN
            vw_first = vw_first + 1
            GOSUB vw_redraw
        ELSE
            RETURN
        END IF
        IF (PEEK(SC_AGD + (vw_first + sel_row) * AGD_STRIDE) AND 255) <> AGD_SEP THEN RETURN
    NEXT ag_try
END
