' bar.bas -- the colour-stack selection bar, from fujinet-config/intv's
' csbar.bas.
'
' WHY THE BAR IS BACKGROUND AND NOT A COLOUR CHANGE
'
' #BACKTAB is the LIVE STIC display list at $0200. IntyBASIC keeps no shadow
' copy and nothing is double-buffered, so a repaint is scanned out as it
' happens. An NTSC frame with the display enabled leaves the CPU about 13518
' cycles, and one IntyBASIC BACKTAB read-modify-write costs roughly 264 --
' every #BACKTAB(expr) re-runs a multiply-by-20 index computation, once per
' side of the assignment. That is a budget of about fifty cells per frame, and
' a full seven-row event list is over 140. Repainting on every disc press was
' visibly torn in the config client.
'
' Here the bar is nothing but two colour-stack advance bits, so moving the
' selection is FOUR BACKTAB writes regardless of how much text is on screen,
' and the glyphs and their colours are never touched at all.
'
' THE STACK
'
' The stack holds four background entries, resets to position 0 at the top of
' every frame, and only ever advances forward, wrapping 3->0. In raster order a
' view is  header / content / selected / content / hint  -- five runs, and the
' fifth wraps neatly onto position 0, so gcal.bas programs it as
'
'     MODE 0, CS_BLUE, CS_BLACK, CS_DARKGREEN, CS_BLACK
'              p0        p1        p2            p3
'
' That only holds while BOTH content runs are non-empty, which is why two
' things about the layout in constants.bas are not negotiable:
'
'   * The bar spans columns 1-19 only. Column 0 keeps the content colour on
'     every row, selected or not, and is the non-empty content run when the TOP
'     row is selected. It is also exactly where the event colour chip goes --
'     the constraint and the feature are the same cell.
'   * ROW_SPACER is permanently blank, and is the non-empty content run when
'     the BOTTOM row is selected.

    DIM bar_row

' ---------------------------------------------------------------------------
' bar_apply: stamp all four advance bits for a screen just drawn from scratch.
' Safe as a single pass only because the caller's CLS zeroed every word first
' -- PRINT and scr_puts both write a bare card*8+colour and would otherwise
' have wiped these.
'
'   A1 (ROW_FIRST, 0)     header   -> content
'   A2 (SEL, 1)           content  -> selected
'   A3 (SEL+1, 0)         selected -> content
'   A4 (ROW_HINT, 0)      content  -> header
'
' All four go down even when the list is empty. The COUNT matters as much as
' the positions: skipping the bar's pair would leave the hint row sitting on p2
' instead of wrapping onto p0, turning it the wrong colour. With nothing to
' highlight the pair parks on the blank spacer row, which costs one visible
' cell at (ROW_SPACER,18) and hides the other on column 19.
' ---------------------------------------------------------------------------
bar_apply: PROCEDURE
    #BACKTAB(ROW_FIRST * SCREEN_COLS) = #BACKTAB(ROW_FIRST * SCREEN_COLS) OR CS_ADVANCE
    IF num_rows > 0 THEN
        GOSUB bar_set
    ELSE
        #BACKTAB(ROW_SPACER * SCREEN_COLS + 18) = #BACKTAB(ROW_SPACER * SCREEN_COLS + 18) OR CS_ADVANCE
        #BACKTAB(ROW_SPACER * SCREEN_COLS + 19) = #BACKTAB(ROW_SPACER * SCREEN_COLS + 19) OR CS_ADVANCE
    END IF
    #BACKTAB(ROW_HINT * SCREEN_COLS) = #BACKTAB(ROW_HINT * SCREEN_COLS) OR CS_ADVANCE
END

' ---------------------------------------------------------------------------
' bar_set / bar_clr: add or remove the bar's two advance bits for whichever
' row sel_row names. Only bit 13 moves; the glyphs and colours underneath are
' untouched.
' ---------------------------------------------------------------------------
bar_set: PROCEDURE
    bar_row = ROW_FIRST + sel_row
    #BACKTAB(bar_row * SCREEN_COLS + 1) = #BACKTAB(bar_row * SCREEN_COLS + 1) OR CS_ADVANCE
    #BACKTAB((bar_row + 1) * SCREEN_COLS) = #BACKTAB((bar_row + 1) * SCREEN_COLS) OR CS_ADVANCE
END

bar_clr: PROCEDURE
    bar_row = ROW_FIRST + sel_row
    #BACKTAB(bar_row * SCREEN_COLS + 1) = #BACKTAB(bar_row * SCREEN_COLS + 1) AND $DFFF
    #BACKTAB((bar_row + 1) * SCREEN_COLS) = #BACKTAB((bar_row + 1) * SCREEN_COLS) AND $DFFF
END

