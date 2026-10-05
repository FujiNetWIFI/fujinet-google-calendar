#!/usr/bin/env python3
"""mkchr.py -- the NES client's pattern tables, from the text assets.

    mkchr.py [--png DIR]

reads tools/nes/assets/{font,tiles,logo16,logo48}.txt and writes, all
committed so a build needs nothing but ca65:

  src/nes/gen_chr.s     segment ART (PRG bank 8): _chr_bg, the background's
                        4K pattern table, and _chr_spr, the sprites' 4K.
                        crt0.s copies both into CHR-RAM at boot.
  src/nes/gen_splash.s  segment ART2 (PRG bank 9): _chr_splash, the 2K that
                        replaces background tiles $80-$FF while the splash
                        and busy screens show the large mark.
  src/nes/gen_logo.s    RODATA (fixed): where the two marks' tiles go and
                        which sprites lie over them.

The background table:

  $00       the ground (blank)
  $01-$1E   icons (tiles.txt)
  $20-$7E   the font, ink colour 1 on the ground
  $7F       an accent block
  $80-$97   today's date in Google's blue disc: $80 the disc's left half
            alone, $81-$83 with a tens digit 1-3; $84-$8D the right half with
            a units digit 0-9 (a two-digit day); $8E-$97 the right half with
            a single digit centred on the pair
  $98-$9B   the previous/next arrows and the bullet on the highlight ground
  $9C-$9F   the small mark (logo16), 2x2
  $A0-$FE   the font again, ink colour 1 on colour 2: the highlight, so a
            selection bar is per tile and not per 16x16 attribute block --
            and tile & $7F is still the character, which is what the MAME
            screen reader (tools/nes-shot.lua) relies on

The sprite table is tiles.txt's sprites from $00, the marks' sprite cells
from $C0.

--png DIR also writes the tables as PNGs (needs Pillow) for looking at.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ASSETS = os.path.join(HERE, "assets")
OUT = os.path.join(ROOT, "src", "nes")

LOGO_BG_SMALL = 0x9C
SPR_LOGO = 0xC0


def die(msg):
    sys.exit("mkchr.py: " + msg)


def blocks(path):
    """Yield (header words, [rows]) for each '= ...' block."""
    head, rows = None, []
    for n, raw in enumerate(open(path, encoding="ascii"), 1):
        line = raw.rstrip("\n")
        if line.startswith("= "):
            if head is not None:
                yield head, rows
            body = line[2:]
            if "#" in body[1:] and not body.startswith("#"):
                body = body[:body.index("#", 1)]
            head, rows = (body.split() if len(body.strip()) > 1 else [body]), []
            continue
        if head is None or line == "" or line.startswith("#") and len(line) > 8 \
                or line.startswith("# "):
            continue
        rows.append((n, line))
    if head is not None:
        yield head, rows


def tile(pix):
    """8 rows of 8 colour indices -> the 16 bytes of an NES tile."""
    p0 = bytearray(8)
    p1 = bytearray(8)
    for y in range(8):
        for x in range(8):
            c = pix[y][x]
            if c & 1:
                p0[y] |= 0x80 >> x
            if c & 2:
                p1[y] |= 0x80 >> x
    return bytes(p0 + p1)


def parse_rows(path, rows, chars):
    if len(rows) != 8:
        die("%s:%d: a tile is 8 rows, not %d" % (path, rows[0][0] if rows else 0, len(rows)))
    out = []
    for n, r in rows:
        if len(r) != 8 or set(r) - set(chars):
            die("%s:%d: bad row %r" % (path, n, r))
        out.append(r)
    return out


def font():
    path = os.path.join(ASSETS, "font.txt")
    glyphs = {}
    for head, rows in blocks(path):
        ch = head[0]
        if len(ch) != 1:
            die("font.txt: bad glyph name %r" % ch)
        g = [r.ljust(8, ".") for _, r in rows] + ["........"] * (8 - len(rows))
        glyphs[ord(ch)] = [[1 if c == "#" else 0 for c in r] for r in g]
    return glyphs


def tiles():
    path = os.path.join(ASSETS, "tiles.txt")
    bg, spr = {}, {}
    for head, rows in blocks(path):
        kind, num = head[0], int(head[1].lstrip("$"), 16)
        if kind == "bg":
            r = parse_rows(path, rows, ".123")
            bg[num] = [[int(c) if c != "." else 0 for c in row] for row in r]
        elif kind == "spr" and len(head) > 2 and head[2] == "slots":
            r = parse_rows(path, rows, ".#")
            for s in (1, 2, 3):
                spr[num + s] = [[s if c == "#" else 0 for c in row] for row in r]
        elif kind == "spr":
            r = parse_rows(path, rows, ".123")
            spr[num] = [[int(c) if c != "." else 0 for c in row] for row in r]
        else:
            die("tiles.txt: bad header %r" % head)
    return bg, spr


# ---- today's disc ------------------------------------------------------------

DIG35 = {
    0: ["111", "101", "101", "101", "111"], 1: ["010", "110", "010", "010", "111"],
    2: ["111", "001", "111", "100", "111"], 3: ["111", "001", "011", "001", "111"],
    4: ["101", "101", "111", "001", "001"], 5: ["111", "100", "111", "001", "111"],
    6: ["111", "100", "111", "101", "111"], 7: ["111", "001", "010", "010", "010"],
    8: ["111", "101", "111", "101", "111"], 9: ["111", "101", "111", "001", "111"],
}


def disc():
    """A 16x8 disc in colour 3 as two halves; digits are punched out (0)."""
    shape = ["...1111111111...", ".11111111111111.", "1111111111111111", "1111111111111111",
             "1111111111111111", "1111111111111111", ".11111111111111.", "...1111111111..."]
    return [[3 if c == "1" else 0 for c in r] for r in shape]


def disc_tile(half, digit=None, x=0):
    d = disc()
    if digit is not None:
        for yy, row in enumerate(DIG35[digit]):
            for xx, c in enumerate(row):
                if c == "1":
                    d[1 + yy][half * 8 + x + xx] = 0
    return [r[half * 8:half * 8 + 8] for r in d]


# ---- the marks -----------------------------------------------------------------

LOGO_BG = {".": 0, "B": 1, "D": 2, "W": 3}
LOGO_SPR = {"R": 1, "Y": 2, "N": 3}


def logo(name):
    path = os.path.join(ASSETS, name)
    rows = [l.rstrip("\n") for l in open(path, encoding="ascii") if not l.startswith("#")]
    rows = [r for r in rows if r]
    size = len(rows)
    if any(len(r) != size for r in rows) or size % 8:
        die("%s: not a square of 8x8 cells" % name)
    cells = size // 8
    bgt, sprt = [], []
    for cy in range(cells):
        for cx in range(cells):
            b = [[0] * 8 for _ in range(8)]
            s = [[0] * 8 for _ in range(8)]
            any_s = False
            for y in range(8):
                for x in range(8):
                    c = rows[cy * 8 + y][cx * 8 + x]
                    if c in LOGO_BG:
                        b[y][x] = LOGO_BG[c]
                    elif c in LOGO_SPR:
                        s[y][x] = LOGO_SPR[c]
                        any_s = True
                    else:
                        die("%s: bad pixel %r" % (name, c))
            bgt.append(b)
            sprt.append(s if any_s else None)
    # Eight sprites on a scanline is the PPU's limit; a cell row of the mark
    # may not need more (and the screens that show it put nothing beside it).
    for cy in range(cells):
        n = sum(1 for cx in range(cells) if sprt[cy * cells + cx])
        if n > 8:
            die("%s: cell row %d needs %d sprites" % (name, cy, n))
    return cells, bgt, sprt


def table(entries, what):
    out = bytearray(4096)
    for n, pix in entries.items():
        if not 0 <= n < 256:
            die("%s tile $%X out of range" % (what, n))
        out[n * 16:n * 16 + 16] = tile(pix)
    return out


def write_bytes(f, data):
    for i in range(0, len(data), 16):
        f.write("        .byte   " + ",".join("$%02X" % b for b in data[i:i + 16]) + "\n")


def main():
    png = None
    if len(sys.argv) == 3 and sys.argv[1] == "--png":
        png = sys.argv[2]
    elif len(sys.argv) != 1:
        print(__doc__.strip(), file=sys.stderr)
        return 2

    glyphs = font()
    bg_icons, spr = tiles()

    bg = {}
    bg.update(bg_icons)
    blank = [[0] * 8 for _ in range(8)]
    bg[0x00] = blank
    for code in range(0x20, 0x7F):
        g = glyphs.get(code, blank)
        bg[code] = [[1 if c else 0 for c in r] for r in g]
        bg[code | 0x80] = [[1 if c else 2 for c in r] for r in g]
    # today's disc
    bg[0x80] = disc_tile(0)
    for t in (1, 2, 3):
        bg[0x80 + t] = disc_tile(0, t, 4)
    for u in range(10):
        bg[0x84 + u] = disc_tile(1, u, 1)
        bg[0x8E + u] = disc_tile(1, u, 0)
    # the highlight-ground copies of three icons
    for dst, src in ((0x98, 0x04), (0x99, 0x05), (0x9A, 0x0F)):
        bg[dst] = [[2 if c == 0 else c for c in r] for r in bg_icons[src]]
    bg[0x9B] = [[2] * 8 for _ in range(8)]

    # the marks
    small_cells, small_bg, small_spr = logo("logo16.txt")
    if small_cells != 2:
        die("logo16.txt is not 16x16")
    for i in range(4):
        bg[LOGO_BG_SMALL + i] = small_bg[i]

    big_cells, big_bg, big_spr = logo("logo48.txt")

    # the marks' sprite cells, shared where they are identical
    spr_tiles = {}
    next_spr = [SPR_LOGO]

    def spr_tile(pix):
        key = tile(pix)
        if key not in spr_tiles:
            if next_spr[0] > 0xFF:
                die("out of sprite tiles for the marks")
            spr_tiles[key] = next_spr[0]
            spr[next_spr[0]] = pix
            next_spr[0] += 1
        return spr_tiles[key]

    small_list = [(cy * 8, cx * 8, spr_tile(small_spr[cy * 2 + cx]))
                  for cy in range(2) for cx in range(2) if small_spr[cy * 2 + cx]]
    big_list = [(cy * 8, cx * 8, spr_tile(big_spr[cy * big_cells + cx]))
                for cy in range(big_cells) for cx in range(big_cells)
                if big_spr[cy * big_cells + cx]]

    # the large mark's background: tiles $80 up in the splash bank, shared
    # where identical (the page's white interior is one tile)
    splash = {}
    big_map = []
    seen = {}
    nxt = 0x80
    for pix in big_bg:
        key = tile(pix)
        if key not in seen:
            seen[key] = nxt
            splash[nxt - 0x80] = pix
            nxt += 1
        big_map.append(seen[key])
    if nxt > 0x100:
        die("the large mark needs %d tiles" % (nxt - 0x80))

    bg_data = table(bg, "background")
    spr_data = table(spr, "sprite")
    splash_data = bytearray(2048)
    for n, pix in splash.items():
        splash_data[n * 16:n * 16 + 16] = tile(pix)

    with open(os.path.join(OUT, "gen_chr.s"), "w") as f:
        f.write("; gen_chr.s -- GENERATED by tools/nes/mkchr.py; do not edit.\n")
        f.write("; The background's and the sprites' pattern tables (PRG bank 8),\n")
        f.write("; copied to CHR-RAM at boot (crt0.s).\n\n")
        f.write("        .export _chr_bg, _chr_spr\n")
        f.write('        .segment "ART"\n\n_chr_bg:\n')
        write_bytes(f, bg_data)
        f.write("_chr_spr:\n")
        write_bytes(f, spr_data)

    with open(os.path.join(OUT, "gen_splash.s"), "w") as f:
        f.write("; gen_splash.s -- GENERATED by tools/nes/mkchr.py; do not edit.\n")
        f.write("; Background tiles $80-$FF while the large mark shows (PRG bank 9,\n")
        f.write("; CHR-RAM 1K banks 8-9).\n\n")
        f.write("        .export _chr_splash\n")
        f.write('        .segment "ART2"\n\n_chr_splash:\n')
        write_bytes(f, splash_data)

    with open(os.path.join(OUT, "gen_logo.s"), "w") as f:
        f.write("; gen_logo.s -- GENERATED by tools/nes/mkchr.py; do not edit.\n")
        f.write(";\n; The marks: _logo_big_map is the large mark's %dx%d tiles (in the\n"
                % (big_cells, big_cells))
        f.write("; splash bank); each _logo_*_spr is a count, then that many\n")
        f.write("; (dy, dx, tile) sprites in palette SP0 laid over the background.\n\n")
        f.write("        .export _logo_big_map, _logo_big_spr, _logo_small_spr\n")
        f.write('        .segment "RODATA"\n\n')
        f.write("_logo_big_map:\n")
        write_bytes(f, big_map)
        for name, lst in (("_logo_big_spr", big_list), ("_logo_small_spr", small_list)):
            f.write("%s:\n        .byte   %d\n" % (name, len(lst)))
            for dy, dx, t in lst:
                f.write("        .byte   %d, %d, $%02X\n" % (dy, dx, t))

    if png:
        from PIL import Image
        pal = [(255, 255, 255), (60, 64, 67), (184, 184, 184), (13, 87, 255)]
        os.makedirs(png, exist_ok=True)
        for name, data, n in (("bg", bg_data, 256), ("spr", spr_data, 256),
                              ("splash", bytes(splash_data), 128)):
            img = Image.new("RGB", (128, (n // 16) * 8))
            for t in range(n):
                for y in range(8):
                    lo, hi = data[t * 16 + y], data[t * 16 + 8 + y]
                    for x in range(8):
                        c = ((lo >> (7 - x)) & 1) | (((hi >> (7 - x)) & 1) << 1)
                        img.putpixel(((t % 16) * 8 + x, (t // 16) * 8 + y), pal[c])
            img.resize((img.width * 4, img.height * 4), Image.NEAREST).save(
                os.path.join(png, name + ".png"))

    print("mkchr.py: background %d tiles, sprites %d, splash %d; large mark %d sprites, "
          "small %d" % (len(bg), len(spr), len(splash), len(big_list), len(small_list)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
