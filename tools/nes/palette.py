#!/usr/bin/env python3
"""palette.py -- Google Calendar's colours on the NES (src/nes/gen_pal.s).

    palette.py src/nes/gen_pal.s        write the tables
    palette.py --report                 print the matches and their distances

The NES has no RGB: a palette entry is one of 54 usable 2C02 colours. This
picks, for every colour the client shows, the nearest by CIEDE2000 against
the reference 2C02 palette below (the one Mesen and the nesdev wiki use),
and for Google's eleven event colours it does that as an assignment -- the
eleven nearest *distinct* colours, so Peacock and Blueberry, or Flamingo
and Tangerine, can never come out the same.

The event colours are sprites (src/nes/screen.c): four sprite palettes of
three give twelve slots, grouped so the Google Calendar mark's three edge
colours -- red, yellow, green -- share one palette:

  SP0  Tomato     Banana     Basil         (and the mark's corner and edges)
  SP1  Peacock    Blueberry  Lavender
  SP2  Flamingo   Tangerine  Sage
  SP3  Grape      Graphite   white         (the cursor, the page)

The background palettes carry the theme (Google's light and dark schemes):

  BP0  body       ground, text, line/selection, accent
  BP1  chrome     ground, accent, line, text  (the tabs: blue labels, the
                  current one on the line-grey pill the selection bar uses)
  BP2  alert      ground, white, red, deep red  (the alarm band flashes by
                  swapping the white and the red)
  BP3  the mark   ground, brand blue, deep blue, white (its page is white
                  in both themes)

The light scheme's ground is white and the dark one's is Google's #202124:
the backdrop ($3F00) is the ground, so the band the scanline interrupt
blanks at the bottom of the screen is invisible in both.
"""

import math
import sys

# The 2C02's 64 colours, sRGB. $0D is "blacker than black" (it can upset
# some TVs' sync) and the $xE/$xF columns are duplicates of black: none of
# them is ever a candidate.
NES = [
    0x626262, 0x001FB2, 0x2404C8, 0x5200B2, 0x730076, 0x800024, 0x730B00, 0x522800,
    0x244400, 0x005700, 0x005C00, 0x005324, 0x003C76, 0x000000, 0x000000, 0x000000,
    0xABABAB, 0x0D57FF, 0x4B30FF, 0x8A13FF, 0xBC08D6, 0xD21269, 0xC72E00, 0x9D5400,
    0x607B00, 0x209800, 0x00A300, 0x009942, 0x007DB4, 0x000000, 0x000000, 0x000000,
    0xFFFFFF, 0x53AEFF, 0x9085FF, 0xD365FF, 0xFF57FF, 0xFF5DCF, 0xFF7757, 0xFA9E00,
    0xBDC700, 0x7AE700, 0x43F611, 0x26EF7E, 0x2CD5F6, 0x4E4E4E, 0x000000, 0x000000,
    0xFFFFFF, 0xB6E1FF, 0xCED1FF, 0xE9C3FF, 0xFFBCFF, 0xFFBDF4, 0xFFC6C3, 0xFFD59A,
    0xE9E681, 0xCEF481, 0xB6FB9A, 0xA9FAC3, 0xA9F0F4, 0xB8B8B8, 0x000000, 0x000000,
]
CANDIDATES = [i for i in range(64)
              if (i & 0x0F) < 0x0E and i != 0x0D and i != 0x20]   # $20 = $30
BLACK = 0x0F

# Google's event colours, in COL_* order (src/gcal.h).
EVENTS = [
    ("LAVENDER", 0x7986CB), ("SAGE", 0x33B679), ("GRAPE", 0x8E24AA),
    ("FLAMINGO", 0xE67C73), ("BANANA", 0xF6BF26), ("TANGERINE", 0xF4511E),
    ("PEACOCK", 0x039BE5), ("GRAPHITE", 0x616161), ("BLUEBERRY", 0x3F51B5),
    ("BASIL", 0x0B8043), ("TOMATO", 0xD50000),
]

# Which sprite palette and which of its three slots each event colour takes.
SLOTS = {
    "TOMATO": (0, 1), "BANANA": (0, 2), "BASIL": (0, 3),
    "PEACOCK": (1, 1), "BLUEBERRY": (1, 2), "LAVENDER": (1, 3),
    "FLAMINGO": (2, 1), "TANGERINE": (2, 2), "SAGE": (2, 3),
    "GRAPE": (3, 1), "GRAPHITE": (3, 2),
}
# COL_NONE: the category was a calendar's name, and the event wears that
# calendar's colour, which Google defaults to Peacock.
NONE_AS = "PEACOCK"

