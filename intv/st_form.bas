' st_form.bas -- compose a new event, or edit an existing one.
'
' The C clients grew this first (src/form.c + src/compose.c); this is the same
' feature and the same wire format on a 20x12 screen with twelve keys. Keypad
' 5 opens a blank form on the anchor date, 6 opens one prefilled from the
' selected event, and text fields are typed with t9.bas.
'
' ---------------------------------------------------------------------------
' THE SCREEN DIVIDES EXACTLY
'
' t9_entry owns rows 0-2 (the value window), row 3 (the candidate strip) and
' row 11 (its hints and the T9/ABC indicator), and touches nothing between.
' There are seven draft fields. Rows 4-10 are seven rows. So the field list
' stays on screen, readable, while a field is being typed into -- no swapping
' screens, and the row you are editing is the row you are looking at.
'
' Outside t9_entry the form owns all twelve rows.
'
' ---------------------------------------------------------------------------
' NO NEW VIDEO PROFILE
'
' The form runs on gcal.bas's existing profile 1 -- MODE 0 with all four
' colour-stack entries black -- which is what every non-view screen already
' uses. That works here only because every colour t9.bas draws with
' (COL_VALUE/HILIGHT/DIM/ERROR/CURSOR/NORMAL) is 0-7, and a GROM card's
' foreground in colour-stack mode is three bits. Introduce a colour above 7
' into the editor and it will need a GRAM card, not a new profile.
'
' ---------------------------------------------------------------------------
' WHAT AN EDIT SENDS
'
' Only the fields you touched. The client holds a title truncated to what the
' listing gave it and NOTHING of location, notes or category, so sending an
' untouched field would replace the server's fuller copy with a blank or a
' stub. A blank field on an edit therefore means "leave alone", and clearing a
' field is not possible from here -- the same bargain the C clients struck, and
' documented for the user in the README.
'
' The one blank that does mean something is START: a date-only START is the
' wire's only spelling of "all-day", so blanking the start time converts the
' event. That is why frm_dirty is tracked per field rather than inferred from
' emptiness.
' ---------------------------------------------------------------------------

    CONST F_TITLE = 0
    CONST F_DATE  = 1
    CONST F_START = 2
    CONST F_END   = 3
    CONST F_LOC   = 4
    CONST F_DESC  = 5
    CONST F_CAT   = 6
    CONST F_COUNT = 7

' Field byte offsets into SC_FORM, and each field's size INCLUDING its NUL.
' Sizes follow the C client's (src/gcal.h FRM_*_MAX) so the two clients
' truncate a title at the same place.
frm_off:
    DATA 0,40,51,57,63,96,193
frm_max:
    DATA 40,11,6,6,33,97,16

' Row 4 upward, one per field, in the order above. FRM_ROW0 itself is in
' constants.bas: bar.bas reads it.
    CONST FRM_LABW  = 5           ' "TITLE" is the longest label
    CONST FRM_VALC  = 6           ' value column
