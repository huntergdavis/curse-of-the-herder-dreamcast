#!/usr/bin/env bash
# Host preview of the framebuffer renderer, for visual inspection without a DC.
# Usage: ./scripts/render-preview.sh [seed] [ticks] [out.png]
set -euo pipefail
cd "$(dirname "$0")/.."
seed="${1:-seed}"; ticks="${2:-40000}"; out="${3:-preview.png}"
SRC="src/render/fb.c src/core/rng.c src/core/noise.c src/core/map/terrain.c src/core/map/path.c src/core/map/generate.c src/core/progression.c src/core/names.c src/core/sim/flock.c src/core/sim/book.c src/core/sim/step.c src/core/lang/morphology.c src/core/lang/banned.c src/core/lang/grammar.c src/core/lang/speech.c src/data/lang_data.c"
mkdir -p build/host
cc -std=gnu11 -O2 -Isrc -o build/host/render_preview tools/render_preview.c $SRC -lm
build/host/render_preview "$seed" "$ticks" /tmp/herder-preview.rgb565
python3 - "$out" <<'PY'
import sys, struct
from PIL import Image
W,H=640,480
data=open('/tmp/herder-preview.rgb565','rb').read()
img=Image.new('RGB',(W,H)); px=img.load()
for i in range(W*H):
    v=struct.unpack_from('<H',data,i*2)[0]
    px[i%W,i//W]=(((v>>11)&0x1f)<<3,((v>>5)&0x3f)<<2,(v&0x1f)<<3)
img.save(sys.argv[1]); print("wrote", sys.argv[1])
PY
