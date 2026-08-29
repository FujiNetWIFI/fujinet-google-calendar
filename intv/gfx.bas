' gfx.bas -- the sixteen GRAM cards, and the Google Calendar logo in MOBs.
'
' Intellivision GRAM cards are 8x8, one byte per row, MSB leftmost. Sixteen of
' the sixty-four are used; the rest stay free.
'
' Safe to sit here as raw data: gcal.bas's GOTO jumps clear of every INCLUDE,
' so straight-line execution never falls into it.
'
' ---------------------------------------------------------------------------
' WHY THERE ARE GRAM DIGITS AT ALL
'
' In colour-stack mode a cell's foreground is bits 0-2 plus bit 12, but on a
' GROM card bit 12 is the Coloured-Squares selector instead, so GROM text can
' only be foreground colours 0-7. Five of the eleven colours Google uses --
' cyan, orange, pink, light blue and purple -- live above 7. Anything that has
' to carry a true event colour therefore has to be a GRAM card, which is why
' the colour chip and the month grid's day numbers are drawn from here rather
' than from the GROM font the rest of the UI uses.
'
' ---------------------------------------------------------------------------
' THE LOGO
'
' The Google Calendar mark is a white square with a four-colour border and a
' blue "31". It is drawn 16x16 -- two cards square -- at the top right, with
' the white body in BACKTAB and everything else in MOBs.
'
' The border is ONE card. GLYPH_CORNER is the top and left edges of a quadrant;
' flipped in X it becomes the top and right edges, in Y the bottom and left, in
' both the bottom and right. Four MOBs pointing at that single card therefore
' close a complete two-pixel frame around the square, each quadrant in its own
' colour, and the four segments can never drift apart the way four hand-drawn
' cards would. (The Gmail client's gfx.bas uses the same trick on the M.)
'
' The "31" costs no GRAM either: a MOB's card field takes 0-255 as GROM, so the
' two digit MOBs point straight at the GROM font's '3' and '1'.
'
' Colours are Google's own, mapped to the nearest Intellivision entries:
' #4285F4 blue, #EA4335 red, #FBBC04 yellow, #34A853 green.
'
' MOB budget: 0 is the month grid's selected-day cursor, 1-4 the logo border,
' 5-6 the logo digits, 7 the month grid's "today" marker. All eight are spoken
' for.
' ---------------------------------------------------------------------------

    CONST MOB_CURSOR = 0
    CONST MOB_LOGO0  = 1
    CONST MOB_DIG3   = 5
    CONST MOB_DIG1   = 6
    CONST MOB_TODAY  = 7

' Top-right corner of the screen, in cards.
    CONST LOGO_COL = 18
    CONST LOGO_ROW = 0

' GROM card numbers for the two digits. Cards are ASCII-32, so '3' is 19.
    CONST GROM_3 = 19
    CONST GROM_1 = 17

' Google's brand colours, nearest Intellivision entries.
    CONST GLOGO_BLUE   = CS_BLUE
    CONST GLOGO_RED    = CS_RED
    CONST GLOGO_YELLOW = CS_YELLOW
    CONST GLOGO_GREEN  = CS_GREEN

    DIM gg_i

' ---------------------------------------------------------------------------
' Card 0: the solid block. Does quadruple duty -- the event colour chip, the
' logo's white body, and the month grid's inverse-video day cursors.
' ---------------------------------------------------------------------------
lit_gram:
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"

' Card 1: one quadrant of the logo's border -- top edge and left edge, two
' pixels thick. Flipped into all four corners.
    BITMAP "########"
    BITMAP "########"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"

