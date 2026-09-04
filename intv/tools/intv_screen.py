#!/usr/bin/env python3
"""intv_screen.py -- scripted, headless UI testing for the Intellivision ROM.

Drives jzIntv's debugger over stdin, injecting controller events through
the t9_poll test hook (cart RAM at $95F0, see t9.bas), snapshotting the
BACKTAB ($0200-$02EF) and decoding it to 12 rows x 20 columns of text.

Timing is breakpoint-driven, not cycle-driven (the debugger's 'r <n>'
argument is not a usable cycle count): the ROM's symbol table is loaded
and a breakpoint set on something the program reaches exactly once per
frame -- so each 'r' runs exactly one frame. Injected events are consumed
by the very poll the breakpoint stopped at.

WHICH label that is depends on where the program is. In the T9 editor it
is t9_poll; in gcal's main loop it is al_scan, because t9_entry blocks in
a WAIT loop of its own and the main loop does not run at all while it is
up. Set them with 'bp' (default: label_T9_POLL, for the standalone demo).
A scenario that runs past a point where NO breakpoint is reachable -- gcal
halting on a missing mailbox, for instance -- will sit there until the
subprocess timeout, so breakpoint whatever the program actually reaches.

Scenario commands (one per line; '#' comments):
    bp <label>      breakpoint to step frames on (repeatable; before 'boot')
    hit             run to the NEXT breakpoint only (for one-shot labels)
    poke <hexaddr> <hexval>    write memory (before boot for boot flags)
    boot            run to the first input-loop frame
    key <0-11>      inject a keypad value (10=Clear, 11=Enter)
    disc <up|down|left|right>
    raw <hex>       replace the raw controller byte for one frame, e.g.
                    "raw 82" is what the hardware sends for keypad 4 -- this
                    exercises in_poll's own decoding, which `key` bypasses
    rawhold <hex> <frames>     the same byte on N CONSECUTIVE frames, which is
                    what a debounced reading needs: in_poll confirms an action
                    button over three frames, so a one-frame `raw A0` is
                    deliberately ignored and only `rawhold A0 4` presses it
    btn             inject an action-button press
    wait <frames>   let the program run N frames
    snap            snapshot the screen
    expect <row> <substring>   assert on the latest snapshot's row
    expect-not <row> <substring>
    expect-bar <row>           assert the colour-stack advance bit at (row,1),
                    i.e. that the selection bar is on that row. Colour is
                    invisible to the text decode, so this reads the raw word.
    expect-no-bar <row>
    expect-same <row>          assert the row is IDENTICAL to the same row of
                    the previous snapshot, and expect-diff that it is not.
                    For "this input changed nothing" / "that one did": the
                    alternative is spelling out the text, which for anything
                    derived from the clock goes stale the next day.

Every decoded snapshot is printed; failed expects exit non-zero.

Usage:
    python3 tools/intv_screen.py t9demo.rom tests/t9_basic.txt
    echo 'boot
    snap' | python3 tools/intv_screen.py t9demo.rom -
"""

import os
import re
import subprocess
import sys

JZINTV_DIR = os.environ.get(
    "JZINTV_DIR", os.path.expanduser("~/Workspace/jzintv-20200712-src"))
JZINTV = os.environ.get("JZINTV", os.path.join(JZINTV_DIR, "bin", "jzintv"))
EXEC_BIN = os.environ.get("EXEC_BIN", os.path.join(JZINTV_DIR, "rom", "exec.bin"))
GROM_BIN = os.environ.get("GROM_BIN", os.path.join(JZINTV_DIR, "rom", "grom.bin"))

# t9.bas's SC_T9_INJECT. It moved from the demo's $95F0 when the module was
# lifted into gcal, whose scratch map already had SC_LINEBUF and SC_DETAIL
# there; override for another host.
INJECT = int(os.environ.get("INTV_INJECT", "0x9390"), 0)
EVENT_FRAMES = 6           # frames allotted for the program to consume an event
DISC = {"down": 1, "right": 2, "up": 4, "left": 8}