# Where the metric and the eye disagree, the eye wins. Banana is the brand
# yellow too (the mark's right edge shares its slot), and the 2C02's only
# yellow that reads as Google's gold is $27 -- the metric would spend it on
# Tangerine and leave Banana the olive $28. Sage, left alone, lands on $1A,
# which beside Basil's $1B is the same green twice; $2B is the lighter mint
# that keeps the two apart. The report still prints both distances.
OVERRIDE = {"BANANA": 0x27, "SAGE": 0x2B}

BRAND_BLUE, BRAND_RED, BRAND_YELLOW, BRAND_GREEN = 0x4285F4, 0xEA4335, 0xFBBC04, 0x34A853
# The metric picks $1C for Google blue, which is teal on a television; $11 is
# the blue everyone recognises from the logo.
BRAND_BLUE_NES = 0x11

THEMES = {
    "light": {
        "ground": 0xFFFFFF, "text": 0x3C4043, "line": 0xDADCE0,
        "accent": 0x1A73E8, "tint": 0xD2E3FC, "secondary": 0x5F6368,
    },
    "dark": {
        "ground": 0x202124, "text": 0xE8EAED, "line": 0x3C4043,
        "accent": 0x8AB4F8, "tint": 0x394457, "secondary": 0x9AA0A6,
    },
}


def lab(rgb):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = lin(rgb >> 16 & 255), lin(rgb >> 8 & 255), lin(rgb & 255)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 216 / 24389 else (24389 / 27 * t + 16) / 116
    return 116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z))


def de2000(c1, c2):
    L1, a1, b1 = lab(c1)
    L2, a2, b2 = lab(c2)
    C1, C2 = math.hypot(a1, b1), math.hypot(a2, b2)
    Cb = (C1 + C2) / 2
    G = 0.5 * (1 - math.sqrt(Cb ** 7 / (Cb ** 7 + 25 ** 7)))
    a1p, a2p = (1 + G) * a1, (1 + G) * a2
    C1p, C2p = math.hypot(a1p, b1), math.hypot(a2p, b2)
    h1p = math.degrees(math.atan2(b1, a1p)) % 360
    h2p = math.degrees(math.atan2(b2, a2p)) % 360
    dLp, dCp = L2 - L1, C2p - C1p
    if C1p * C2p == 0:
        dhp = 0
    elif abs(h2p - h1p) <= 180:
        dhp = h2p - h1p
    elif h2p - h1p > 180:
        dhp = h2p - h1p - 360
    else:
        dhp = h2p - h1p + 360
    dHp = 2 * math.sqrt(C1p * C2p) * math.sin(math.radians(dhp / 2))
    Lbp, Cbp = (L1 + L2) / 2, (C1p + C2p) / 2
    if C1p * C2p == 0:
        hbp = h1p + h2p
    elif abs(h1p - h2p) <= 180:
        hbp = (h1p + h2p) / 2
    elif h1p + h2p < 360:
        hbp = (h1p + h2p + 360) / 2
    else:
        hbp = (h1p + h2p - 360) / 2
    T = (1 - 0.17 * math.cos(math.radians(hbp - 30)) + 0.24 * math.cos(math.radians(2 * hbp))
         + 0.32 * math.cos(math.radians(3 * hbp + 6)) - 0.20 * math.cos(math.radians(4 * hbp - 63)))
    dth = 30 * math.exp(-((hbp - 275) / 25) ** 2)
    Rc = 2 * math.sqrt(Cbp ** 7 / (Cbp ** 7 + 25 ** 7))
    Sl = 1 + 0.015 * (Lbp - 50) ** 2 / math.sqrt(20 + (Lbp - 50) ** 2)
    Sc, Sh = 1 + 0.045 * Cbp, 1 + 0.015 * Cbp * T
    Rt = -math.sin(math.radians(2 * dth)) * Rc
    return math.sqrt((dLp / Sl) ** 2 + (dCp / Sc) ** 2 + (dHp / Sh) ** 2
                     + Rt * (dCp / Sc) * (dHp / Sh))


def nearest(rgb, avoid=()):
    best = min((c for c in CANDIDATES if c not in avoid), key=lambda c: de2000(rgb, NES[c]))
    if NES[best] == 0x000000:
        best = BLACK
    return best


