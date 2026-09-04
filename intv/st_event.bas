' st_event.bas -- the event detail screen.
'
' Opens the same devicespec the listing used with "/N" appended and aux1 = 4
' (READ) instead of 6 (DIRECTORY), which the adapter answers with a
' line-oriented record:
'
'   <summary, or "(no title)">
'   <when, word-wrapped>
'   Repeats                     <- only when recurring
'   Category: <cat>             <- only when non-empty
'   Where: <location>           <- only when non-empty
'                               <- blank, only when there is a description
'   <description, word-wrapped>
'
' Two things about that open differ from the listing and both matter:
'
'   * aux2 is IGNORED for a READ. The detail is always rendered at the
'     platform default width, which is 80 for BUILD_RS232 -- what
'     fujiversal-intv builds. So the text arrives wrapped for an 80-column
'     screen and has to be re-wrapped here to twenty. wrap.bas does that on
'     whole words, after the incoming line structure is flattened.
'   * The selector must not be "*". util_devicespec_fix_for_parsing() rewrites
'     '*' to an embedded NUL on any open that is not a DIRECTORY, so a
'     wildcard that listed fine would corrupt this fetch. gc_build_url only
'     ever emits an empty selector or a real name, so this is safe by
'     construction.
'
' N is the adapter's own event number from the listing (EVT_NUMLO/HI), not this
' client's array index: the adapter re-runs the query and re-numbers from a
' total ordering, so /N addresses the same event on a later open.

    CONST EV_ROWS  = 8        ' wrapped rows the detail pane can show
    CONST EV_STRIDE = 21      ' wrap.bas emits 20 chars + NUL

    DIM ev_shown, ev_i, ev_c, ev_run
' #ev_num is DIM'd in constants.bas -- st_form.bas builds its edit URL from
' it, and is included ahead of this file.
    DIM #ev_ci

' ---------------------------------------------------------------------------
' ev_open_read: aux1 = 4 (READ), aux2 ignored by the adapter for this mode.
' ---------------------------------------------------------------------------
ev_open_read: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_OPEN
    mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = OPEN_MODE_READ : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    GOSUB fn_transact
END

' ---------------------------------------------------------------------------
' ev_fetch: pull the detail and wrap it into SC_DETAIL.
'
' The adapter's own line breaks are dropped and the text re-flowed: they were
' computed for eighty columns, so honouring them here would leave four-fifths
' of every row blank. A newline becomes a space, and wrap.bas re-breaks on
' whole words at twenty.
' ---------------------------------------------------------------------------
ev_fetch: PROCEDURE
    FOR ev_i = 0 TO EV_ROWS - 1
        POKE (SC_DETAIL + ev_i * EV_STRIDE), 0
    NEXT ev_i

    ' Same window as the listing, plus /N -- gc_build_url puts it in the path
    ' ahead of any query string, and keeps the query, both of which the
    ' adapter's event numbering depends on.
    gc_view = cur_view
    #gc_evnum = #ev_num
    GOSUB gc_build_url
    #gc_evnum = 0
    GOSUB ev_open_read
    IF fn_ok = 0 THEN
        gc_err = 0
        RETURN
    END IF

    ' Flatten the reply into SC_EDIT as one NUL-terminated run of words.
    ev_run = 0
    gc_err = NS_SUCCESS
