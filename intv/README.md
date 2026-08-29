# Google Calendar for Intellivision (IntyBASIC)

A Google Calendar client for the Mattel Intellivision with a FujiNet
cartridge, talking to the `GCAL:` protocol adapter in fujinet-firmware.

Day, week, month and agenda views with scrolling, Google's colour scheme mapped
onto the Intellivision palette, all-day and timed events, an event detail
screen, a calendar picker, and audible alarms for upcoming events. The Google
Calendar logo is drawn with MOBs.

## Controls

| Control | Action |
|---|---|
| keypad `1` `2` `3` `4` | day / week / month / agenda view |
| keypad `0` | jump back to today |
| keypad `9` | toggle the second page of key hints |
| keypad `CLEAR` | settings (timezone, alarm lead, calendar) |
| disc up / down | move the selection |
| disc left / right | previous / next period — in the month grid, move a day |
| action button | open the selected event; in week/month, drill into that day |

On the settings screen: `1` opens the calendar picker, the disc changes the
alarm lead time, `CLEAR` saves and returns.

## Views

**Day** — seven events at a time, each a colour chip, a time (or `ALLDY`) and a
title. The selected row bounce-scrolls its full title.

**Week** — one row per day, showing the date and a chip per event, so a week
reads as a density map. The button drills into that day.

**Month** — a real 7×6 grid. Day numbers are drawn from GRAM digit cards so
they can carry the day's leading event colour; grey means nothing scheduled.
Today and the selected day are marked with MOBs using `SPR_BEHIND`, which
punches the digits out of a solid block for true inverse video.

**Agenda** — a rolling list across days with dim date separators, fetched with
`?count=96&days=90`.

## Building

Needs [IntyBASIC](https://github.com/nanochess/IntyBASIC) v1.4.2 and jzintv's
`as1600`.

```sh
make          # -> gcal.bin (+ .cfg) and gcal.rom
make check    # verify the ROM landed only in bootable segments
```

**Run `make check` after any change that adds code.** IntyBASIC silently
auto-continues past the end of the default `$5000-$6FFF` segment — first into
`$7000`, then into `$8000`, which on this cartridge is the FujiNet RAM window
rather than ROM at all. The result assembles and links cleanly and produces a
cart that fails EXEC's boot detection. `gcal.bas` declares `ASM ORG $D000` and
`ASM ORG $F000` explicitly for this reason, and `make check` fails the build if
a mapping line ever names anything else.

## Running

Start a `fujinet-firmware` PC build for the **RS232** target
(`./build.sh -p RS232`), configured with:

```ini
[General]
timezone=CST6CDT          ; a POSIX TZ string -- see the warning below
[Device]
enable_apetime=1          ; the clock device, $45
[BOIP]
enabled=1
port=9995
```

Authorize Google in its web UI, and make sure the Calendar API is enabled for
the project. `BoIPChannel` listens, so start fujinet first, then:

```sh
./run.sh                    # or: make run
./run.sh --fujinet-debug    # trace mailbox / FujiBus frames
```

### The timezone is not optional

`[General] timezone=` drives both the clock this client reads and the window
the adapter resolves events in. With it unset, `get_general_timezone()` returns
`UTC`, and after ~19:00 local the console's idea of "today" is already
tomorrow — events land on the wrong day and the day view looks empty.

It must be a **POSIX** TZ string (`CST6CDT`, `PST8PDT`,
`CST+6CDT,M3.2.0/2,M11.1.0/2`). An IANA name like `America/Chicago` is rejected
by the adapter's own `PosixTz` parser, which then silently falls back to UTC —
the failure looks identical to not setting it at all.

## How it works

### Wire format

The adapter offers packed 277-byte binary structs (`aux2 = 255`) or
fixed-column text (`aux2` = the line width). This client asks for **text at
width 80**, because the adapter has already resolved every timestamp into local
wall-clock time — consuming the binary form would mean 64-bit epoch-to-civil
conversion on a CP-1610 — and because a text row is 40–80 bytes against 277,
so a 512-byte `FN_RX` read brings back six or more events instead of one.

A directory reply is a window-title line, a header line, then one line per
event:

```
Fri 28 Aug 2026
--#-Time--------Category-------Event--------------------------------------------
* 1 all day     thom.cherry... All Day Test
  2 21:30-22:30 thom.cherry... Test 1
```

`numW` (the width of the number column) is not known in advance, but the header
is built by `dashed()`, which turns every space into `-`, so the `#` sits at
index `2 + numW - 1`. Scanning for it recovers the layout from a line that has
to be read anyway — no second request.

### Two firmware behaviours worth knowing

**Line endings are `$9B`, not `$0A`.** `lib/device/rs232/network.cpp` defines
`DEFAULT_LINE_ENDING "\n"` and calls `protocol->setLineEnding()` with it — but
at the *end* of `rs232_open()`, after `protocol->open()` has already composed
the entire reply. Calendar output is built in `open()` (`read()` just drains
the buffer), so it keeps `Protocol.h`'s `"\x9B"` default and the RS232 setting
never reaches it. The parser accepts either.

**`END_OF_FILE` is success.** `NetworkProtocolCalendar::status()` reports 136
as soon as its buffer drains, so every complete fetch ends with it. A
`net_status` that treats anything but `SUCCESS` as failure aborts on the last
read of every listing; `gc_status` handles it.

### Colours

Google exposes no numeric `colorId` to clients — `category_for()` folds it in
as a colour *name* instead, ahead of the calendar's own name. Matching the
category against the eleven names recovers the colour; anything else is a
calendar name and takes the default.

| Google | Intellivision | | Google | Intellivision |
|---|---|---|---|---|
| Tomato | red | | Peacock | cyan |
| Flamingo | pink | | Blueberry | blue |
| Tangerine | orange | | Lavender | light blue |
| Banana | yellow | | Grape | purple |
| Sage | green | | Graphite | grey |
| Basil | dark green | | *(none)* | cyan |

Event colours are drawn as GRAM block chips rather than coloured text because
in colour-stack mode a GROM cell's foreground is limited to colours 0–7 (bit 12
selects Coloured Squares instead of being the fourth colour bit), and five of
Google's eleven land above 7.

