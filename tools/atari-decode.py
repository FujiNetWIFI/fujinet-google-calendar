#!/usr/bin/env python3
"""
Decode an atari800 full-RAM dump into something readable.

Renders the 40x24 text screen back to ASCII, shows which display list rows
carry an interrupt bit, draws the Google Calendar mark as it will actually
appear, and lists the colour chip in each row of the gutter. Driven by
tools/atari-shot.sh, which supplies the addresses it cannot guess.
"""
import sys

# See src/atari/pmg.c -- these have to match it.
PM_BASE = 0xB800            # 1K-aligned P/M buffer
PM_M = PM_BASE + 0x180      # all four missiles, two bits each
PM_P0 = PM_BASE + 0x200
PM_STRIDE = 0x80
PM_TOP = 16                 # byte holding the first scanline of text row 0
PM_ROWBYTES = 4
PM_LEFT = 48                # HPOS of screen column 0, in colour clocks
PM_COLCLK = 4

PLAYERS = [
    ("P0 blue   top-left", "B"),
    ("P1 red    top-right", "R"),
    ("P2 green  bottom-left", "G"),
    ("P3 yellow bottom-right", "Y"),
]
CHIPS = {"B": "blue", "R": "red", "G": "green", "Y": "yellow", "M": "graphite"}


def main(path, logo_hpos_addr=None):
    mem = open(path, 'rb').read()
    if len(mem) != 65536:
        sys.exit(f"expected a 64K dump, got {len(mem)} bytes")

    def word(a):
        return mem[a] | (mem[a + 1] << 8)

    savmsc = word(0x58)
    dl = word(0x230)

    print(f"SAVMSC = ${savmsc:04X}   SDLST = ${dl:04X}   "
          f"MEMTOP = ${word(0x2E5):04X}   APPMHI = ${word(0x0E):04X}")

    # ---- text screen -------------------------------------------------
    def unscr(c):
        """Atari screen code back to ASCII.

        An inverse *space* becomes '#'. Inverse padding is invisible otherwise,
        and it is exactly what the selection bar and the month view's density
        bars are made of -- so without this the two things most worth checking
        do not show up in the capture at all.
        """
        v = c & 0x7F
        if v == 0:
            return "#" if c & 0x80 else " "
        if v < 0x40:
            return chr(v + 0x20)
        if v < 0x60:
            return chr(v - 0x40)
        return chr(v)

    print("\n     +" + "-" * 40 + "+")
    for row in range(24):
        cells = mem[savmsc + row * 40:savmsc + row * 40 + 40]
        text = "".join(unscr(c) for c in cells)
        inv = "  <-INVERSE" if any(c & 0x80 for c in cells) else ""
        print(f"  {row:2d} |{text}|{inv}")
    print("     +" + "-" * 40 + "+")

    # ---- display list ------------------------------------------------
    print(f"\ndisplay list @ ${dl:04X}:")
    raw = mem[dl:dl + 32]
    print("  " + " ".join(f"{b:02X}" for b in raw))

    dli_rows, row, i = [], 0, 0
    while i < 32:
        b = raw[i]
        if (b & 0x0F) == 0:             # blank scanlines
            i += 1
            continue
        if (b & 0x0F) == 1:             # jump / JVB ends the list
            break
        if b & 0x80:
            dli_rows.append(row)
        row += 1
        i += 3 if (b & 0x40) else 1     # LMS is a mode byte plus a 2-byte address
    print(f"  text rows carrying a DLI bit: {dli_rows}   (expect [2, 22])")

    # ---- player/missile ----------------------------------------------
    print(f"\nP/M buffer @ ${PM_BASE:04X} (double-line, {PM_STRIDE} bytes/player)")

    shapes = [mem[PM_P0 + p * PM_STRIDE:PM_P0 + (p + 1) * PM_STRIDE]
              for p in range(4)]
    missiles = mem[PM_M:PM_M + PM_STRIDE]

    for p, (name, _) in enumerate(PLAYERS):
        nz = [(i, b) for i, b in enumerate(shapes[p]) if b]
        if not nz:
            print(f"  {name:24s}: empty")
        else:
            print(f"  {name:24s}: bytes ${nz[0][0]:02X}..${nz[-1][0]:02X}")

    # The logo lives above the content band and the chips live in it, and the
    # row-2 DLI is what separates them -- so on a banded screen the split is at
    # text row 3. A flat screen carries no DLI bits and no chips at all, and
    # its logo is the large one, well below that line.
    banded = bool(dli_rows)
    logo_end = PM_TOP + PM_ROWBYTES * 3 if banded else PM_STRIDE
    logo_rows = [i for i in range(PM_TOP, logo_end)
                 if any(s[i] for s in shapes)]

    if logo_hpos_addr is not None:
        hpos = list(mem[logo_hpos_addr:logo_hpos_addr + 4])
    else:
        hpos = None

    if logo_rows and hpos:
        # Eight bits span 16 colour clocks at double width and 8 at normal.
        # The two side-by-side players are offset by exactly that, so the gap
        # between the left and right HPOS gives the width away.
        step = 2 if (hpos[1] - hpos[0]) >= 16 else 1
        left = min(hpos)
        print(f"\nmark as rendered (HPOS {hpos}, "
              f"{'double' if step == 2 else 'normal'} width):")
        for r in range(min(logo_rows), max(logo_rows) + 1):
            line = [' '] * 80
            for p in range(4):
                b = shapes[p][r]
                for bit in range(8):
                    if b & (0x80 >> bit):
                        x = hpos[p] - left + bit * step
                        for k in range(step):
                            line[x + k] = PLAYERS[p][1]
            print("    " + "".join(line).rstrip())
        print("    (B blue, R red, G green, Y yellow -- the ring should close"
              " on all four sides)")
    elif logo_rows:
        print("\nmark present but no HPOS address given; pass it as argv[2]")
    else:
        print("\nno logo shapes -- pmg_hide() screen, or PMG never came up")

    # ---- chip gutter -------------------------------------------------
    if not banded:
        print("\nchip gutter: none (flat screen, no DLI and no chips)")
        return

    found = []
    for idx in range(logo_end, PM_STRIDE):
        text_row = (idx - PM_TOP) // PM_ROWBYTES
        if (idx - PM_TOP) % PM_ROWBYTES:
            continue
        letter = None
        for p in range(4):
            if shapes[p][idx]:
                letter = PLAYERS[p][1]
        if missiles[idx] & 0x03:
            letter = "M"
        if letter:
            found.append((text_row, CHIPS[letter]))

    print("\nchip gutter (column 0 of the content band):")
    if not found:
        print("    none")
    for text_row, colour in found:
        print(f"    row {text_row:2d}  {colour}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    addr = int(sys.argv[2], 16) if len(sys.argv) > 2 else None
    main(sys.argv[1], addr)