' Cards 2-11: digits 0-9, five pixels wide on a seven-row body. Used only
' where a digit must carry a colour above 7 -- the month grid.
    BITMAP ".###...."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP ".###...."
    BITMAP "........"

    BITMAP "..#....."
    BITMAP ".##....."
    BITMAP "..#....."
    BITMAP "..#....."
    BITMAP "..#....."
    BITMAP "..#....."
    BITMAP ".###...."
    BITMAP "........"

    BITMAP ".###...."
    BITMAP "#...#..."
    BITMAP "....#..."
    BITMAP "...#...."
    BITMAP "..#....."
    BITMAP ".#......"
    BITMAP "#####..."
    BITMAP "........"

    BITMAP ".###...."
    BITMAP "#...#..."
    BITMAP "....#..."
    BITMAP "..##...."
    BITMAP "....#..."
    BITMAP "#...#..."
    BITMAP ".###...."
    BITMAP "........"

    BITMAP "...#...."
    BITMAP "..##...."
    BITMAP ".#.#...."
    BITMAP "#..#...."
    BITMAP "#####..."
    BITMAP "...#...."
    BITMAP "...#...."
    BITMAP "........"

    BITMAP "#####..."
    BITMAP "#......."
    BITMAP "####...."
    BITMAP "....#..."
    BITMAP "....#..."
    BITMAP "#...#..."
    BITMAP ".###...."
    BITMAP "........"

    BITMAP "..##...."
    BITMAP ".#......"
    BITMAP "#......."
    BITMAP "####...."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP ".###...."
    BITMAP "........"

    BITMAP "#####..."
    BITMAP "....#..."
    BITMAP "...#...."
    BITMAP "..#....."
    BITMAP "..#....."
    BITMAP "..#....."
    BITMAP "..#....."
    BITMAP "........"

    BITMAP ".###...."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP ".###...."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP ".###...."
    BITMAP "........"

    BITMAP ".###...."
    BITMAP "#...#..."
    BITMAP "#...#..."
    BITMAP ".####..."
    BITMAP "....#..."
    BITMAP "...#...."
    BITMAP ".##....."
    BITMAP "........"

' Card 12: the all-day chip. A bar rather than a full block, so an all-day
' event reads differently from a timed one at a glance even in monochrome.
    BITMAP "........"
    BITMAP "........"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"

' Card 13: the alarm bell.
    BITMAP "...##..."
    BITMAP "..####.."
    BITMAP ".######."
    BITMAP ".######."
    BITMAP "########"
    BITMAP "........"
    BITMAP "...##..."
    BITMAP "........"

' Cards 14-15: scroll-more arrows.
    BITMAP "........"
    BITMAP "...##..."
    BITMAP "..####.."
    BITMAP ".######."
    BITMAP "########"
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"

    BITMAP "........"
    BITMAP "........"
    BITMAP "########"
    BITMAP ".######."
    BITMAP "..####.."
    BITMAP "...##..."
    BITMAP "........"
    BITMAP "........"

' ---------------------------------------------------------------------------
' gc_define_gram: upload all sixteen cards. Called once at boot, before
' anything draws -- until the DEFINE lands, GRAM holds whatever the EXEC left
' in it.
'
' Split into two halves because a DEFINE takes effect on the NEXT frame and
' only the last one issued in a frame is honoured; the per-frame ceiling is
' around eighteen cards, so sixteen in one go would sit right on the edge for
' no benefit.
' ---------------------------------------------------------------------------
gc_define_gram: PROCEDURE
    WAIT
    DEFINE 0, 8, lit_gram
    WAIT
    DEFINE 8, 8, VARPTR lit_gram(32)
    WAIT
END

