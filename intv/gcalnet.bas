' gcalnet.bas -- compose a GCAL devicespec, stream the reply, parse it.
'
' THE WIRE FORMAT
'
' The GCAL adapter (fujinet-firmware lib/network-protocol/Calendar.cpp) offers
' two output shapes for an event index. aux2 = 255 gives packed 277-byte
' CalEventItem structs whose start/end are 64-bit UTC epoch seconds; aux2 =
' anything else gives fixed-column text, with the line width taken from the low
' seven bits.
'
' This client asks for text at width 80, for two reasons that both matter more
' than the format's slight looseness:
'
'   * The adapter has already resolved every timestamp into local wall-clock
'     time through [General] timezone. Consuming the binary form would mean
'     doing 64-bit epoch-to-civil conversion on a CP-1610 -- a large amount of
'     multi-precision code to arrive at what the text form hands over as ASCII.
'   * A text row is 40-80 bytes against the struct's 277. FN_RX is 512 bytes,
'     so text brings back six or more events per mailbox transaction where the
'     binary form manages one, and fn_transact blocks for up to 900 frames.
'
' A DIR reply is: a window-title line ("Fri 28 Aug 2026"), a header line, then
' one line per event.
'
' THE LINE ENDING IS $9B, NOT $0A
'
' lib/device/rs232/network.cpp defines DEFAULT_LINE_ENDING "\n" and calls
' protocol->setLineEnding() with it -- but at the END of rs232_open(), AFTER
' protocol->open() has already composed the entire reply. Calendar output is
' built in open() (read() is a no-op that just drains the buffer), so it is
' terminated with Protocol.h's default "\x9B" and the RS232 setting never
' reaches it. Verified on the wire:
'
'   4D 6F 6E 20 33 31 20 41 75 67 20 32 30 32 36 9B 2D 2D 23 2D 54 69 6D 65
'   M  o  n     3  1     A  u  g     2  0  2  6  ^  -  -  #  -  T  i  m  e
'
' Both $9B and $0A are accepted below so the client keeps working either way.
'
' COLUMN LAYOUT
'
' At width 80 every view takes the adapter's "wide" branch, so a row is
'
'   marker(1) sp num(numW) sp [date(dateW) sp] time(11) sp category(14) sp title
'
' with marker ' ' timed / '*' all-day / '~' recurring, and dateW 0 for DAY,
' 3 ("Fri") for WEEK, 5 ("Fr 28") for MONTH, 6 ("28 Aug") for AGENDA.
'
' numW is digits(highest event number), which is not known in advance -- but it
' does not need a second request to discover. The header line is built by
' dashed(), which replaces every space with '-', so it reads
'
'   --#-Date---Time--------Category---------...
'
' and the '#' sits at index 2 + numW - 1. Scanning for it recovers numW in one
' pass over a line we have to read anyway.
'
' VARIABLE DISCIPLINE
'
' IntyBASIC PROCEDUREs share one flat global namespace and are not reentrant,
' so scratch names here are layered by call depth and never reused across a
' GOSUB boundary:
'   gf_*  gc_fetch            gp_*  gc_parse_chunk
'   ge_*  gc_take_event       gt_*  gc_take_title      gm_*  gc_tally_month
'   gx_*  leaf helpers only (gc_read_num, gc_colour and friends)
' A caller must not hold a value in a gx_ name across a GOSUB.

    CONST GC_WIDTH   = 80     ' aux2 for a DIR open; must be >= 16 or the
                              ' adapter silently substitutes its own default
    CONST GC_TIMEW   = 11
    CONST GC_CATW    = 14
    CONST OPEN_MODE_DIR  = $06 ' ACCESS_MODE::DIRECTORY
    CONST OPEN_MODE_READ = $04 ' ACCESS_MODE::READ
' ACCESS_MODE::WRITE opens a DRAFT channel: field lines go out with net_write
' and the adapter commits the event when the channel CLOSES. A selector-only
' spec composes; the listing's own spec plus /N edits that event. aux2 is
' ignored. See st_form.bas.
    CONST OPEN_MODE_WRITE = $08

