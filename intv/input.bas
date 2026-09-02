' input.bas -- edge-detected controller input, shared by every screen.
'
' Reads the unqualified CONT, which merges both controller ports -- this is a
' single-player utility, so there's no reason to force player 1's jack
' specifically -- and decodes the disc, the buttons and the keypad from that
' one byte. CONT.UP/DOWN/LEFT/RIGHT, CONT.B0/B1/B2 and CONT.KEY are all
' deliberately unused; each of them gets one of the three wrong, and the
' comments below say how.
'
' in_poll() populates, once per call:
'   in_disc  - DISC_UP/DOWN/LEFT/RIGHT on a fresh press or an auto-repeat
'              tick while held; 0 otherwise.
'   in_btn   - 1 on a fresh action-button press (any of B0/B1/B2); 0 otherwise.
'   in_key   - the decoded keypad digit/CLEAR/ENTER on a fresh press, or
'              KEYPAD_NONE (12) if nothing new. Decoded here from the raw
'              byte rather than read from CONT.KEY -- see below.

    CONST IN_REPEAT_DELAY = 18   ' frames held before auto-repeat kicks in (~0.3s)
    CONST IN_REPEAT_RATE  = 6    ' frames between repeats once repeating (~0.1s)
    CONST IN_BTN_CONFIRM  = 3    ' frames a button pattern must hold (see below)
    CONST IN_KEY_CONFIRM  = 3    ' frames a keypad byte must hold (see below)

    DIM in_disc, in_pdisc, in_rdelay
    DIM in_braw, in_btn,  in_pbtn, in_bcnt
    DIM in_key,  in_kraw, in_kcnt, in_hold, in_kb, in_ki
    DIM in_raw,  in_inj

' ---------------------------------------------------------------------------
' THE DISC AND THE KEYPAD ARE THE SAME EIGHT LINES
'
' A hand controller reports one byte. The keypad grounds one ROW line (bits
' 5-7) and one COLUMN line (bits 0-3); the disc reports on bits 0-3; the three
' action buttons ground TWO row lines at once. So a keypad press and a disc
' direction are not distinguishable by the column bits alone -- every keypad
' press also reads as a disc direction:
'
'   1 $81   2 $41   3 $21   -> bit 0, reads as DOWN
'   4 $82   5 $42   6 $22   -> bit 1, reads as RIGHT
'   7 $84   8 $44   9 $24   -> bit 2, reads as UP
'   0 $48   CLEAR $88   ENTER $28 -> bit 3, reads as LEFT
'
' Reading CONT.UP/DOWN/LEFT/RIGHT as bare bit tests -- which is what they are,
' `value AND 1/2/4/8` -- therefore fires the disc on every keypress, and that
' is not cosmetic here. Keypad 4, 5 and 6 read as RIGHT, which steps the view
' to the next period and starts a BLOCKING fetch of up to 900 frames. Meanwhile
' IntyBASIC's CONT.KEY only latches a value after three consecutive identical
' frames (see _cnt1_p0/_cnt1_p1 in intybasic_epilogue.asm). The fetch eats
' those frames, the key is released before it ever latches, and the observable
' behaviour is a console whose keypad does nothing but advance the date --
' whatever you press.
'
' The row bits are what tells them apart, so the disc is only believed when
' bits 5-7 are clear. That also means the disc reads neutral while an action
' button is held, which is a real property of the hardware and not a loss here:
' nothing in this app reads the two together.
    CONST CONT_ROWS = $E0        ' bits 5-7: keypad row / button lines
' The three action buttons are these EXACT bytes -- nothing in bits 0-4 -- and
' that is what tells them from a keypad ghost. See the button section below.
    CONST CONT_B0   = $A0
    CONST CONT_B1   = $60
    CONST CONT_B2   = $C0

' The twelve keypad bytes, in KEYPAD_* order: 0-9, CLEAR, ENTER. Same table
' IntyBASIC's epilogue carries for CONT.KEY; this file owns the decode now.
' Data is safe here -- the program's opening GOTO never falls into an include.
in_ktbl:
    DATA $48,$81,$41,$21,$82,$42,$22,$84,$44,$24,$88,$28

