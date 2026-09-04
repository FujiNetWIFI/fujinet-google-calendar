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
and an 8K cartridge window, so the portable C core does not fit. The image
is a 20K **banked** cart for the protocol-v2 FujiNet cartridge (see "The
banked cartridge" below). Like `intv/`, it is standalone: its own
`build.sh`, outside the mekkogx/defoogi build.

## Building and running

```
./build.sh                 # -> build/gcal.bin (a 20K banked APPBANK image)
DEMO=1 ./build.sh          # static mock screens, no FujiNet needed
./run.sh                   # MAME against a live fujinet-pc
make demoshot              # headless screenshots of the DEMO screens
make smoke                 # headless live smoke against fujinet-pc
```

`build.sh` assembles with zmac (found on `PATH`, or `~/Workspace/zmac-1.3`,
or the firmware's `pico/astrocade` tree) — one assembly, the pages laid out
with ORG/PHASE — pads to the 20K image, stamps the `FUJI` claim at
`0x1CFC`, and runs `tools/checkrom.py --banked` and `tools/checksize.py` —
the latter prints the byte budget per region (page 0, the resident half,
each 4K page) from the `MB_*` fences and fails any region over its
ceiling.

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
deletes, `=` accepts and `C` cancels. Dates and times get the digit-mask
editor instead: the first digit lays an underscore mask, `CE` backs up one
slot (and with nothing typed EMPTIES the field -- a blank start is the
wire's spelling of all-day), and a half-typed value is rolled back rather
than sent as nonsense. On the form, `=` saves and `C` leaves (through a
save-or-discard ask if anything was typed). An edit sends only the fields
you changed. Nothing touches the network until the save, and the form
validates everything it can locally -- a required title, an
end-without-start, a real date (leap Februaries included), well-formed
times -- because the adapter reports every draft rejection as one opaque
code.

## The banked cartridge

The client is a 20K **APPBANK** image for the protocol-v2 FujiNet cart
(fujinet-firmware `pico/astrocade`): the 8K window plus three 4K pages,
selected by one read at `FNBKSEL+page` with the mailbox fully live. Page 0
holds the screens (views, week, month, detail, the parser, alarms), the
resident half 3000H-3AFFH the shared library and the bank trampolines (the
only code that switches pages), page 2 the form and grid keyboard, page 3
settings and the calendar picker, page 4 the splash and DEMO screens.
`tools/checksize.py` budgets each region on every build.

With the single-8K ceiling gone, the five features cut from the first
release are back, full parity with the other clients:

- **Week view** (`2`) — seven rows Sun-Sat with the leading event's colour
  chip, the day of month (inverse for today), an event count and one block
  per event; `=` re-anchors to the selected day and drills to the day
  view, `5` composes on it, left/right step a whole week.
- **Synthesised alarms** — in the day view on today, an event starting
  within the alarm lead rings once: the chime and a status-row banner with
  the start time. Any key dismisses it. (The adapter's field mask never
  carries reminders, so alarms are synthesised client-side, the C and
  Intellivision rule.)
- **Calendar picker and settings** (`C`) — alarm lead, the calendar
  selector (picked by NAME from the adapter's list; index 0 = all), and
  the timezone readout that catches a rejected POSIX TZ. Persisted in the
  shared appkey (creator 4743H / app 1 / key 0) and loaded at boot.
- **The digit-mask date/time editor** — above.
- **Detail paging** — up/down page the detail eleven wrapped rows at a
  time; a page turn re-opens the read and skips ahead, riding the
  adapter's 120-second window cache, so nothing is buffered and the last
  page is sticky.

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