' ---------------------------------------------------------------------------
' bar_move: move the selection to bar_new.
'
' The four advance-bit stores happen FIRST and back to back, before any
' cosmetic work. Between bar_clr and bar_set the screen carries only two of its
' four advance bits, and every row below the bar then renders one stack
' position out of phase -- a whole-screen colour shift, not a local glitch. Any
' frame that lands in that gap shows it.
' ---------------------------------------------------------------------------
    DIM bar_new, bar_old
bar_move: PROCEDURE
    IF bar_new = sel_row THEN RETURN
    GOSUB bar_clr
    bar_old = sel_row
    sel_row = bar_new
    GOSUB bar_set

    ' Cosmetic half: stop the row being left from scrolling, and point the
    ' scroller at the newly selected one. Safe to straddle a frame -- the
    ' stack is already consistent.
    '
    ' #sc_adv stays 0 for BOTH rows, unlike the file browser this came from.
    ' There the bar's advance bit lived in the first cell scroll_draw
    ' repainted, so the scroller had to put it back or the bar collapsed a
    ' second after the cursor landed. Here the bar sits at column 1 and the
    ' scroller only ever touches TITLE_COL onward, so it cannot disturb it.
    #sc_adv = 0
    sc_row = ROW_FIRST + bar_old : sc_col = TITLE_COL
    sc_max = TITLE_W : sc_color = COL_NORMAL
    GOSUB scroll_reset
    sc_row = ROW_FIRST + sel_row : sc_col = TITLE_COL
    sc_max = TITLE_W : sc_color = COL_NORMAL
END

' ---------------------------------------------------------------------------
' bar_rearm: re-stamp A4 after anything has printed over the hint row. Every
' message there starts at column 0 and so wipes the advance bit that makes the
' row the header colour; without this the whole bar drops to the content
' colour. Called at each print site rather than once per frame, so a message
' shown during a blocking fetch is right for the whole time it is up.
' ---------------------------------------------------------------------------
bar_rearm: PROCEDURE
    #BACKTAB(ROW_HINT * SCREEN_COLS) = #BACKTAB(ROW_HINT * SCREEN_COLS) OR CS_ADVANCE
END

' ---------------------------------------------------------------------------
' THE FORM'S BAR
'
' st_form.bas gets its own three routines rather than reusing bar_set/bar_clr,
' for two reasons.
'
' First, ownership: those key off sel_row, which is the VIEW's selection and
' has to survive a round trip through the form -- you edit the selected event
' and come back to it. The form's selection is fm_sel.
'
' Second, geometry. The views put content on rows 3-9 with row 10 a permanent
' blank spacer, so bar_set can end the selected run at column 0 of the NEXT
' row. The form cannot: t9_entry owns rows 0-3, which puts the seven fields on
' rows 4-10, and the last of them is therefore hard against the hint row --
' whose own advance bit is already at (11,0). Ending the run at column 19 of
' the SAME row instead sidesteps that collision without a special case, at the
' cost of one column of bar. Both content runs then stay non-empty in every
' position: row 3 is always above the bar, cell (selrow,19) always below it.
'
'   A1 (3, 0)               header   -> content   (row 3 is the T9 strip)
'   A2 (FRM_ROW0+fm_sel, 1) content  -> selected
'   A3 (FRM_ROW0+fm_sel,19) selected -> content
'   A4 (ROW_HINT, 0)        content  -> header, wrapping 3->0
'
' The bar therefore spans columns 1-18.
' ---------------------------------------------------------------------------
frm_bar_set: PROCEDURE
    bar_row = FRM_ROW0 + fm_sel
    #BACKTAB(bar_row * SCREEN_COLS + 1) = #BACKTAB(bar_row * SCREEN_COLS + 1) OR CS_ADVANCE
    #BACKTAB(bar_row * SCREEN_COLS + 19) = #BACKTAB(bar_row * SCREEN_COLS + 19) OR CS_ADVANCE
END

frm_bar_clr: PROCEDURE
    bar_row = FRM_ROW0 + fm_sel
    #BACKTAB(bar_row * SCREEN_COLS + 1) = #BACKTAB(bar_row * SCREEN_COLS + 1) AND $DFFF
    #BACKTAB(bar_row * SCREEN_COLS + 19) = #BACKTAB(bar_row * SCREEN_COLS + 19) AND $DFFF
END

' frm_bar_apply: all four bits, for a form just drawn from scratch. frm_draw's
' scr_clear is a CLS, which zeroes every BACKTAB word -- bit 13 included -- and
' scr_puts/vw_puts_lit then write a bare card*8+colour over the top, so nothing
' survives a repaint and the whole set has to go back down. The COUNT matters
' as much as the positions: drop one and every row below it renders a stack
' position out of phase.
frm_bar_apply: PROCEDURE
    #BACKTAB(T9_STRIP_ROW * SCREEN_COLS) = #BACKTAB(T9_STRIP_ROW * SCREEN_COLS) OR CS_ADVANCE
    GOSUB frm_bar_set
    #BACKTAB(ROW_HINT * SCREEN_COLS) = #BACKTAB(ROW_HINT * SCREEN_COLS) OR CS_ADVANCE
END
