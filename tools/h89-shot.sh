#!/bin/sh
#
# Headless screen capture for the H89 CP/M build, in fujinet-go-h89-desktop's
# h89_headless: it boots Heath CP/M 2.2.04 from the H17, runs B:GCAL from a
# second H17 disk, presses keys on the H19, and prints the screen as text and
# writes it as a PNG at each "shot".
#
#   tools/h89-shot.sh                         canned data, the first view
#   tools/h89-shot.sh "f3 shot right shot"    f3, capture, right, capture
#   tools/h89-shot.sh "blue type:Lunch shot"  type into the form
#   REAL=1 tools/h89-shot.sh "shot f3 shot"   against a live fujinet-pc
#   SMALL=1 tools/h89-shot.sh "f4 down down shot"
#                                             windows small enough to scroll
#
# A step is a key the harness knows (f1..f5 erase blue red gray up down left
# right home esc return delete backspace tab), "type:TEXT" (\r and \e
# escapes), "wait:MS", or "shot". Each key is followed by WAIT ms (default
# 800) for the 9600-baud screen to catch up; a script with no shot gets one
# at the end.
#
# The text dump is the H19's characters only -- reverse video does not show
# in it, so the selection bar, the chips and the month's cursor are checked
# in the PNG.
#
# REAL=1 builds without GC_FAKE_DATA and dials the FujiNet at FUJINET
# (default 127.0.0.1:9993) over BoIP -- an RS232 fujinet-pc with the GCAL
# adapter authorised and timezone= set. It runs without --warp: warp resumes
# after two seconds of card silence, and a Google open is longer than that.
# Nothing here writes to the calendar unless the script saves a form, so a
# script that does is a deliberate act.
#
# Shots land in r2r/h89-cpm/shot/ as NN.txt and NN.png.
set -e

RIG="${RIG:-$HOME/Workspace/fujinet-go-h89-desktop}"
BUILD="${BUILD:-defoogi make}"
WAIT="${WAIT:-800}"
BOOT="${BOOT:-$RIG/tools/disks/boot/cpm-2.2.04-64k.h8d}"

FLAGS="-DGC_FAKE_DATA"
[ -n "$REAL" ] && FLAGS=""
# SMALL=1 shrinks the list and detail windows to three rows, so the canned
# lists are long enough to scroll.
SIZES=""
[ -n "$SMALL" ] && SIZES="H89_LIST_ROWS=3 H89_DET_WIN=3"

# The shot flags reach CFLAGS_EXTRA_H89-CPM through H89_SHOT_FLAGS. Objects
# are keyed on timestamps, not flags, so a switch between canned and live
# data rebuilds from clean.
STAMP=build/gcal/h89-cpm/.shotflags
if [ "$(cat $STAMP 2>/dev/null)" != "$FLAGS $SIZES" ]; then
    rm -rf build/gcal/h89-cpm r2r/h89-cpm
fi
$BUILD h89-cpm H89_SHOT_FLAGS="$FLAGS" $SIZES >/dev/null
mkdir -p build/gcal/h89-cpm && printf '%s' "$FLAGS $SIZES" > $STAMP

OUT=r2r/h89-cpm/shot
rm -rf $OUT
mkdir -p $OUT

SCRIPT="${1:-shot}"
case " $SCRIPT " in
    *" shot "*) ;;
    *) SCRIPT="$SCRIPT shot" ;;
esac

# The machine, whose options come before any step: then CP/M from h17:1,
# the program on h17:2, both read-only so a run never changes either image.
set -- --roms "$RIG/tools/roms" --timeout 120
if [ -n "$REAL" ]; then
    set -- "$@" --boip "${FUJINET:-127.0.0.1:9993}"
else
    set -- "$@" --warp
fi
set -- "$@" --disk "h17:1=$BOOT:ro" --disk "h17:2=$(pwd)/r2r/h89-cpm/gcal.h8d:ro"
set -- "$@" --until 'H:' --type 'B\r' --until 'A>' --wait 1000 \
    --prompt 'A>' --type 'B:GCAL\r' --until 'Google Calendar' --wait 3000

n=0
for step in $SCRIPT; do
    case "$step" in
        shot)
            n=$((n + 1))
            NN=$(printf '%02d' $n)
            set -- "$@" --screen --ppm "$OUT/$NN.ppm"
            ;;
        type:*) set -- "$@" --type "${step#type:}" --wait "$WAIT" ;;
        wait:*) set -- "$@" --wait "${step#wait:}" ;;
        *)      set -- "$@" --key "$step" --wait "$WAIT" ;;
    esac
done

"$RIG/build/core/h89_headless" "$@" > $OUT/run.log 2>&1 || {
    cat $OUT/run.log >&2
    echo "h89-shot.sh: the run failed" >&2
    exit 1
}

# One text file per shot, out of the run's log, in the order they were taken
awk -v dir="$OUT" '
    /^---- screen ----/ { n++; f = sprintf("%s/%02d.txt", dir, n); inshot = 1; next }
    inshot && /^----------------/ { inshot = 0; close(f); next }
    inshot { print > f }
' $OUT/run.log

for p in $OUT/*.ppm; do
    convert "$p" "${p%.ppm}.png" && rm -f "$p"
done
ls $OUT/*.png
