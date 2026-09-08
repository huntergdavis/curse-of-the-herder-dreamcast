#!/usr/bin/env bash
# Build and run the host parity tests with the system C compiler (no Dreamcast toolchain).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host
CC="${CC:-cc}"
CFLAGS="-std=gnu11 -O2 -Wall -Wextra -Isrc"
run_one() {
  local name="$1"; shift
  "$CC" $CFLAGS -o "build/host/$name" "$@"
  "./build/host/$name"
}
fail=0
run_one test_rng tests/test_rng.c src/core/rng.c src/core/noise.c src/core/map/terrain.c src/core/map/path.c src/core/map/generate.c src/core/progression.c src/core/names.c src/core/sim/flock.c src/core/sim/book.c src/core/sim/step.c src/core/lang/morphology.c src/core/lang/banned.c src/core/lang/grammar.c src/data/lang_data.c src/core/lang/speech.c -lm || fail=1
exit $fail
