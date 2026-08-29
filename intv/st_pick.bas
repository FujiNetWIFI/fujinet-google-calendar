' st_pick.bas -- the setup screen and the calendar picker.
'
' THE CALENDAR LIST IS NOT AN EVENT INDEX
'
' Opening GCAL:/// with no view and an empty selector gives the calendar list,
' which format_list_human() renders as a header row followed by one row per
' calendar -- and, unlike an event index, NO window-title line ahead of it. So
' this module parses the reply itself rather than reusing gc_parse_line, whose
' whole line-numbering assumes the title is line 0.
'
' At width 80 the list columns are catW = 80/3 = 26 and nameW = 80 - 26 - 1 =
' 53, with the name ljust'd to nameW and the category merely ellipsized (data
' rows are ragged right).
'
' The NAME is stored as the selector, not the id. resolve_selector() matches a
' calendar name case-insensitively before falling back to treating the selector
' as a literal calendarId, so the name works and is far shorter than an id like
' "abc123@group.calendar.google.com" -- which matters when the whole setting
' has to fit in a 64-byte AppKey alongside the alarm lead.
'
' PERSISTENCE
'
' One AppKey holds both settings: byte 0 is the alarm lead in minutes, bytes 1
' onward are the NUL-terminated selector. Two firmware details are easy to get
' wrong and are handled by fujinet.bas: the open payload is SIX bytes
' (creator_lo, creator_hi, app, key, mode, reserved) and sending five leaves
' transaction_get() waiting for a byte that never comes, which surfaces as a
' timeout rather than an error; and an appkey READ reply carries a two-byte
' little-endian length prefix ahead of the data, which a network read does not.

    CONST AK_CREATOR_LO = $43   ' 'C'
    CONST AK_CREATOR_HI = $47   ' 'G'
    CONST AK_APP_ID     = 1
    CONST AK_KEY_ID     = 0
    CONST AK_MODE_READ  = 0
    CONST AK_MODE_WRITE = 1

    CONST PK_NAMEW = 53         ' the adapter's name column at width 80
    CONST PK_ROW0  = 3
    CONST PK_ROWS  = 7

    DIM pk_shown, pk_i, pk_j, pk_c, pk_n, pk_len, pk_hold, pk_line
    DIM pk_count, pk_sel, pk_first, su_shown
    DIM #pk_p, #pk_ci

lit_allcal:
    DATA 65,76,76,32,83,72,79,87,78,32,67,65,76,83     ' "ALL SHOWN CALS"
    CONST LEN_ALLCAL = 14

' =============================================================================
' AppKey settings
' =============================================================================

' ---------------------------------------------------------------------------
' pk_settings_load: read the AppKey into al_lead and SC_CAL. A missing key is
' not an error -- it just means first run, and the defaults stand.
' ---------------------------------------------------------------------------
pk_settings_load: PROCEDURE
    POKE SC_CAL, 0
    al_lead = AL_LEAD_DEFAULT

    ak_creator_lo = AK_CREATOR_LO
    ak_creator_hi = AK_CREATOR_HI
    ak_app = AK_APP_ID
    ak_key = AK_KEY_ID
    ak_mode = AK_MODE_READ
    GOSUB appkey_open
    IF fn_ok = 0 THEN RETURN

    #fn_src = SC_EDIT
    ls_max = 64
    GOSUB appkey_read
    GOSUB appkey_close
    IF fn_len < 1 THEN RETURN

    pk_c = PEEK(SC_EDIT) AND 255
    IF pk_c > 0 AND pk_c <= AL_LEAD_MAX THEN al_lead = pk_c

    ' The selector follows, already NUL-terminated by appkey_read.
    FOR pk_i = 0 TO 62
        pk_c = PEEK(SC_EDIT + 1 + pk_i) AND 255
        POKE (SC_CAL + pk_i), pk_c
        IF pk_c = 0 THEN EXIT FOR
    NEXT pk_i
    POKE (SC_CAL + 63), 0
END

' ---------------------------------------------------------------------------
' pk_settings_save
' ---------------------------------------------------------------------------
pk_settings_save: PROCEDURE
    POKE (SC_EDIT), al_lead
    #fn_src = SC_CAL : ls_max = 62 : GOSUB fn_strlen
    pk_len = fn_len
    FOR pk_i = 0 TO pk_len - 1
        POKE (SC_EDIT + 1 + pk_i), PEEK(SC_CAL + pk_i) AND 255
    NEXT pk_i
    POKE (SC_EDIT + 1 + pk_len), 0

    ak_creator_lo = AK_CREATOR_LO
    ak_creator_hi = AK_CREATOR_HI
    ak_app = AK_APP_ID
    ak_key = AK_KEY_ID
    ak_mode = AK_MODE_WRITE
    GOSUB appkey_open
    IF fn_ok = 0 THEN RETURN
    #fn_src = SC_EDIT
    fn_len = pk_len + 2          ' lead byte + selector + its NUL
    GOSUB appkey_write
    GOSUB appkey_close
END

' =============================================================================
' The calendar list
' =============================================================================

' ---------------------------------------------------------------------------
' pk_fetch_list: open GCAL:/// as a directory and fill SC_LIST.
' ---------------------------------------------------------------------------
pk_fetch_list: PROCEDURE
    pk_count = 0
    pk_line = 0
    pk_hold = 0

    ' Entry 0 is always "all shown calendars", whose selector is the empty
    ' string -- the same thing an empty SC_CAL means to gc_build_url.
    FOR pk_i = 0 TO LEN_ALLCAL - 1
        POKE (SC_LIST + pk_i), PEEK(VARPTR lit_allcal(0) + pk_i) AND 255
    NEXT pk_i
    POKE (SC_LIST + LEN_ALLCAL), 0
    POKE (SC_LIST + CAL_SELOFF), 0
    pk_count = 1

    #fn_txlen = 0
    #fn_src = VARPTR lit_scheme(0) : fn_len = LEN_SCHEME : GOSUB fn_putstr
    GOSUB gc_open_dir
    IF fn_ok = 0 THEN
        gc_err = 0
        RETURN
    END IF
    gc_err = NS_SUCCESS

pf_loop:
    GOSUB gc_status
    IF fn_ok = 0 THEN GOTO pf_done
    IF #net_avail = 0 THEN GOTO pf_done
    #net_readlen = #net_avail
    IF #net_readlen > 512 THEN #net_readlen = 512
    GOSUB net_read
    IF fn_ok = 0 THEN GOTO pf_done
    IF #net_gotlen = 0 THEN GOTO pf_done

    ' 16-bit counter: see the note on #gp_i in gcalnet.bas.
    FOR #pk_ci = 0 TO #net_gotlen - 1
        pk_c = PEEK(FN_RX + #pk_ci) AND 255
        IF pk_c = 155 OR pk_c = 10 THEN
            GOSUB pk_take_line
            pk_hold = 0
        ELSE
            IF pk_c <> 13 AND pk_hold < 127 THEN
                POKE (SC_LINEBUF + pk_hold), pk_c
                pk_hold = pk_hold + 1
            END IF
        END IF
    NEXT #pk_ci
    GOTO pf_loop

pf_done:
    IF pk_hold > 0 THEN GOSUB pk_take_line
    GOSUB net_close
    IF gc_err = NS_EOF THEN gc_err = NS_SUCCESS
END

' ---------------------------------------------------------------------------
' pk_take_line: one line of the calendar list. Line 0 is the column header --
' there is no window title on this listing.
' ---------------------------------------------------------------------------
pk_take_line: PROCEDURE
    IF pk_line = 0 THEN
        pk_line = 1
        RETURN
    END IF
    IF pk_hold = 0 THEN RETURN
    IF pk_count >= CAL_MAX THEN RETURN

    ' Trim the ljust padding off the name column.
    pk_len = pk_hold
    IF pk_len > PK_NAMEW THEN pk_len = PK_NAMEW
pk_trim:
    IF pk_len > 0 THEN
        IF (PEEK(SC_LINEBUF + pk_len - 1) AND 255) = 32 THEN
            pk_len = pk_len - 1
            GOTO pk_trim
        END IF
    END IF
    IF pk_len = 0 THEN RETURN
    ' "(none)" is what the adapter prints for an empty account.
    IF (PEEK(SC_LINEBUF) AND 255) = 40 THEN RETURN

    #pk_p = SC_LIST + pk_count * CAL_STRIDE

    ' Display name: upper-cased and clipped to the column the picker shows.
    pk_n = pk_len
    IF pk_n > CAL_SELOFF - 1 THEN pk_n = CAL_SELOFF - 1
    FOR pk_j = 0 TO pk_n - 1
        pk_c = PEEK(SC_LINEBUF + pk_j) AND 255
        IF pk_c >= 97 AND pk_c <= 122 THEN pk_c = pk_c - 32
        POKE (#pk_p + pk_j), pk_c
    NEXT pk_j
    POKE (#pk_p + pk_n), 0

    ' Selector: the name verbatim, case intact -- resolve_selector() matches it
    ' case-insensitively, but a literal calendarId fallback would not.
    pk_n = pk_len
    IF pk_n > CAL_STRIDE - CAL_SELOFF - 1 THEN pk_n = CAL_STRIDE - CAL_SELOFF - 1
    FOR pk_j = 0 TO pk_n - 1
        POKE (#pk_p + CAL_SELOFF + pk_j), PEEK(SC_LINEBUF + pk_j) AND 255
    NEXT pk_j
    POKE (#pk_p + CAL_SELOFF + pk_n), 0

    pk_count = pk_count + 1
END

' ---------------------------------------------------------------------------
' pk_draw_list
' ---------------------------------------------------------------------------
pk_draw_list: PROCEDURE
    GOSUB scr_clear
    PRINT AT screenpos(0, ROW_TITLE) COLOR COL_NORMAL, "CHOOSE CALENDAR"

    IF gc_err <> NS_SUCCESS THEN
        s_row = ROW_SUB : s_col = 0 : s_col_color = COL_ERROR
        vl_len = LEN_E_GEN : #vw_p = VARPTR lit_e_gen(0)
        IF gc_err = 0 THEN vl_len = LEN_E_NET : #vw_p = VARPTR lit_e_net(0)
        IF gc_err = NS_ACCESS_DENIED THEN vl_len = LEN_E_AUTH : #vw_p = VARPTR lit_e_auth(0)
        GOSUB vw_puts_lit
    END IF

    num_rows = pk_count - pk_first
    IF num_rows > PK_ROWS THEN num_rows = PK_ROWS
    IF num_rows < 0 THEN num_rows = 0

    IF num_rows > 0 THEN
        FOR pk_i = 0 TO num_rows - 1
            pk_n = pk_first + pk_i
            s_row = PK_ROW0 + pk_i : s_col = 1 : s_max = 18
            s_col_color = COL_NORMAL
            IF pk_n = pk_sel THEN s_col_color = COL_HILIGHT
            #s_src = SC_LIST + pk_n * CAL_STRIDE
            GOSUB scr_puts
        NEXT pk_i
    END IF

    PRINT AT screenpos(0, ROW_HINT) COLOR COL_DIM, "BTN=PICK CLR=BACK   "
END

' ---------------------------------------------------------------------------
' do_pick: the ST_PICK state handler.
' ---------------------------------------------------------------------------
do_pick: PROCEDURE
    IF pk_shown = 0 THEN
        GOSUB gc_hide_all_mobs
        GOSUB scr_clear
        PRINT AT screenpos(0, ROW_SUB) COLOR COL_HILIGHT, "READING CALENDARS..."
        GOSUB pk_fetch_list
        pk_sel = 0
        pk_first = 0
        GOSUB pk_draw_list
        pk_shown = 1
        RETURN
    END IF

    GOSUB in_poll

    IF in_key = KEYPAD_CLEAR THEN
        pk_shown = 0
        state = ST_SETUP
        su_shown = 0
        RETURN
    END IF

    IF in_btn <> 0 AND pk_count > 0 THEN
        ' Copy the chosen selector into SC_CAL and persist it.
        #pk_p = SC_LIST + pk_sel * CAL_STRIDE + CAL_SELOFF
        FOR pk_i = 0 TO 62
            pk_c = PEEK(#pk_p + pk_i) AND 255
            POKE (SC_CAL + pk_i), pk_c
            IF pk_c = 0 THEN EXIT FOR
        NEXT pk_i
        POKE (SC_CAL + 63), 0
        GOSUB pk_settings_save
        pk_shown = 0
        state = ST_VIEW
        vw_shown = 0
        vw_dirty = 1
        RETURN
    END IF

    IF in_disc = DISC_UP AND pk_sel > 0 THEN
        pk_sel = pk_sel - 1
        IF pk_sel < pk_first THEN pk_first = pk_sel
        GOSUB pk_draw_list
    END IF
    IF in_disc = DISC_DOWN AND pk_sel < pk_count - 1 THEN
        pk_sel = pk_sel + 1
        IF pk_sel >= pk_first + PK_ROWS THEN pk_first = pk_sel - PK_ROWS + 1
        GOSUB pk_draw_list
    END IF
END

' =============================================================================
' The setup screen
' =============================================================================

' ---------------------------------------------------------------------------
' su_draw: timezone, alarm lead, and the current calendar.
'
' The timezone is worth showing because it is the single setting most likely to
' be wrong in a way nothing else reveals: the adapter parses [General] timezone
' with its own POSIX evaluator, which REJECTS an IANA name like
' "America/Chicago" and silently falls back to UTC. Seeing "UTC" here when the
' web UI says otherwise is the symptom.
' ---------------------------------------------------------------------------
su_draw: PROCEDURE
    GOSUB scr_clear
    PRINT AT screenpos(0, ROW_TITLE) COLOR COL_NORMAL, "SETTINGS"

    PRINT AT screenpos(0, 2) COLOR COL_DIM, "TIMEZONE"
    GOSUB clk_get_tz
    s_row = 3 : s_col = 1 : s_max = 19 : s_col_color = COL_VALUE
    #s_src = SC_EDIT
    GOSUB scr_puts

    PRINT AT screenpos(0, 5) COLOR COL_DIM, "CALENDAR"
    s_row = 6 : s_col = 1 : s_max = 19 : s_col_color = COL_VALUE
    IF (PEEK(SC_CAL) AND 255) = 0 THEN
        PRINT AT screenpos(1, 6) COLOR COL_VALUE, "ALL SHOWN CALS     "
    ELSE
        #s_src = SC_CAL
        GOSUB scr_puts
    END IF

    PRINT AT screenpos(0, 8) COLOR COL_DIM, "ALARM LEAD"
    GOSUB su_draw_lead

    PRINT AT screenpos(0, 10) COLOR COL_NORMAL, "1=CALENDAR  DISC=MIN"
    PRINT AT screenpos(0, ROW_HINT) COLOR COL_NORMAL, "CLR=BACK            "
END

su_draw_lead: PROCEDURE
    PRINT AT screenpos(1, 9) COLOR COL_VALUE, <.2>al_lead, " MINUTES BEFORE"
END

' ---------------------------------------------------------------------------
' do_setup: the ST_SETUP state handler.
' ---------------------------------------------------------------------------
do_setup: PROCEDURE
    IF su_shown = 0 THEN
        GOSUB gc_hide_all_mobs
        GOSUB su_draw
        su_shown = 1
        RETURN
    END IF

    GOSUB in_poll

    IF in_key = KEYPAD_CLEAR THEN
        GOSUB pk_settings_save
        su_shown = 0
        state = ST_VIEW
        vw_shown = 0
        RETURN
    END IF
    IF in_key = KEYPAD_1 THEN
        su_shown = 0
        pk_shown = 0
        state = ST_PICK
        RETURN
    END IF

    IF in_disc = DISC_LEFT AND al_lead > 1 THEN
        al_lead = al_lead - 1
        GOSUB su_draw_lead
    END IF
    IF in_disc = DISC_RIGHT AND al_lead < AL_LEAD_MAX THEN
        al_lead = al_lead + 1
        GOSUB su_draw_lead
    END IF
END
