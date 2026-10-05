#!/bin/sh
#
# Headless screen capture for the NES build, in MAME with the FujiNet NES
# cartridge (fn-nes pico/nes/emu, grafted into a MAME tree by its apply.sh).
#
# Unlike the Adam's harness this one is event-driven: the pad is pressed from
# a Lua script on a schedule, so a capture can walk the real input paths --
# the START menu, the on-screen keyboard, the pickers -- and the screen comes
# back as text (from the shown name table) as well as a PNG.
#
#   tools/nes-shot.sh                          canned data, the first view
#   tools/nes-shot.sh "3:Select 4:Select"      press SELECT at 3 s and 4 s
#   AT=10 tools/nes-shot.sh "3:Start 4:Down"   capture at 10 s instead of 6
#   KEYS="K_VIEW3" tools/nes-shot.sh           scripted K_* codes (GC_FAKE_KEYS)
#   REAL=1 tools/nes-shot.sh                   against a live fujinet-pc
#   KBD=1 tools/nes-shot.sh "4:K_F3"           a Famicom with the Family BASIC
#                                              keyboard; K_<key> presses one
#
# REAL=1 builds without GC_FAKE_DATA and talks to whatever FujiNet is on
# FUJINET_TCP (default 127.0.0.1:9995) -- an RS232 fujinet-pc with the GCAL
# adapter authorised. Nothing here writes to the calendar unless the pad
# script saves a form, so a script that does is a deliberate act.
#
# The PNG lands in r2r/nes/snap/nes/ (MAME numbers them).
set -e

MAME="${MAME:-$HOME/Workspace/mame}"
BUILD="${BUILD:-defoogi make}"
AT="${AT:-6}"

FLAGS="-DGC_FAKE_DATA"
[ -n "$REAL" ] && FLAGS=""
[ -n "$KEYS" ] && FLAGS="$FLAGS -DGC_FAKE_KEYS=$KEYS"

# The shot flags reach CFLAGS_EXTRA_NES through NES_SHOT_FLAGS, which every
# other knob in that variable survives. Objects are keyed on timestamps, not
# flags, so a switch between canned and live data rebuilds from clean.
STAMP=build/gcal/nes/.shotflags
if [ "$(cat $STAMP 2>/dev/null)" != "$FLAGS" ]; then
    rm -rf build/gcal/nes r2r/nes
fi
$BUILD nes NES_SHOT_FLAGS="$FLAGS" >/dev/null
mkdir -p build/gcal/nes && printf '%s' "$FLAGS" > $STAMP

CTRL=$(awk '$1 == "_ppu_ctrl" { print $2; exit } $3 == "_ppu_ctrl" { print $4; exit }' r2r/nes/gcal.map)
[ -n "$CTRL" ] || { echo "nes-shot.sh: no _ppu_ctrl in the map" >&2; exit 1; }

SYSTEM=nes
[ -n "$KBD" ] && SYSTEM="famicom -exp fc_keyboard"

HERE=$(pwd)
mkdir -p r2r/nes/snap
cd "$MAME"
SHOT_PRESS="$1" SHOT_AT="$AT" SHOT_CTRL="$CTRL" FUJINET_TCP="${FUJINET_TCP:-127.0.0.1:9995}" \
    ./mame $SYSTEM -nes_slot fujinet -cart "$HERE/r2r/nes/gcal.nes" \
    -snapshot_directory "$HERE/r2r/nes/snap" \
    -autoboot_script "$HERE/tools/nes-shot.lua" \
    -video none -sound none -nothrottle -seconds_to_run $(( ${AT%.*} + 30 )) 2>&1 \
    | grep -v '^fujinet:'
ls -t "$HERE"/r2r/nes/snap/nes/*.png 2>/dev/null | head -1
