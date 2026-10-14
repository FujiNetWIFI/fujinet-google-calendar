#!/usr/bin/env python3
"""xbank.py MAP OBJDIR -- no call crosses a bank except through tramp.c.

The NES image (src/nes/gcal-nes.cfg) maps one 8K overlay at a time into
$8000. ld65 links a call from one overlay into another without a murmur --
both are just addresses -- and at run time it lands in whatever bank happens
to be mapped. So, from the linked map (which modules went into which
segment) and each module's imports and exports (od65):

  - a module in an overlay may use only what is always mapped (the fixed
    bank, R7, RAM) or what is in its own overlay;
  - a module that is always mapped may not use anything in an overlay --
    except tramp.o, whose whole job that is, and crt0.o, which copies the
    pattern tables out of the art banks at boot with the bank switched in.

Anything the modules list does not place (the cc65 runtime, fujinet-lib) is
in the fixed bank, and a symbol whose address is not in the $8000-$9FFF
window -- a variable in RAM, anything in R7 or the fixed bank -- is always
reachable whichever module defines it. A failure names both ends of the
offending reference.
"""
import os
import re
import subprocess
import sys

OVERLAYS = {"OVL_NET", "OVL_VIEWS", "OVL_GRID", "OVL_DETAIL", "OVL_FORM",
            "OVL_PICK", "OVL_BOOT", "ART", "ART2"}
ALWAYS = {"CODE", "RODATA", "CODE2", "STARTUP", "ONCE", "LOWCODE"}
TRUSTED = {"tramp.o", "crt0.o"}


def homes(mapfile):
    """module -> its bank segment ('FIXED' if only always-mapped ones)."""
    text = open(mapfile).read()
    mods = text.split("Modules list:")[1].split("Segment list:")[0]
    home, cur = {}, None
    for line in mods.splitlines():
        m = re.match(r"^(\S.*):$", line)
        if m:
            cur = os.path.basename(m.group(1).split("(")[0])
            home.setdefault(cur, "FIXED")
            continue
        m = re.match(r"^\s+(\w+)\s+Offs=\w+\s+Size=(\w+)", line)
        if m and cur and int(m.group(2), 16) > 0 and m.group(1) in OVERLAYS:
            if home[cur] not in ("FIXED", m.group(1)):
                sys.exit("xbank.py: %s is in two overlays" % cur)
            home[cur] = m.group(1)
    return home


def addresses(mapfile):
    """symbol -> address, from the map's exports list."""
    text = open(mapfile).read()
    part = text.split("Exports list by name:")[1].split("Exports list by value:")[0]
    return {name: int(addr, 16)
            for name, addr in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\w+", part)}


def od65(path, what):
    out = subprocess.run(["od65", "--dump-" + what, path], capture_output=True,
                         text=True, check=True).stdout
    return re.findall(r'Name:\s+"([^"]+)"', out)


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    mapfile, objdir = sys.argv[1:]
    home = homes(mapfile)
    addr = addresses(mapfile)

    objs = []
    for root, _, files in os.walk(objdir):
        objs += [os.path.join(root, f) for f in files if f.endswith(".o")]

    defined = {}
    imports = {}
    for path in objs:
        name = os.path.basename(path)
        for sym in od65(path, "exports"):
            defined[sym] = name
        imports[name] = od65(path, "imports")

    problems = []
    for mod, syms in imports.items():
        mine = home.get(mod, "FIXED")
        for sym in syms:
            other = defined.get(sym)
            if other is None:
                continue                        # the runtime or the library
            if not 0x8000 <= addr.get(sym, 0x8000) < 0xA000:
                continue                        # RAM, R7 or the fixed bank
            theirs = home.get(other, "FIXED")
            if theirs == "FIXED" or theirs == mine:
                continue
            if mine == "FIXED" and mod in TRUSTED:
                continue
            problems.append("%s (%s) uses %s from %s (%s)"
                            % (mod, mine, sym, other, theirs))

    if problems:
        for p in problems:
            print("xbank.py: " + p, file=sys.stderr)
        return 1
    print("xbank.py: %d modules, no call across banks but the trampolines'"
          % len(imports))
    return 0


if __name__ == "__main__":
    sys.exit(main())
