#!/usr/bin/env bash
# Headless screenshot of the real build in native Flycast (reios HLE BIOS),
# for verifying the Dreamcast port without a TV. Requires: a native Flycast
# binary/AppImage in $FLYCAST (or ./flycast.AppImage), xvfb-run, and ImageMagick.
#
#   FLYCAST=/path/to/flycast.AppImage ./scripts/emulator-shot.sh herder.elf 30 shot.png
#
# Notes: map generation is slow on the emulated SH-4, so allow >90s to reach
# gameplay; Flycast boots a bare .elf directly via reios. The keyboard is not
# mapped to Start by default, so to capture in-game (past the title) build with
# a temporary auto-start, or inject a gamepad button.
set -euo pipefail
cd "$(dirname "$0")/.."
ELF="${1:-herder.elf}"; WAIT="${2:-30}"; OUT="${3:-emulator-shot.png}"
FLY="${FLYCAST:-./flycast.AppImage}"
[ -x "$FLY" ] || { echo "set FLYCAST to a native Flycast binary (not the WASM core)"; exit 2; }
work="$(mktemp -d)"; export HOME="$work"
timeout "$((WAIT+30))" xvfb-run -a -s "-screen 0 640x480x24" bash -c "
  '$FLY' '$ELF' > '$work/flycast.log' 2>&1 & P=\$!
  sleep $WAIT
  import -window root '$OUT' 2>/dev/null || (xwd -root -silent | convert xwd:- '$OUT')
  kill \$P 2>/dev/null || true
"
echo "wrote $OUT"; grep -i reios "$work/flycast.log" | tail -2 || true
rm -rf "$work"
