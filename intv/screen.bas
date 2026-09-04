' screen.bas -- low-level text drawing helpers shared by every screen.
'
' Character encoding follows the standard IntyBASIC card formula used
' throughout the FujiNet Intellivision tree (fujitest.bas, 5cardstud):
'     card = ascii - 32   (clamped to a printable placeholder if out of range)
'     screen word = card*8 + color
' Nothing here ever buffers a whole string in an IntyBASIC variable -- text
' is always drawn straight from a source address (ROM DATA or scratch RAM)
' via PEEK, one character at a time, per the RAM budget rule in constants.bas.

    CONST COL_NORMAL   = CS_WHITE
    CONST COL_HILIGHT  = CS_YELLOW
    CONST COL_DIM      = CS_BLUE
    CONST COL_ERROR    = CS_RED
    CONST COL_VALUE    = CS_TAN
    CONST COL_CURSOR   = CS_GREEN
' COL_HEAD is for the column-heading row of a view. It cannot be COL_DIM:
' rows 0-2 sit on colour-stack entry p0, which the views set to CS_BLUE, so
' blue-on-blue text there is invisible. Tan reads clearly against it and
' against the black content run, and a GROM card can express it (foreground
' is limited to 0-7 on GROM cards in colour-stack mode).
    CONST COL_HEAD     = CS_TAN

    DIM s_row, s_col, s_i, s_c, s_len, s_max, s_col_color
    DIM #s_src, #s_val
' No #s_bg here. It carried FGBG background bits for a scr_fgbg_row that
' came from fgbg.bas, which is not in this tree -- and the 16-bit pool is
' the binding constraint now that t9.bas claims eleven of its own.

' ---------------------------------------------------------------------------
' The text-entry value window, ported from kbd.bas (netcat/intv, itself from
' fujinet-config/intv). These four names are the whole of what t9.bas needs
' from that module; grid_entry and its 6x16 charset renderer are deliberately
' NOT here -- T9 replaces them, and IntyBASIC compiles included code whether
' it is reachable or not.
'
' The contract both editors share: the caller sets #ge_dst (a cart-RAM
' buffer) and #g_max (its size INCLUDING the NUL), primes the buffer -- a NUL
' at offset 0 for a fresh field, or existing NUL-terminated text to edit in
' place -- and reads g_len back afterwards.
'
' #g_max must stay 16-bit: at 8 bits, 256 wraps to 0 and g_ent_append's
' overflow guard stops guarding.
'
' g_i/g_c rather than screen.bas's own s_i/s_c because these run underneath
' t9_entry, which callers reach from st_form.bas while it is mid-way through
' its own scr_puts loop -- sharing the scratch would corrupt the field list.
    DIM g_len, g_ch, g_i, g_c, g_cc
    DIM #ge_dst, #g_max

' ---------------------------------------------------------------------------
' scr_clear: blank the whole 20x12 screen.
' ---------------------------------------------------------------------------
scr_clear: PROCEDURE
    CLS
END

' ---------------------------------------------------------------------------
' scr_row_clear: blank row s_row (all 20 columns).
' ---------------------------------------------------------------------------
scr_row_clear: PROCEDURE
    FOR s_i = 0 TO SCREEN_COLS - 1
        #BACKTAB(s_row * SCREEN_COLS + s_i) = CS_BLACK
    NEXT s_i
END

' ---------------------------------------------------------------------------
' scr_puts: draw bytes from #s_src onto row s_row starting at column s_col,
' in color s_col_color, stopping at the first NUL or after s_max characters,
' then space-padding the remainder of the s_max-wide field. Sets s_len to
' the number of real (non-pad) characters drawn. Caller ensures
' s_col + s_max <= SCREEN_COLS.
' ---------------------------------------------------------------------------
scr_puts: PROCEDURE
    s_len = 0
    WHILE (s_len < s_max) AND ((PEEK(#s_src + s_len) AND 255) <> 0)
        s_len = s_len + 1
    WEND
    FOR s_i = 0 TO s_max - 1
        IF s_i < s_len THEN
            s_c = PEEK(#s_src + s_i) AND 255
        ELSE
            s_c = 32
        END IF
        IF s_c < 32 OR s_c > 126 THEN s_c = 32
        #BACKTAB(s_row * SCREEN_COLS + s_col + s_i) = (s_c - 32) * 8 + s_col_color
    NEXT s_i
END
' ---------------------------------------------------------------------------
' scr_hilite_digits: recolor every ASCII digit already drawn on row s_row to
' s_col_color, leaving every other cell alone. Used on the key-hint footers
' so the key you press reads at a glance against its label -- IntyBASIC's
' PRINT applies one color to a whole literal, and splitting each hint into
' per-colored PRINT AT fragments would cost far more ROM than one pass over
' the row.
'
' Cards are ASCII-32 (see the header), so '0'-'9' are cards 16-25, and the
' screen word is card*8 + color -- hence the /8 to recover the card. Only
' call this on rows whose digits are all key names; a row showing arbitrary
' text (a filter, a filename) would get its digits highlighted too.
' ---------------------------------------------------------------------------
scr_hilite_digits: PROCEDURE
    FOR s_i = 0 TO SCREEN_COLS - 1
        #s_val = #BACKTAB(s_row * SCREEN_COLS + s_i)
        s_c = (#s_val / 8) AND 255
        IF s_c >= 16 AND s_c <= 25 THEN
            #BACKTAB(s_row * SCREEN_COLS + s_i) = (#s_val AND $FFF8) + s_col_color
        END IF
    NEXT s_i
END
' ---------------------------------------------------------------------------
' g_ent_draw: tail-anchored VAL_CELLS-cell window (rows 0-2) onto #ge_dst,
' with a trailing cursor block. A value longer than the window scrolls: what
' is shown is always the tail, where the typing is happening.
' ---------------------------------------------------------------------------
g_ent_draw: PROCEDURE
    g_i = 0
    IF g_len > VAL_CELLS - 1 THEN g_i = g_len - (VAL_CELLS - 1)
    FOR g_c = 0 TO VAL_CELLS - 1
        g_cc = 32
        IF g_i + g_c < g_len THEN g_cc = PEEK(#ge_dst + g_i + g_c) AND 255
        IF g_cc < 32 OR g_cc > 126 THEN g_cc = 32
        #BACKTAB(VAL_ROW0 * SCREEN_COLS + g_c) = (g_cc - 32) * 8 + COL_VALUE
    NEXT g_c
    IF g_len - g_i < VAL_CELLS THEN
        #BACKTAB(VAL_ROW0 * SCREEN_COLS + (g_len - g_i)) = (95 - 32) * 8 + COL_CURSOR
    END IF
END

' ---------------------------------------------------------------------------
' g_ent_append: append g_ch, honouring the #g_max ceiling. Silent on a full
' buffer -- t9.bas checks the ceiling itself first so it can flash instead.
' ---------------------------------------------------------------------------
g_ent_append: PROCEDURE
    IF g_len >= #g_max - 1 THEN RETURN
    POKE (#ge_dst + g_len), g_ch
    g_len = g_len + 1
    POKE (#ge_dst + g_len), 0
    GOSUB g_ent_draw
END

' ---------------------------------------------------------------------------
' g_ent_backspace
' ---------------------------------------------------------------------------
g_ent_backspace: PROCEDURE
    IF g_len = 0 THEN RETURN
    g_len = g_len - 1
    POKE (#ge_dst + g_len), 0
    GOSUB g_ent_draw
END