' One column short of the screen, so that column 19 is always blank. That is
' where the selection bar's closing advance bit goes (see frm_bar_set in
' bar.bas) -- with a character under it the bar would end in a one-cell black
' notch with the last letter of the value stranded in it. Blank, it simply
' stops one column short of the edge, the way it already stops one column in
' from the left.
    CONST FRM_VALW  = SCREEN_COLS - FRM_VALC - 1

' Digit slots within the two masked fields: "YYYY-MM-DD" and "HH:MM".
frm_dmap:
    DATA 0,1,2,3,5,6,8,9
frm_tmap:
    DATA 0,1,3,4

    DIM fm_shown, fm_edit, fm_i, fm_c, fm_n, fm_prev   ' fm_sel: constants.bas
    DIM fe_n, fe_max, fs_i, fs_err, fs_lines, fm_w
    DIM #fm_p

' ---------------------------------------------------------------------------
' Labels and draft keys.
'
' The keys carry their own ": " -- the adapter splits on the FIRST colon and
' trims the value, but the single space is what the C clients emit and what
' tests/hosttest.c pins, so the two stay byte-identical on the wire.
' ---------------------------------------------------------------------------
lit_f_labels:
    DATA 84,73,84,76,69          ' TITLE
    DATA 68,65,84,69,32          ' DATE
    DATA 83,84,65,82,84          ' START
    DATA 69,78,68,32,32          ' END
    DATA 87,72,69,82,69          ' WHERE
    DATA 78,79,84,69,83          ' NOTES
    DATA 67,65,84,32,32          ' CAT

lit_k_summary:
    DATA 83,85,77,77,65,82,89,58,32              ' "SUMMARY: "
    CONST LEN_K_SUMMARY = 9
lit_k_start:
    DATA 83,84,65,82,84,58,32                    ' "START: "
    CONST LEN_K_START = 7
lit_k_end:
    DATA 69,78,68,58,32                          ' "END: "
    CONST LEN_K_END = 5
lit_k_loc:
    DATA 76,79,67,65,84,73,79,78,58,32           ' "LOCATION: "
    CONST LEN_K_LOC = 10
lit_k_desc:
    DATA 68,69,83,67,82,73,80,84,73,79,78,58,32  ' "DESCRIPTION: "
    CONST LEN_K_DESC = 13
lit_k_cat:
    DATA 67,65,84,69,71,79,82,89,58,32           ' "CATEGORY: "
    CONST LEN_K_CAT = 10

' ---------------------------------------------------------------------------
' frm_ptr: #fm_p = the address of field fm_i, fm_n = its size including NUL.
' ---------------------------------------------------------------------------
frm_ptr: PROCEDURE
    #fm_p = SC_FORM + frm_off(fm_i)
    fm_n = frm_max(fm_i)
END

' ---------------------------------------------------------------------------
' frm_clear: empty every field and drop every dirty flag.
' ---------------------------------------------------------------------------
frm_clear: PROCEDURE
    FOR fm_i = 0 TO F_COUNT - 1
        GOSUB frm_ptr
        POKE #fm_p, 0
        POKE (SC_FDIRTY + fm_i), 0
    NEXT fm_i
END

' ---------------------------------------------------------------------------
' frm_put_date / frm_put_time: write the anchor date, or an event's start or
' end time, into field fm_i as text.
' ---------------------------------------------------------------------------
frm_put_date: PROCEDURE
    GOSUB frm_ptr
    POKE (#fm_p + 0), 48 + (#cur_y / 1000) % 10
    POKE (#fm_p + 1), 48 + (#cur_y / 100) % 10
    POKE (#fm_p + 2), 48 + (#cur_y / 10) % 10
    POKE (#fm_p + 3), 48 + #cur_y % 10
    POKE (#fm_p + 4), 45
    POKE (#fm_p + 5), 48 + cur_mo / 10
    POKE (#fm_p + 6), 48 + cur_mo % 10
    POKE (#fm_p + 7), 45
    POKE (#fm_p + 8), 48 + cur_d / 10
    POKE (#fm_p + 9), 48 + cur_d % 10
    POKE (#fm_p + 10), 0
END

' fm_c = hours, fm_prev = minutes.
frm_put_time: PROCEDURE
    GOSUB frm_ptr
    POKE (#fm_p + 0), 48 + fm_c / 10
    POKE (#fm_p + 1), 48 + fm_c % 10
    POKE (#fm_p + 2), 58
    POKE (#fm_p + 3), 48 + fm_prev / 10
    POKE (#fm_p + 4), 48 + fm_prev % 10
    POKE (#fm_p + 5), 0
END

' ---------------------------------------------------------------------------
' frm_prefill: load the selected event into the form, for an edit.
'
' Only what the index record already holds: title, date and the two times.
' Location, notes and category stay blank -- the client has never fetched them,
' and spending a detail round trip to prefill fields the user probably will not
' touch would cost a blocking transaction for nothing. Blank plus the
' send-only-dirty rule means they are left alone on the server.
'
' All-day events get blank times, which is the same thing the form means by
' all-day, so an all-day event edited and saved stays all-day.
' ---------------------------------------------------------------------------
frm_prefill: PROCEDURE
    GOSUB frm_clear

    #evrec = SC_EVT + ev_sel * EVT_STRIDE

    fm_i = F_TITLE
    GOSUB frm_ptr
    FOR fm_c = 0 TO fm_n - 2
        fm_prev = PEEK(SC_TITLE + ev_sel * TITLE_STRIDE + fm_c) AND 255
        POKE (#fm_p + fm_c), fm_prev
        IF fm_prev = 0 THEN EXIT FOR
    NEXT fm_c
    POKE (#fm_p + fm_n - 1), 0

    fm_i = F_DATE
    GOSUB frm_put_date

    IF ((PEEK(#evrec + EVT_FLAGS) AND 255) AND EVF_ALLDAY) = 0 THEN
        fm_i = F_START
        fm_c = PEEK(#evrec + EVT_SH) AND 255
        fm_prev = PEEK(#evrec + EVT_SM) AND 255
        GOSUB frm_put_time
        fm_i = F_END
        fm_c = PEEK(#evrec + EVT_EH) AND 255
        fm_prev = PEEK(#evrec + EVT_EM) AND 255
        GOSUB frm_put_time
    END IF
END

' ---------------------------------------------------------------------------
' frm_row: draw field fm_i on its row. A dirty field in COL_VALUE, an untouched
' one in COL_NORMAL, its label always dim.
'
' Nothing here marks the selection: that is the colour-stack bar (frm_bar_* in
' bar.bas), which is why dirty-vs-clean stays readable on the selected row
' instead of being overpainted yellow the way it used to be.
' ---------------------------------------------------------------------------
frm_row: PROCEDURE
    s_row = FRM_ROW0 + fm_i
    s_col = 0 : s_max = FRM_LABW : s_col_color = COL_DIM
    vl_len = FRM_LABW
    #vw_p = VARPTR lit_f_labels(0) + fm_i * FRM_LABW
    GOSUB vw_puts_lit

    GOSUB frm_ptr
    s_col = FRM_VALC : s_max = FRM_VALW
    s_col_color = COL_NORMAL
    IF (PEEK(SC_FDIRTY + fm_i) AND 255) <> 0 THEN s_col_color = COL_VALUE
    #s_src = #fm_p
    GOSUB scr_puts
END

' frm_row_sel: repaint the SELECTED row and put its two advance bits back.
' scr_puts and vw_puts_lit both write a bare card*8+colour, which clears bit
' 13, and the bar's own cells are (row,1) in the label and (row,19) in the
' value -- so any repaint of the selected row takes the bar down with it.
frm_row_sel: PROCEDURE
    fm_i = fm_sel
    GOSUB frm_row
    GOSUB frm_bar_set
END

frm_draw: PROCEDURE
    GOSUB scr_clear
    s_row = 0 : s_col = 0 : s_col_color = COL_HEAD
    IF fm_edit = 0 THEN
        PRINT AT screenpos(0, 0) COLOR COL_HEAD, "NEW EVENT"
    ELSE
        PRINT AT screenpos(0, 0) COLOR COL_HEAD, "EDIT EVENT"
    END IF
    FOR fm_i = 0 TO F_COUNT - 1
        GOSUB frm_row
    NEXT fm_i
    GOSUB frm_hints
    GOSUB frm_bar_apply
END

' The hint row wraps onto colour-stack entry 0, which is CS_BLUE now that the
' form runs on the views' profile -- so COL_DIM here would be blue on blue,
' the same trap screen.bas documents for COL_HEAD. The views' hint row is
' COL_NORMAL for exactly this reason.
'
' It prints from column 0 and so wipes the advance bit that makes the row the
' header colour; bar_rearm exists for that and every caller is followed by one.
frm_hints: PROCEDURE
    PRINT AT screenpos(0, ROW_HINT) COLOR COL_NORMAL, " FIRE EDIT ENT SAVE "
    GOSUB bar_rearm
END

' ---------------------------------------------------------------------------
' frm_snap / frm_check_dirty: exact change detection around a field editor.
'
' "Dirty" has to mean the bytes actually changed, not that the field was
' opened. Opening TITLE and pressing ENTER without typing must NOT mark it,
' because the title in this buffer is the LISTING's copy, truncated to 39
' characters -- resending it would replace a longer summary on the server with
' this stub. So the field is snapshotted before the editor runs and compared
' after. A byte compare rather than a checksum: a checksum that collided would
' silently drop the user's edit, which is the one failure mode not worth
' risking to save 97 bytes of a 512-byte buffer.
' ---------------------------------------------------------------------------
frm_snap: PROCEDURE
    GOSUB frm_ptr
    FOR fs_i = 0 TO fm_n - 1
        POKE (SC_FSNAP + fs_i), PEEK(#fm_p + fs_i) AND 255
    NEXT fs_i
END

frm_check_dirty: PROCEDURE
    GOSUB frm_ptr
    FOR fs_i = 0 TO fm_n - 1
        IF (PEEK(#fm_p + fs_i) AND 255) <> (PEEK(SC_FSNAP + fs_i) AND 255) THEN
            POKE (SC_FDIRTY + fm_i), 1
            EXIT FOR
        END IF
        IF (PEEK(#fm_p + fs_i) AND 255) = 0 THEN EXIT FOR
    NEXT fs_i
END

' ---------------------------------------------------------------------------
' frm_edit_text: hand the field to t9.bas.
'
' No scr_clear first, and that is the point: t9_entry writes rows 0-3 and 11
' and leaves 4-10 alone, so the field list stays on screen under the editor and
' the row being typed into is still visible. t9_entry scans the buffer for its
' existing length itself, so pre-loaded text is edited rather than replaced.
'
' It blocks until keypad ENTER and has no cancel gesture -- every key is spoken
' for. That is survivable here because the form's own save is a separate,
' later, deliberate act: backing out of the whole form still discards.
' ---------------------------------------------------------------------------
frm_edit_text: PROCEDURE
    fm_i = fm_sel
    GOSUB frm_snap
    GOSUB frm_ptr
    #ge_dst = #fm_p
    #g_max = fm_n
    GOSUB t9_entry
    GOSUB frm_check_dirty
END

' ---------------------------------------------------------------------------
' frm_edit_num: the masked editor for DATE ("YYYY-MM-DD") and the two times
' ("HH:MM"). Digits go straight in -- they are the keys the console actually
' has, and routing them through T9's ABC mode to reach a '3' would be absurd.
'
'   0-9     fill the next digit slot; the first one blanks the field
'   CLEAR   back up one slot; with nothing typed, EMPTY the field
'   ENTER   done. Nothing typed leaves the field as it was; a partly typed
'           value is rejected rather than sent as nonsense.
'   button  same as ENTER
'
' Emptying is not a nicety: a blank start time is the wire's only way to say
' all-day, so CLEAR on an untouched START is how a timed event is converted.
' ---------------------------------------------------------------------------
frm_edit_num: PROCEDURE
    fm_i = fm_sel
    GOSUB frm_snap
    GOSUB frm_ptr
    fe_n = 0
    fe_max = 4
    IF fm_i = F_DATE THEN fe_max = 8

    DO WHILE 1
        WAIT
        GOSUB in_poll

        IF in_key <= 9 THEN
            IF fe_n = 0 THEN GOSUB frm_num_blank
            IF fe_n < fe_max THEN
                IF fm_i = F_DATE THEN
                    POKE (#fm_p + frm_dmap(fe_n)), 48 + in_key
                ELSE
                    POKE (#fm_p + frm_tmap(fe_n)), 48 + in_key
                END IF
                fe_n = fe_n + 1
                GOSUB frm_row_sel
            END IF
        END IF

        IF in_key = KEYPAD_CLEAR THEN
            IF fe_n = 0 THEN
                POKE #fm_p, 0
                GOSUB frm_row_sel
            ELSE
                fe_n = fe_n - 1
                IF fm_i = F_DATE THEN
                    POKE (#fm_p + frm_dmap(fe_n)), 95
                ELSE
                    POKE (#fm_p + frm_tmap(fe_n)), 95
                END IF
                GOSUB frm_row_sel
            END IF
        END IF

        IF in_key = KEYPAD_ENTER OR in_btn <> 0 THEN
            ' A half-typed value is worse than none: put the old one back.
            IF fe_n > 0 AND fe_n < fe_max THEN
                FOR fs_i = 0 TO fm_n - 1
                    POKE (#fm_p + fs_i), PEEK(SC_FSNAP + fs_i) AND 255
                NEXT fs_i
            END IF
            EXIT DO
        END IF
    LOOP

    GOSUB frm_check_dirty
    GOSUB frm_row_sel
END

' frm_num_blank: lay down the field's mask, underscores in every digit slot,
' so a half-typed value reads as obviously unfinished rather than as a date.
frm_num_blank: PROCEDURE
    IF fm_i = F_DATE THEN
        POKE (#fm_p + 0), 95 : POKE (#fm_p + 1), 95
        POKE (#fm_p + 2), 95 : POKE (#fm_p + 3), 95
        POKE (#fm_p + 4), 45
        POKE (#fm_p + 5), 95 : POKE (#fm_p + 6), 95
        POKE (#fm_p + 7), 45
        POKE (#fm_p + 8), 95 : POKE (#fm_p + 9), 95
        POKE (#fm_p + 10), 0
    ELSE
        POKE (#fm_p + 0), 95 : POKE (#fm_p + 1), 95
        POKE (#fm_p + 2), 58
        POKE (#fm_p + 3), 95 : POKE (#fm_p + 4), 95
        POKE (#fm_p + 5), 0
    END IF
END

' ---------------------------------------------------------------------------
' Status/error lines, 20 columns each, indexed by fs_err - 1.
'
' The first four exist because the adapter collapses EVERY draft rejection --
' bad time, missing summary, end before start, mixed all-day and timed forms --
' into one code (132). By the time a rejection comes back there is nothing left
' to tell the user, so the checks that can run here, run here.
' ---------------------------------------------------------------------------
lit_f_msgs:
    DATA 78,69,69,68,32,65,32,84,73,84,76,69,0 ' NEED A TITLE
    DATA 66,65,68,32,68,65,84,69,0            ' BAD DATE
    DATA 66,65,68,32,84,73,77,69,0            ' BAD TIME
    DATA 69,78,68,32,78,69,69,68,83,32,65,32,83,84,65,82,84,0 ' END NEEDS A START
    DATA 78,79,84,72,73,78,71,32,67,72,65,78,71,69,68,0 ' NOTHING CHANGED
    DATA 83,65,86,73,78,71,46,46,46,0        ' SAVING...
    DATA 83,65,86,69,68,0                        ' SAVED
    DATA 82,69,74,69,67,84,69,68,32,66,89,32,71,79,79,71,76,69,0 ' REJECTED BY GOOGLE
    DATA 78,79,84,32,65,85,84,72,79,82,73,83,69,68,0 ' NOT AUTHORISED
    DATA 69,86,69,78,84,32,86,65,78,73,83,72,69,68,0 ' EVENT VANISHED
    DATA 78,79,32,67,79,78,78,69,67,84,73,79,78,0 ' NO CONNECTION
' Byte offset of message i into lit_f_msgs.
frm_msgoff:
    DATA 0,13,22,31,49,65,75,81,100,115,130
    CONST FM_NEEDTITLE = 1
    CONST FM_BADDATE   = 2
    CONST FM_BADTIME   = 3
    CONST FM_ENDALONE  = 4
    CONST FM_NOTHING   = 5
    CONST FM_SAVING    = 6
    CONST FM_SAVED     = 7
    CONST FM_REJECTED  = 8
    CONST FM_DENIED    = 9
    CONST FM_GONE      = 10
    CONST FM_NONET     = 11

' Only two of the eleven are good news, so error red is the default and the
' two exceptions are named. scr_puts rather than vw_puts_lit: it stops at the
' NUL and pads the rest of the row, which is what lets the table hold
' variable-length strings -- 155 words against the 220 that eleven
' 20-column-padded rows cost, and the padding was only ever there to blank the
' tail of the row.
frm_say: PROCEDURE
    s_row = 2 : s_col = 0 : s_max = SCREEN_COLS
    s_col_color = COL_ERROR
    IF fs_err = FM_SAVING THEN s_col_color = COL_HILIGHT
    IF fs_err = FM_SAVED THEN s_col_color = COL_HILIGHT
    #s_src = VARPTR lit_f_msgs(0) + frm_msgoff(fs_err - 1)
    GOSUB scr_puts
END

' ---------------------------------------------------------------------------
' Validation. fs_err = 0 means the draft is worth sending.
' ---------------------------------------------------------------------------
frm_isdig: PROCEDURE
    fm_c = PEEK(#fm_p + fs_i) AND 255
    fm_prev = 0
    IF fm_c >= 48 AND fm_c <= 57 THEN fm_prev = 1
END

' Two digits at fs_i into fm_n.
frm_two: PROCEDURE
    fm_n = ((PEEK(#fm_p + fs_i) AND 255) - 48) * 10
    fm_n = fm_n + ((PEEK(#fm_p + fs_i + 1) AND 255) - 48)
END

frm_chk_date: PROCEDURE
    FOR fs_i = 0 TO 9
        IF fs_i = 4 OR fs_i = 7 THEN
            IF (PEEK(#fm_p + fs_i) AND 255) <> 45 THEN
                fs_err = FM_BADDATE
                EXIT FOR
            END IF
        ELSE
            GOSUB frm_isdig
            IF fm_prev = 0 THEN
                fs_err = FM_BADDATE
                EXIT FOR
            END IF
        END IF
    NEXT fs_i
    IF fs_err <> 0 THEN RETURN
    IF (PEEK(#fm_p + 10) AND 255) <> 0 THEN
        fs_err = FM_BADDATE
        RETURN
    END IF

    ' Real month length, via clock.bas -- 31 February is rejected here rather
    ' than coming back as an opaque 132.
    #cd_y = 0
    FOR fs_i = 0 TO 3
        #cd_y = #cd_y * 10 + ((PEEK(#fm_p + fs_i) AND 255) - 48)
    NEXT fs_i
    fs_i = 5 : GOSUB frm_two : cd_mo = fm_n
    IF cd_mo < 1 OR cd_mo > 12 THEN
        fs_err = FM_BADDATE
        RETURN
    END IF
    GOSUB clk_dim
    fs_i = 8 : GOSUB frm_two
    IF fm_n < 1 OR fm_n > cd_n THEN fs_err = FM_BADDATE
END

frm_chk_time: PROCEDURE
    FOR fs_i = 0 TO 4
        IF fs_i = 2 THEN
            IF (PEEK(#fm_p + fs_i) AND 255) <> 58 THEN
                fs_err = FM_BADTIME
                EXIT FOR
            END IF
        ELSE
            GOSUB frm_isdig
            IF fm_prev = 0 THEN
                fs_err = FM_BADTIME
                EXIT FOR
            END IF
        END IF
    NEXT fs_i
    IF fs_err <> 0 THEN RETURN
    IF (PEEK(#fm_p + 5) AND 255) <> 0 THEN
        fs_err = FM_BADTIME
        RETURN
    END IF
    fs_i = 0 : GOSUB frm_two
    IF fm_n > 23 THEN
        fs_err = FM_BADTIME
        RETURN
    END IF
    fs_i = 3 : GOSUB frm_two
    IF fm_n > 59 THEN fs_err = FM_BADTIME
END

frm_validate: PROCEDURE
    fs_err = 0

    IF fm_edit = 0 THEN
        fm_i = F_TITLE
        GOSUB frm_ptr
        IF (PEEK(#fm_p) AND 255) = 0 THEN
            fs_err = FM_NEEDTITLE
            RETURN
        END IF
    END IF

    fm_i = F_DATE
    GOSUB frm_ptr
    IF (PEEK(#fm_p) AND 255) <> 0 THEN
        GOSUB frm_chk_date
        IF fs_err <> 0 THEN RETURN
    ELSE
        IF fm_edit = 0 THEN
            fs_err = FM_BADDATE
            RETURN
        END IF
    END IF

    fm_i = F_START
    GOSUB frm_ptr
    IF (PEEK(#fm_p) AND 255) <> 0 THEN
        GOSUB frm_chk_time
        IF fs_err <> 0 THEN RETURN
    END IF

    fm_i = F_END
    GOSUB frm_ptr
    IF (PEEK(#fm_p) AND 255) <> 0 THEN
        GOSUB frm_chk_time
        IF fs_err <> 0 THEN RETURN
        ' END alone cannot switch a timed event to all-day or back; the
        ' adapter answers MIXED_FORMS, which arrives as the same opaque 132.
        fm_i = F_START
        GOSUB frm_ptr
        IF (PEEK(#fm_p) AND 255) = 0 THEN fs_err = FM_ENDALONE
    END IF
END

' ---------------------------------------------------------------------------
' THE WIRE
'
' A write-mode open, one net_write per "KEY: value" line, then CLOSE -- and
' the close is the commit. The verdict is not in the close; it is in the
' STATUS that follows it, which is the only reason net_status is called here
' at all (Calendar.cpp deliberately preserves `error` across the base close so
' the bus can latch it).
'
' Compose targets the selector alone: no view, no date, no /N, no query. An
' edit targets exactly the spec the listing used, plus /N -- gc_build_url
' already emits that, query included, and the query matters: the adapter
' numbers events WITHIN the ?count=/&days= window, so an edit URL that dropped
' it would address a different event than the one on screen.
' ---------------------------------------------------------------------------

' frm_url_root: "N:GCAL://" + the selected calendar, and nothing else.
' Never "*": util_devicespec_fix_for_parsing() rewrites a wildcard to an
' embedded NUL on any non-DIRECTORY open, and a wildcard is not a compose
' target anyway. SC_CAL is empty for "the primary calendar", which is exactly
' what the adapter wants.
frm_url_root: PROCEDURE
    #fn_txlen = 0
    #fn_src = VARPTR lit_scheme(0)
    fn_len = LEN_SCHEME
    GOSUB fn_putstr
    #fn_src = SC_CAL
    ls_max = 63
    GOSUB fn_strlen
    GOSUB fn_putstr
END

frm_open_write: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_OPEN
    mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = OPEN_MODE_WRITE : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    GOSUB fn_transact
END

' Stage the key literal at #vw_p/vl_len as the start of a line.
frm_line_key: PROCEDURE
    #fn_txlen = 0
    #fn_src = #vw_p
    fn_len = vl_len
    GOSUB fn_putstr
END

' Terminate the staged line and send it. $0D -- the adapter's splitter takes
' CR, LF, CRLF or $9B, and CR costs nothing to emit from here.
frm_line_send: PROCEDURE
    POKE (FN_TX + #fn_txlen), 13
    #fn_txlen = #fn_txlen + 1
    fn_len = #fn_txlen
    GOSUB net_write
    fs_lines = fs_lines + 1
END

' frm_emit_plain: "KEY: <field fm_w>" verbatim.
frm_emit_plain: PROCEDURE
    GOSUB frm_line_key
    fm_i = fm_w
    GOSUB frm_ptr
    #fn_src = #fm_p
    ls_max = fm_n - 1
    GOSUB fn_strlen
    GOSUB fn_putstr
    GOSUB frm_line_send
END

' frm_emit_summary: the title, Title-Cased on the way out.
'
' The buffer is lowercase, because that is what T9 commits -- and it displays
' as lowercase too: GROM carries the full printable ASCII set, so the form
' shows exactly what will be sent. Sending it raw would still put "dentist" in
' everyone else's calendar, next to events that are capitalised. One pass over
' the field on the way out is cheaper than making the editor shift-capable, and
' the other fields are left as typed.
frm_emit_summary: PROCEDURE
    GOSUB frm_line_key
    fm_i = F_TITLE
    GOSUB frm_ptr
    fm_prev = 32
    fs_i = 0
    fm_c = 1
    WHILE (fs_i < fm_n - 1) AND (fm_c <> 0)
        fm_c = PEEK(#fm_p + fs_i) AND 255
        IF fm_c <> 0 THEN
            IF fm_prev = 32 AND fm_c >= 97 AND fm_c <= 122 THEN fm_c = fm_c - 32
            POKE (FN_TX + #fn_txlen + fs_i), fm_c
            fm_prev = fm_c
            fs_i = fs_i + 1
        END IF
    WEND
    #fn_txlen = #fn_txlen + fs_i
    GOSUB frm_line_send
END

' frm_emit_when: "START: YYYY-MM-DD" or "START: YYYY-MM-DD HH:MM", from the
' shared DATE field plus the time in fm_w. A missing time is not an omission:
' a date-only value is the wire's spelling of all-day.
frm_emit_when: PROCEDURE
    GOSUB frm_line_key
    fm_i = F_DATE
    GOSUB frm_ptr
    #fn_src = #fm_p
    ls_max = 10
    GOSUB fn_strlen
    GOSUB fn_putstr
    fm_i = fm_w
    GOSUB frm_ptr
    IF (PEEK(#fm_p) AND 255) <> 0 THEN
        POKE (FN_TX + #fn_txlen), 32
        #fn_txlen = #fn_txlen + 1
        #fn_src = #fm_p
        ls_max = 5
        GOSUB fn_strlen
        GOSUB fn_putstr
    END IF
    GOSUB frm_line_send
END

' frm_want: fm_prev = 1 if field fm_w should go out. Empty fields never do;
' on an edit, neither do untouched ones.
frm_want: PROCEDURE
    fm_prev = 0
    fm_i = fm_w
    GOSUB frm_ptr
    IF (PEEK(#fm_p) AND 255) = 0 THEN RETURN
    IF fm_edit = 0 THEN
        fm_prev = 1
    ELSE
        IF (PEEK(SC_FDIRTY + fm_w) AND 255) <> 0 THEN fm_prev = 1
    END IF
END

' frm_anydirty: fm_prev = 1 if anything was touched at all.
frm_anydirty: PROCEDURE
    fm_prev = 0
    FOR fs_i = 0 TO F_COUNT - 1
        IF (PEEK(SC_FDIRTY + fs_i) AND 255) <> 0 THEN fm_prev = 1
    NEXT fs_i
END

' ---------------------------------------------------------------------------
' frm_save
' ---------------------------------------------------------------------------
frm_save: PROCEDURE
    GOSUB frm_validate
    IF fs_err <> 0 THEN
        GOSUB frm_say
        RETURN
    END IF

    ' An edit that changed nothing must not open a channel: an edit open costs
    ' a whole window fetch upstream before it answers.
    IF fm_edit <> 0 THEN
        GOSUB frm_anydirty
        IF fm_prev = 0 THEN
            fs_err = FM_NOTHING
            GOSUB frm_say
            RETURN
        END IF
    END IF

    fs_err = FM_SAVING
    GOSUB frm_say

    IF fm_edit = 0 THEN
        GOSUB frm_url_root
    ELSE
        gc_view = cur_view
        #gc_evnum = #ev_num
        GOSUB gc_build_url
        #gc_evnum = 0
    END IF

    #fn_tmo = FN_TMO_SAVE
    GOSUB frm_open_write
    #fn_tmo = FN_TMO_DEF
    IF fn_ok = 0 THEN
        fs_err = FM_NONET
        GOSUB frm_say
        RETURN
    END IF

    ' Order follows the C clients and tests/hosttest.c. The adapter itself is
    ' order-indifferent.
    fs_lines = 0

    fm_w = F_TITLE
    GOSUB frm_want
    IF fm_prev <> 0 THEN
        vl_len = LEN_K_SUMMARY : #vw_p = VARPTR lit_k_summary(0)
        GOSUB frm_emit_summary
    END IF

    ' START goes out whenever the date OR the time moved, never one alone:
    ' emitting a date without its time would silently convert a timed event to
    ' all-day, and emitting a time without its date is not a thing the wire
    ' has. Compose always sends it -- the adapter requires it.
    fm_prev = 0
    IF fm_edit = 0 THEN
        fm_prev = 1
    ELSE
        IF (PEEK(SC_FDIRTY + F_DATE) AND 255) <> 0 THEN fm_prev = 1
        IF (PEEK(SC_FDIRTY + F_START) AND 255) <> 0 THEN fm_prev = 1
    END IF
    IF fm_prev <> 0 THEN
        fm_w = F_START
        vl_len = LEN_K_START : #vw_p = VARPTR lit_k_start(0)
        GOSUB frm_emit_when
    END IF

    fm_prev = 0
    IF fm_edit = 0 THEN
        fm_i = F_END
        GOSUB frm_ptr
        IF (PEEK(#fm_p) AND 255) <> 0 THEN fm_prev = 1
    ELSE
        IF (PEEK(SC_FDIRTY + F_END) AND 255) <> 0 THEN fm_prev = 1
    END IF
    IF fm_prev <> 0 THEN
        fm_w = F_END
        vl_len = LEN_K_END : #vw_p = VARPTR lit_k_end(0)
        GOSUB frm_emit_when
    END IF

    fm_w = F_LOC
    GOSUB frm_want
    IF fm_prev <> 0 THEN
        vl_len = LEN_K_LOC : #vw_p = VARPTR lit_k_loc(0)
        GOSUB frm_emit_plain
    END IF

    fm_w = F_DESC
    GOSUB frm_want
    IF fm_prev <> 0 THEN
        vl_len = LEN_K_DESC : #vw_p = VARPTR lit_k_desc(0)
        GOSUB frm_emit_plain
    END IF

    fm_w = F_CAT
    GOSUB frm_want
    IF fm_prev <> 0 THEN
        vl_len = LEN_K_CAT : #vw_p = VARPTR lit_k_cat(0)
        GOSUB frm_emit_plain
    END IF

    GOSUB net_close

    ' Nothing written means the adapter saw an empty buffer and aborted
    ' cleanly rather than committing -- no event was touched.
    IF fs_lines = 0 THEN
        fs_err = FM_NOTHING
        GOSUB frm_say
        RETURN
    END IF

    GOSUB net_status
    fs_err = FM_SAVED
    IF #net_err = NS_BAD_COMMAND THEN fs_err = FM_REJECTED
    IF #net_err = NS_ACCESS_DENIED THEN fs_err = FM_DENIED
    IF #net_err = NS_NOT_AUTH THEN fs_err = FM_DENIED
    IF #net_err = NS_NOT_FOUND THEN fs_err = FM_GONE
    IF #net_err = NS_BAD_SPEC THEN fs_err = FM_REJECTED

    IF fs_err = FM_SAVED THEN
        ' The listing on the way back is now stale: a committed event has to be
        ' re-fetched to get its number, colour and placement from the adapter
        ' rather than guessed at here.
        vw_dirty = 1
        fm_shown = 0
        state = ST_VIEW
    ELSE
        GOSUB frm_draw
        GOSUB frm_say
    END IF
END

' ---------------------------------------------------------------------------
' do_form: the ST_FORM state handler.
' ---------------------------------------------------------------------------
do_form: PROCEDURE
    IF fm_shown = 0 THEN
        GOSUB frm_draw
        fm_shown = 1
        RETURN
    END IF

    GOSUB in_poll

    ' Moving the selection changes no TEXT at all -- the row colours say
    ' dirty or clean, not selected -- so it is four advance-bit stores and no
    ' repaint. They go back to back for the reason bar_move documents: between
    ' the clear and the set the screen carries two of its four bits, and every
    ' row below renders one stack position out of phase. Any frame that lands
    ' in the gap shows it as a whole-screen colour shift.
    IF in_disc = DISC_UP THEN
        IF fm_sel > 0 THEN
            GOSUB frm_bar_clr
            fm_sel = fm_sel - 1
            GOSUB frm_bar_set
        END IF
    END IF
    IF in_disc = DISC_DOWN THEN
        IF fm_sel < F_COUNT - 1 THEN
            GOSUB frm_bar_clr
            fm_sel = fm_sel + 1
            GOSUB frm_bar_set
        END IF
    END IF

    IF in_btn <> 0 THEN
        IF fm_sel = F_DATE OR fm_sel = F_START OR fm_sel = F_END THEN
            GOSUB frm_edit_num
        ELSE
            GOSUB frm_edit_text
        END IF
        ' t9_entry owns rows 0-3 and 11 while it runs, and the numeric editor
        ' leaves its own row highlighted; either way the chrome has to come
        ' back. Rows 4-10 are redrawn too, cheaply, rather than reasoning about
        ' which of them survived.
        GOSUB frm_draw
        ' Both editors exit on keypad ENTER, and in_key is a global that
        ' survives the return -- so without this the very keypress that
        ' finished the field fell straight through to the save test below and
        ' sent the draft. Typing a title and accepting it silently created the
        ' event. in_btn goes too: the numeric editor also exits on the button.
        in_key = KEYPAD_NONE
        in_btn = 0
    END IF

    IF in_key = KEYPAD_ENTER THEN GOSUB frm_save

    IF in_key = KEYPAD_CLEAR THEN
        fm_shown = 0
        vw_shown = 0
        state = ST_VIEW
    END IF
END

' ---------------------------------------------------------------------------
' Entry points, called from st_view.bas (keypad 5 and 6) and st_event.bas (6).
' ---------------------------------------------------------------------------

' frm_new: a blank form on the anchor date. In the MONTH view that is the
' cursor's day, because #cur_y/cur_mo/cur_d IS the month cursor; in the other
' three it is the period's anchor.
frm_new: PROCEDURE
    fm_edit = 0
    GOSUB frm_clear
    fm_i = F_DATE
    GOSUB frm_put_date
    fm_sel = F_TITLE
    fs_err = 0
    fm_shown = 0
    state = ST_FORM
END

' frm_edit_event: a form prefilled from SC_EVT[ev_sel], which the caller has
' already selected.
'
' #ev_num is the ADAPTER's event number, not this array index: the adapter
' re-runs the query and re-numbers from a total ordering, so /N is what
' addresses the same event on a later open.
frm_edit_event: PROCEDURE
    fm_edit = 1
    GOSUB frm_prefill
    #evrec = SC_EVT + ev_sel * EVT_STRIDE
    #ev_num = (PEEK(#evrec + EVT_NUMLO) AND 255) + (PEEK(#evrec + EVT_NUMHI) AND 255) * 256
    fm_sel = F_TITLE
    fs_err = 0
    fm_shown = 0
    state = ST_FORM
END
