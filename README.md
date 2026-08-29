# FujiNet Google Calendar

A Google Calendar client for 8-bit machines, talking to the `GCAL:` network
protocol adapter in [fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware).

Two implementations live here:

- `intv/` — the original, in IntyBASIC for the Intellivision.
- `src/` — a C port. The portable core is in `src/`, the machine-specific half
  in `src/<platform>/`. Atari 8-bit and Apple II backends exist so far.

Day, week, month and agenda views; an event detail screen; a calendar picker; a
settings page; and alarms synthesised on the client, because the adapter's
field mask never asks Google for reminders.

## Building

```
defoogi make atari          # -> r2r/atari/gcal.com and r2r/atari/gcal.atr
defoogi make apple2enh      # -> r2r/apple2enh/gcal.a2s and gcal.po
make -C tests               # host-native tests of the portable core
```

[defoogi](https://github.com/FozzTexx/defoogi) carries cl65, `dir2atr`, `atr`
and `ac`, so no host toolchain is needed; plain `make atari` works too if you
have them. fujinet-lib is fetched into `_cache/` automatically.

The Apple II target is `apple2enh` — an enhanced //e with 80-column hardware.
`apple2` is not a build of this: it is the unenhanced machine, with no
MouseText and a character generator that would render the chip column as
random inverse capitals.

## Using it

| Key | |
|---|---|
| `1` `2` `3` `4` | day / week / month / agenda |
| `0` | jump to today |
| `←` `→` | previous / next period |
| `↑` `↓` | move the selection |
| `RETURN` | open the selection |
| `ESC` | settings, or back |
| `R` `Q` | refresh / quit |

The cursor keys need Ctrl held down on an Atari, so the bare keycaps they live
on — `-` `=` `+` `*` — are accepted too. An enhanced //e has real arrow keys
and needs no such workaround.

The header carries a tab strip showing which digit selects which view, which is
what keeps the footer to a single row; the Intellivision needed two hint pages
at twenty columns.

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
enabled for the project and `calendar.readonly` in the grant's scope.

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

## Testing

`make -C tests` builds the portable core natively, twice, and runs about 200
assertions over the date arithmetic, the whole-token colour match, the listing
parser's column derivation, the line splitter, the agenda builder, the wrap and
sanitize helpers, the detail ingest and every one of the alarm firing rules.
None of it needs a 6502.

Twice, because the core's fixed widths are overridable and the backends do
override them: `hosttest` is the Atari's shape and `hosttest80` the Apple II's,
which is the only way the reflow path is covered at all — it compiles out
entirely without `DET_REFLOW`.

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

## Layout

```
src/            portable core
  gcal.h        every shared type and the plat_* / ui_* contract
  main.c        boot and the nested screen loops
  net.c         device specs, the streaming read, canned data
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
tests/          host-native tests, built at both screen shapes
tools/          headless capture and decode, per platform
mekkogx/        the cross-platform build template
```

`SRC_DIRS = src src/%PLATFORM%` globs both directories, so adding a file is all
that is needed to build it.
