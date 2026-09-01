#!/bin/bash
#
# Headless screen capture for the MS-DOS build.
#
# Builds the client with canned data, boots it in dosbox-x with no display,
# and decodes the text page it dumps. No X server, no window, no FujiNet and
# no human required -- and unlike the Atari and CoCo captures there is no
# debugger to drive: a DOS program can hand over its own B800 page, so the
# GC_SHOT hook in src/msdos/input.c writes SCREEN.BIN where a person would
# start typing, and the emulator's only job is to exist.
#
#   tools/msdos-shot.sh                          fake data, DAY view
#   tools/msdos-shot.sh "K_VIEW3"                fake data, MONTH view
#   tools/msdos-shot.sh "K_DOWN,K_DOWN,K_ENTER"  event detail
#   MODE=40 tools/msdos-shot.sh                  40 columns  (gcal /40)
#   MODE=mono tools/msdos-shot.sh                BW table    (gcal /mono)
#   MACHINE=hercules tools/msdos-shot.sh         the MDA path, mode 7
#   MACHINE=pcjr tools/msdos-shot.sh             the PCjr's BIOS
#
# The key names are the K_* codes from src/gcal.h. Scripted keys are
# consumed first and then the program reaches the blocking read -- which is
# exactly where GC_SHOT catches it with the screen of interest painted. The
# view loop polls, but under GC_FAKE_DATA plat_getkey_poll() delegates to
# the blocking read once the script is spent, so every screen funnels into
# the same catch point; only the alarm banner cannot be captured this way,
# because it needs the loop to keep turning.
#
# MACHINE=hercules is the capture that earns its keep: dosbox-x's hercules
# machine boots claiming mode 3, which is how the equipment-word probe in
# src/msdos/screen.c got its regression test. Check the --attrs pane for
# the 0x70 bars, the 0x01 underline under the detail title and the 0x09
# bright underline on today; none of them are visible in the glyphs.
#
# The build runs through defoogi (wcc lives nowhere else) and clobbers the
# msdos objects first: MSDOS_SHOT_FLAGS changes every object and make cannot
# see a flag change. Rebuild without flags afterwards to get the product
# binary back.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(dirname "$HERE")"
KEYS="${1:-}"
MODE="${MODE:-}"
MACHINE="${MACHINE:-}"
DOSBOX="${DOSBOX:-dosbox-x}"
TMO="${TMO:-60}"

cd "$PROJ"

if ! command -v "$DOSBOX" >/dev/null; then
    echo "dosbox-x not found -- set DOSBOX=/path/to/dosbox-x" >&2
    exit 1
fi

# wcc cannot carry a comma through -D (everything after it parses as a
# second source file), so the K_* names become the letter string
# GC_FAKE_KEYS_STR, 'a' + code -- letters rather than the gmail client's
# digits because thirteen codes run '0' + code into ';' and '<', either of
# which would hand the shell inside the make recipe a command separator.
# See the note in src/msdos/input.c. The escaped quotes ride the same path
# GIT_VERSION does.
FLAGS="-DGC_FAKE_DATA -DGC_SHOT"
if [ -n "$KEYS" ]; then
    LETTERS=$(printf '%s' "$KEYS" | sed \
        -e 's/K_UP/b/g'      -e 's/K_DOWN/c/g'   \
        -e 's/K_LEFT/d/g'    -e 's/K_RIGHT/e/g'  \
        -e 's/K_ENTER/f/g'   -e 's/K_BACK/g/g'   \
        -e 's/K_REFRESH/h/g' -e 's/K_QUIT/i/g'   \
        -e 's/K_TODAY/j/g'                       \
        -e 's/K_VIEW1/k/g'   -e 's/K_VIEW2/l/g'  \
        -e 's/K_VIEW3/m/g'   -e 's/K_VIEW4/n/g'  \
        -e 's/K_NEW/o/g'     -e 's/K_EDIT/p/g'   \
        -e 's/[ ,]//g')
    if printf '%s' "$LETTERS" | grep -q '[^b-p]'; then
        echo "unrecognised key in \"$KEYS\"" >&2
        exit 1
    fi
    FLAGS="$FLAGS -DGC_FAKE_KEYS_STR=\\\"$LETTERS\\\""
fi

rm -rf build/gcal/msdos r2r/msdos
defoogi make msdos/product FUJINET_LIB= MSDOS_SHOT_FLAGS="$FLAGS"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp r2r/msdos/gcal.exe "$STAGE"

ARGS=""
case "$MODE" in
    40)   ARGS=" /40" ;;
    80)   ARGS=" /80" ;;
    mono) ARGS=" /mono" ;;
esac

SDL_VIDEODRIVER=dummy timeout "$TMO" "$DOSBOX" \
    -defaultconf -nogui -fastlaunch \
    ${MACHINE:+-machine "$MACHINE"} \
    -c "mount c $STAGE" -c "c:" -c "gcal$ARGS" -c "exit" \
    >/dev/null 2>&1 || true

if [ ! -f "$STAGE/SCREEN.BIN" ]; then
    echo "no capture produced -- the program never reached a key read" >&2
    exit 1
fi

python3 "$HERE/msdos-decode.py" --attrs "$STAGE/SCREEN.BIN"