### Alarms

Google's per-event reminder times never reach the client — `GCAL.cpp`'s
`fields=` mask does not request `reminders`. Alarms are therefore synthesised:
an event sounds once, `al_lead` minutes before it starts, with the lead settable
on the settings screen and persisted in an AppKey. They fire only on a calendar
view, because the banner borrows the hint row and only a view knows how to paint
it back; nothing is lost, since an event still inside the window fires as soon
as you return.

## Files

| File | Role |
|---|---|
| `gcal.bas` | main: includes, segment layout, boot, video profiles, state machine |
| `constants.bas` | screen geometry, scratch-RAM map, colours, the palette table |
| `fujinet.bas` | mailbox transport, `N:` and AppKey primitives (from `netcat/intv`) |
| `clock.bas` | the clock device `$45`, and all civil-date arithmetic |
| `gcalnet.bas` | devicespec composition, the streaming fetch, the column parser |
| `st_view.bas` | chrome, navigation and dispatch shared by the four views |
| `st_day/week/month/agenda.bas` | the views |
| `st_event.bas` | event detail (`aux1=4` + `/N`), re-wrapped to 20 columns |
| `st_pick.bas` | calendar picker, settings, AppKey persistence |
| `alarm.bas` | the upcoming-event scan, banner and chime |
| `gfx.bas` | the sixteen GRAM cards and the MOB logo |
| `bar.bas` | the colour-stack selection bar |
| `screen.bas`, `scroll.bas`, `input.bas`, `wrap.bas` | from `fujinet-config/intv` and `fujinet-gmail-client/intv` |

## Constraints this code is shaped by

1. **`#BACKTAB` is the live STIC display list** — about fifty cell writes per
   frame. Moving the selection is four writes (two colour-stack advance bits),
   never a repaint.
2. **`fn_transact` blocks** for up to 900 frames. A fetch happens once, when a
   view is entered or its period changes, never inside an input loop.
3. **228 8-bit and 47 16-bit variables, and no string type.** All text lives in
   ROM `DATA` or cart scratch RAM and is `PEEK`ed a byte at a time.
4. **IntyBASIC folds names to upper case**, so a constant and a variable that
   differ only in case are the same identifier. `SC_LINEBUF` is not called
   `SC_HOLD` because `scroll.bas` already owns `sc_hold`.
5. **Never widen the `MEMATTR` past `$9BFF`** — it shadows jzIntv's FujiNet
   peripheral with inert RAM and the mailbox never comes up.
