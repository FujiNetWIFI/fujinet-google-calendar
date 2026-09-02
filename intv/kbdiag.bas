' kbdiag.bas -- what is the console ACTUALLY reporting on the controller port?
'
' Standalone: no FujiNet, no includes, no clock. Build and run it with
'
'     make kbdiag && ./run.sh kbdiag.rom        (or point jzIntv at it directly)
'
' WHY THIS EXISTS. Two separate typing bugs in the T9 editor were diagnosed
' from screenshots of the editor, and the second one turned out not to be in
' the ROM at all: every key after the first arrived as the FIRST key, which is
' what the console reports when the first key is still held down. A keypad key
' grounds one row line and one column line, so two keys held together OR into a
' byte that is not any key -- CONT.KEY decodes it as NONE, and the moment the
' second key is released the byte is the first key again and it re-fires.
'
' There is nothing the ROM can do about that, and no way to tell it apart from
' real typing by looking at the editor. This screen shows the byte itself.
'
' WHAT TO LOOK FOR: press a key, release it, press a DIFFERENT key. The HELD
' counter must return to a RAW of 00 between them. If it does not -- if RAW
' keeps the first key's bits, or the history shows the first key still there
' after you let go -- the console (or the emulator's key handling) is holding
' the key down, and every client on this machine will misbehave the same way.
'
' RAW is CONT: $01FE XOR $01FF, the two hand-controller ports merged. P1/P2 are
' the ports themselves, unmerged and un-inverted, so an idle port reads FF.
'
'   keypad   1 $81  2 $41  3 $21   4 $82  5 $42  6 $22
'            7 $84  8 $44  9 $24   0 $48  CLEAR $88  ENTER $28
'   buttons  B0 $A0  B1 $60  B2 $C0        (nothing in bits 0-4)
'   disc     bits 0-4 only

    CONST CW = 7                 ' white, colour-stack entry
    CONST CY = 2                 ' a second colour for the live row
    CONST COLS = 20
    CONST HISTN = 6              ' transitions kept on screen (rows 6-11)

    DIM raw, prv, v, p, n, i, hp, hused
    DIM hb(HISTN)                ' the byte
    DIM #hf(HISTN)               ' how many frames it lasted
    DIM #held

    MODE 0, 0, 0, 0, 0
    BORDER 0
    WAIT
    CLS
    PRINT AT 0,  "KEYPAD DIAG"
    PRINT AT 40, "RAW    P1     P2"
    PRINT AT 100, "WAS  FRAMES"

    prv = 255                    ' impossible, so frame 1 always logs
    #held = 0
    hp = 0
    hused = 0

diag_loop:
    WAIT
    raw = CONT AND 255

    IF raw = prv THEN
        #held = #held + 1
        IF #held > 9999 THEN #held = 9999
    ELSE
        ' Log the run that just ended, then start counting the new byte.
        IF prv <> 255 THEN
            hb(hp) = prv
            #hf(hp) = #held
            hp = hp + 1
            IF hp >= HISTN THEN hp = 0
            IF hused < HISTN THEN hused = hused + 1
            GOSUB diag_hist
        END IF
        prv = raw
        #held = 1
    END IF

    ' --- the live row: RAW, then each port on its own ---
    v = raw   : p = 60 : GOSUB diag_hex
    v = PEEK(510) AND 255 : p = 67 : GOSUB diag_hex
    v = PEEK(511) AND 255 : p = 74 : GOSUB diag_hex
    PRINT AT 80, "HELD ", <4>#held, " FRAMES"
    GOTO diag_loop

' diag_hist: redraw the transition list, newest first.
diag_hist: PROCEDURE
    FOR i = 0 TO HISTN - 1
        n = hp - 1 - i
        IF n < 0 THEN n = n + HISTN
        p = (6 + i) * COLS
        IF i < hused THEN
            v = hb(n) : GOSUB diag_hex
            PRINT AT p + 5, <5>#hf(n)
        ELSE
            PRINT AT p, "          "
        END IF
    NEXT i
END

' diag_hex: byte v as two hex cards at BACKTAB position p.
' '0' is ASCII 48, card 16; 'A' is ASCII 65, card 33 = 10 + 7 + 16.
diag_hex: PROCEDURE
    n = (v / 16) AND 15
    IF n > 9 THEN n = n + 7
    #BACKTAB(p) = (n + 16) * 8 + CW
    n = v AND 15
    IF n > 9 THEN n = n + 7
    #BACKTAB(p + 1) = (n + 16) * 8 + CW
END
