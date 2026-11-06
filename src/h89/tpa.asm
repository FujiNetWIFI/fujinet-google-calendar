;
; The memory check, made before BSS is cleared.
;
; z88dk's CP/M startup puts the stack at the BDOS and then clears BSS. On a
; CP/M whose BDOS sits below the end of BSS, that clear runs over the BDOS
; and over the stack -- the return address of the call doing the clearing
; included -- so the startup "returns" to 0000H, the warm boot reloads the
; CCP and the BDOS, and the user is back at the prompt without a word. A
; CP/M with a bigger BIOS than Heath's (a CF card's, say) does exactly that.
;
; So the link turns the startup's clear off (CRT_INITIALIZE_BSS=0), and this
; runs in its place: code_crt_init, after the startup's own part of it, and
; before anything has touched BSS but the startup's note of the default
; drive. It wants the end of BSS plus H89_STACK under the BDOS -- the same
; margin tools/h89/budget.py fails the link on -- and says what it found if
; not.
;
; Assembled for h89-cpm only (H89_CPM): HDOS's startup is another matter.
;

IFDEF H89_CPM

    EXTERN  __BSS_head
    EXTERN  __BSS_END_tail
    EXTERN  defltdsk

    defc    BDOS = 5
    defc    TOP = __BSS_END_tail + H89_STACK

    SECTION code_crt_init

    ld      hl,(BDOS + 1)
    ld      de,TOP
    and     a
    sbc     hl,de
    jr      nc,tpa_fits

    ld      de,tpa_msg1
    ld      c,9
    call    BDOS
    ld      hl,TOP
    call    tpa_hex
    ld      de,tpa_msg2
    ld      c,9
    call    BDOS
    ld      hl,(BDOS + 1)
    call    tpa_hex
    ld      de,tpa_msg3
    ld      c,9
    call    BDOS
    jp      0

tpa_fits:
    ld      a,(defltdsk)
    push    af
    xor     a
    ld      hl,__BSS_head
    ld      de,__BSS_head + 1
    ld      bc,__BSS_END_tail - __BSS_head - 1
    ld      (hl),a
    ldir
    pop     af
    ld      (defltdsk),a

    SECTION code_user

; HL as four hex digits, through the BDOS one at a time.
tpa_hex:
    ld      a,h
    call    tpa_hex2
    ld      a,l
tpa_hex2:
    push    hl
    push    af
    rrca
    rrca
    rrca
    rrca
    call    tpa_hex1
    pop     af
    call    tpa_hex1
    pop     hl
    ret
tpa_hex1:
    and     15
    add     a,'0'
    cp      '9' + 1
    jr      c,tpa_digit
    add     a,'A' - '9' - 1
tpa_digit:
    ld      e,a
    ld      c,2
    jp      BDOS

    SECTION rodata_user

; One line: the H19 does not wrap, and would write the end of a longer one
; over its last column.
tpa_msg1:
    defm    "GCAL needs more memory: the BDOS has to be at $"
tpa_msg2:
    defm    "H or above, not $"
tpa_msg3:
    defm    "H."
    defb    13, 10, '$'

ENDIF