' Adapter status codes worth telling apart (status_error_codes.h).
    CONST NS_SUCCESS       = 1
    CONST NS_EOF           = 136
    CONST NS_BAD_SPEC      = 165
    CONST NS_ACCESS_DENIED = 167
    CONST NS_NOT_FOUND     = 170
' Write-channel verdicts, latched by close() and read by the STATUS that
' follows it. EVERY draft rejection reason -- bad time, missing summary, end
' before start, all-day/timed mismatch -- arrives as INVALID_COMMAND, which is
' why st_form.bas validates before it sends.
    CONST NS_BAD_COMMAND   = 132
    CONST NS_NOT_AUTH      = 212

' Every DIM ahead of the first PROCEDURE.
    DIM gc_numw, gc_datew, gc_used, gc_timec, gc_catc, gc_titlec
    DIM gc_line, gc_hold, gc_err, gc_view
    DIM gc_sh, gc_sm, gc_eh, gc_em, gc_all_day, gc_open_end
    DIM gf_i, gp_c, gl_len
' #gp_i is 16-bit. It indexes a read of up to 512 bytes, and IntyBASIC's
' FOR/NEXT increments in a register, stores the TRUNCATED value back to the
' 8-bit variable, then compares the UN-truncated register against the limit:
'     MVI var_GP_I,R0 / INCR R0 / MVO R0,var_GP_I / CMPR R1,R0 / BLE loop
' so at 255 it stores 0, compares 256 <= limit, and loops forever. An 8-bit
' counter here hangs the console on any reply over 256 bytes -- which is every
' month and agenda window.
    DIM #gp_i
    DIM ge_flags, gt_len, gm_day
    DIM gx_i, gx_j, gx_c, gx_n, gx_v, gx_col
    DIM #gx_p, #gx_q, #gx_num, #gc_evnum

' ---------------------------------------------------------------------------
' Devicespec literals. IntyBASIC has no string type, so every fixed token is a
' DATA list of ASCII bytes with its length as a CONST.
' ---------------------------------------------------------------------------
' "N:GCAL://" -- the scheme and its empty authority, and NOTHING more. The
' third slash that makes the documented "GCAL:///DAY/..." form is the leading
' slash of the PATH, and the view literals below each supply their own. Folding
' it in here instead produced "N:GCAL:////DAY/...", which the adapter still
' parsed (it drops one leading slash, and scans the tail for the view) but which
' is one segment away from being wrong for any non-empty selector.
lit_scheme:
    DATA 78,58,71,67,65,76,58,47,47          ' "N:GCAL://"
    CONST LEN_SCHEME = 9

lit_v_day:
    DATA 47,68,65,89                        ' "/DAY"
    CONST LEN_V_DAY = 4
lit_v_week:
    DATA 47,87,69,69,75                     ' "/WEEK"
    CONST LEN_V_WEEK = 5
lit_v_month:
    DATA 47,77,79,78,84,72                  ' "/MONTH"
    CONST LEN_V_MONTH = 6
lit_v_agenda:
    DATA 47,65,71,69,78,68,65               ' "/AGENDA"
    CONST LEN_V_AGENDA = 7

' "?count=96&days=90" -- AGENDA only. Without count= the adapter returns its
' 20-event default, which is a thin agenda; 96 is this client's own ceiling
' (MAX_EVENTS), so asking for more would only waste transfer.
lit_q_agenda:
    DATA 63,99,111,117,110,116,61,57,54,38,100,97,121,115,61,57,48
    CONST LEN_Q_AGENDA = 17