' ---------------------------------------------------------------------------
' in_decode: in_kb (one key's worth of bits) -> in_key, or KEYPAD_NONE when
' the bits are not exactly one key -- two keys pressed inside the same confirm
' window, a disc direction, or an action button.
' ---------------------------------------------------------------------------
in_decode: PROCEDURE
    in_key = KEYPAD_NONE
    IF in_kb = 0 THEN RETURN
    in_ki = 0
    WHILE in_ki < 12
        IF in_ktbl(in_ki) = in_kb THEN
            in_key = in_ki
            in_ki = 12
        ELSE
            in_ki = in_ki + 1
        END IF
    WEND
END

' ---------------------------------------------------------------------------
' in_inject: scripted input, the RAW half.
'
' If cart RAM at SC_INJECT holds the magic $A5,$5A, byte 5 -- when non-zero --
' replaces the controller byte for this frame, before anything is decoded from
' it. That is the difference that matters for testing: a scenario can say "the
' console reported $82", which is what the hardware actually sends for keypad
' 4, and the disc gating and button decode below are then genuinely under test.
' Overriding the decoded values instead (bytes 2-4, at the end of in_poll)
' skips all of that, which is exactly why the keypad/disc aliasing bug survived
' a green test suite.
'
' One-shot: consumed and cleared, like the other three.
'
' Cost when the magic is absent, which is every real run: one PEEK a frame.
' gcal.bas clears the byte at boot so uninitialised cart RAM cannot arm it.
' ---------------------------------------------------------------------------
in_inject: PROCEDURE
    IF (PEEK(SC_INJECT) AND 255) = $A5 THEN
        IF (PEEK(SC_INJECT + 1) AND 255) = $5A THEN
            in_inj = PEEK(SC_INJECT + 5) AND 255
            IF in_inj <> 0 THEN
                in_raw = in_inj
                POKE SC_INJECT + 5, 0
            END IF
        END IF
    END IF
END

