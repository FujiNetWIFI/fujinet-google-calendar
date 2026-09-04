; appkey.inc -- settings in a FujiNet appkey.
;
; creator 0x4743 / app 1 / key 0, the same key every other client of this
; app uses: byte 0 = alarm lead minutes, bytes 1.. = a NUL-terminated
; calendar selector. The open payload is six RAW tx bytes -- NOT the
; size-prefixed FNPARB the N: device wants -- and the reserved sixth byte
; must be sent or the firmware blocks waiting for it (intv/fujinet.bas).
; The read reply is self-describing: a 2-byte little-endian length ahead
; of the payload. Appkey payloads are small, so slice 0 always holds them
; and they are read straight from FNRDATA.

; AKOPEN: A = mode (AKMRD / AKMWR). Carry/err from FNCOMMIT.
AKOPEN: PUSH    AF
        LD      A,FNDFUJI
        LD      E,FCOKEY
        LD      L,0
        CALL    FNBEGIN
        LD      A,AKCRLO
        CALL    FNTXBYT
        LD      A,AKCRHI
        CALL    FNTXBYT
        LD      A,AKAPP
        CALL    FNTXBYT
        LD      A,AKKEY
        CALL    FNTXBYT
        POP     AF
        CALL    FNTXBYT         ; mode
        XOR     A
        CALL    FNTXBYT         ; reserved
        LD      B,3
        JP      FNCOMMIT

AKCLOSE: LD     A,FNDFUJI
        LD      E,FCCKEY
        LD      L,0
        CALL    FNBEGIN
        LD      B,3
        JP      FNCOMMIT

; AKLOAD: read the settings into ALLEAD and CALSEL. Missing key or any
; failure leaves the caller's defaults untouched.
AKLOAD: LD      A,AKMRD
        CALL    AKOPEN
        RET     C
        LD      A,FNDFUJI
        LD      E,FCRKEY
        LD      L,0
        CALL    FNBEGIN
        LD      B,3
        CALL    FNCOMMIT
        JR      C,AKLDC
        XOR     A
        LD      (CURSLC),A
        LD      A,(FNRDATA)     ; the LE length prefix
        LD      L,A
        LD      A,(FNRDATA+1)
        LD      H,A
        LD      A,H
        OR      L
        JR      Z,AKLDC         ; empty payload
        LD      A,(FNRDATA+2)   ; byte 0: alarm lead
        CP      61
        JR      NC,AKL1
        LD      (ALLEAD),A
AKL1:   LD      DE,CALSEL       ; bytes 1..: the selector, to its NUL
        LD      HL,FNRDATA+3
        LD      B,39
AKL2:   LD      A,(HL)
        OR      A
        JR      Z,AKL3
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    AKL2
AKL3:   XOR     A
        LD      (DE),A
AKLDC:  CALL    AKCLOSE
        OR      A
        RET

; AKSAVE: write ALLEAD then the NUL-terminated CALSEL as the payload.
AKSAVE: LD      A,AKMWR
        CALL    AKOPEN
        RET     C
        LD      A,FNDFUJI
        LD      E,FCWKEY
        LD      L,0
        CALL    FNBEGIN
        LD      A,(ALLEAD)
        CALL    FNTXBYT
        LD      HL,CALSEL
AKS1:   LD      A,(HL)
        CALL    FNTXBYT
        OR      A
        JR      Z,AKS2
        INC     HL
        JR      AKS1
AKS2:   LD      B,3
        CALL    FNCOMMIT
        JP      AKCLOSE