' ---------------------------------------------------------------------------
' gc_put_date: append "/YYYY-MM-DD" from the view anchor.
' ---------------------------------------------------------------------------
gc_put_date: PROCEDURE
    POKE (FN_TX + #fn_txlen), 47 : #fn_txlen = #fn_txlen + 1
    #pn_val4 = #cur_y : GOSUB fn_putnum4
    POKE (FN_TX + #fn_txlen), 45 : #fn_txlen = #fn_txlen + 1
    pn_val = cur_mo : GOSUB fn_putnum2
    POKE (FN_TX + #fn_txlen), 45 : #fn_txlen = #fn_txlen + 1
    pn_val = cur_d : GOSUB fn_putnum2
END

' ---------------------------------------------------------------------------
' gc_build_url: stage the devicespec for gc_view into FN_TX.
'
' The selector comes from SC_CAL, which is empty for "the calendars Google is
' showing". It is deliberately never "*": util_devicespec_fix_for_parsing()
' rewrites '*' to an embedded NUL on any open that is not a DIRECTORY, so a
' spec that works for the listing would silently corrupt the aux1=4 detail
' fetch of the same event.
'
' #gc_evnum, when non-zero, appends the adapter's "/N" for an event-detail
' open. It goes in the PATH, ahead of any query string -- parse_devicespec()
' takes everything after the first '?' as the query, so an /N placed after it
' would never be seen. Equally, the query must NOT simply be dropped for a
' detail fetch: ?count= and ?days= define the AGENDA window, and the adapter
' numbers events within that window, so a detail opened without them would
' address a different event entirely.
' ---------------------------------------------------------------------------
gc_build_url: PROCEDURE
    #fn_txlen = 0
    #fn_src = VARPTR lit_scheme(0) : fn_len = LEN_SCHEME : GOSUB fn_putstr
    #fn_src = SC_CAL : ls_max = 63 : GOSUB fn_strlen : GOSUB fn_putstr

    IF gc_view = VIEW_DAY THEN
        #fn_src = VARPTR lit_v_day(0) : fn_len = LEN_V_DAY
    ELSEIF gc_view = VIEW_WEEK THEN
        #fn_src = VARPTR lit_v_week(0) : fn_len = LEN_V_WEEK
    ELSEIF gc_view = VIEW_MONTH THEN
        #fn_src = VARPTR lit_v_month(0) : fn_len = LEN_V_MONTH
    ELSE
        #fn_src = VARPTR lit_v_agenda(0) : fn_len = LEN_V_AGENDA
    END IF
    GOSUB fn_putstr

    GOSUB gc_put_date

    IF #gc_evnum > 0 THEN
        POKE (FN_TX + #fn_txlen), 47 : #fn_txlen = #fn_txlen + 1
        #pn_val4 = #gc_evnum
        GOSUB fn_putnum3
    END IF

    IF gc_view = VIEW_AGENDA THEN
        #fn_src = VARPTR lit_q_agenda(0) : fn_len = LEN_Q_AGENDA
        GOSUB fn_putstr
    END IF
END

' ---------------------------------------------------------------------------
' gc_open_dir: open the staged devicespec as a directory listing at width
' GC_WIDTH. aux1 = 6 selects the event index; aux2 doubles as the format and
' width byte, and any value other than 255 means "human-readable".
' ---------------------------------------------------------------------------
gc_open_dir: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_OPEN
    mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = OPEN_MODE_DIR : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = GC_WIDTH : GOSUB fn_param
    GOSUB fn_transact
END

' ---------------------------------------------------------------------------
' gc_status: net_status, but END_OF_FILE is normal termination.
'
' NetworkProtocolCalendar::status() reports END_OF_FILE (136) as soon as its
' receiveBuffer drains, and the whole reply is composed at open() rather than
' streamed, so a successful fetch ALWAYS ends with 136. net_status treats any
' code other than SUCCESS as failure and would abort every fetch on its last
' read. Sets fn_ok = 1 while data remains, 0 at the end or on a real error,
' with gc_err carrying the code so the caller can tell the two apart.
' ---------------------------------------------------------------------------
gc_status: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_STATUS
    mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    #fn_txlen = 0
    GOSUB fn_transact
    IF fn_ok = 0 THEN
        gc_err = 0            ' mailbox timeout: the RP2040 never answered
        #net_avail = 0
        RETURN
    END IF
    #net_avail = (PEEK(FN_RX) AND 255) + (PEEK(FN_RX + 1) AND 255) * 256
    #net_err = (PEEK(FN_RX + 3) AND 255)
    gc_err = #net_err
    IF #net_err = NS_SUCCESS THEN
        fn_ok = 1
    ELSE
        fn_ok = 0
        #net_avail = 0
    END IF
END

' ---------------------------------------------------------------------------
' gc_fetch: run a whole listing. Fills SC_EVT/SC_TITLE (or SC_DAYS/SC_DCOL for
' MONTH) and sets ev_count, gc_trunc and gc_err.
'
' This is the only place a fetch happens. It blocks -- fn_transact spins on
' WAIT for up to 900 frames per transaction -- so it is called once when a view
' is entered or its period changes, never from an input loop. Moving the
' selection afterwards only recolours two rows.
' ---------------------------------------------------------------------------
gc_fetch: PROCEDURE
    ev_count = 0
    gc_trunc = 0
    gc_line = 0
    gc_hold = 0
    gc_numw = 0
    gc_err = NS_SUCCESS
    #gc_evnum = 0                 ' a listing, not a detail
    POKE SC_HDR, 0

    IF gc_view = VIEW_MONTH THEN
        FOR gf_i = 0 TO 41
            POKE (SC_DAYS + gf_i), 0
            POKE (SC_DCOL + gf_i), 0
        NEXT gf_i
    END IF

    GOSUB gc_build_url
    GOSUB gc_open_dir
    IF fn_ok = 0 THEN
        gc_err = 0
        RETURN
    END IF

gf_loop:
    GOSUB gc_status
    IF fn_ok = 0 THEN GOTO gf_done
    IF #net_avail = 0 THEN GOTO gf_done

    #net_readlen = #net_avail
    IF #net_readlen > 512 THEN #net_readlen = 512
    GOSUB net_read
    IF fn_ok = 0 THEN GOTO gf_done
    IF #net_gotlen = 0 THEN GOTO gf_done

    GOSUB gc_parse_chunk
    GOTO gf_loop

gf_done:
    ' A reply whose final line has no trailing newline would otherwise be
    ' dropped; flush whatever is still held.
    IF gc_hold > 0 THEN
        gl_len = gc_hold
        #gx_p = SC_LINEBUF
        GOSUB gc_parse_line
        gc_hold = 0
    END IF
    GOSUB net_close
    ' END_OF_FILE is how a complete listing ends, not a failure.
    IF gc_err = NS_EOF THEN gc_err = NS_SUCCESS
END

' ---------------------------------------------------------------------------
' gc_parse_chunk: split the #net_gotlen bytes now in FN_RX into $0A-terminated
' lines and hand each to gc_parse_line.
'
' A line can straddle a 512-byte read, so a partial tail stays in SC_LINEBUF and
' the next chunk's leading fragment is appended to it. SC_LINEBUF is 128 bytes;
' a width-80 row plus an unpadded title can exceed that, but the title is the
' last field and gets clipped to 31 characters anyway, so overflow only drops
' text that was never going to be displayed. Every column the parser reads
' lives below offset 80.
' ---------------------------------------------------------------------------
gc_parse_chunk: PROCEDURE
    FOR #gp_i = 0 TO #net_gotlen - 1
        gp_c = PEEK(FN_RX + #gp_i) AND 255
        IF gp_c = 155 OR gp_c = 10 THEN
            gl_len = gc_hold
            #gx_p = SC_LINEBUF
            GOSUB gc_parse_line
            gc_hold = 0
        ELSE
            ' Drop CR so a bus that ever switches to CRLF cannot leave one
            ' trailing in a title.
            IF gp_c <> 13 AND gc_hold < 127 THEN
                POKE (SC_LINEBUF + gc_hold), gp_c
                gc_hold = gc_hold + 1
            END IF
        END IF
    NEXT #gp_i
END

' ---------------------------------------------------------------------------
' gc_parse_line: consume one complete line, gl_len bytes at #gx_p.
'
' Line 0 is the window title ("Fri 28 Aug 2026") -- kept for the view header.
' Line 1 is the column header, from which numW and every column offset follow.
' Everything after that is an event.
' ---------------------------------------------------------------------------
gc_parse_line: PROCEDURE
    IF gc_line = 0 THEN
        gc_line = 1
        GOSUB gc_take_window_title
        RETURN
    END IF
    IF gc_line = 1 THEN
        gc_line = 2
        GOSUB gc_layout_from_header
        RETURN
    END IF
    ' "  (no events)" is the adapter's empty-window line and has no columns.
    IF gl_len < gc_titlec THEN RETURN
    GOSUB gc_take_event
END

' ---------------------------------------------------------------------------
' gc_take_window_title: stash line 0 into SC_HDR, upper-cased. The GROM font
' has lowercase, but the rest of the UI is uppercase and mixing the two on one
' header row reads as an accident.
' ---------------------------------------------------------------------------
gc_take_window_title: PROCEDURE
    gx_n = gl_len
    IF gx_n > SC_HDR_MAX - 1 THEN gx_n = SC_HDR_MAX - 1
    ' An IntyBASIC FOR runs its body before testing, so "FOR i = 0 TO -1"
    ' wraps and iterates 256 times rather than zero. Every count-driven loop
    ' in this file that can legitimately see a length of 0 is guarded.
    IF gx_n = 0 THEN
        POKE SC_HDR, 0
        RETURN
    END IF
    FOR gx_i = 0 TO gx_n - 1
        gx_c = PEEK(#gx_p + gx_i) AND 255
        IF gx_c >= 97 AND gx_c <= 122 THEN gx_c = gx_c - 32
        POKE (SC_HDR + gx_i), gx_c
    NEXT gx_i
    POKE (SC_HDR + gx_n), 0
END

' ---------------------------------------------------------------------------
' gc_layout_from_header: recover numW from the header's '#' and derive every
' column offset from it.
'
' The offsets mirror how format_index_human() builds a row:
'   marker(1) sp            -> 2
'   rjust(num, numW) sp     -> numW + 1
'   [ljust(date, dateW) sp] -> dateW + 1, omitted entirely when dateW is 0
'   ljust(time, 11)         -> 11
' which is exactly the adapter's own `used`. The category then follows one
' space later, and the title one space after its 14 columns.
' ---------------------------------------------------------------------------
gc_layout_from_header: PROCEDURE
    gc_numw = 1
    FOR gx_i = 0 TO gl_len - 1
        IF (PEEK(#gx_p + gx_i) AND 255) = 35 THEN
            gc_numw = gx_i - 1
            EXIT FOR
        END IF
    NEXT gx_i
    IF gc_numw < 1 THEN gc_numw = 1

    IF gc_view = VIEW_DAY THEN
        gc_datew = 0
    ELSEIF gc_view = VIEW_WEEK THEN
        gc_datew = 3
    ELSEIF gc_view = VIEW_MONTH THEN
        gc_datew = 5
    ELSE
        gc_datew = 6
    END IF

    gc_used = 2 + gc_numw + 1 + GC_TIMEW
    IF gc_datew > 0 THEN gc_used = gc_used + gc_datew + 1
    gc_timec = gc_used - GC_TIMEW
    gc_catc = gc_used + 1
    gc_titlec = gc_catc + GC_CATW + 1
END

' ---------------------------------------------------------------------------
' gc_take_event: turn one event row into an SC_EVT record plus a title.
'
' Flags accumulate in ge_flags, not in gx_v: gc_parse_time and gc_read_num
' both write gx_v, so a partially built value held there would not survive the
' first GOSUB.
' ---------------------------------------------------------------------------
gc_take_event: PROCEDURE
    ' MONTH only ever needs per-day counts and a colour, so it tallies into a
    ' 42-byte array instead of storing events. That is what lets a 300-event
    ' month cost 42 bytes rather than overrunning MAX_EVENTS.
    IF gc_view = VIEW_MONTH THEN
        GOSUB gc_tally_month
        RETURN
    END IF

    IF ev_count >= MAX_EVENTS THEN
        gc_trunc = 1
        RETURN
    END IF

    #gx_q = SC_EVT + ev_count * EVT_STRIDE

    ' --- flags, from the marker column ---
    ge_flags = 0
    gx_c = PEEK(#gx_p) AND 255
    IF gx_c = 42 THEN ge_flags = ge_flags + EVF_ALLDAY        ' '*'
    IF gx_c = 126 THEN ge_flags = ge_flags + EVF_RECURRING    ' '~'

    ' --- times ---
    GOSUB gc_parse_time
    IF gc_open_end THEN ge_flags = ge_flags + EVF_OPENEND
    IF gc_all_day THEN
        IF (ge_flags AND EVF_ALLDAY) = 0 THEN ge_flags = ge_flags + EVF_ALLDAY
    END IF
    POKE (#gx_q + EVT_FLAGS), ge_flags
    POKE (#gx_q + EVT_SH), gc_sh
    POKE (#gx_q + EVT_SM), gc_sm
    POKE (#gx_q + EVT_EH), gc_eh
    POKE (#gx_q + EVT_EM), gc_em

    ' --- event number, for the detail fetch. Up to three digits, so this is
    ' the one field that genuinely needs 16 bits. ---
    gx_col = 2 : gx_n = gc_numw : GOSUB gc_read_num
    POKE (#gx_q + EVT_NUMLO), #gx_num AND 255
    POKE (#gx_q + EVT_NUMHI), #gx_num / 256

    ' --- day, and month for AGENDA ---
    GOSUB gc_parse_day

    ' --- colour, from the category column ---
    GOSUB gc_colour
    POKE (#gx_q + EVT_COLOR), gx_v

    ' --- title ---
    GOSUB gc_take_title

    ev_count = ev_count + 1
END

' ---------------------------------------------------------------------------
' gc_read_num: read gx_n characters at column gx_col as a decimal number into
' #gx_num. The field is right-justified and space-padded, so non-digits are
' skipped rather than treated as zeroes.
' ---------------------------------------------------------------------------
gc_read_num: PROCEDURE
    #gx_num = 0
    FOR gx_j = 0 TO gx_n - 1
        gx_c = PEEK(#gx_p + gx_col + gx_j) AND 255
        IF gx_c >= 48 AND gx_c <= 57 THEN #gx_num = #gx_num * 10 + (gx_c - 48)
    NEXT gx_j
END

' ---------------------------------------------------------------------------
' gc_parse_time: read the 11-column time field.
'
' Three literal forms, and the field is never empty:
'   "all day"      -> gc_all_day
'   "HH:MM->"      -> starts today, ends on a later day
'   "HH:MM-HH:MM"  -> both ends today
' Distinguished by whether column 0 and column 6 of the field hold digits.
' ---------------------------------------------------------------------------
gc_parse_time: PROCEDURE
    gc_sh = 0 : gc_sm = 0 : gc_eh = 0 : gc_em = 0
    gc_all_day = 0 : gc_open_end = 0

    gx_c = PEEK(#gx_p + gc_timec) AND 255
    IF gx_c < 48 OR gx_c > 57 THEN
        gc_all_day = 1
        RETURN
    END IF

    gx_col = gc_timec + 0 : gx_n = 2 : GOSUB gc_read_num : gc_sh = #gx_num
    gx_col = gc_timec + 3 : gx_n = 2 : GOSUB gc_read_num : gc_sm = #gx_num

    gx_c = PEEK(#gx_p + gc_timec + 6) AND 255
    IF gx_c < 48 OR gx_c > 57 THEN
        gc_open_end = 1
        gc_eh = gc_sh : gc_em = gc_sm
        RETURN
    END IF
    gx_col = gc_timec + 6 : gx_n = 2 : GOSUB gc_read_num : gc_eh = #gx_num
    gx_col = gc_timec + 9 : gx_n = 2 : GOSUB gc_read_num : gc_em = #gx_num
END

' ---------------------------------------------------------------------------
' gc_parse_day: fill EVT_DAY (and EVT_MON for AGENDA) from the date column.
'
' The date column starts one space past the number field, so its offset is
' gc_timec - gc_datew - 1 -- expressed that way rather than recomputed from
' numW, so it stays correct if a column ever moves.
' ---------------------------------------------------------------------------
gc_parse_day: PROCEDURE
    IF gc_view = VIEW_DAY THEN
        POKE (#gx_q + EVT_DAY), 0
        RETURN
    END IF

    IF gc_view = VIEW_WEEK THEN
        GOSUB gc_dow_from_text
        POKE (#gx_q + EVT_DAY), gx_v
        RETURN
    END IF

    ' AGENDA: "28 Aug" -- day at offset 0, month name at offset 3.
    gx_col = gc_timec - 7 : gx_n = 2 : GOSUB gc_read_num
    POKE (#gx_q + EVT_DAY), #gx_num AND 255
    GOSUB gc_mon_from_text
    POKE (#gx_q + EVT_MON), gx_v
END

' gc_dow_from_text: gx_v = 0-6 from the three-letter day name. First letters
' collide (Sun/Sat, Tue/Thu); the first two together do not.
gc_dow_from_text: PROCEDURE
    gx_col = gc_timec - 4
    gx_c = PEEK(#gx_p + gx_col) AND 255
    gx_j = PEEK(#gx_p + gx_col + 1) AND 255
    gx_v = 0
    IF gx_c = 83 AND gx_j = 117 THEN gx_v = 0      ' Su(n)
    IF gx_c = 77 THEN gx_v = 1                     ' Mon
    IF gx_c = 84 AND gx_j = 117 THEN gx_v = 2      ' Tu(e)
    IF gx_c = 87 THEN gx_v = 3                     ' Wed
    IF gx_c = 84 AND gx_j = 104 THEN gx_v = 4      ' Th(u)
    IF gx_c = 70 THEN gx_v = 5                     ' Fri
    IF gx_c = 83 AND gx_j = 97 THEN gx_v = 6       ' Sa(t)
END

' gc_mon_from_text: gx_v = 1-12 from the three-letter month name three columns
' into an AGENDA date field. Jun and Jul share their first two letters, so the
' third settles those; every other pair separates by the second.
gc_mon_from_text: PROCEDURE
    gx_col = gc_timec - 4
    gx_c = PEEK(#gx_p + gx_col) AND 255
    gx_j = PEEK(#gx_p + gx_col + 1) AND 255
    gx_n = PEEK(#gx_p + gx_col + 2) AND 255
    gx_v = 1
    IF gx_c = 74 AND gx_j = 97 THEN gx_v = 1                  ' Jan
    IF gx_c = 70 THEN gx_v = 2                                ' Feb
    IF gx_c = 77 AND gx_n = 114 THEN gx_v = 3                 ' Mar
    IF gx_c = 65 AND gx_j = 112 THEN gx_v = 4                 ' Apr
    IF gx_c = 77 AND gx_n = 121 THEN gx_v = 5                 ' May
    IF gx_c = 74 AND gx_j = 117 AND gx_n = 110 THEN gx_v = 6  ' Jun
    IF gx_c = 74 AND gx_j = 117 AND gx_n = 108 THEN gx_v = 7  ' Jul
    IF gx_c = 65 AND gx_j = 117 THEN gx_v = 8                 ' Aug
    IF gx_c = 83 THEN gx_v = 9                                ' Sep
    IF gx_c = 79 THEN gx_v = 10                               ' Oct
    IF gx_c = 78 THEN gx_v = 11                               ' Nov
    IF gx_c = 68 THEN gx_v = 12                               ' Dec
END

' ---------------------------------------------------------------------------
' gc_colour: gx_v = the Intellivision colour for this row's category column.
'
' The adapter does not expose Google's numeric colorId. category_for() folds it
' in as a NAME instead, third in precedence behind extendedProperties and ahead
' of the calendar's own name -- so a category matching one of the eleven colour
' names IS the event colour, and anything else is a calendar name and takes the
' default.
'
' Names are compared whole and case-insensitively. Prefixes are not enough:
' Grape and Graphite first differ at index 4, Banana / Basil / Blueberry at
' index 1-2. A match also requires the column to end where the name does,
' otherwise "Tomato Soup" would match "Tomato".
' ---------------------------------------------------------------------------
gc_colour: PROCEDURE
    FOR gx_i = 0 TO GC_NCOLORS - 1
        gx_j = 0
gcc_cmp:
        gx_n = PEEK(VARPTR lit_colnames(0) + gx_i * COLNAME_STRIDE + gx_j) AND 255
        gx_c = PEEK(#gx_p + gc_catc + gx_j) AND 255
        IF gx_c >= 97 AND gx_c <= 122 THEN gx_c = gx_c - 32
        IF gx_n = 0 THEN
            IF gx_c = 32 OR gx_c = 0 OR gc_catc + gx_j >= gl_len THEN
                gx_v = lit_colvals(gx_i)
                RETURN
            END IF
            GOTO gcc_next
        END IF
        IF gx_c <> gx_n THEN GOTO gcc_next
        gx_j = gx_j + 1
        IF gx_j < COLNAME_STRIDE THEN GOTO gcc_cmp
gcc_next:
    NEXT gx_i
    gx_v = COLOR_DEFAULT
END

' ---------------------------------------------------------------------------
' gc_take_title: copy the title tail into SC_TITLE, upper-cased and clipped to
' TITLE_STRIDE-1 characters. The title is the last field and is not padded, so
' it runs to the end of the line.
' ---------------------------------------------------------------------------
gc_take_title: PROCEDURE
    gt_len = gl_len - gc_titlec
    IF gt_len > TITLE_STRIDE - 1 THEN gt_len = TITLE_STRIDE - 1
    IF gt_len = 0 THEN
        POKE (SC_TITLE + ev_count * TITLE_STRIDE), 0
        POKE (#gx_q + EVT_TLEN), 0
        RETURN
    END IF
    FOR gx_j = 0 TO gt_len - 1
        gx_c = PEEK(#gx_p + gc_titlec + gx_j) AND 255
        IF gx_c >= 97 AND gx_c <= 122 THEN gx_c = gx_c - 32
        IF gx_c < 32 OR gx_c > 126 THEN gx_c = 32
        POKE (SC_TITLE + ev_count * TITLE_STRIDE + gx_j), gx_c
    NEXT gx_j
    POKE (SC_TITLE + ev_count * TITLE_STRIDE + gt_len), 0
    POKE (#gx_q + EVT_TLEN), gt_len
END

' ---------------------------------------------------------------------------
' gc_tally_month: MONTH view. Bump the count for this row's day of month and
' remember the colour of its FIRST event. The adapter sorts by start time, so
' "first" is the day's leading event -- the most useful single colour to show
' in a two-character grid cell.
'
' gm_day rather than a gx_ name: gc_colour below reuses the whole leaf set.
' ---------------------------------------------------------------------------
gc_tally_month: PROCEDURE
    ' "Fr 28": the day number is three characters into the date field.
    gx_col = gc_timec - 3 : gx_n = 2 : GOSUB gc_read_num
    IF #gx_num < 1 OR #gx_num > 31 THEN RETURN
    gm_day = #gx_num
    IF (PEEK(SC_DAYS + gm_day) AND 255) < 255 THEN
        POKE (SC_DAYS + gm_day), (PEEK(SC_DAYS + gm_day) AND 255) + 1
    END IF
    IF (PEEK(SC_DAYS + gm_day) AND 255) = 1 THEN
        GOSUB gc_colour
        POKE (SC_DCOL + gm_day), gx_v
    END IF
    ev_count = ev_count + 1
END