def assign(targets):
    """The cheapest assignment of distinct NES colours to `targets` (a list
    of RGB), by trying every candidate set the greedy pass leaves room for:
    the eleven event colours against ~50 candidates is a small enough
    problem to solve exactly with a min-cost search over each target's
    eight nearest."""
    near = [sorted(CANDIDATES, key=lambda c: de2000(t, NES[c]))[:8] for t in targets]
    cost = [{c: de2000(t, NES[c]) for c in n} for t, n in zip(targets, near)]
    order = sorted(range(len(targets)), key=lambda i: cost[i][near[i][1]] - cost[i][near[i][0]],
                   reverse=True)
    best = [math.inf, None]

    def go(k, used, total, pick):
        if total >= best[0]:
            return
        if k == len(order):
            best[0], best[1] = total, dict(pick)
            return
        i = order[k]
        for c in near[i]:
            if c in used:
                continue
            pick[i] = c
            used.add(c)
            go(k + 1, used, total + cost[i][c], pick)
            used.discard(c)
            del pick[i]
    go(0, set(), 0.0, {})
    return [best[1][i] for i in range(len(targets))]


def build():
    free = [(n, rgb) for n, rgb in EVENTS if n not in OVERRIDE]
    saved = list(CANDIDATES)
    CANDIDATES[:] = [c for c in CANDIDATES if c not in OVERRIDE.values()]
    ev = assign([rgb for _, rgb in free])
    CANDIDATES[:] = saved
    evc = {name: c for (name, _), c in zip(free, ev)}
    evc.update(OVERRIDE)

    spr = [[0x0F, 0, 0, 0] for _ in range(4)]
    for name, (p, s) in SLOTS.items():
        spr[p][s] = evc[name]
    spr[3][3] = 0x30

    themes = {}
    for tname, t in THEMES.items():
        g = 0x30 if tname == "light" else nearest(t["ground"])
        bp = [
            [g, nearest(t["text"]), nearest(t["line"], (g,)), nearest(t["accent"])],
            [g, nearest(t["accent"]), nearest(t["line"], (g,)), nearest(t["text"])],
            [g, 0x30, evc["TOMATO"], 0x06],
            [g, BRAND_BLUE_NES, 0x01 if tname == "light" else 0x21, 0x30],
        ]
        pal = []
        for p in bp:
            pal += p
        for p in spr:
            pal += [g] + p[1:]
        themes[tname] = pal
    return evc, spr, themes


def report(evc, themes):
    print("event colours (CIEDE2000 to the nearest distinct 2C02 colour):")
    for name, rgb in EVENTS:
        c = evc[name]
        print("  %-10s #%06X -> $%02X #%06X  dE %.1f" % (name, rgb, c, NES[c], de2000(rgb, NES[c])))
    for name, rgb in (("brand blue", BRAND_BLUE), ("brand red", BRAND_RED),
                      ("brand yellow", BRAND_YELLOW), ("brand green", BRAND_GREEN)):
        c = nearest(rgb)
        print("  %-10s #%06X -> $%02X #%06X  dE %.1f" % (name, rgb, c, NES[c], de2000(rgb, NES[c])))
    for tname, pal in themes.items():
        print("%s: %s" % (tname, " ".join("%02X" % b for b in pal)))


def emit(path, evc, themes):
    order = [name for name, _ in EVENTS] + [NONE_AS]
    with open(path, "w") as f:
        f.write("; gen_pal.s -- GENERATED by tools/nes/palette.py; do not edit.\n")
        f.write(";\n; The two themes' 32-byte palettes (background then sprites) and,\n")
        f.write("; for each COL_* (and COL_NONE last), the sprite palette (0-3) and\n")
        f.write("; the slot (1-3) its chip tile draws in.\n\n")
        f.write("        .export _pal_light, _pal_dark, _col_pal, _col_slot\n")
        f.write('        .segment "RODATA"\n\n')
        for tname in ("light", "dark"):
            f.write("_pal_%s:\n" % tname)
            pal = themes[tname]
            for i in range(0, 32, 4):
                f.write("        .byte   " + ", ".join("$%02X" % b for b in pal[i:i + 4]) + "\n")
        f.write("\n_col_pal:\n        .byte   "
                + ", ".join("%d" % SLOTS[n][0] for n in order) + "\n")
        f.write("_col_slot:\n        .byte   "
                + ", ".join("%d" % SLOTS[n][1] for n in order) + "\n")
        for name in order[:-1]:
            f.write("; %-10s $%02X\n" % (name, evc[name]))


def main():
    evc, spr, themes = build()
    if len(sys.argv) == 2 and sys.argv[1] == "--report":
        report(evc, themes)
        return 0
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    emit(sys.argv[1], evc, themes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
