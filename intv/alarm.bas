' alarm.bas -- upcoming-event notifications.
'
' WHY THESE ARE SYNTHESISED
'
' Google's own per-event reminder times never reach the console. GCAL.cpp asks
' the Calendar API for
'
'   fields=nextPageToken,items(id,summary,location,status,colorId,
'          recurringEventId,start,end,extendedProperties)
'
' and `reminders` is not in that mask, so no override minutes and no default
' reminder are in the reply at any output width. Rather than change the
' firmware, the alarm is generated here: an event fires once, al_lead minutes
' before it starts. al_lead is settable on the setup screen and persisted in an
' AppKey, so it survives a power cycle.
'
' WHAT CAN FIRE
'
' Only events in the DAY view's index, and only when that view is anchored on
' today. The other views either do not store per-event times at all (MONTH
' tallies counts) or span days the wall clock cannot be compared against
' without dates the columns do not carry. Anchoring the check on "the loaded
' day is today" keeps it honest instead of firing on a day the user merely
' happens to be browsing.
'
' EVF_FIRED is set on the record once an event has sounded, so re-entering the
' minute does not retrigger it. A re-fetch clears the whole index and therefore
' the flags -- which is correct: after a refresh the event genuinely is news
' again only if it is still inside the lead window, and al_last stops it
' repeating within the same minute.
'
' SOUND, not PLAY: the tracker would claim 28 of the 228 8-bit variables and
' cut the per-frame GRAM budget, for one three-note chime.

    CONST AL_LEAD_DEFAULT = 10
    CONST AL_LEAD_MAX     = 60
' 240, not 300: al_frames is an 8-bit variable, and a 16-bit one would cost a
' slot from a much scarcer pool for the sake of one extra second of banner.
    CONST AL_BANNER_FRAMES = 240   ' ~4s at 60Hz
    CONST AL_CHIME_STEP    = 8     ' frames per chime note

' al_active is DIM'd in constants.bas -- st_view.bas reads it, and this file
' is included after that one.
    DIM al_lead, al_frames, al_note, al_ev
    DIM al_i, al_now, al_evm, al_flags

' ---------------------------------------------------------------------------
' al_init: called once at boot, after the AppKey read has had its say.
' ---------------------------------------------------------------------------
al_init: PROCEDURE
    IF al_lead = 0 OR al_lead > AL_LEAD_MAX THEN al_lead = AL_LEAD_DEFAULT
    al_active = 0
    al_frames = 0
END

' ---------------------------------------------------------------------------
' al_scan: called once per frame. Fires at most one alarm per call.
' ---------------------------------------------------------------------------
al_scan: PROCEDURE
    IF al_active <> 0 THEN
        GOSUB al_run
        RETURN
    END IF
    IF clk_ok = 0 THEN RETURN
    IF ev_count = 0 THEN RETURN
    ' New alarms only start on a calendar view. The banner borrows the hint
    ' row, and only a view knows how to paint that row back; on the detail or
    ' settings screens it would be left stranded. Nothing is lost by waiting --
    ' EVF_FIRED is not set until it actually sounds, so an event still inside
    ' the lead window fires as soon as the user is back on a view.
    IF state <> ST_VIEW THEN RETURN
    IF cur_view <> VIEW_DAY THEN RETURN
    ' Only when the loaded day IS today.
    IF #cur_y <> #clk_y OR cur_mo <> clk_mo OR cur_d <> clk_d THEN RETURN

    al_now = clk_h * 60 + clk_mi

    FOR al_i = 0 TO ev_count - 1
        #evrec = SC_EVT + al_i * EVT_STRIDE
        al_flags = PEEK(#evrec + EVT_FLAGS) AND 255
        IF (al_flags AND EVF_FIRED) = 0 THEN
            ' All-day events have no meaningful start minute to count down to.
            IF (al_flags AND EVF_ALLDAY) = 0 THEN
                al_evm = (PEEK(#evrec + EVT_SH) AND 255) * 60 + (PEEK(#evrec + EVT_SM) AND 255)
                IF al_evm >= al_now THEN
                    IF al_evm - al_now <= al_lead THEN
                        al_ev = al_i
                        POKE (#evrec + EVT_FLAGS), al_flags + EVF_FIRED
                        GOSUB al_fire
                        RETURN
                    END IF
                END IF
            END IF
        END IF
    NEXT al_i
END

' ---------------------------------------------------------------------------
' al_fire: start the banner and the chime.
' ---------------------------------------------------------------------------
al_fire: PROCEDURE
    al_active = 1
    al_frames = AL_BANNER_FRAMES
    al_note = 0
END

' ---------------------------------------------------------------------------
' al_run: one frame of an active alarm -- banner, bell, chime.
'
' The banner takes over the hint row, which means it also wipes that row's
' colour-stack advance bit; bar_rearm puts it back so the rest of the screen
' does not shift a stack position.
' ---------------------------------------------------------------------------
al_run: PROCEDURE
    #evrec = SC_EVT + al_ev * EVT_STRIDE

    ' Flash by alternating the banner colour every 16 frames.
    s_row = ROW_HINT
    IF (al_frames AND 16) <> 0 THEN
        s_col_color = COL_HILIGHT
    ELSE
        s_col_color = COL_ERROR
    END IF
    #BACKTAB(ROW_HINT * SCREEN_COLS) = CS_BLACK
    #s_src = SC_TITLE + al_ev * TITLE_STRIDE
    s_col = 2 : s_max = SCREEN_COLS - 2
    GOSUB scr_puts
    #BACKTAB(ROW_HINT * SCREEN_COLS + 1) = GLYPH_BELL * 8 + GRAM_SELECT + gramfg(CS_YELLOW)
    GOSUB bar_rearm

    ' A three-note rising chime, then silence for the rest of the banner.
    IF al_note < 3 * AL_CHIME_STEP THEN
        al_i = al_note / AL_CHIME_STEP
        IF al_i = 0 THEN SOUND 0, 500, 14
        IF al_i = 1 THEN SOUND 0, 400, 14
        IF al_i = 2 THEN SOUND 0, 315, 14
        al_note = al_note + 1
        IF al_note >= 3 * AL_CHIME_STEP THEN SOUND 0, 1, 0
    END IF

    al_frames = al_frames - 1
    IF al_frames = 0 THEN
        SOUND 0, 1, 0
        SOUND 1, 1, 0
        al_active = 0
        ' Hand the row back to whatever owns the screen.
        IF state = ST_VIEW THEN GOSUB vw_hints
    END IF
END

' ---------------------------------------------------------------------------
' al_dismiss: any button press clears a banner early.
' ---------------------------------------------------------------------------
al_dismiss: PROCEDURE
    IF al_active = 0 THEN RETURN
    al_frames = 1
    GOSUB al_run
END