' ---------------------------------------------------------------------------
' gc_logo_show: park the six logo MOBs over cards (18,0)-(19,1) and paint the
' white body underneath them.
'
' SPR_ZOOMY2 is on every sprite deliberately: bits 9-8 of the Y word are the
' vertical scale and 01 is NORMAL height, so omitting it gives a half-height
' sprite with every other row dropped. SPR_FLIPX/SPR_FLIPY are bits 10 and 11
' of that same Y word -- SPR_ZOOMX2 shares $0400 with SPR_FLIPX but lives in
' the X word, so the two never actually collide.
' ---------------------------------------------------------------------------
gc_logo_show: PROCEDURE
    ' The white body, in BACKTAB behind the sprites.
    #BACKTAB(screenpos(LOGO_COL, LOGO_ROW))     = GLYPH_BLOCK * 8 + GRAM_SELECT + gramfg(CS_WHITE)
    #BACKTAB(screenpos(LOGO_COL + 1, LOGO_ROW)) = GLYPH_BLOCK * 8 + GRAM_SELECT + gramfg(CS_WHITE)
    #BACKTAB(screenpos(LOGO_COL, LOGO_ROW + 1)) = GLYPH_BLOCK * 8 + GRAM_SELECT + gramfg(CS_WHITE)
    #BACKTAB(screenpos(LOGO_COL + 1, LOGO_ROW + 1)) = GLYPH_BLOCK * 8 + GRAM_SELECT + gramfg(CS_WHITE)

    ' Border: one card, four flips, four colours.
    SPRITE MOB_LOGO0 + 0, \
        SPR_VISIBLE + MOB_X0 + LOGO_COL * 8, \
        SPR_ZOOMY2 + MOB_Y0 + LOGO_ROW * 8, \
        SPR_GRAM + GLYPH_CORNER * 8 + GLOGO_BLUE
    SPRITE MOB_LOGO0 + 1, \
        SPR_VISIBLE + MOB_X0 + (LOGO_COL + 1) * 8, \
        SPR_ZOOMY2 + SPR_FLIPX + MOB_Y0 + LOGO_ROW * 8, \
        SPR_GRAM + GLYPH_CORNER * 8 + GLOGO_RED
    SPRITE MOB_LOGO0 + 2, \
        SPR_VISIBLE + MOB_X0 + LOGO_COL * 8, \
        SPR_ZOOMY2 + SPR_FLIPY + MOB_Y0 + (LOGO_ROW + 1) * 8, \
        SPR_GRAM + GLYPH_CORNER * 8 + GLOGO_GREEN
    SPRITE MOB_LOGO0 + 3, \
        SPR_VISIBLE + MOB_X0 + (LOGO_COL + 1) * 8, \
        SPR_ZOOMY2 + SPR_FLIPX + SPR_FLIPY + MOB_Y0 + (LOGO_ROW + 1) * 8, \
        SPR_GRAM + GLYPH_CORNER * 8 + GLOGO_YELLOW

    ' "31", straight out of GROM, inset into the white body.
    '
    ' +1 and +7, not +2 and +8: MOB priority runs by index and LOWER wins, so
    ' the border MOBs (1-4) draw in front of the digits (5-6). At +8 the '1'
    ' glyph's baseline serif reaches x=14, which is inside the right border,
    ' and the border would clip it. One pixel left of that the whole "31" sits
    ' clear of all four segments.
    SPRITE MOB_DIG3, \
        SPR_VISIBLE + MOB_X0 + LOGO_COL * 8 + 1, \
        SPR_ZOOMY2 + MOB_Y0 + LOGO_ROW * 8 + 4, \
        GROM_3 * 8 + GLOGO_BLUE
    SPRITE MOB_DIG1, \
        SPR_VISIBLE + MOB_X0 + LOGO_COL * 8 + 7, \
        SPR_ZOOMY2 + MOB_Y0 + LOGO_ROW * 8 + 4, \
        GROM_1 * 8 + GLOGO_BLUE
END

' ---------------------------------------------------------------------------
' gc_hide_all_mobs: take every MOB down -- the logo and both month cursors.
' MOBs are STIC registers, not BACKTAB, so they survive a CLS; leaving them up
' would park the logo over whatever the next screen draws there. Used on the
' way into the picker and settings screens, which show no logo.
' ---------------------------------------------------------------------------
gc_hide_all_mobs: PROCEDURE
    FOR gg_i = 0 TO 7
        SPRITE gg_i, 0, 0, 0
    NEXT gg_i
END
