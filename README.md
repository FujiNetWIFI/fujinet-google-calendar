# FujiNet Google Calendar

A Google Calendar client for 8-bit machines, talking to the `GCAL:` network
protocol adapter in [fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware).

Two implementations live here:

- `intv/` — the original, in IntyBASIC for the Intellivision.
- `src/` — a C port. The portable core is in `src/`, the machine-specific half
  in `src/<platform>/`. Atari 8-bit, Apple II, CoCo, Coleco Adam and MS-DOS
  backends exist so far.

Day, week, month and agenda views; an event detail screen; a calendar picker; a
settings page; and alarms synthesised on the client, because the adapter's
field mask never asks Google for reminders.

## Building

```
defoogi make atari          # -> r2r/atari/gcal.com and r2r/atari/gcal.atr
defoogi make apple2enh      # -> r2r/apple2enh/gcal.a2s and gcal.po
defoogi make coco           # -> r2r/coco/gcal.bin and gcal.dsk
defoogi make adam           # -> r2r/adam/gcal.ddp and gcal_BOOTSTRAP.bin
defoogi make msdos          # -> r2r/msdos/gcal.exe and gcal.img
make -C tests               # host-native tests of the portable core
```

[defoogi](https://github.com/FozzTexx/defoogi) carries cl65, cmoc, zcc, wcc,
`eos.lib`, `smartkeys.lib`, `dir2atr`, `atr`, `ac` and `decb`, so no host
toolchain is needed; plain `make atari` works too if you have them. fujinet-lib
is fetched into `_cache/` automatically. Leave `FUJINET_LIB` empty for the
MS-DOS build rather than pointing it at a host checkout: defoogi mounts the
project directory and nothing else, so an absolute host path is invisible
inside the container.

The Apple II target is `apple2enh` — an enhanced //e with 80-column hardware.
`apple2` is not a build of this: it is the unenhanced machine, with no
MouseText and a character generator that would render the chip column as
random inverse capitals.

The CoCo disk holds `GCAL.BIN` and nothing else. Start it with

```
LOADM"GCAL":EXEC
```

There is deliberately no `AUTOEXEC.BAS`, and the top-level `Makefile` explains
at length why neither way of putting one there survives contact.

The Adam target produces a 256K digital data pack. Copy `gcal.ddp` somewhere a
FujiNet can serve it, mount it in disk slot 1 from CONFIG, and boot — what is in
slot 1 is what the ADAM boots. `gcal_BOOTSTRAP.bin` is the 43-byte boot block,
already the first 1K of the `.ddp`; it is written out separately by z88dk's
appmake and nothing needs to be done with it.

`adam_cpm` is deliberately not in `PLATFORMS`. `PLATFORM_COMBOS` expands it to
`src/adam/` as well, so listing it would compile this backend's EOS and
SmartKeys calls into a CP/M binary that links neither library.

## Using it

| Key | |
|---|---|
| `1` `2` `3` `4` | day / week / month / agenda |
| `0` | jump to today |
| `←` `→` | previous / next period |
| `↑` `↓` | move the selection |
| `RETURN` | open the selection |
| `N` | compose a new event on the shown date |
| `E` | edit the selected event (day and agenda views, and the open event) |
| `ESC` | settings, or back |
| `R` `Q` | refresh / quit |

The cursor keys need Ctrl held down on an Atari, so the bare keycaps they live
on — `-` `=` `+` `*` — are accepted too. An enhanced //e has real arrow keys
and needs no such workaround.

A CoCo has no `ESC` key, so **`BREAK`** is settings-and-back there, which is
what every other FujiNet CoCo client does; `CLEAR` joins `R` as refresh.

The Adam has all of the above and six labelled SmartKeys besides, whose captions
are drawn on the bottom three rows of the screen. Every screen declares its own,
and the list screens have two banks because they have more than six actions:

| | I | II | III | IV | V | VI |
|---|---|---|---|---|---|---|
| list | Day | Week | Month | Agenda | Today | More |
| more | Refresh | Setup | Quit | New | Edit | Back |
| detail | Pg Up | Pg Dn | Up | Down | Edit | Back |
| picker | Up | Down | | | Pick | Back |
| settings | Less | More | | Cal | | Save |
| form | Up | Down | | | Save | Done |

`More` and `Back` on the list screens are the bank toggle and never reach the
program's key handling at all. The keyboard keys keep working everywhere: the
SmartKeys are the discoverable subset, not the whole of it.

On the Atari and the Apple the header carries a tab strip showing which digit
selects which view, which is what keeps the footer to a single row. Sixteen rows
will not pay for one, so the CoCo's footer says `1-4:VIEW` instead and its
settings screen legends the rest — the trade the Intellivision made at twenty
columns with two hint pages.

## Composing and editing

`N` opens a blank form on the date under you — the anchor date, or the selected
day in the week view. `E` opens the same form prefilled from the selected event;
it works wherever a single event is selected, which is the day and agenda views
and the open event detail. In the form, printable keys type into the active
field, `RETURN` moves to the next field, and `ESC` (or the Adam's `Done`, or the
CoCo's `BREAK`) leaves — silently if nothing was typed, through a save-or-discard
ask otherwise. Nothing touches the network until the save itself, so backing out
costs nothing.

The fields mirror the adapter's draft keys: title, date, start and end times,
location, notes and category. The rules worth knowing:

- **A blank start time makes an all-day event** — that is the wire's own
  spelling of all-day, mirrored rather than translated. A blank end time takes
  the adapter's default: an hour after the start, or the same day for all-day.
- **An edit sends only the fields you change.** The client keeps a truncated
  copy of the title and nothing at all of the location, notes or category, so
  sending untouched fields would clobber the server's fuller versions. The
  corollary: a blank field on an edit means *leave it alone*, and clearing a
  field to empty is not possible from these clients. Editing the title replaces
  it with exactly what the field shows.
- Blanking the start time *is* meaningful on an edit: it converts the event to
  all-day. Changing the date alone moves a timed event and keeps its time and
  duration.
- Editing one occurrence of a repeating event edits that occurrence only.
- Multi-day and open-ended timed events cannot be entered in this version.

A save is committed by the adapter when the channel closes, and the verdict
comes back in the device status that follows — every draft rejection collapses
to one code there, which is why the form validates dates, times, and an
end-without-start before a byte goes out. On the Apple II the bus does not
carry that verdict (see its notes below): the save reports optimistically and
the refetch on the way back to the view is the ground truth.

## Server setup

A FujiNet running firmware with the `GCAL:` adapter, and in its config:

```ini
[General]
timezone=CST6CDT          ; a POSIX TZ string -- mandatory
[Device]
enable_apetime=1          ; the clock device this client reads at boot
```

`[General] timezone` drives both the clock and the window the adapter resolves
events in. It must be **POSIX** (`CST6CDT`, `PST8PDT`,
`CST+6CDT,M3.2.0/2,M11.1.0/2`). An IANA name like `America/Chicago` is rejected
by the adapter's parser, which then silently falls back to UTC — after which
"today" rolls over in the evening and the day view looks empty. The settings
screen displays the timezone specifically so that is diagnosable.

Google must also be authorised in the FujiNet web UI, with the Calendar API
enabled for the project and both `calendar.readonly` **and `calendar.events`**
in the grant's scope. The second arrived with the compose/edit support, and an
older grant never gains it on its own — if reading works but a save comes back
`Authorize in the web UI` (code 167), re-authorise Google in the FujiNet web UI
so the grant picks up the new scope.

## Atari implementation notes

**No display list of our own.** The OS already has a correct 24-row GRAPHICS 0
list with its LMS pointing at the screen memory we write to; two interrupt bits
poked into it give three colour bands — a light header on rows 0–2, a white
page on rows 3–22, and a Calendar-blue footer on row 23. Poking two bits is far
less to get wrong than owning a display list, and `clrscr()` keeps working.

**The players do double duty.** Four players draw the Google Calendar mark in
the header: one quadrant of the coloured ring each, with the white page showing
through as playfield and "31" as ordinary text in the middle. The row-2
interrupt then moves their HPOS to column 0, where the same four players become
the list's colour gutter — one solid block on each row whose event is that
player's colour. Since only one player ever has data on a given row they cannot
overlap, so per-event colour costs five stores in an interrupt that had to fire
anyway. The four missiles, combined into a fifth player by GPRIOR, supply
Graphite.

That matters because GRAPHICS 0 has no per-cell colour at all: a character's
hue comes from COLPF2, which is a whole-scanline property. Without the players
the eleven Google colours would have nowhere to go.

**Text goes straight into screen RAM.** `cputc()` wraps at column 40 and
scrolls the whole screen when it runs off row 23, which would wreck a
full-width footer. Writing screen codes ourselves avoids that and is faster.

**The selection bar spans columns 1–39, not 0–39.** Column 0 is the chip
gutter, and an inverse space is COLPF1 — which draws in front of the players
and would cover the chip on exactly the row the selection is on. The
Intellivision kept its column 0 out of the bar for the same reason.

**The view loop polls; every other screen blocks.** The clock has to advance
and alarms have to fire while nobody is touching the keyboard, and neither
happens inside a blocking `KEYBDV` read. The other screens block, which is
cheaper and still correct: the clock runs off RTCLOK, which the OS vertical
blank keeps incrementing underneath them.

**Two flags carry the whole repaint policy.** `shown` means the screen needs
painting; `dirty` means the data is stale. Only the second costs a network
round trip, which is why backing out of an event does not re-download the day.

**The listing is fetched as text, not as packed structs.** `aux2 = 255` would
give 277-byte `CalEventItem` records carrying uint64 UTC epochs — and rendering
one means evaluating a POSIX TZ rule and converting a 64-bit epoch to a civil
date, on a 6502, as a second and divergent copy of arithmetic the adapter has
already done. The text form arrives resolved to local wall clock and is about
80 bytes per event. It has to be requested at width 80: at 40 the adapter falls
back to a two-line layout this parser does not understand.

**Event detail arrives pre-wrapped at 38 columns** on real Atari firmware
(`Calendar.cpp`'s `BUILD_ATARI` default) and at 80 from a fujinet-pc RS232
build, so the client re-wraps regardless and handles both.

**`clock_get_tz()` does not NUL-terminate** what it writes — it reads a length
byte and copies exactly that many bytes. Zero the buffer first.

**The compose editor uses the real Ctrl-arrows.** The bare `- = + *` aliases
deliberately do not apply inside the form — there they are text, because a
title or a time can contain any of them. `BACK S` erases; `RETURN` moves to
the next field.

**Compose is what spent the memory slack.** The form and its editor cost about
5K of 6502 code, against a build that had 707 bytes free — so the P/M reserve
came down from a lazy 2048 to the exact-fit 1056, the C stack from cc65's
default 2K to 1K, `MAX_EVENTS` from 64 to 48 (the agenda then asks
`?count=48`), `DET_ROWS` from 48 to 40, `GC_RXBUF` to 256, and the form buffer
and the picker's list are both overlaid on `gc_det` (`GC_FORM_OVERLAY` /
`GC_CALS_OVERLAY` in `gcal.h` — sound because neither is ever alive at the
same time as the detail rows or each other). The BSS ceiling is now `$B400`;
check `r2r/atari/gcal.map` after touching any knob.

## Apple II implementation notes

**One bit per pixel, so shape replaces colour everywhere.** The Atari carries
each event's Google colour in a player; 80-column text has no colour at all.
Two things stand in. Each event gets a one-character MouseText **chip** — a
diamond, a dither, a rule, a double rule, a solid block — which is a grouping
cue rather than a colour, and the settings screen legends it. More usefully,
the wire has always sent a 14-character **category** column that the Atari has
no room for and throws away: colour name, or the *calendar's* name for an event
with no colorId, which is something no colour can express. At 80 columns it is
simply a column.

**The chip column and the tick column stay out of the selection bar.** The
Atari keeps its column 0 out for a hardware reason — an inverse space is COLPF1
and draws in front of the player holding the chip. Here the reason rhymes:
MouseText occupies the very character codes the inverse forms would have used,
so there is no inverse of a glyph. A chip inside the bar would fall back to
ASCII and change shape on the one row the cursor is on.

**The header bar is inverse and the hint bar deliberately is not.** Same
reason. Row 23 is the row with the arrows on it, and an inverse hint bar would
have to spell them `^v<>`. Drawing hints on ordinary background is also what
every Apple II program that uses MouseText does.

**MouseText travels in the control range.** `copy_san()` clamps every wire
field to `$20-$7E` before a painter can see it, so bytes `$01-$1F` can only
come from a string literal in `src/apple2enh/`. The blitter reads one as
MouseText glyph `$40 + byte`, which lets hint strings carry arrows as ordinary
C literals. Spell them as octal escapes: `"\x1B"` followed by a hex digit is
one escape, not two characters.

**Text goes straight into the two text pages.** Even columns live in auxiliary
memory and odd in main, so a run of text is two interleaved runs in two banks.
`screen.c` composes a whole field and `blit.s` writes the odd half and then the
even half, so a field costs one bank switch rather than one per character. The
technique is cc65's own (`libsrc/apple2/cputc.s`): with 80STORE on, a touch of
`HISCR` pages `$0400-$07FF` to aux. It is `blit.s` and not `screen.s` because
the build globs `*.c` and `*.s` from one directory onto `<name>.o`.

**There is no RTCLOK, so the frame counter is one we keep.** `plat_getkey()` is
a polling loop around `plat_vsync()` rather than a firmware read, which is what
keeps the count rising while the event, picker and settings screens wait — the
property `clk_tick()` needs. It does *not* rise during a SmartPort transfer, so
the clock loses the length of every fetch; the half-hour resync bounds it.

**The chime is played, not started.** The whole audio surface is one bit at
`$C030`, so a note is a delay loop and `plat_tone()` returns when the note is
over. Three bursts of about eighty milliseconds is all the view loop gives up.

**`__HIMEM__` goes to `$BF00`.** `apple2enh.cfg` presumes RAM ends at `$9600`,
leaving room for ProDOS file buffers this client never opens — fujinet-lib
talks SmartPort directly and nothing here touches the filesystem. The wider
screen's buffers are paid for out of the 10.5K between the two.

**Event detail is re-flowed, not just re-wrapped.** `detail.c` deliberately
never joins across the adapter's own line breaks, which is right at 40 columns
and wrong at 78 — a description wrapped at 38 arrives as a ragged column down
the left half of the screen. Under `DET_REFLOW` its breaks are undone first.
They are recoverable because `append_wrapped()` flushes the moment a line
reaches `width`, so every line it emits is shorter than the wrap width and a
noticeably short one can only be the last of something. The wrap width is
estimated from the reply itself, which is what lets one rule serve both the 38
of Atari firmware and the 80 of every other bus.

**A save reports optimistically on this bus.** The adapter commits an event
when the write channel closes and latches the verdict into the device status
that follows — but the IWM bus layer in current firmware does not carry that
latch, so there is nothing useful to ask after the close. The client reports
the save as done and refetches the listing on the way back to the view, which
is the ground truth: a rejected draft shows up as the event not appearing.
(SIO, AdamNet, DriveWire and RS232 all carry the verdict; this is the one bus
that does not, tracked as a firmware fix.) In the form itself, `DELETE`
erases and the left arrow moves the cursor — this machine has a real `DELETE`
key, so BASIC's erase-with-left-arrow convention does not get a vote.

## CoCo implementation notes

**Semigraphics is per byte, so colour is free.** The 6847 decides from bit 7
whether a cell is a character or a 2×2 block of colour, with no mode switch and
no second display list: `$00-$3F` is a glyph with INV asserted, `$40-$7F` the
same glyph normal, `$80-$FF` a colour and a quadrant mask. So this is the only
backend besides the Atari that shows an event's real Google colour, and unlike
the Atari it does not have to steer four players from an interrupt to do it —
the chip is a byte in screen RAM. Google's four brand colours land on four of
the eight the VDG has, Graphite lands on buff, and that is exactly the five
chips `color.c` quantises its eleven colour names onto.

**Two views are better here than at 40 or 80 columns.** WEEK draws each day's
whole load as a strip of chips, one per event in its own colour — the
Intellivision's design, which both wider backends dropped in favour of a single
lead-event title. MONTH gives each day a four-cell density bar in the colour of
its leading event: sixteen steps, against the Atari's monochrome four and the
Apple's monochrome eight. That is where the 32-column budget is paid back.

**Column 0 stays out of the selection bar, for a third unrelated reason.**
Inverse video is XOR `$40`, and on a byte `≥ $80` bit 6 is part of the colour
field — so inverting a chip recolours it rather than highlighting it. The Atari
keeps its column 0 out because an inverse space is COLPF1 and covers the
player; the Apple because MouseText has no inverse form; the Intellivision
because the colour-stack run has to continue past the selection. Four machines,
four unrelated reasons, one rule.

**The gutter is black, not empty.** An unlit SG4 quadrant is black and the text
background is green, so a solid *green* chip on a text row is invisible — and
green is one of the five, carrying Sage and Basil. Every row therefore gets an
explicit `SG_BLACK` in column 0 whether it has a chip or not, which makes every
colour read and hands the backend a black rule primitive besides. That is what
draws the agenda separator's rule and the divider under the week grid, where
the Apple uses MouseText.

**The blank byte is `$60`, and there is no lowercase.** `memset(scr, 0, ...)`
paints a screen of inverse `@`. And the ROM has sixty-four glyphs, uppercase
only, so `sc()` folds case and every literal in `src/coco/` is written in
capitals. Recurring events are marked `+` rather than the `~` the other two use,
because a tilde is not one of the sixty-four and would fold to `?` — which
reads as "something went wrong" instead of "this repeats".

**The mark is drawn, and the "31" is punched out of it.** The Atari and the
Apple both print those two digits as ordinary text in the middle of the mark;
text here is green and never white. So the large mark is a cell-thick
four-colour ring around a buff page with the digits punched out as *unlit*
quadrants, which works because one colour plus a 2×2 on/off mask per cell is
exactly what SG4 gives you. The small one is four cells on two rows — a page
with a brand-coloured post at each corner — and it costs the header no rows,
because the header is two rows whatever goes in it.

**`plat_ticks()` extends a 16-bit counter.** Color BASIC's 60 Hz TIMER at
`$0112` wraps every eighteen minutes, and `clock.c` treats a backwards step as a
wrap and resets its baseline — throwing away everything since. So the backend
folds it into a monotonic 32-bit count, which is only correct if somebody calls
it once per wrap. That is why `plat_getkey()` polls `inkey()` around
`plat_vsync()` rather than calling CMOC's `waitkey()`: `plat_vsync()` folds, so
the count keeps rising while the event, picker and settings screens wait. It is
the Apple II's reason for polling arriving at the same answer from the other
direction.

**`clock_get_tz()` does not exist on this bus.** fujinet-lib declares it for
every platform and builds it for some; the CoCo archive holds
`fn_clock/clock_get_time.o` and nothing else, so calling it is an undefined
symbol at link — hence `GC_NO_CLOCK_TZ`. The settings screen must not then
print "(unset)": the timezone may be perfectly set and we simply cannot read it
back. It shows the FujiNet clock's own reading instead, which is the observable
*consequence* of the same `[General] timezone` and the actual symptom anyone
would check.

**The program is linked at `$1000`, not `$0E00`.** With Disk BASIC present a
BASIC program lives at `$0E00`, so `LOADM` into `$0E00` destroys the line that
is running it. fujinet-news and fujinet-config pay for that address with a
second-stage loader that pokes BASIC's direct-mode buffer and jumps into RUNM;
that trick is ROM-sensitive and gives `?UL ERROR` on stock Disk BASIC 1.1.
Giving BASIC 512 bytes is a cheaper price than a second binary that only works
on some ROMs, and it still leaves about 1.5K spare under the `$7C00` ceiling.
`--limit` is what turns "silently corrupts the stack" into a build failure, and
`plat_shutdown()` cold-starts rather than returning, because there is nothing
left to return to.

**A DriveWire transfer stops the clock.** `dwread` jumps through `[$D93F]`,
which masks interrupts for the duration, so TIMER itself stops and the wall
clock loses the length of every fetch. That is the Apple's SmartPort situation
exactly, and the half-hour resync in `clock.c` is what bounds it.

**The compose editor is append-and-backspace, in uppercase.** The left arrow
*is* the erase key on this keyboard — BASIC's own convention — which leaves no
key to walk the cursor with, and a 24-cell field window does not miss one.
`inkey()` yields the 6847's uppercase-only set, which is also all the screen
could echo, so that is what events typed here are titled in.

**Compose is what re-drew the memory map.** The form costs ~4K of 6809 code
on a machine that had a kilobyte and a half spare, so everything moved: the
org came down to `$0E80`, the C stack moved to `$0D00` — growing down through
Disk BASIC's buffer space, which only matters while `LOADM` itself is running
— and `--limit` rose to `$7F00` where the stack's old top-of-memory gap used
to be. The form buffer and the picker's list overlay `gc_det`, and
`MAX_EVENTS` (24), `TITLE_LEN` (36), `DET_ROWS` (22), `GC_RXBUF` (96) and
`LINE_CAP` (96) each gave up a notch; the Makefile carries the reasoning per
knob. The link ends within a few dozen bytes of the ceiling by construction —
`--limit` turns any future overrun into a build failure, which is the check.

## Adam implementation notes

**Every one of Google's eleven colours gets its own hue.** GRAPHICS II is a
256×192 bitmap whose foreground and background are settable per 8×1 strip out of
fifteen inks, and z88dk lays the name table out linearly so all 768 cells own
their own eight pattern bytes and their own eight colour bytes. So this is the
first backend that does not have to quantise: `color_chip()` is never called in
`src/adam/`, and `ink_for_color()` maps `e->color` straight onto an ink. The
Atari and the CoCo both collapse eleven names onto five because they run out of
colours; the Apple has none at all.

Ten of the eleven pick themselves. Graphite is the awkward one — it is
`#616161`, a *dark* gray, and the TMS9918A's only gray is `#CCCCCC`, which
against the white page is barely a chip. It takes black instead, which is
further from the hex and much closer to the intent, and that frees gray for
`COL_NONE` — so an event with no `colorId` now reads as a faint chip rather than
as an explicitly graphite one.

**The SmartKeys pay for two content rows.** The band is rows 21–23 and belongs
to smartkeyslib, which leaves twenty-one — but the Atari and the Apple each
spend a header row on a tab strip and the CoCo spends its whole footer on
`1-4:VIEW`, and none of that is needed when the machine has six labelled keys
with captions on the screen. The header is three rows instead of two, and the
third names the calendar being shown: a persistent piece of state that changes
what every other row means, and which until now was visible only on the settings
page — exactly where nobody looks when a day comes back emptier than expected.

**MONTH is a real grid.** Seventeen content rows hold six bands on a two-row
pitch, seven four-column cells across, with the day number on one row and a
density bar under it. The bar is thirty-two pixel columns and each event lights
four, in the *true* colour of the day's leading event. The CoCo manages sixteen
quadrant steps quantised to five colours and the Atari four monochrome ones.

That last part is why `gc_daycol` is a `COL_*` and not a chip. It used to hold
the quantised chip, which threw away the difference between Peacock and
Blueberry at parse time — free for the three backends that quantise anyway, and
a real loss here. A backend that wants the chip now calls `color_chip()` on it,
which is where that decision belongs.

**The mark is four hardware sprites, and four is the ceiling.** A TMS9918A shows
at most four sprites on any scanline and silently drops the fifth — and a sprite
occupies every line its 16-pixel box covers whether or not its colour is
transparent. So the small mark is four 16×16 sprites stacked on one spot, in
Google's four brand colours, carrying disjoint pixels of one ring; the large one
is the same four laid out 2×2. A fifth colour would cost the first sprite that
shared a line with it.

Which makes ending the sprite list load-bearing. `vdp_set_mode(2)` clears VRAM,
so slots 4 to 31 read `y=0` and sit across scanlines 1 to 16; unterminated,
twenty-eight invisible sprites push the mark off its own budget. `logo_init()`
writes `y=208` into slot 4 once, and `logo_hide()` writes it into slot 0.

The "31" is deliberately not a sprite. The large mark spells it with two
ordinary characters in the cells the ring encloses, the way the Atari and the
Apple do; the small mark's interior is twelve pixels square and will not hold
two glyphs, so it carries a hand-drawn pair in the pattern table. Either way the
digits cost no sprite.

**fujinet-lib has no clock for this bus at all.** Not `clock_get_time`, which
the CoCo does have, and not `clock_get_tz`, which it does not — `adam/src/` has
`fn_network/` and `fn_fuji/` and no `fn_clock/`. A calendar client with no clock
has nothing to put in the date field of any device spec it builds.

It does not need one. The firmware already answers this on the Fuji device
rather than on a clock device: `adamFuji.cpp` dispatches `FUJI_GET_TIME`
(`0xD2`) to `adamnet_get_time()`, which replies with
`fujiClock::get_current_time_simple()` — and that is `SIMPLE_BINARY`'s seven
bytes exactly, resolved through the same `[General] timezone` the GCAL adapter
resolves its windows with. So `src/adam/clock_adam.c` is a seven-byte read, not
a conversion, and `src/clock.c` needs no change. Only `GC_NO_CLOCK_TZ` is
required, as on the CoCo, and the settings screen shows the clock's own reading
for the same reason.

**Column 0 stays out of the selection bar, for a fifth unrelated reason.** Here
it is the plainest of the five: the chip's attribute byte *is* a Google colour
and the selection bar's is dark blue, so a chip inside the bar stops being the
event's colour. The Atari keeps its column 0 out because an inverse space is
COLPF1 and covers the player; the Apple because MouseText has no inverse form;
the CoCo because XOR `$40` on a semigraphics byte recolours it; the
Intellivision because the colour-stack run has to continue past the selection.

**Colour and glyphs are written by different means, and every field repaints
both.** Glyphs go through z88dk's console, which knows how to blit a font cell
into eight pattern bytes anywhere; colour is `vdp_vfill` straight into the
attribute plane, one call per run. The console keeps its own notion of the
current colour and changes it whenever anything else prints, so a field whose
colour came from whatever `vdp_color()` was last called with is a field whose
colour is a function of paint order. Repainting the run makes each field depend
on its own arguments and nothing else.

**Nothing paints below row 20.** `scr_clear()` clears twenty-one rows rather
than calling `clrscr()`, which would take the SmartKeys legend with it and mean
asking smartkeyslib to paint it again. `sk_bind()` likewise suppresses the
repaint when the legend has not actually changed, which is what keeps a
`smartkeys_display()` — it clears and redraws all three rows — off every
`ui_view()`.

**The time column is seven wide, and it costs the title two.** `All day` is
seven characters and there is no shorter spelling of it that reads: the CoCo
abbreviates to `ALLDY` because thirty-two columns of uppercase have nothing to
spare. Two characters of title column is the cheaper price. The buffer that
holds it is `W_TIME + 1` for exactly that reason — the label is as wide as the
column, so the two have to move together.

**The frame count is not stopped by a transfer.** The VDP raises NMI once per
frame and z88dk keeps a chain of up to eight handlers on it, so counting frames
costs nothing and does not displace smartkeyslib's sound handler. EOS masks
maskable interrupts around an AdamNet transfer, but the VDP's is an NMI and
cannot be masked — so unlike the CoCo's DriveWire and the Apple's SmartPort, a
fetch here does not lose the wall clock. `plat_ticks()` still reads its counter
twice and retries on a disagreement, because four bytes is not one instruction
and `clock.c` reads a backwards step as a wrap and discards everything since.

**fujinet-lib's Adam `fuji_*` calls return their booleans inverted, and this
one is on the critical path.** Thirty-seven of the `bool`-returning entry points
in `adam/src/fn_fuji/` return `fujiError_t` codes — so `FN_ERR_OK`, which is
zero, comes back as `false` and `FN_ERR_IO_ERROR` as `true`. The backend was
written to the `uint8_t` convention the `network_*` half uses and then declared
with the `fuji_*` half's.

`main.c`'s `have_fujinet()` probes with `fuji_get_adapter_config_extended()`
precisely because it is something only a real adapter can answer, so the symptom
is a FujiNet that is present, answering, and logging `Fuji cmd: GET ADAPTER
CONFIG EXTENDED` while the client insists it is not there.
`src/adam/fuji_adam.c` defines a corrected version, which leaves the library
member unreferenced so the linker never pulls it — the same trick
`clock_adam.c` uses for a function the archive does not carry at all. Delete it
once upstream returns real booleans. The only other library calls this client
makes, `fuji_read_appkey()` and `fuji_write_appkey()`, are among the correct
ones.

**With no FujiNet answering, the client sits on the splash screen.** It looks as
though the cause is fujinet-lib wrapping each call in `while (1) { if (err ==
ADAMNET_TIMEOUT) continue; }`, but that loop is unreachable. eoslib spins one
level further down: `eos_write_character_device()` restarts itself internally
until the device settles and only ever returns a settled status, so
`ADAMNET_TIMEOUT` never reaches any caller on this bus. `ui_notfound()` is
therefore unreachable without a presence check that does not go through
`eos_write_character_device()` at all — which belongs in eoslib or fujinet-lib,
not here. Every Adam FujiNet client shares this.

**The chime is the machine's own.** smartkeyslib knows where the SmartWriter
ROM keeps its sound effects, so the three notes `alarm.c` asks for are three
SmartWriter fragments that escalate rather than three pitches — a synthesised
triad would be the one sound on the machine that did not belong to it. It is
also the only backend whose chime is *started* rather than played:
`smartkeys_sound_play()` queues a fragment and the raster interrupt advances it,
so the banner keeps flashing while the sound runs. `plat_silence()` is
deliberately empty — EOS `$FD53` is TURN_OFF_SOUND, which shuts the engine down
rather than the note, and the handler would go on calling `eos_play_sound()`
into a dead engine for the rest of the run.

**The compose form's SmartKey bank carries editor codes.** `SK_FORM`'s slots
hold `E_*` values rather than `K_*` ones, and `plat_getch()` reads the same
`sk_key[]` table `sk_bind()` fills — so `Save` and `Done` are labelled keys
exactly as every other action on this machine is, with no second mechanism.
`Save` skips the save-or-discard ask that `Done` poses. Form messages go on
the status row (20), leaving the band's legend in place while they show. The
compose code also ended the old 3.8K of headroom: the form and picker overlay
`gc_det` here too, `DET_ROWS` came down to 40, and `GC_RXBUF` and `LINE_CAP`
took the CoCo's cheap trades — check `__BSS_END_tail` against `$C800` after
touching anything.

## MS-DOS implementation notes

Open Watcom `wcc`, 8086 code, small model, BIOS text modes only — which is
what lets one `GCAL.EXE` run on anything from a PCjr to a 486, on a CGA, an
MDA, a Hercules or anything later that emulates them.

**The screen width is not known until the program is running.** Every other
backend compiles its geometry in; a PC inherits whatever text mode it was
started in — 40×25 in modes 0/1, 80×25 in 2/3, the MDA's mode 7. So
`scr_cols` and `scr_wide` are variables probed in `plat_init()`, `ui_geom()`
fills in the column layout — the Apple's at 80 with the category column, the
Atari's at 40 without — and the wrap width goes through the `GC_RT_COLS` hook
in `gcal.h`: `DET_COLS` stays 78 and goes on sizing `gc_det`'s stride, while
`gc_wrap_cols` carries the 38 or 78 the text is actually wrapped to. Wrapping
narrower than the stride is safe; the reverse would be an overrun, which is
why `DET_STRIDE` stays derived and non-overridable. Rows are still
compile-time — every mode here is 25 of them, one more than the Apple, spent
on a two-row detail panel, a taller detail window and a month summary row
with a blank above it. `/40` and `/80` on the command line force a width;
EGA/VGA 43- and 50-row modes are put back into mode 3 rather than teaching
the views a third geometry.

**The adapter, not the mode, decides where the text page is.** Bits 4–5 of
the BDA equipment word are `11` for a monochrome adapter, and that is the
authoritative test: an MDA or Hercules machine is not necessarily *in* mode 7
when the program starts — dosbox-x's hercules machine boots reporting mode
3 — but its page is at `B000` regardless, and a probe that trusted the mode
wrote 4,000 bytes into an address no hardware was decoding.
`MACHINE=hercules tools/msdos-shot.sh` is that bug's regression test.

**Painters name a role, not a byte.** The Adam's attribute roles, grown to
eleven and resolved through one of three tables picked at init: colour,
black-and-white (modes 0/2, or `/MONO` for the LCD and composite screens that
render colour as mud), and MDA. Colour is Google Calendar's own reading
quantised to CGA — black text on a light-grey page under blue chrome bands —
rather than the gmail client's blue desktop. The MDA table is where mode 7
earns its own column: reverse video for the bars and the selection, intensity
for the active tab and emphasis, and a real underline — the one attribute no
other adapter in this repo has — under the detail title, the panel headings,
and today's day number and column head (`0x09`, bright *and* underlined).

**Eleven inks, no quantisation.** In colour mode `ink_attr()` gives each of
Google's eleven colour names its own CGA foreground on the page — the Adam's
`ink_for_color()` arrangement, affordable for the second time. Two trades
worth naming: Tangerine gets brown, the only orange CGA has, and Graphite
gets true black, visible because the page behind it is light grey. The chip
gutter, the WEEK strip and the MONTH density bars all draw from it, so the
month grid says *what kind* of busy as well as how much. The two monochrome
tables fall back to `color_chip()`'s five-way quantisation rendered as a
CP437 density ramp — `█ ▓ ▒ ░ ■` — legended on the settings screen, which in
colour lists all eleven names against their inks instead.

**The mark is cells, not sprites.** A white page with `31` on it, ringed in
the four brand colours as CP437 full blocks — the gmail client's row-table
technique with one extension: the page cells are *painted* rather than
skipped, because on the monochrome tables the page has to be laid down as
reverse video or the mark would be a ring around a hole. Bright ring, reverse
page, dark digits — the Apple's one-bit rendering, arrived at from the other
direction.

**Cells are written straight into the text page.** INT 10h writes one cell
per two interrupts and a full repaint is 4,000 of them, visible on a 4.77 MHz
8088. The one machine direct writes upset is the genuine IBM CGA, which snows
in 80-column text; `/SNOW` gates every write on the start of a horizontal
retrace for that card — a switch rather than a heuristic, because there is no
reliable way to detect a true CGA and everything else would pay the wait for
a fault it does not have.

**One clock, 18.2 ticks a second.** The BDA tick count at `0040:006C` is the
PC's RTCLOK: it advances in the background whatever the program is doing, so
the wall clock stays honest across every blocking screen with nobody pumping
a counter the way the Apple backend must. `plat_fps()` says 18 — calling it
60 would run the clock at a third speed — and the ~1% left over is absorbed
by the half-hour resync. The consequence is that `alarm.c`'s frame constants,
tuned at 60, run the banner ~13 seconds and the chime ~1.3; acceptable for an
alarm, and preferred over a second timing path, because the MDA has no
vertical-retrace bit to build one from. The chime itself is the 8253's
channel 2 gated through port `61h` — no CPU in the loop, which is why
`plat_silence()` genuinely matters here and `plat_shutdown()` calls it too.

**Everything on the bus goes through `INT F5` into FUJINET.SYS, and
fujinet-lib 4.11.2 gets three parts of that wrong.** Each fix is a file in
`src/msdos/` that shadows the archive member — the `src/adam/` pattern — and
each says in its header when it can be deleted:

- `net_msdos.c` — the driver speaks the FujiBusPacket protocol, in which `DH`
  describes how the aux bytes become typed parameters, and the library always
  sends `DH=0`: no parameters. Calls that need none work by luck; an open
  arrives as `Insufficient open paramaters: 0` in the firmware log and a NAK
  on the wire. `network_open()` gets its own bus entry with `DH=2` and sends
  the devicespec at its real length (the firmware takes the payload into a
  `std::string` verbatim, stack garbage and all), `network_read()` gets
  `DH=5`, and `network_error()` stops returning success from a failure path.
- `fuji_msdos.c` — guards the first bus call with `_dos_getvect(0xF5)`,
  because without FUJINET.SYS resident the vector is null and `int86x`
  through it jumps to `0000:0000` — a crash, not an error return; that check
  is what makes `ui_notfound()` reachable enough to name CONFIG.SYS. It also
  rewrites both appkey calls: the archive's treat the bus reply as a boolean,
  and `'E'` and `'N'` are as nonzero as `'C'` — so a *missing* key, which is
  every first run, read back as 66 bytes of garbage adopted as settings. (The
  read's failure path also assigned `count = 0` to the pointer rather than
  the count.)
- `clock_msdos.c` — the archive has no `fn_clock` at all on this bus.
  `clock_get_time()` is a seven-byte `'T'` read from device `0x45`, the atari
  fn_clock's own command table; `clock_get_tz()` is the `'L'` length read and
  the `'G'` string read from the same device. If a live run shows the RS-232
  firmware not answering `'L'`/`'G'`, delete the tz shim and add
  `-DGC_NO_CLOCK_TZ` — the CoCo's gate; `ui_setup` already carries the
  clock-reading fallback under that flag.

**A failed open reports its real error.** The portable `open_error()` was
written for the Atari bus, where the SIO layer leaves the protocol's status
byte in `fn_network_error`. The INT F5 layer never writes it, so every failed
open — including the 212 *authorize Google in the Web UI* a first-time user
is guaranteed to hit — would have reported as a timeout. The `__MSDOS__`
branch re-probes the still-addressable channel once and reports what it says,
restoring the open's own device code afterwards.

**The disk is a driver disk without DOS.** `gcal.img` carries `GCAL.EXE`,
`FUJINET.SYS`, `FUJIPRN.SYS`, `FCONFIG.COM`, a `CONFIG.SYS` and an
`AUTOEXEC.BAT`; `mformat` lays no system tracks, so `SYS A:` it from a DOS
disk or copy the files onto one. The driver parts are built from the
fujinet-msdos repo as named targets inside the same defoogi run (its own
`disk` target grew nasm dependencies, and defoogi ships `wasm`). The
`CONFIG.SYS` is this project's own rather than the clone's verbatim copy, for
one line: `FUJI_PORT=2`, because the 86Box IBM 5160 this disk is tested on
wires its FujiNet (BoIP) to serial 2 — COM1 is the virtual console — and the
PCjr needs COM2 as well, its internal UART sitting at the COM2 address. On a
machine with the FujiNet cabled to COM1, change it back.

**The budget is DGROUP, not address space.** One 64K group holds every
static and the 4K stack; the link map (`r2r/msdos/gcal.map`, kept by
`OPTION map=`) puts it at about 19K, and the `-DGC_FAKE_DATA` build — which
links the canned wire data alongside the real transport — at just under 20K.
Check it there before raising `MAX_EVENTS` or `DET_ROWS`.

**`network_write` is the third shimmed call.** Like the open and the read
before it, the library's version sends no FujiBusPacket parameter descriptor;
the draft channel's writes need the length as one u16 (`DH=5`, the read's own
shape) with command `'W'`, so `net_msdos.c` carries the corrected entry. In
the form, `TAB` joins `RETURN` as next-field — what DOS fingers expect — and
the form's geometry follows `ui_geom()` like everything else: the 80-column
layout is the Apple's, the 40-column one the Atari's.

## Testing

`make -C tests` builds the portable core natively, five times, and runs about
200 assertions over the date arithmetic, the whole-token colour match, the
listing parser's column derivation, the line splitter, the agenda builder, the
wrap and sanitize helpers, the detail ingest and every one of the alarm firing
rules. None of it needs a 6502.

Five times, because the core's fixed widths are overridable and the backends do
override them. `hosttest` is the Atari's shape; `hosttest80` the Apple II's,
which is the only way the reflow path is covered at all — it compiles out
entirely without `DET_REFLOW`; `hosttest32` the CoCo's, which is the only one
with `MAX_EVENTS` at anything but 64 and so the only one where the truncation
and agenda-overflow assertions mean anything; `hosttestadam` the Adam's,
which is the only one that pairs a 32-column detail with the default 64 events
and a 48-row buffer — the CoCo is 32 columns but pays for it with half the
events and half the rows, so this is where the widest index and the narrowest
wrap meet; and `hosttestdos` the MS-DOS shape, the only one where the wrap
width is a runtime variable — it drives the same buffer at 38 and then 78,
which is the only coverage the `GC_RT_COLS` hook gets at all.

One assertion differs by shape rather than passing everywhere, and it is the
honest kind: text the adapter already wrapped to 38 columns passes through
untouched at 40 and 78, and at 32 it cannot — the client necessarily re-wraps,
and what is worth asserting there is that the words survive it.

`src/lines.c` exists because of one of those shapes. `Protocol.h`'s
`lineEnding` is per bus and they do not agree: SIO leaves it at `$9B`, but
`iwm/network.cpp` — the Apple II's — sets CR, and so do DriveWire and AdamNet.
The splitter used to treat CR as the leading half of a CRLF and drop it, which
ran every line of an Apple II listing into the next: the window title swallowed
the header row and not one event was parsed. It lives in its own file so that
is a host assertion rather than a hardware surprise.

`tools/atari-shot.sh` runs the client headless in `atari800`, breaks where it
blocks on the keyboard, dumps all 64K and decodes the text screen, the display
list and the player/missile buffer out of it:

```
tools/atari-shot.sh                    # canned data, first screen
tools/atari-shot.sh "K_VIEW3"          # canned data, scripted keys
REAL=1 TMO=300 tools/atari-shot.sh     # against a real FujiNet or fujinet-pc
```

The canned data is real wire text — window title, a dashed header with the `#`
at the right index, properly spaced columns — pushed through the same line
splitter and parser the network path uses, so a headless run exercises them
rather than bypassing them. The decoder renders inverse spaces as `#`, because
the selection bar and the month view's density bars are made of nothing else.

`tools/apple2-shot.sh` is the counterpart, and takes the same arguments:

```
tools/apple2-shot.sh                   # canned data, first screen
tools/apple2-shot.sh "K_VIEW3"         # canned data, scripted keys
REAL=1 WAIT=45 tools/apple2-shot.sh    # against a real FujiNet or fujinet-pc
```

`tools/coco-shot.sh` is the third, same arguments again:

```
tools/coco-shot.sh                     # canned data, first screen
tools/coco-shot.sh "K_VIEW3"           # canned data, scripted keys
REAL=1 TMO=300 tools/coco-shot.sh      # against fujinet-pc-coco
```

It drives [xroar](https://www.6809.org.uk/xroar/) over its GDB target, waits
until the CPU is inside `plat_vsync()` — where the program comes to rest once
its scripted keys are spent — and reads the 512 bytes of the text page out of
the running machine. That is the *entire* visual state, so `coco-decode.py` can
report every cell's glyph, its video sense and, for a semigraphics cell, its
colour and quadrant mask. The chip gutter, the WEEK chip strip, the MONTH
density bars and the "31" punched out of the mark are all checkable that way,
and not one of them would be from a picture of text. It is better instrumented
than either of the other two.

Point `XROAR` at the binary if it is not at `~/Workspace/xroar-1.5.5/src/xroar`,
and `ROMPATH` at Color BASIC plus an HDB-DOS DriveWire ROM — `REAL=1` needs the
latter, because `dwread`'s `[$D93F]` vector only exists there. xroar's Becker
port defaults to 65504 and so does fujinet-pc-coco's boip port, so the two find
each other with no configuration.

`tools/adam-shot.sh` is the fourth, and the one that cannot work the way the
other three do. ADAMEm has no monitor and no GDB stub, so there is nothing to
break in. What it
has instead is a snapshot format carrying all 16K of VRAM and an `-autosnap`
mode that writes one at shutdown — so the recipe is to run the machine blind
under SDL's dummy video and audio drivers, stop it after a fixed wall-clock
time, and decode the screen out of the state it left behind:

```
tools/adam-shot.sh                     # canned data, first screen
tools/adam-shot.sh "K_VIEW3"           # canned data, scripted keys
REAL=1 WAIT=45 tools/adam-shot.sh      # against fujinet-pc-adam
```

The capture is therefore timing-based rather than event-based, which is the one
respect in which this harness is weaker than the other three; `WAIT` is generous
by default for that reason.

`tools/adam-decode.py` renders the result as a PNG, because on this machine a
picture is the honest output — the screen is a bitmap and the parts of the
client that only exist here are not checkable from glyphs. Two text panes come
with it: the background ink of every cell, which is what shows the header band,
the selection bar, the chip gutter and the MONTH bars without needing to
recognise a character; and the sprite attribute table with a per-scanline count.

That second pane is the one worth reading. It is what caught the unterminated
sprite list — twenty-eight invisible sprites sitting on scanlines 1 to 16 and
pushing the mark past the hardware's four-per-line budget — which is invisible
in a screenshot precisely because the emulator, unlike the hardware, draws all
of them.

It drives `applen`, the ncurses frontend of the
[FujiNet fork of AppleWin](https://github.com/FujiNetWIFI/AppleWin), which is
not built by default — configure that tree with `-DBUILD_APPLEN=ON` and point
`APPLEN` at the result. It shares the emulator core with `sa2`, so the
SmartPort device relay works from it and a `REAL=1` run reaches fujinet-pc.

What gets decoded is a save state, not the terminal: `applen`'s own
`MapCharacter()` folds screen codes `$00-$1F` and `$40-$5F` onto the same
reversed `@`-`_`, so inverse uppercase and MouseText come out identical, and
those are the two things worth checking. Three things about driving it are easy
to get wrong and all three are documented in `tools/apple2-run.py`:
`--state-filename` is ignored unless the file already exists, `--headless`
never creates the window that F11 is read through, and `set_escdelay(0)` means
the F11 sequence has to arrive in one write.

Four things about driving it are easy to get wrong and all four are documented
in the two scripts: `-timeout` counts *emulated* seconds, so pairing it with
`-no-ratelimit` makes it fire before the machine has booted; the RETURN in
`-type` must be `\r`, because `\n` maps to the CoCo's DOWN ARROW and types the
command without entering it; the GDB listen backlog is one deep and `accept()`
runs off the emulator thread, so the first `?` has to be retried; and `Z0`
breakpoints are accepted, answer `OK`, and never fire — which is why the
capture polls the PC instead.

`tools/msdos-shot.sh` is the fifth harness and by far the easiest, because a
DOS program needs no debugger to give its screen up: the `GC_SHOT` hook in
`src/msdos/input.c` dumps the program's own text page to `SCREEN.BIN` exactly
where it would otherwise block for a key, and dosbox-x's only job is to
exist. The view loop polls, but under `GC_FAKE_DATA` `plat_getkey_poll()`
delegates to the blocking read once the scripted keys are spent, so every
screen funnels into the same catch point; only the alarm banner cannot be
captured this way, because it needs the loop to keep turning.

```
tools/msdos-shot.sh                          # canned data, DAY view
tools/msdos-shot.sh "K_VIEW3"                # canned data, scripted keys
MODE=40 tools/msdos-shot.sh                  # 40 columns    (gcal /40)
MODE=mono tools/msdos-shot.sh                # the B&W table (gcal /mono)
MACHINE=hercules tools/msdos-shot.sh         # the MDA path, mode 7
MACHINE=pcjr tools/msdos-shot.sh             # the PCjr's BIOS
```

`MACHINE=hercules` is the capture that earns its keep: dosbox-x's hercules
machine boots claiming mode 3, so it is the regression test for the
equipment-word probe. `tools/msdos-decode.py --attrs` prints the attribute
bytes alongside the glyphs, which is the only way to see the `0x70` bars, the
colour inks, the `0x01` underline under the detail title and the `0x09`
bright underline on today — none of them are visible in the text. The capture
header carries the BIOS mode the probe saw, because a capture that cannot say
"that was really mode 7" cannot check the MDA path at all.

The script clobbers the msdos objects before and after itself:
`MSDOS_SHOT_FLAGS` changes every object and make cannot see a flag change, so
run `defoogi make msdos` afterwards to get the shipping binary back.

## Layout

```
src/            portable core
  gcal.h        every shared type and the plat_* / ui_* contract
  main.c        boot and the nested screen loops
  net.c         device specs, the streaming read, the draft channel
  form.c        the compose form's model, validation and wire format
  compose.c     the compose/edit screen loop and line editor
  index.c       the listing parser
  lines.c       the line splitter, and the bus disagreement behind it
  color.c       Google's eleven colour names
  date.c        civil date arithmetic
  agenda.c      the separator-interleaved display list
  detail.c      event-detail ingest
  clock.c       the wall clock
  settings.c    alarm lead and calendar selector, in an appkey
  alarm.c       the firing scan and the banner state machine
  wrap.c        greedy word wrap
  sanitize.c    charset clamping
  model.c       definitions of the shared arrays
src/atari/      Atari 8-bit backend
  platform.h    geometry, palette, internal API
  screen.c      the screen-RAM blitter
  ui.c          chrome, flat screens, picker, settings
  views.c       the four views, event detail, alarm banner
  dli.c         colour bands
  dlihw.s       the interrupts and the vertical blank hook
  pmg.c         the mark and the colour chips
  input.c       key mapping
  key.s         the blocking read
  sound.c       the chime
  timer.c       frame timing
src/adam/       Coleco Adam backend
  platform.h    geometry, the VRAM map, the ink table, internal API
  screen.c      the GRAPHICS II blitter and the eleven Google inks
  logo.c        the mark, in four hardware sprites
  ui.c          chrome, flat screens, picker, settings, SmartKey legends
  views.c       the four views, event detail, alarm banner
  input.c       key mapping and the SmartKey banks
  clock_adam.c  clock_get_time(), which fujinet-lib does not ship here
  fuji_adam.c   the adapter probe; the library's returns an inverted bool
  sound.c       the chime, out of the SmartWriter ROM
  timer.c       frame timing off the VDP's NMI
  system.c      bring-up and teardown
src/apple2enh/  Apple //e (enhanced) backend
  platform.h    geometry, MouseText codes, internal API
  screen.c      the 80-column blitter and the screen-code mapping
  blit.s        the aux/main column split
  ui.c          chrome, flat screens, picker, settings
  views.c       the four views, event detail, alarm banner
  logo.c        the "31" mark
  input.c       key mapping and the polling blocking read
  timer.c       the frame counter we keep ourselves
  sound.c       the chime
  tone.s        the square wave
src/coco/       Tandy Color Computer backend
  platform.h    geometry, the SG4 byte map, internal API
  screen.c      the blitter, the 6847 screen-code mapping, raw SG4 access
  ui.c          chrome, flat screens, picker, settings
  views.c       the four views, event detail, alarm banner
  logo.c        the mark, with "31" punched out of it
  input.c       key mapping and the polling blocking read
  timer.c       the 16-bit TIMER, extended to 32 bits
  sound.c       the chime
  include/      a <string.h> shim, because CMOC ships none
src/msdos/      MS-DOS backend (Open Watcom, 8086, small model)
  platform.h    runtime geometry, the eleven attribute roles, internal API
  screen.c      the video probe, the three attribute tables, the blitter
  ui.c          chrome, flat screens, picker, settings, ui_geom()
  views.c       the four views, event detail, alarm banner
  logo.c        the mark, page and ring in CP437 cells
  input.c       INT 16h key mapping, the poll, the GC_SHOT hook
  timer.c       frame timing off the BIOS tick at 0040:006C
  sound.c       the chime, on the 8253's channel 2
  clock_msdos.c clock_get_time()/clock_get_tz(), which fujinet-lib lacks here
  fuji_msdos.c  the INT F5 vector guard and the corrected appkey calls
  net_msdos.c   the FujiBusPacket DH field descriptors the library omits
  CONFIG.SYS    the driver lines, FUJI_PORT=2 for this 86Box XT
  AUTOEXEC.BAT  starts GCAL
tests/          host-native tests, built at all five screen shapes
tools/          headless capture and decode, per platform
mekkogx/        the cross-platform build template
```

`SRC_DIRS = src src/%PLATFORM%` globs both directories, so adding a file is all
that is needed to build it.
