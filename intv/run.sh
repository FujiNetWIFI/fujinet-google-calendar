#!/bin/sh
# run.sh -- build (if needed) and launch the Google Calendar client in the
# FujiNet-patched jzIntv, connected to a real fujinet-firmware instance over
# BoIP.
#
# The firmware is the listener (BoIPChannel defaults to listening = true) and
# jzIntv dials in, so start fujinet first. It must be built from the
# add-google-calendar branch for the fujiversal-intv / BUILD_RS232 target,
# with [BOIP] enabled=1 port=9995, [Device] enable_apetime=1, and [General]
# timezone= set to a POSIX TZ string -- an IANA name like America/Chicago is
# rejected by the adapter's PosixTz parser and silently drops GCAL to UTC.
#
# Override any of these on the command line, e.g.:
#   FUJINET_TARGET=localhost:9995 ./run.sh
#   ./run.sh --fujinet-debug        # extra flags are passed straight to jzintv

set -e

cd "$(dirname "$0")"
SDL_AUDIODRIVER=pulseaudio
JZINTV_DIR=${JZINTV_DIR:-$HOME/Workspace/jzintv-20200712-src}
JZINTV=${JZINTV:-$JZINTV_DIR/bin/jzintv}
EXEC_BIN=${EXEC_BIN:-$JZINTV_DIR/rom/exec.bin}
GROM_BIN=${GROM_BIN:-$JZINTV_DIR/rom/grom.bin}
FUJINET_TARGET=${FUJINET_TARGET:-localhost:9995}

if [ ! -x "$JZINTV" ]; then
    echo "jzIntv not found or not executable at: $JZINTV" >&2
    echo "Set JZINTV_DIR or JZINTV to point at your FujiNet-patched jzIntv build." >&2
    exit 1
fi
if [ ! -f "$EXEC_BIN" ] || [ ! -f "$GROM_BIN" ]; then
    echo "Missing EXEC/GROM BIOS images:" >&2
    echo "  EXEC_BIN=$EXEC_BIN" >&2
    echo "  GROM_BIN=$GROM_BIN" >&2
    exit 1
fi

# Rebuild only if the ROM is missing or a source file changed since it was
# last built.
if [ ! -f gcal.rom ] || [ -n "$(find . -maxdepth 1 -name '*.bas' -newer gcal.rom)" ]; then
    echo "Building gcal.rom..."
    make
fi

echo "Launching jzIntv against FujiNet at $FUJINET_TARGET ..."
exec "$JZINTV" \
    -z 6 \
    -e "$EXEC_BIN" \
    -g "$GROM_BIN" \
    --fujinet="$FUJINET_TARGET" \
    "$@" \
    gcal.rom
