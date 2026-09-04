# Google Calendar for the Bally Astrocade

A Google Calendar client for the Bally Astrocade with a FujiNet cartridge,
talking to the `GCAL:` protocol adapter in fujinet-firmware.

Day, month and agenda views; an event detail screen; and **composing and
editing events** with an on-screen keyboard. Google's colour scheme is
mapped onto the Astrocade's four-colour display, the Google Calendar logo is
drawn on the splash, and the clock in the header is the FujiNet's, re-synced
as you use it.

This is the third implementation in this repository, beside the C core
(`src/`) and the IntyBASIC one (`intv/`). It is hand-written Z80 assembly
(zmac), because the Astrocade has 4 KB of RAM total — all of it screen RAM —
and a cartridge whose usable half is 6,912 bytes, so the portable C core
does not fit. Like `intv/`, it is standalone: its own `build.sh`, outside the
mekkogx/defoogi build.

## Building and running

```
./build.sh                 # -> build/gcal.bin (exactly 8192 bytes)
DEMO=1 ./build.sh          # static mock screens, no FujiNet needed
./run.sh                   # MAME against a live fujinet-pc
make demoshot              # headless screenshots of the DEMO screens
make smoke                 # headless live smoke against fujinet-pc
```

`build.sh` assembles with zmac (found on `PATH`, or `~/Workspace/zmac-1.3`,
or the firmware's `pico/astrocade` tree), pads to the 8 KB cartridge window,
stamps the `FUJI` claim at `0x1CFC`, and runs `tools/checkrom.py` and
`tools/checksize.py` — the latter prints the per-module byte budget from the
`MB_*` fences and fails over the 6,912-byte ceiling.

Running needs a fujinet-pc (or Pico) with the `GCAL:` adapter, Google
authorised in the web UI with **both** `calendar.readonly` and
`calendar.events` in the grant, a **POSIX** `[General] timezone` (e.g.
`CST6CDT` — an IANA name like `America/Chicago` is silently rejected and
"today" comes out wrong), and `[Device] enable_apetime=1` for the clock.
MAME needs the FujiNet cart device grafted in (see the firmware's
`pico/astrocade/emu/apply.sh`). At the console's SELECT GAME menu, keypad `1`
launches the cart.

## Using it

| Control | Action |
|---|---|
| keypad `1` `2` `3` | day / month / agenda view |
| keypad `0` | jump back to today |
| keypad `5` | compose a new event on the shown date |
| keypad `6` | edit the selected event (day and agenda views, and detail) |
| disc up / down | move the selection; in the month grid, move a week |
| disc left / right | previous / next day; in the month grid, a day |
| trigger / `=` | open the selection; in month, drill into that day |
| `MR` | refresh |

Events carry a colour chip in the gutter — red, green or blue, the eleven
Google category colours binned into three — because that column sits left of
the screen's palette split and can show colours the rest of the screen
cannot. The month grid shows a density mark per day (`.` none, `+` a few,
`#` many), today as a blue tile, and the cursor day inverse.

### Composing and editing

`5` opens a blank form on the shown date; `6` opens it prefilled from the
selected event. Move between the seven fields (title, date, start, end,
where, notes, category) with the disc; the trigger opens the selected field
in the on-screen grid keyboard (netcat's 96-glyph mixed-case set), where the
keypad types digits straight in, the disc + trigger pick any character, `CE`
deletes, `=` accepts and `C` cancels. On the form, `=` saves and `C` leaves
(through a save-or-discard ask if anything was typed). A blank start time
makes an all-day event; an edit sends only the fields you changed. Nothing
touches the network until the save, and the form checks a required title and
an end-without-start first, because the adapter reports every draft
rejection as one opaque code.

## What is not here, and why

The Astrocade cartridge is a single 8 KB bank, and full parity with the
other clients does not fit in 6,912 bytes. Dropped, in the order they would
come back if the firmware gains **bank switching** (a larger, banked cart):

- **Week view** — day, month and agenda cover its uses.
- **Synthesised alarms** — the machine has no interrupt-driven time base of
  its own; the clock is re-fetched from the FujiNet as you navigate.
- **Calendar picker and multi-calendar** — the client always shows all
  calendars. There is no settings screen; the alarm-lead and calendar
  appkey the other clients persist are not used here.
- **The digit-mask date/time editor** — dates and times are typed in the
  same grid keyboard as the text fields (the keypad still types digits;
  `-` is on the keypad, `:` is on the grid). The other clients mask them.
- **Detail paging** — the detail screen shows one screenful; the adapter
  wraps at 80 columns, so all but the longest events fit.

Everything else — the four cut features included — was implemented and works;
the cuts are a size decision, not a capability one. See the top-level plan
and the port notes in the repo's memory for the full history.

## How it is built

The infrastructure is copied verbatim from
`~/Workspace/netcat/astrocade`: the 4×6 magic-expander renderer (`gfx.inc`),
the 96-glyph mixed-case font (`assets/font.inc`), the keypad + hand-controller
event space (`input.inc`), the on-screen grid keyboard (`edit.inc`), the
cart-mailbox transport (`fujilib.inc`), the read-the-reply-where-it-lies
helpers (`state.inc`), and the layout checkers in `tools/`. The network round
trip follows 5cardstud's one-shot shape (`net.inc` / `url.inc`), because a
`GCAL:` fetch is request/response, not a held stream.

Nothing is ever buffered from the network: every screen renders straight out
of the cart's repainted 1 KB reply window, and the day/agenda list and the
month tally are the **same** streaming parser (`parse.inc`) driven in two
modes — it draws rows for a list and folds a per-day count/colour array for
the grid. The month cursor *is* the anchor date, so moving it is plain date
arithmetic and rolling into a neighbouring month is free.

Colours come from MAME's palette maths inverted for the Google hues; the
working screens use the right palette (white page, black ink, Calendar blue
bands, grey dim) with `HORCB=1` isolating the column-0 chip gutter on the
left palette, and the splash sets its own palette with the split down the
logo's midline for a full four-colour mark.

### Live verification status

Smoke-tested against a live fujinet-pc: the splash, day/month/agenda views
(real events, colour chips, the month grid), the live clock, the compose
form (draw, date prefill, the grid keyboard, and client-side validation).
**Not** exercised live, to avoid leaving stray events in a real calendar:
the actual create/edit network write and the detail-open/edit-prefill path
(both need a selected real event or a real save). They are the same wire
format the IntyBASIC client smoked end to end.