' ---------------------------------------------------------------------------
' in_poll: call once per frame (after WAIT). Sets in_disc/in_btn/in_key.
' ---------------------------------------------------------------------------
in_poll: PROCEDURE
    ' One read of the whole byte, then everything is decoded from it -- both
    ' because the row bits have to gate the disc (see above) and because the
    ' old form read the port ten times a frame to answer seven questions.
    '
    ' Masked to eight bits at the read rather than at each use: CONT compiles
    ' to a bare MVI $01FE / XOR $01FF with nothing clearing the high half, and
    ' the button test below is an EQUALITY against the whole byte.
    in_raw = CONT AND 255
    GOSUB in_inject

    ' --- disc, with auto-repeat while held in one direction ---
    in_disc = 0
    IF (in_raw AND CONT_ROWS) = 0 THEN
        IF (in_raw AND DISC_UP) <> 0 THEN in_disc = DISC_UP
        IF (in_raw AND DISC_DOWN) <> 0 THEN in_disc = DISC_DOWN
        IF (in_raw AND DISC_LEFT) <> 0 THEN in_disc = DISC_LEFT
        IF (in_raw AND DISC_RIGHT) <> 0 THEN in_disc = DISC_RIGHT
    END IF

    IF in_disc <> 0 THEN
        IF in_disc <> in_pdisc AND in_pdisc = 0 THEN
            in_rdelay = IN_REPEAT_DELAY     ' fresh press from neutral -- fire now
        ELSE
            ' Same direction held, OR slid/wobbled to a neighbour without
            ' releasing. Both go through the repeat gate, and that "OR" is the
            ' point: the disc has 16 positions and the diagonals set TWO
            ' cardinal bits, so ENE and NE read as both UP and RIGHT (jzintv
            ' src/pads/pads.c, "Pad bit 1 is set for SSE through NE / Pad bit 2
            ' is set for ENE through NW"). The tests above are last-match-wins,
            ' so a thumb parked near that boundary alternates in_disc between
            ' DISC_UP and DISC_RIGHT frame to frame. Treating a changed
            ' direction as a fresh press fired the handler every other frame
            ' instead of every seventh, which is what turned the list screens'
            ' redraw into a continuous flicker.
            '
            ' The cost is that changing direction without releasing the disc
            ' waits up to IN_REPEAT_RATE frames (~0.1s) instead of acting at
            ' once. Not perceptible, and it stops the cursor racing.
            IF in_rdelay > 0 THEN
                in_rdelay = in_rdelay - 1
                in_disc = 0
            ELSE
                in_rdelay = IN_REPEAT_RATE
            END IF
        END IF
    END IF
    in_pdisc = 0
    IF (in_raw AND CONT_ROWS) = 0 THEN
        IF (in_raw AND DISC_UP) <> 0 THEN in_pdisc = DISC_UP
        IF (in_raw AND DISC_DOWN) <> 0 THEN in_pdisc = DISC_DOWN
        IF (in_raw AND DISC_LEFT) <> 0 THEN in_pdisc = DISC_LEFT
        IF (in_raw AND DISC_RIGHT) <> 0 THEN in_pdisc = DISC_RIGHT
    END IF

    ' --- action buttons, edge-triggered (no repeat), confirmed over 3 frames ---
    ' A button grounds two ROW lines (bits 5-7) and NOTHING ELSE. A keypad key
    ' grounds one row line and one COLUMN line (bits 0-3); the disc reports on
    ' bits 0-4. So two keys from different rows OR together into a byte whose
    ' row bits are exactly a button's:
    '
    '   1/4/7/CLR ($80) + 3/6/9/ENT ($20)  -> $A_, the row bits of B0
    '   2/5/8/0   ($40) + 3/6/9/ENT ($20)  -> $6_, the row bits of B1
    '   1/4/7/CLR ($80) + 2/5/8/0   ($40)  -> $C_, the row bits of B2
    '
    ' and that is not a two-handed stunt, it is ordinary typing: a finger
    ' rolling off one key onto the next holds both down for a few frames.
    ' jzintv merges every pad input with OR before inverting (src/pads/pads.c);
    ' on hardware, contact bounce and matrix ghosting do the same. 48 of the 66
    ' key pairs land on one of those three patterns, so it happens constantly.
    '
    ' Masking to the row bits and comparing therefore does NOT identify a
    ' button, and a debounce does not rescue it: four frames of overlap -- 67ms,
    ' slower than anyone types -- outlasts any confirm window short enough to
    ' keep the button feeling instant. That is what turned "meeting" into
    ' "offtimi": t9.bas has the T9/ABC toggle on the button, so every ghost
    ' committed the pending word and flipped the mode, and the next digit was
    ' typed in the mode it had just been flipped into.
    '
    ' The blank low bits are the discriminator, and they are exact rather than
    ' statistical: EVERY key sets one of bits 0-3, so no combination of keys
    ' can spell a bare $A0/$60/$C0, and no ghost survives an equality test
    ' against the whole byte. It costs the same three compares, minus the AND.
    '
    ' What it gives up is the disc: bits 0-4 are also where the disc reports,
    ' so a button pressed while the disc is off centre is ignored. That is the
    ' mirror of the gate above, which ignores the disc while a button is held,
    ' and nothing here reads the two together.
    '
    ' IN_BTN_CONFIRM stays for the make/break skew a matrix has on release: two
    ' keys whose column contacts open a frame before their row contacts do
    ' spell a bare button pattern for that frame. Three frames of it is not a
    ' released key, and ~50ms is why the button has never felt slow.
    in_bcnt = in_bcnt + 1
    IF in_raw <> CONT_B0 THEN
        IF in_raw <> CONT_B1 THEN
            IF in_raw <> CONT_B2 THEN in_bcnt = 0
        END IF
    END IF
    IF in_bcnt > IN_BTN_CONFIRM THEN in_bcnt = IN_BTN_CONFIRM
    in_braw = 0
    IF in_bcnt = IN_BTN_CONFIRM THEN in_braw = 1
    IF in_braw <> 0 AND in_pbtn = 0 THEN
        in_btn = 1
    ELSE
        in_btn = 0
    END IF
    in_pbtn = in_braw

    ' --- keypad, edge-triggered, decoded from the byte with ROLL-OVER ---
    ' This used to be `CONT.KEY`, and `CONT.KEY` cannot express what a keypad
    ' actually does when two keys are down at once. It matches the whole byte
    ' against the twelve key patterns, so `8` + `3` = $44 | $21 = $65 decodes
    ' as NOTHING -- and then, when the second key is released, the byte is $44
    ' again and it decodes as a FRESH PRESS of 8.
    '
    ' So a key that stays down -- a finger that has not lifted yet, an emulator
    ' that dropped a key-up, a sticky membrane -- makes every LATER key read as
    ' that first key. That is exactly what it looked like: typing "test" gave
    ' `t`, then `v` (a second 8, not the 3), and then a red flash on everything
    ' because no dictionary word continues "88". Space, ENTER and CLEAR were
    ' dead for the same reason -- they were all arriving as 8.
    '
    ' A matrix is read by watching which lines went DOWN, not by matching the
    ' whole byte, so that is what this does: confirm the byte, then decode only
    ' the bits it ADDED since the last confirmed state. $44 -> $65 adds $21,
    ' which is 3. Releasing 3 goes $65 -> $44, which adds nothing, so the held
    ' 8 does not re-fire. Roll-over typing -- pressing the next key before
    ' letting go of the last -- is how people type, and it now works.
    '
    ' Deciding it here rather than in the epilogue also means the raw byte is
    ' the WHOLE input model: tests/*.txt `raw`/`rawhold` now drives the keypad
    ' too, where before it could only reach the disc and the buttons. And
    ' dropping CONT.KEY gives back the 6 8-bit variables the IntyBASIC manual
    ' charges for it, which the constants.bas budget was down to its last two of.
    IF in_raw = in_kraw THEN
        IF in_kcnt < IN_KEY_CONFIRM THEN in_kcnt = in_kcnt + 1
    ELSE
        in_kraw = in_raw
        in_kcnt = 1
    END IF
    in_key = KEYPAD_NONE
    IF in_kcnt = IN_KEY_CONFIRM THEN
        IF in_kraw <> in_hold THEN
            ' Bits set in the confirmed byte but not in the last one. IntyBASIC
            ' has no NOT, and `a XOR (a AND b)` is `a AND NOT b`.
            in_kb = in_kraw XOR (in_kraw AND in_hold)
            in_hold = in_kraw
            GOSUB in_decode
        END IF
    END IF

    ' --- scripted input: the decoded half ---
    ' Bytes 2-4 override the DECODED values, which is how a scenario says
    ' "the user pressed 5" without caring how a controller spells it. The raw
    ' half is in_inject, above, and is the one that exercises this procedure's
    ' own decoding -- see tests/keypad.txt.
    IF (PEEK(SC_INJECT) AND 255) = $A5 THEN
        IF (PEEK(SC_INJECT + 1) AND 255) = $5A THEN
            in_inj = PEEK(SC_INJECT + 2) AND 255
            IF in_inj <> KEYPAD_NONE THEN
                in_key = in_inj
                POKE SC_INJECT + 2, KEYPAD_NONE
            END IF
            in_inj = PEEK(SC_INJECT + 3) AND 255
            IF in_inj <> 0 THEN
                in_disc = in_inj
                POKE SC_INJECT + 3, 0
            END IF
            in_inj = PEEK(SC_INJECT + 4) AND 255
            IF in_inj <> 0 THEN
                in_btn = 1
                POKE SC_INJECT + 4, 0
            END IF
        END IF
    END IF
END

' The character-grid text entry that came with this module in fujinet-config
' was dropped when calendars were only ever chosen from a fetched list. Compose
' brought text entry back, but as t9.bas -- predictive rather than a 6x16
' charset grid, and it draws through screen.bas's value window instead of
' bringing a MOB cursor and a colour-stack profile of its own.
