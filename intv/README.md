# Google Calendar for Intellivision (IntyBASIC)

A Google Calendar client for the Mattel Intellivision with a FujiNet
cartridge, talking to the `GCAL:` protocol adapter in fujinet-firmware.

Day, week, month and agenda views with scrolling, Google's colour scheme mapped
onto the Intellivision palette, all-day and timed events, an event detail
screen, a calendar picker, audible alarms for upcoming events, and **composing
and editing events** with a T9 predictive-text keyboard on the twelve-key
keypad. The Google Calendar logo is drawn with MOBs.

## Controls

| Control | Action |
|---|---|
| keypad `1` `2` `3` `4` | day / week / month / agenda view |
| keypad `0` | jump back to today |
| keypad `5` | compose a new event on the anchor date |
| keypad `6` | edit the selected event (day and agenda views, and the detail screen) |
| keypad `9` | cycle the three pages of key hints |
| keypad `CLEAR` | settings (timezone, alarm lead, calendar) |
| disc up / down | move the selection |
| disc left / right | previous / next period — in the month grid, move a day |
| action button | open the selected event; in week/month, drill into that day |

On the settings screen: `1` opens the calendar picker, the disc changes the
alarm lead time, `CLEAR` saves and returns.

## Composing and editing

`5` opens a blank form on the date the view is anchored on -- in the month grid
that is the day under the cursor. `6` opens the same form prefilled from the
selected event, wherever a single event is selected: the day and agenda views,
and the open event detail. In week and month a row is a day rather than an
event, so `6` does nothing there.

The seven fields are the adapter's seven draft keys, one per row:

| | |
|---|---|
| disc up/down | move between fields |
| action button | open the selected field for editing |

The selected field is marked the same way a selected event is: the colour-stack
bar from `bar.bas`, not a colour change on the text, so a field's own colouring
stays free to say whether it has been edited. The bar stays lit on the field
you are typing into, because the editor leaves rows 4-10 alone.
| `ENTER` | save |
| `CLEAR` | leave without saving |

Title, where, notes and category are typed with **T9** (below). Date, start and
end take keypad digits straight into a `YYYY-MM-DD` / `HH:MM` mask -- the digits
are on the keypad already, and routing them through a predictive editor to reach
a `3` would be absurd. In the numeric editor `CLEAR` backs up a slot, and
`CLEAR` on an untouched field empties it.

The rules worth knowing, which are the same ones the C clients follow:

- **A blank start time makes an all-day event.** A date-only `START` is the
  wire's own spelling of all-day, mirrored rather than translated. A blank end
  takes the adapter's default: an hour later, or the same day for all-day.
- **An edit sends only the fields you changed.** The client holds a title
  truncated to what the listing gave it and nothing at all of the location,
  notes or category, so sending an untouched field would replace the server's
  fuller copy with a stub. A blank field on an edit means *leave it alone*, and
  clearing a field to empty is not possible from here.
- Blanking the start time *is* meaningful on an edit: it converts the event to
  all-day. Changing the date alone moves the event and keeps its time.
- The title is Title-Cased on the way out, so an event composed here reads like
  every other event in the account. The other fields go as typed.
- Multi-day and open-ended timed events cannot be entered.

Nothing touches the network until the save, so backing out costs nothing. The
form validates the date against real month lengths, the times, and an end
without a start, all before it opens a channel -- not out of caution but because
the adapter collapses *every* draft rejection into one status code, so a check
that does not happen on the client reaches you as "REJECTED BY GOOGLE" with no
reason attached.

## T9

The keypad is the keyboard. `2`-`9` type a letter group and the most frequent
matching word appears; the disc cycles the other matches; `0` commits the shown
word and appends a space. A yellow pending word is an exact dictionary match, a
green one is a prefix of a longer word. A digit that matches nothing flashes red
and is ignored, so you can never type into a dead end.

`1` commits and then multi-taps punctuation, and a side button toggles
**multi-tap (ABC)** mode for anything the dictionary does not have -- names,
places, anything. Every field opens in T9; an ABC excursion lasts as long as
the field and no longer. `CLEAR` un-types a digit and then backspaces into
committed text; `ENTER` accepts the field.

