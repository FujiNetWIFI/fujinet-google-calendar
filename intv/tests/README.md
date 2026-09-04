# Scripted UI tests

`tools/intv_screen.py` drives jzIntv's debugger with no display and no hands:
it steps the program one frame at a time by breakpointing something reached
exactly once per frame, feeds controller events through the injection hook in
`in_poll`, dumps `#BACKTAB` and decodes it back to 12 rows of 20 characters.

```sh
make test FUJINET=localhost:9995        # everything in tests/
make test-one T=tests/t9.txt FUJINET=localhost:9995
```

**Everything except `boot.txt` needs a FujiNet to talk to.** `gcal` halts at
boot without a clock, so there is no screen to test past that point. Point
`FUJINET` at a `fujinet-firmware` RS232 build with `[BOIP] enabled=1`,
`enable_apetime=1`, and a **POSIX** `[General] timezone=` -- an IANA name like
`America/Chicago` is rejected by the adapter's own parser and silently falls
back to UTC, which looks identical to not setting it.

| Scenario | What it pins down |
|---|---|
| `boot.txt` | The cart boots. This is the memory-map guard: compose added a fourth ROM window and two more `ASM ORG`s, and getting that wrong produces a ROM that assembles, links, and fails EXEC's boot detection with nothing on screen. |
| `form.txt` | Keypad 5 opens a blank form, all seven fields on rows 4-10, date prefilled from the anchor. |
| `t9.txt` | `6338464` predicts `meeting`; the editor leaves rows 4-10 alone; ENTER accepts the field without falling through to the form's save. |
| `validate.txt` | The masked numeric editor lays down `09:30`; an untitled compose is refused before a channel is opened. |
| `views.txt` | All four views still render — this guards the `#evrec` merge, where five per-screen record pointers became one. |
| `keypad.txt` | A keypad press does not move the disc. The two share eight lines, so `4`/`5`/`6` used to step the period on every press. Asserted with `expect-same` — row 0 is the clock's date, so spelling it out passes only on the day it was written. |
| `t9keypad.txt` | The same aliasing inside the editor: a letter key must not cycle the candidate list. |
| `t9hold.txt` | A key that is still down must not swallow the next one. Two keys OR into a byte that is not any key, so `CONT.KEY` decoded a held `8` plus a pressed `3` as nothing and then re-fired the `8` on release — every later key read as the first one. `in_poll` decodes the bits *added* since the last confirmed byte instead. |
| `t9btn.txt` | The third face of it, on bits 5-7: two keys held together OR into a button pattern, which used to flip T9/ABC on every keystroke. The ghosts are `rawhold` and carry their column bits — an earlier version used one-frame `raw` of the bare button bytes and passed against the broken ROM. Also pins that a real button still works, and that the form's selection bar survives underneath the editor. |

## When the screen cannot tell you why

`make kbdiag` builds a standalone ROM that shows the controller byte itself,
plus each port, plus how many frames the current byte has lasted and a short
history of what came before. Reach for it when input misbehaves: a key that
never lifts looks exactly like a key pressed over and over, and no amount of
staring at the editor distinguishes them. jzIntv does not clear pad state when
its window loses focus (`FOCUS_LOST` is bound only to `WINDOW` in
`src/cfg/mapping.c`, and `pad->l`/`pad->r` are cleared only in `pad_init`), so
letting focus go while a key is down leaves that key held forever.

## tests/live/ is not run by `make test`

Those scenarios **commit events to whichever Google account the FujiNet is
authorised against**, and this client has no way to delete one again. Run them
deliberately, by name, and clean up by hand afterwards:

```sh
make test-one T=tests/live/compose.txt FUJINET=localhost:9995
```

`compose.txt` creates a timed event titled "Test" at 04:05 on the anchor day.
`edit.txt` expects that event to be the second row of the day view and moves it
to 05:06, which is what exercises the send-only-dirty-fields rule -- the title
must come back unchanged.

## Writing a scenario

`bp <label>` (before `boot`/`hit`), `hit` (run to a one-shot breakpoint),
`boot`, `key 0-11` (10 = Clear, 11 = Enter), `disc up|down|left|right`, `btn`,
`raw <hex>`, `rawhold <hex> <frames>`, `wait <frames>`, `poke <hex> <hex>`,
`snap`, `expect <row> <text>`, `expect-not <row> <text>`, `expect-bar <row>`,
`expect-no-bar <row>`, `expect-same <row>`, `expect-diff <row>`.

`rawhold` is `raw` on N **consecutive** frames. `raw` injects one byte and then
lets the program run, which is a one-frame press -- and one frame is precisely
what `in_poll` now rejects for an action button, so a real button press has to
be held: `rawhold A0 5`.

`expect-same`/`expect-diff` compare a row against the same row of the previous
snapshot, for "this input changed nothing" / "that one did". Prefer them to
spelling out the text whenever the row is derived from the clock: an `expect`
that names a date passes only on the day it was written.

`expect-bar` reads the colour-stack advance bit at `(row, 1)` out of the raw
BACKTAB word. The selection bar is *only* a background colour, and the snapshot
decode throws colour away, so this is the one thing about it a scenario can
see. Snapshots print a `<` beside any row carrying the bit.

**Prefer `raw` when the thing under test is input handling.** `key` and `disc`
override what `in_poll` already decoded, so they skip its decoding entirely;
`raw` replaces the controller byte and lets it decode. That distinction is not
academic — a green suite built entirely on `key` missed the keypad/disc
aliasing bug completely, because injected keys never touched the code that was
wrong. `raw 82` is what the hardware sends for keypad 4.

Two things to know. `hit` runs to the next breakpoint once; `boot` and `wait`
issue one `r` per frame and will block until the subprocess timeout if the
program reaches a place where no breakpoint is live -- `gc_halt`, for one. And
an editor consumes keys itself: inside `t9_entry` or the numeric editor, ENTER
ends the field, so reaching the form's own save takes a second one.
