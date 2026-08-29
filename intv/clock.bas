' clock.bas -- the FujiNet real-time clock, and civil-date arithmetic.
'
' THE CLOCK IS A SEPARATE DEVICE
'
' FUJICMD_GET_TIME ($D2) is NOT in rs232Fuji.cpp's command list, so looking for
' the time on the Fuji device ($70) finds nothing and suggests a firmware patch
' is needed. It isn't. The clock is its own device: lib/device/rs232/
' rs232Clock.cpp declares `rs232Clock platformClock;` and src/main.cpp
' registers it at FUJI_DEVICEID_CLOCK = $45 whenever [Device] enable_apetime is
' set, which it is by default. The mailbox copies mb_dev straight into the
' FujiBus request (jzintv src/fujinet/fujinet.c, fujibus_build_request(device,
' cmd, ...)), so any device id is reachable from here.
'
' WHY ISO LOCAL AND NOT APETIME
'
' fujiClock offers several formats. APETime ($93) is six packed bytes but
' carries only a two-digit year and no offset. CLKCMD_ISO_LOCAL ($49) returns
' "YYYY-MM-DDTHH:MM:SS+HHMM" plus a NUL -- 25 bytes at fixed offsets, giving a
' four-digit year (which the devicespec's ISO date needs) and the UTC offset,
' already resolved through [General] timezone. Parsing ASCII costs a few
' subtractions and saves carrying a century rule.
'
' NO EPOCH ARITHMETIC ANYWHERE
'
' The GCAL adapter resolves every event into local wall-clock time before it
' reaches us (that is what the human-readable format buys), so this module only
' ever does civil-date arithmetic on small integers. There is no 64-bit epoch
' conversion in this program.

    CONST CLOCK_DEVICEID   = $45
    CONST CLKCMD_ISO_LOCAL = $49   ' 'I'
    CONST CLKCMD_GET_TZ    = $47   ' 'G'

' Byte offsets into the fixed-width ISO reply:
'   0123456789...
'   YYYY-MM-DDTHH:MM:SS+HHMM
    CONST ISO_YEAR = 0
    CONST ISO_MON  = 5
    CONST ISO_DAY  = 8
    CONST ISO_HOUR = 11
    CONST ISO_MIN  = 14
    CONST ISO_SEC  = 17

' Re-sync this often. The console's frame counter is a crystal, not an RTC, and
' PAL/NTSC detection is a guess at the margins, so a periodic re-fetch keeps
' the alarm honest without a transaction every frame.
    CONST CLK_RESYNC_MINUTES = 30

' Every DIM in this module sits here, ahead of the first PROCEDURE: IntyBASIC
' will not accept a DIM of a name that earlier code already referenced.
    DIM clk_mo, clk_d, clk_h, clk_mi, clk_s
    DIM #clk_y
    DIM clk_frames, clk_since, clk_ok, clk_resync
' #ck_v is 16-bit and must stay that way: it accumulates the four-digit ISO
' year, and an 8-bit accumulator silently wraps -- 2026 becomes 2026 - 7*256 =
' 234, which sails through every sanity check here (month and day parse fine on
' their own) and only surfaces much later as a devicespec reading
' "/DAY/0234-08-29", which the adapter turns into a timeMin in the year
' 438497232.
    DIM ck_i, ck_n, ck_j, ck_c
    DIM #ck_v
    DIM cd_mo, cd_d, cd_n, cd_i
    DIM #cd_y, #dw_y

' ---------------------------------------------------------------------------
' ck_num: read ck_n ASCII digits from FN_RX starting at offset ck_i into #ck_v.
' Anything non-numeric yields 0 for that digit rather than garbage, so a short
' or malformed reply degrades to an obviously-wrong date instead of a wild one.
'
' ck_j/ck_c rather than the tick counters: IntyBASIC PROCEDUREs share one flat
' global namespace and are not reentrant, so borrowing clk_frames here would
' quietly reset the frame accumulator on every parse.
' ---------------------------------------------------------------------------
ck_num: PROCEDURE
    #ck_v = 0
    FOR ck_j = 0 TO ck_n - 1
        ck_c = PEEK(FN_RX + ck_i + ck_j) AND 255
        IF ck_c < 48 OR ck_c > 57 THEN ck_c = 48
        #ck_v = #ck_v * 10 + (ck_c - 48)
    NEXT ck_j
