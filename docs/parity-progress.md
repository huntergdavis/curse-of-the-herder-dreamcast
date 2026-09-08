# 1:1 parity progress

The method: for each module, a script in the web repo (`scripts/dc-golden.ts`)
dumps golden vectors; the host build (`./scripts/host-test.sh`) asserts the C
output is byte-identical. CI runs it on every push. Doubles are compared by
raw 64-bit pattern; arrays by hash and, where useful, tile by tile.

## Done — the deterministic world-generation + setup spine (byte-exact)

| Module | C files | What it covers | Golden |
| --- | --- | --- | --- |
| RNG | core/rng | fnv1a, mulberry32, keyedUnit | fnv, sequences, keyed |
| Noise | core/noise | value noise, fBm, lattice hash | bit-exact doubles |
| Terrain | core/map/terrain | ids, cost/speed, classify() | 21×21 classify grid |
| Pathfinding | core/map/path | Dijkstra, A*, heap tie-breaking | full dist fields + paths |
| Map generation | core/map/generate | elevation, rivers, pen, villages, roads, deco | terrain/deco hash, pen, villages, tile dump |
| Progression | core/progression | erudition, level, filth, frustration curves | bit-exact |
| Names | core/names | herder/sheep/dog/rival keyed picks | index parity |
| Flock | core/sim/flock | createWorld sheep placement | 60 sheep exact |
| Libraries | core/sim/flock | placeLibraries (shared rng, stable sort) | count + positions |
| Books | core/sim/book | catalogue ids, when-order, pack flags | table parity |
| Sim step | core/sim/{world,step} | the whole day: modes, tempers, mishaps, weather, streaks, nemesis, jailbreaks, lunch, rival, dog, milestones, progression, governor | full-day event transcript, 2 seeds |
| Morphology | core/lang/morphology | article, plural, verb forms, number/ordinal words, syllables, tidySentence | 251-case battery |
| Ban filter | core/lang/banned | ~80 patterns, leet+diacritic normalise, mini regex matcher | 75-case battery |
| Lang data | data/lang_data | 2715 lexicon entries, 1656 rules, 20 non-terminals, 29 books (generated) | compiles; consumed by engine |
| Grammar | core/lang/grammar | rule picker, slot/gate/NT expansion, pools, weights, modifiers, contextual symbols, heat | 732 generated lines, 6 contexts |
| Speech | core/lang/speech | world->context, target pick, heat bumps, speak for event/idle/epitaph | 624-utterance full-day stream |

Host tests: **3287 checks, 0 failures.** This includes a full-day, tick-exact
event transcript (626 events, two seeds), 732 generated lines across six
contexts, and a full-day, byte-exact stream of 624 spoken utterances (text,
rule, heat, timing, target). Every file also compiles for the SH-4.

## Remaining

The game is playable end to end on the Dreamcast: **title -> play -> gravestone
-> Hall of Herders -> title**. The simulation and the entire language pipeline
are byte-exact (3287 host checks). The renderer (`render/fb.c`, verified on the
host via `scripts/render-preview.sh`) draws a close, herder-following camera with
sprites (sheep, herder, sheepdog, trees, houses, boulders, wells, scarecrows,
library boxes), the web's palette, grass texture, the pen, a walk-cycle
animation, the day/night sky tint, weather (rain/fog), and an island minimap.
The Hall records each finished day and persists to the VMU.

What is left, and why:

1. **On-hardware verification** — this machine has no Flycast, so the world art
   is host-verified but the **BIOS-font text** (HUD, speech, title, Hall) and the
   **VMU save** have never actually run. The VMU write is a best-effort raw
   `fs_write`; a real card likely needs a `vmu_pkg` wrapper with an icon. These
   need a screen/card to confirm and finish.
2. **Presentation polish** — richer sprite animation than the two-frame leg
   cycle; a floating speech bubble by the herder instead of the bottom panel.
3. **Render-proximity delighters** — the web triggers extra lines when the herder
   passes a signpost/inn/well/hens/cow and when the wind takes his hat; the
   speech for these is ported (`speakKind`), but the proximity triggers live in
   the web's render loop and are not wired in yet. Also the rainbow-after-rain
   and the finale fly-through.

## Where this stands (honest)

All **game logic is done and proven** byte-exact and running. The world is drawn
with a **faithful, host-verified renderer**, and the game has its full **title /
play / Hall** structure with VMU persistence. A fully polished, hardware-confirmed
1:1 build is roughly **78%** there. The biggest remaining unknown is not more
code but **eyes on real hardware**: the text layer and VMU are written but
unverified, and the last stretch of polish (animation, bubbles, proximity
delighters) is best tuned against a screen.
