; sound.inc -- the alarm chime, written straight to the music processor.
;
; 5cardstud's BEEP: set the master oscillator and tone A period, open the
; volume, busy-wait, close it. Blocking is fine -- the only caller is the
; view loop, and the chime is under half a second.

TONEA   EQU     11H             ; tone A period (TONMO+1)

; BEEP: A = tone A period (higher = lower pitch), B = duration, ~10 ms
; units. Clobbers A,B,DE.
BEEP:   PUSH    BC
        LD      C,A
        LD      A,20H
        OUT     (TONMO),A
        LD      A,C
        OUT     (TONEA),A
        LD      A,08H
        OUT     (VOLAB),A
        POP     BC
BEEPW:  LD      DE,600
BEEPD:  DEC     DE
        LD      A,D
        OR      E
        JR      NZ,BEEPD
        DJNZ    BEEPW
        XOR     A
        OUT     (VOLAB),A
        RET

; SNDAL: the alarm: low then high, twice feels naggy, once is a chime.
SNDAL:  LD      A,30H
        LD      B,6
        CALL    BEEP
        LD      A,18H
        LD      B,8
        JP      BEEP