ef_loop:
    GOSUB gc_status
    IF fn_ok = 0 THEN GOTO ef_done
    IF #net_avail = 0 THEN GOTO ef_done
    #net_readlen = #net_avail
    IF #net_readlen > 512 THEN #net_readlen = 512
    GOSUB net_read
    IF fn_ok = 0 THEN GOTO ef_done
    IF #net_gotlen = 0 THEN GOTO ef_done

    ' 16-bit counter: see the note on #gp_i in gcalnet.bas.
    FOR #ev_ci = 0 TO #net_gotlen - 1
        ev_c = PEEK(FN_RX + #ev_ci) AND 255
        ' $9B is the line ending this bus actually emits (see gcalnet.bas).
        IF ev_c = 155 OR ev_c = 10 OR ev_c = 13 THEN ev_c = 32
        IF ev_c >= 97 AND ev_c <= 122 THEN ev_c = ev_c - 32
        IF ev_c < 32 OR ev_c > 126 THEN ev_c = 32
        IF ev_run < 250 THEN
            POKE (SC_EDIT + ev_run), ev_c
            ev_run = ev_run + 1
        END IF
    NEXT #ev_ci
    GOTO ef_loop

ef_done:
    POKE (SC_EDIT + ev_run), 0
    GOSUB net_close
    IF gc_err = NS_EOF THEN gc_err = NS_SUCCESS

    #w_src = SC_EDIT
    #w_dst = SC_DETAIL
    w_rows = EV_ROWS
    GOSUB wrap_text
END

' ---------------------------------------------------------------------------
' ev_draw
' ---------------------------------------------------------------------------
ev_draw: PROCEDURE
    GOSUB scr_clear
    GOSUB gc_logo_show

    ' The chip and time of the event this detail belongs to, so the screen is
    ' identifiable before the text arrives.
    #evrec = SC_EVT + ev_sel * EVT_STRIDE
    s_row = ROW_TITLE : s_col = CHIP_COL
    vw_chipc = PEEK(#evrec + EVT_COLOR) AND 255
    vw_chipa = (PEEK(#evrec + EVT_FLAGS) AND 255) AND EVF_ALLDAY
    GOSUB vw_chip
    s_col = TIME_COL : s_col_color = COL_HILIGHT
    vw_h = PEEK(#evrec + EVT_SH) AND 255
    vw_m = PEEK(#evrec + EVT_SM) AND 255
    vw_alld = vw_chipa
    GOSUB vw_time

    IF (PEEK(#evrec + EVT_FLAGS) AND 255) AND EVF_RECURRING THEN
        PRINT AT screenpos(7, ROW_TITLE) COLOR COL_DIM, "REPEATS"
    END IF

    IF gc_err <> NS_SUCCESS THEN
        s_row = ROW_SUB : s_col = 0 : s_col_color = COL_ERROR
        vl_len = LEN_E_GEN : #vw_p = VARPTR lit_e_gen(0)
        IF gc_err = 0 THEN vl_len = LEN_E_NET : #vw_p = VARPTR lit_e_net(0)
        IF gc_err = NS_ACCESS_DENIED THEN vl_len = LEN_E_AUTH : #vw_p = VARPTR lit_e_auth(0)
        GOSUB vw_puts_lit
    ELSE
        FOR ev_i = 0 TO EV_ROWS - 1
            s_row = ROW_SUB + ev_i : s_col = 0 : s_max = SCREEN_COLS
            s_col_color = COL_NORMAL
            IF ev_i = 0 THEN s_col_color = COL_VALUE
            #s_src = SC_DETAIL + ev_i * EV_STRIDE
            GOSUB scr_puts
        NEXT ev_i
    END IF

    PRINT AT screenpos(0, ROW_HINT) COLOR COL_DIM, "6 EDIT  BUTTON BACK "
END

' ---------------------------------------------------------------------------
' do_event: the ST_EVENT state handler.
' ---------------------------------------------------------------------------
do_event: PROCEDURE
    IF ev_shown = 0 THEN
        #evrec = SC_EVT + ev_sel * EVT_STRIDE
        #ev_num = (PEEK(#evrec + EVT_NUMLO) AND 255) + (PEEK(#evrec + EVT_NUMHI) AND 255) * 256
        GOSUB scr_clear
        PRINT AT screenpos(0, ROW_SUB) COLOR COL_HILIGHT, "LOADING EVENT..."
        GOSUB ev_fetch
        GOSUB ev_draw
        ev_shown = 1
        RETURN
    END IF

    GOSUB in_poll
    IF in_key = KEYPAD_6 THEN
        ' Edit this event. ev_sel still names it, and frm_edit_event re-reads
        ' the index record for the adapter's event number.
        ev_shown = 0
        GOSUB frm_edit_event
        RETURN
    END IF
    IF in_btn <> 0 OR in_key = KEYPAD_CLEAR THEN
        ev_shown = 0
        vw_shown = 0
        state = ST_VIEW
    END IF
END