The module is `t9.bas`, from
[intv-keyboard-experiment](https://github.com/thomcherryhomes/intv-keyboard-experiment),
which built it to be lifted into a host like this one. It is kept as a module
with an unchanged contract rather than welded into the calendar, because the
Gmail Intellivision client wants it next. Three things changed on the way in,
all marked `PORT:` in the source: its cart-RAM scratch moved out of `$9500`,
which collides with this app's map; its scripted-input hook moved from `t9_poll`
into `in_poll`, so every screen is testable and not just the editor; and two
latent bugs are fixed (a punctuation character clobbered by the next letter, and
a candidate-strip write that ran one cell past row 3 -- which here is the form's
title field).

### The dictionary is small, and calendar-shaped

650 words, generated at build time by `tools/mkdict.py` and packed two letters
per ROM word. That number is what fits (see below), and at that size *which*
650 matters more than it would at two thousand: the list is
`wordlist/gcal-extra.txt` -- meeting, dentist, birthday, appointment, standup,
anniversary -- ahead of the general frequency list, because a general top-650
has none of those and does have several hundred words that never appear in an
event title. `DICT_COUNT` is the knob; raise it and `make check` will say
exactly which table ran out of room.

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

Needs [IntyBASIC](https://github.com/nanochess/IntyBASIC) v1.4.2, jzintv's
`as1600`, and python3 for the dictionary generator and the segment check.

```sh
make          # -> gcal.bin (+ .cfg) and gcal.rom
make check    # verify where the ROM actually landed
make test FUJINET=localhost:9995   # scripted UI tests, headless
make kbdiag   # -> kbdiag.rom, a standalone controller-port monitor
```

**Run `make check` after any change that adds code.** There are two silent
failures waiting and neither produces an error from the toolchain.

IntyBASIC auto-continues past the end of a full segment — first into `$7000`,
then into `$8000`, which on this cartridge is the FujiNet RAM window rather
than ROM at all. The result assembles and links cleanly and produces a cart
that fails EXEC's boot detection: the PC lands in unprogrammed GROM a couple of
instructions in, with nothing on screen. This happened once during the compose
work, into `$E000`.

And `t9dict.bas` carries its own `ASM ORG`s, which *rewind* the assembly
pointer — so code that outgrows its budget is simply overwritten by the table
that follows, and as1600 reports nothing at all. That happened too:
`st_form.bas` grew past `$AFFF` and the letter pool landed on top of `frm_new`.
The only evidence was a symbol address.

`tools/checkcfg.py` catches both. It expands every mapping to its real address
range, checks each lies inside a window this cart boots from, that nothing
reaches `$B800` (GRAM is aliased there, so ROM placed at `$B800` is not what
reads back), that no two regions overlap, and — from the symbol table, because
the `.cfg` cannot show it — that no code label sits inside a dictionary table.
`tools/checkdict.py` then decodes the dictionary out of the assembled `.bin`
and checks it against the word list, which is the only thing tying `t9.bas`'s
hardcoded table addresses to the ones the Makefile passed the generator.

### The memory map

Four windows, and the margins are thin:

| Window | Holds | Free |
|---|---|---|
| `$5000-$6FFF` | core, transport, the four views, both selection bars | ~140 |
| `$A000-$B7FF` | `t9.bas`, `st_form.bas`, `st_week.bas`, the dictionary **index** | ~215 |
| `$D000-$DFFF` | the cold views, alarms, the dictionary **letter pool** | ~425 |
| `$F000-$FFFF` | event detail, picker, the IntyBASIC epilogue | ~155 |

`$A000-$B7FF` is new for compose — the other three had about 2K words left
between them and text entry needs roughly four. `$B800-$BFFF` is unusable and
must stay empty.

The dictionary's two tables are split across two windows rather than kept
together because the pool costs ~2.5 ROM words per entry and the index costs 1,
and the two free holes are different sizes. `st_week.bas` is included with the
text-entry code rather than with the other cold views for the same reason:
moving one ~430-word module across moved the binding constraint and bought
about seventy more dictionary entries.

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

### Writing one back

A save is a **write-mode open** (`aux1 = 8`, `aux2` ignored), one
`network_write` per line, then a close — and the close *is* the commit.

Compose targets the selector alone, with no view, no date and no `/N`:

```
N:GCAL://           the primary calendar
N:GCAL://Work       a named one
```

An edit targets exactly the spec the listing was fetched with, plus `/N`, which
is what `gc_build_url` already emits. Keeping the query matters: the adapter
numbers events *within* the `?count=&days=` window, so an edit URL that dropped
it would address a different event than the one on screen.

The payload is plain text, one `KEY: value` line each, terminated with `$0D` —
no JSON to assemble on a CP-1610:

```
SUMMARY: Dentist
START: 2026-09-01 14:00
END: 2026-09-01 14:30
LOCATION: 12 High St
```

`START`/`END` are `YYYY-MM-DD HH:MM` with a space, not a `T`; a date-only value
is all-day. The adapter accepts `$0D`, `$0A`, `$9B` or CRLF, upper-cases keys
before matching, and treats an **unknown key as a hard error** rather than
dropping it.

**The verdict is not in the close.** `Calendar.cpp` preserves `error` across the
base close specifically so the STATUS that follows can carry it, so a save is
open → write → close → *status*. `1`/`0`/`136` are success; `132` is a rejected
draft (and is every rejection reason, which is why the form validates first);
`167` or `212` mean the OAuth grant is missing `calendar.events` or Google was
never authorised; `170` means the `/N` no longer resolves.

One transaction needs a longer leash than the rest: opening a write channel for
an **edit** makes the adapter fetch and number the whole window upstream before
it answers, because `/N` resolves at open time rather than at commit.
`fn_transact`'s timeout is a variable for that reason and `st_form.bas` raises
it to ~90s around that one call.

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
| `st_form.bas` | the compose/edit form: fields, validation, and the draft wire format |
| `t9.bas` | T9 predictive text entry (from `intv-keyboard-experiment`) |
| `t9dict.bas` | generated by `tools/mkdict.py`; two ROM tables, read by `PEEK` |
| `st_pick.bas` | calendar picker, settings, AppKey persistence |
| `alarm.bas` | the upcoming-event scan, banner and chime |
| `gfx.bas` | the sixteen GRAM cards and the MOB logo |
| `bar.bas` | the colour-stack selection bar |
| `screen.bas`, `scroll.bas`, `input.bas`, `wrap.bas` | from `fujinet-config/intv` and `fujinet-gmail-client/intv` |
| `tools/` | the dictionary generator, the segment and dictionary checks, the headless test driver |

## Constraints this code is shaped by

1. **`#BACKTAB` is the live STIC display list** — about fifty cell writes per
   frame. Moving the selection is four writes (two colour-stack advance bits),
   never a repaint.
2. **`fn_transact` blocks** for up to 900 frames. A fetch happens once, when a
   view is entered or its period changes, never inside an input loop.
3. **228 8-bit and 47 16-bit variables, and no string type.** All text lives in
   ROM `DATA` or cart scratch RAM and is `PEEK`ed a byte at a time. The 16-bit
   pool is the binding one and compose very nearly exhausted it: `t9.bas` alone
   wants eleven. Fitting it meant deleting three unused library routines from
   `fujinet.bas`/`screen.bas` (`api_call`, `net_open`, `fn_putnum`, `scr_dec`,
   `scr_recolor` — ~270 ROM words and six slots, all still in `netcat/intv` if
   they are ever wanted), demoting `#w_c` and T9's multi-tap timer to 8 bits,
   and merging five per-screen `SC_EVT` record pointers into one `#evrec`. The
   rule that makes that last one safe: `#evrec` belongs to whichever screen is
   drawing and is never live across a state change.
4. **IntyBASIC folds names to upper case**, so a constant and a variable that
   differ only in case are the same identifier. `SC_LINEBUF` is not called
   `SC_HOLD` because `scroll.bas` already owns `sc_hold`.

5. **The disc and the keypad are the same eight lines.** A hand controller
   reports one byte: the keypad grounds a row line (bits 5-7) and a column line
   (bits 0-3), the disc reports on bits 0-3, and an action button grounds two
   row lines. `CONT.UP`/`DOWN`/`LEFT`/`RIGHT` are bare bit tests, so read
   naively **every keypad press also reads as a disc direction** — `1`/`2`/`3`
   as DOWN, `4`/`5`/`6` as RIGHT, `7`/`8`/`9` as UP, `0`/`CLEAR`/`ENTER` as
   LEFT. `in_poll` believes the disc only when bits 5-7 are clear.

   That is not a cosmetic filter. `4`, `5` and `6` read as RIGHT, which steps
   the view to the next period and starts a *blocking* fetch — and IntyBASIC's
   `CONT.KEY` only latches a value after three consecutive identical frames
   (`_cnt1_p0`/`_cnt1_p1` in `intybasic_epilogue.asm`). The fetch ate those
   frames, the key was released before it decoded, and the whole keypad did
   nothing but advance the date. In the T9 editor the same aliasing walked the
   candidate list on every letter typed.

   The cost is that the disc reads neutral while a button is held, which is a
   real property of the hardware. Nothing here reads the two together.

   The **buttons** need the other half of the same argument, and it took two
   tries to get right. A button grounds two row lines and a key one, so
   comparing the row bits against `$A0`/`$60`/`$C0` is necessary -- but it is
   not sufficient, because two keys from different rows OR into exactly those
   three row patterns (`4` + `6` = `$82 | $22` = `$A2`). 48 of the 66 key pairs
   do. That is not a two-handed stunt, it is a finger rolling off one key onto
   the next while typing, and the T9/ABC toggle is on the button: every ghost
   committed the pending word and flipped the mode, so the next digit was typed
   in the mode it had just been flipped into. `6338464` came out as `offtimi`
   instead of `meeting`.

   **A debounce does not fix this, and the first attempt was one.** Requiring
   the row pattern to hold for three frames rejects a one-frame transient and
   nothing else; a finger overlap lasts far longer than 50ms, and no confirm
   window long enough to outlast one leaves the button feeling instant.

   What separates them is the *low* bits. A button grounds two row lines and
   **nothing else**; every key also grounds one of bits 0-3, so no combination
   of keys can spell a bare `$A0`/`$60`/`$C0`. `in_poll` compares `in_raw`
   against the three button bytes whole, which is exact rather than
   statistical, and costs one compare less than masking did. The price is that
   a button pressed while the disc is off centre is ignored -- the mirror of
   the gate above, and nothing here reads the two together.

   `IN_BTN_CONFIRM` stays, for the make/break skew a matrix has on release: two
   keys whose column contacts open a frame before their row contacts do spell a
   bare button pattern for that frame.

   The **keypad** is decoded here too, rather than read from `CONT.KEY`, and
   for the third face of the same fact. `CONT.KEY` matches the whole byte
   against the twelve key patterns, so two keys held at once — `8` + `3` =
   `$44 | $21` = `$65` — decode as *nothing*, and the instant the second key is
   released the byte is `$44` again and decodes as a **fresh press of the
   first key**. One key that stays down therefore makes every later key read as
   that first one: typing `test` gave `t`, then `v` (a second `8`, not the `3`),
   then a red flash on everything, with space, ENTER and CLEAR dead because
   they were all arriving as `8`.

   A matrix is read by watching which lines went *down*, so `in_poll` confirms
   the byte and then decodes only the bits it **added** since the last
   confirmed state. `$44` → `$65` adds `$21`, which is `3`. Releasing it goes
   `$65` → `$44`, which adds nothing, so the held key does not re-fire.
   Roll-over typing — pressing the next key before letting go of the last —
   is how people type, and it works now.

   Two things fall out. The raw byte is the *whole* input model, so
   `tests/*.txt` `raw`/`rawhold` drives the keypad as well as the disc and the
   buttons; and dropping `CONT.KEY` gives back the six 8-bit variables the
   IntyBASIC manual charges for it, which the RAM budget was down to its last
   two of.

   What the ROM cannot fix is a key the *console* is holding down. `make
   kbdiag` builds a standalone ROM that shows the controller byte itself — see
   `tests/README.md`. jzIntv does not clear pad state when its window loses
   focus, so letting focus go mid-keystroke leaves that key held forever.

6. **A name used before the file that `DIM`s it is auto-created, and the real
   `DIM` then fails.** Anything read across module boundaries — `al_active`,
   `#ev_num`, `#evrec` — is declared in `constants.bas`, ahead of every
   `INCLUDE`, for that reason. The same trap applies to `CONST`s, except that
   those fail *silently*, reading as zero: `SC_INJECT` lives in `constants.bas`
   rather than `t9.bas` because `input.bas` needs it and comes first.
5. **Never widen the `MEMATTR` past `$9BFF`** — it shadows jzIntv's FujiNet
   peripheral with inert RAM and the mailbox never comes up.
6. **The scratch RAM map starts at `$8040`, not `$8000`.** The STIC's control
   registers are decoded at `$0000-$003F` and mirrored at `$4000`, `$8000` and
   `$C000`, on real hardware and in jzIntv both, so a write to cart RAM in the
   first 64 bytes lands in the RAM *and* in the STIC. Most of that register
   file is rewritten by the interrupt routine every frame and so repairs
   itself; the colour stack (`$8028-$802B`) is written only by `MODE` and does
   not. `SC_EVT` used to start at `$8000`, which put event record 2's
   `EVT_NUMLO` — the adapter's event number, 3 for the third event of any
   listing — onto colour-stack entry 0, and the header and hint rows turned
   `CS_TAN` as soon as a fetch returned three events. `make check` now fails
   the build if any `SC_*` is declared below `$8040`.