def build_script(lines, sym_file):
    """Translate scenario lines into debugger commands + expectation list."""
    dbg = [
        f"l {sym_file}",
        # Arm the injection hook before the program boots.
        f"e {INJECT:X} A5",
        f"e {INJECT + 1:X} 5A",
        f"e {INJECT + 2:X} C",
        f"e {INJECT + 3:X} 0",
        f"e {INJECT + 4:X} 0",
    ]
    expects = []   # (snap_index, row, substring, want_present, line_no)
    snaps = 0
    breakpoints = []

    def run(frames):
        dbg.extend(["r"] * frames)

    def arm():
        # Re-arm before every event, not just once at startup: gcal.bas clears
        # SC_INJECT during boot (deliberately -- uninitialised cart RAM could
        # otherwise spell the magic), which disarms a hook armed before the ROM
        # ran. Two pokes per event is cheaper than reasoning about who clears
        # what and when.
        dbg.append(f"e {INJECT:X} A5")
        dbg.append(f"e {INJECT + 1:X} 5A")

    for n, raw in enumerate(lines, 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        op, _, arg = line.partition(" ")
        arg = arg.strip()
        if op == "bp":
            if snaps or any(c.startswith("r") for c in dbg):
                sys.exit(f"scenario line {n}: bp must come before 'boot'")
            breakpoints.append(arg)
        elif op == "poke":
            addr, _, val = arg.partition(" ")
            dbg.append(f"e {addr.strip()} {val.strip()}")
        elif op == "boot":
            run(2)
        elif op == "hit":
            # One 'r', for a breakpoint the program reaches exactly once.
            # 'boot' issues two, which is right for a per-frame tick and hangs
            # forever on a label like fn_wait_mailbox.
            run(1)
        elif op == "key":
            arm()
            dbg.append(f"e {INJECT + 2:X} {int(arg):X}")
            run(EVENT_FRAMES)
        elif op == "disc":
            arm()
            dbg.append(f"e {INJECT + 3:X} {DISC[arg]:X}")
            run(EVENT_FRAMES)
        elif op == "btn":
            arm()
            dbg.append(f"e {INJECT + 4:X} 1")
            run(EVENT_FRAMES)
        elif op == "raw":
            arm()
            dbg.append(f"e {INJECT + 5:X} {int(arg, 16):X}")
            run(EVENT_FRAMES)
        elif op == "rawhold":
            val, _, frames = arg.partition(" ")
            # Re-injected every frame: SC_INJECT+5 is one-shot, so a single
            # poke followed by N 'r's would be one frame of press and N-1 of
            # nothing -- which is exactly the transient in_poll now rejects.
            for _ in range(int(frames)):
                arm()
                dbg.append(f"e {INJECT + 5:X} {int(val, 16):X}")
                run(1)
        elif op == "wait":
            run(int(arg))
        elif op == "snap":
            dbg.append("m 200 F0")
            snaps += 1
        elif op in ("expect", "expect-not"):
            row, _, text = arg.partition(" ")
            if not snaps:
                sys.exit(f"scenario line {n}: {op} before any snap")
            expects.append((snaps - 1, "text", int(row), text.strip(),
                            op == "expect", n))
        elif op in ("expect-bar", "expect-no-bar"):
            if not snaps:
                sys.exit(f"scenario line {n}: {op} before any snap")
            expects.append((snaps - 1, "bar", int(arg), "",
                            op == "expect-bar", n))
        elif op in ("expect-same", "expect-diff"):
            if snaps < 2:
                sys.exit(f"scenario line {n}: {op} needs a previous snap")
            expects.append((snaps - 1, "same", int(arg), "",
                            op == "expect-same", n))
        else:
            sys.exit(f"scenario line {n}: unknown command {op!r}")
    dbg.append("q")
    # IntyBASIC prefixes BASIC labels with label_. Breakpoints have to be set
    # before the first 'r', so they are spliced in after the whole scenario has
    # been read rather than appended as they are seen.
    at = 1 + 5   # after 'l <sym>' and the five injection-hook pokes
    for b in reversed(breakpoints or ["label_IN_POLL"]):
        dbg.insert(at, f"b {b}")
    return dbg, expects, snaps


CS_ADVANCE = 0x2000     # colour-stack advance bit, constants.bas


def decode_snaps(output):
    """Pull every 'm 200 F0' dump out of debugger output.

    Returns a list of (rows, grid): rows is 12 strings of 20 characters, grid
    the 12x20 raw BACKTAB words behind them. The text decode throws away the
    colour and the advance bit, and the selection bar is nothing BUT an advance
    bit, so expect-bar needs the words.
    """
    words = {}
    screens = []
    for line in output.splitlines():
        m = re.match(r"^(0[23][0-9A-F]{2}):((?:\s+[0-9A-F]{4}\*?){1,8})", line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        for i, tok in enumerate(re.findall(r"[0-9A-F]{4}", m.group(2))):
            words[addr + i] = int(tok, 16)
        if addr == 0x2E8 and all(0x200 + j in words for j in range(240)):
            rows = []
            grid = []
            for r in range(12):
                chars = []
                cells = []
                for c in range(20):
                    w = words[0x200 + r * 20 + c]
                    cells.append(w)
                    card = (w >> 3) & 0xFF
                    chars.append(chr(card + 32) if not (w & 0x800)
                                 and card <= 94 else "?")
                rows.append("".join(chars))
                grid.append(cells)
            screens.append((rows, grid))
            words = {}
    return screens


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    rom, scenario = sys.argv[1], sys.argv[2]
    sym_file = re.sub(r"\.rom$", "", rom) + ".sym"
    if not os.path.exists(sym_file):
        sys.exit(f"symbol file {sym_file} not found (build with as1600 -s)")
    lines = (sys.stdin.readlines() if scenario == "-"
             else open(scenario, encoding="utf-8").readlines())
    dbg, expects, n_snaps = build_script(lines, sym_file)

    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    proc = subprocess.run(
        [JZINTV, "-d", "-e", EXEC_BIN, "-g", GROM_BIN]
        + os.environ.get("JZINTV_ARGS", "").split() + [rom],
        input="\n".join(dbg) + "\n", capture_output=True, text=True,
        env=env, timeout=int(os.environ.get("INTV_TIMEOUT", "120")))
    screens = decode_snaps(proc.stdout)
    if len(screens) != n_snaps:
        print(proc.stdout[-2000:], file=sys.stderr)
        sys.exit(f"expected {n_snaps} snapshots, decoded {len(screens)}")

    for i, (rows, grid) in enumerate(screens):
        print(f"--- snap {i} ---")
        for r, row in enumerate(rows):
            bar = " <" if grid[r][1] & CS_ADVANCE else ""
            print(f"{r:2}|{row}|{bar}")

    # Each breakpoint stop prints the cycle counter; the gaps between
    # consecutive stops are whole frames unless a keypress overran one.
    stops = []
    for line in proc.stdout.splitlines():
        m = re.match(r"^ [0-9A-F]{4} [0-9A-F]{4} .*?(\d+)$", line)
        if m:
            stops.append(int(m.group(1)))
    deltas = [b - a for a, b in zip(stops[2:], stops[3:])]
    if deltas:
        mx = max(deltas)
        print(f"frame stats: {len(deltas)} steps, max gap {mx} cycles "
              f"(~{mx / 14934:.2f} frames)")

    failed = 0
    for snap, what, row, text, want, line_no in expects:
        rows, grid = screens[snap]
        if what == "bar":
            got = bool(grid[row][1] & CS_ADVANCE)
            ok = got == want
            detail = f"row {row} advance bit is {got}"
            kind = "expect-bar" if want else "expect-no-bar"
            shown = ""
        elif what == "same":
            prev = screens[snap - 1][0][row]
            ok = (rows[row] == prev) == want
            detail = f"row {row} was {prev!r}, is {rows[row]!r}"
            kind = "expect-same" if want else "expect-diff"
            shown = ""
        else:
            got = rows[row]
            ok = (text in got) == want
            detail = f"row {row} is {got!r}"
            kind = "expect" if want else "expect-not"
            shown = f" {text!r}"
        if not ok:
            failed += 1
            print(f"FAIL line {line_no}: {kind} {row}{shown}; {detail}",
                  file=sys.stderr)
    if failed:
        sys.exit(f"{failed} expectation(s) failed")
    print(f"OK ({len(expects)} expectation(s), {n_snaps} snapshot(s))")


if __name__ == "__main__":
    main()