END

' ---------------------------------------------------------------------------
' clk_fetch: ask the clock for local time and parse it. Sets clk_ok.
'
' nparam = 0 deliberately: rs232Clock::alt_requested() returns true only when
' param(0) == $01, and we want the SYSTEM timezone from [General] timezone --
' the same one the GCAL adapter resolves events with -- not a session override.
' ---------------------------------------------------------------------------
clk_fetch: PROCEDURE
    mb_dev = CLOCK_DEVICEID
    mb_cmd = CLKCMD_ISO_LOCAL
    mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
    IF fn_ok = 0 THEN
        clk_ok = 0
        RETURN
    END IF

    ck_i = ISO_YEAR : ck_n = 4 : GOSUB ck_num : #clk_y = #ck_v
    ck_i = ISO_MON  : ck_n = 2 : GOSUB ck_num : clk_mo = #ck_v
    ck_i = ISO_DAY  : ck_n = 2 : GOSUB ck_num : clk_d  = #ck_v
    ck_i = ISO_HOUR : ck_n = 2 : GOSUB ck_num : clk_h  = #ck_v
    ck_i = ISO_MIN  : ck_n = 2 : GOSUB ck_num : clk_mi = #ck_v
    ck_i = ISO_SEC  : ck_n = 2 : GOSUB ck_num : clk_s  = #ck_v

    ' A zero month or day means the reply was not an ISO timestamp at all
    ' (a NAK body, or a firmware without the clock device registered).
    ' Better to report no clock than to anchor every view on year 0.
    IF clk_mo < 1 OR clk_mo > 12 OR clk_d < 1 OR clk_d > 31 THEN
        clk_ok = 0
        RETURN
    END IF

    clk_frames = 0
    clk_since = 0
    clk_ok = 1
END

' ---------------------------------------------------------------------------
' clk_get_tz: fetch the POSIX timezone string into SC_EDIT for the setup
' screen to display, so the user can see which zone the adapter is resolving
' events in. Sets clk_ok; the string is NUL-terminated by the firmware.
' ---------------------------------------------------------------------------
clk_get_tz: PROCEDURE
    mb_dev = CLOCK_DEVICEID
    mb_cmd = CLKCMD_GET_TZ
    mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
    POKE SC_EDIT, 0
    IF fn_ok = 0 THEN
        clk_ok = 0
        RETURN
    END IF
    FOR ck_i = 0 TO 62
        #ck_v = PEEK(FN_RX + ck_i) AND 255
        POKE (SC_EDIT + ck_i), #ck_v
        IF #ck_v = 0 THEN EXIT FOR
    NEXT ck_i
    POKE (SC_EDIT + 63), 0
    clk_ok = 1
END

' ---------------------------------------------------------------------------
' clk_tick: call once per frame. Advances the local clock between fetches and
' raises clk_resync when it is time to re-sync.
'
' NTSC is a built-in that reads 0 on a PAL console, where the frame interrupt
' is 50Hz rather than 60Hz -- getting this backwards would drift the clock by
' 20%, which is more than enough to fire alarms at the wrong minute.
' ---------------------------------------------------------------------------
clk_tick: PROCEDURE
    IF clk_ok = 0 THEN RETURN
    clk_frames = clk_frames + 1
    IF NTSC THEN
        IF clk_frames < 60 THEN RETURN
    ELSE
        IF clk_frames < 50 THEN RETURN
    END IF
    clk_frames = 0

    clk_s = clk_s + 1
    IF clk_s < 60 THEN RETURN
    clk_s = 0

    clk_mi = clk_mi + 1
    clk_since = clk_since + 1
    IF clk_since >= CLK_RESYNC_MINUTES THEN clk_resync = 1
    IF clk_mi < 60 THEN RETURN
    clk_mi = 0

    clk_h = clk_h + 1
    IF clk_h < 24 THEN RETURN
    clk_h = 0

    ' Past midnight. Roll the civil date the same way the navigation helpers
    ' do, so a console left running overnight still knows what "today" is.
    #cd_y = #clk_y : cd_mo = clk_mo : cd_d = clk_d
    GOSUB clk_addday
    #clk_y = #cd_y : clk_mo = cd_mo : clk_d = cd_d
