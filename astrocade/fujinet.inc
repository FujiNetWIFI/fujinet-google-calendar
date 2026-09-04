; fujinet.inc -- N: device, appkey and clock commands, and the GCAL: wire.
;
; The N: command values are hand-synchronised with the other Astrocade
; ports (netcat, 5cardstud); the canonical list is fujinet-firmware's
; include/fujiCommandID.h. The mailbox transport itself is fujilib.inc.
;
; The clock is its own device, 45H, not a Fuji-device command: FUJICMD
; GET_TIME (D2H) is not in the RS232 build's dispatch (the Intellivision
; client documents the discovery in intv/clock.bas). It answers when the
; FujiNet config has [Device] enable_apetime=1.

; ---- Network device (N:) ----------------------------------------------
NETDEV  EQU     71H             ; N1:
NCOPEN  EQU     4FH             ; 'O'
NCCLOSE EQU     43H             ; 'C'
NCREAD  EQU     52H             ; 'R'
NCSTAT  EQU     53H             ; 'S'
NCWRITE EQU     57H             ; 'W'

NTRNONE EQU     0               ; translation: none

; GCAL: open modes (aux1) and the listing width (aux2). The detail READ
; ignores aux2 (the adapter wraps at its default 80 and we re-wrap to 40);
; listings are fetched at 80 like the Intellivision client and re-composed
; into 40-column rows.
GMDIR   EQU     6               ; aux1: directory (event index)
GMREAD  EQU     4               ; aux1: read (event detail)
GMWRITE EQU     8               ; aux1: write (a draft; close commits)
GCWIDTH EQU     80              ; aux2 on listings

; NET_STATUS reply: 4 bytes in slice 0.
NSAVLO  EQU     0               ; bytes waiting, low
NSAVHI  EQU     1               ; bytes waiting, high
NSCONN  EQU     2               ; connected flag
NSDEVST EQU     3               ; nDevStatus_t, below

; Device status codes. 1/0/136 are success -- every complete fetch ends in
; 136 as soon as the buffer drains. The write side collapses every draft
; rejection into 132, which is why the form validates before a byte goes
; out.
NDSOK   EQU     1               ; SUCCESS
NDSEOF  EQU     136             ; END_OF_FILE
GEWRONLY EQU    131             ; read on a write channel
GEBADDR EQU     132             ; rejected draft (every reason)
GERDONLY EQU    135             ; write on a read channel
GEFULL  EQU     162             ; draft buffer full
GEBADSP EQU     165             ; bad devicespec
GEDENY  EQU     167             ; missing calendar.events scope
GENOTFND EQU    170             ; /N no longer resolves
GENOSRV EQU     210             ; no GCAL adapter
GENOAUTH EQU    212             ; Google never authorised

; ---- Fuji device appkeys ----------------------------------------------
; Settings live in creator 4743H / app 1 / key 0, byte 0 = alarm lead
; minutes, bytes 1.. = NUL-terminated calendar selector -- the same key
; every other client of this app uses (src/settings.c). The open payload
; is 6 raw TX bytes {creator lo, hi, app, key, mode, reserved}; omitting
; the reserved byte hangs the firmware's transaction_get (intv/fujinet.bas
; documents the misdiagnosis). The read reply is self-describing: a 2-byte
; LE length prefix ahead of the payload.
FCOKEY  EQU     0DCH            ; OPEN_APPKEY
FCCKEY  EQU     0DBH            ; CLOSE_APPKEY
FCRKEY  EQU     0DDH            ; READ_APPKEY
FCWKEY  EQU     0DEH            ; WRITE_APPKEY
AKCRLO  EQU     43H             ; creator 4743H, little-endian on the wire
AKCRHI  EQU     47H
AKAPP   EQU     1
AKKEY   EQU     0
AKMRD   EQU     0
AKMWR   EQU     1

; ---- Clock device -----------------------------------------------------
CLKDEV  EQU     45H             ; the APETIME-shaped clock device
CLKISO  EQU     49H             ; 'I': 25-byte ASCII YYYY-MM-DDTHH:MM:SS+HHMM
CLKTZ   EQU     47H             ; 'G': the POSIX TZ string

; ---- Sizes -------------------------------------------------------------
RXMAX   EQU     1024            ; FUJIMAIL_RX_MAX: the READ clamp
TXMAX   EQU     128             ; our WRITE cap, far inside FN_TX_MAX 320