END

' ---------------------------------------------------------------------------
' Civil-date arithmetic. All of it works on the #cd_y/cd_mo/cd_d triple, in
' place, because IntyBASIC PROCEDUREs take neither arguments nor return values.
' ---------------------------------------------------------------------------
lit_dim:
    DATA 0,31,28,31,30,31,30,31,31,30,31,30,31

' clk_leap: cd_n = 1 if #cd_y is a leap year. Full Gregorian rule -- 2100 is
' not a leap year, and a calendar has no business getting that wrong.
clk_leap: PROCEDURE
    cd_n = 0
    IF #cd_y % 4 = 0 THEN cd_n = 1
    IF #cd_y % 100 = 0 THEN cd_n = 0
    IF #cd_y % 400 = 0 THEN cd_n = 1
END

' clk_dim: cd_n = number of days in month cd_mo of year #cd_y.
clk_dim: PROCEDURE
    cd_n = lit_dim(cd_mo)
    IF cd_mo = 2 THEN
        GOSUB clk_leap
        cd_n = 28 + cd_n
    END IF
END

clk_addday: PROCEDURE
    GOSUB clk_dim
    cd_d = cd_d + 1
    IF cd_d <= cd_n THEN RETURN
    cd_d = 1
    cd_mo = cd_mo + 1
    IF cd_mo <= 12 THEN RETURN
    cd_mo = 1
    #cd_y = #cd_y + 1
END

clk_subday: PROCEDURE
    cd_d = cd_d - 1
    IF cd_d >= 1 THEN RETURN
    cd_mo = cd_mo - 1
    IF cd_mo < 1 THEN
        cd_mo = 12
        #cd_y = #cd_y - 1
    END IF
    GOSUB clk_dim
    cd_d = cd_n
END

' clk_addmonth / clk_submonth: step a month, clamping the day so that the 31st
' of January steps to the 28th (or 29th) of February rather than an invalid
' date the adapter would reject with INVALID_DEVICESPEC.
clk_addmonth: PROCEDURE
    cd_mo = cd_mo + 1
    IF cd_mo > 12 THEN
        cd_mo = 1
        #cd_y = #cd_y + 1
    END IF
    GOSUB clk_dim
    IF cd_d > cd_n THEN cd_d = cd_n
END

clk_submonth: PROCEDURE
    cd_mo = cd_mo - 1
    IF cd_mo < 1 THEN
        cd_mo = 12
        #cd_y = #cd_y - 1
    END IF
    GOSUB clk_dim
    IF cd_d > cd_n THEN cd_d = cd_n
END

' ---------------------------------------------------------------------------
' clk_dow: cd_n = day of week for #cd_y/cd_mo/cd_d, 0 = Sunday.
'
' Sakamoto's method. The table offsets each month so that a single
' divide-by-4/100/400 year term works; for January and February the year is
' decremented first, which is what folds the leap day to the end of the
' "year" and makes the table constant.
' ---------------------------------------------------------------------------
lit_dowt:
    DATA 0,0,3,2,5,0,3,5,1,4,6,2,4

clk_dow: PROCEDURE
    #dw_y = #cd_y
    IF cd_mo < 3 THEN #dw_y = #dw_y - 1
    cd_n = (#dw_y + #dw_y / 4 - #dw_y / 100 + #dw_y / 400 + lit_dowt(cd_mo) + cd_d) % 7
END

' ---------------------------------------------------------------------------
' clk_today: point the view anchor at the clock's idea of today.
' ---------------------------------------------------------------------------
clk_today: PROCEDURE
    #cur_y = #clk_y : cur_mo = clk_mo : cur_d = clk_d
END

' ---------------------------------------------------------------------------
' clk_is_today: cd_n = 1 if the anchor triple #cd_y/cd_mo/cd_d is the clock's
' today. Used by the month grid to mark the current day.
' ---------------------------------------------------------------------------
clk_is_today: PROCEDURE
    cd_n = 0
    IF #cd_y = #clk_y AND cd_mo = clk_mo AND cd_d = clk_d THEN cd_n = 1
END
