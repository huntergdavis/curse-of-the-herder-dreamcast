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

Host tests: **879 checks, 0 failures.** Every file also compiles for the SH-4.

## Remaining (in dependency order)

1. **Libraries** (small): the last piece of world setup; same method.
2. **Sim step** (`step.ts`, ~840 lines): the whole day. Herder modes, sheep
   temperaments, mishaps, weather, streaks, nemesis, jailbreaks, lunch, the
   rival, the dog's moments, milestones, progression, frustration, the
   governor, wall-clock catch-up. Golden: a full-day event transcript per seed.
   The critical path; its RNG-draw ordering is unforgiving.
3. **Grammar**: morphology, the rule engine (bands × levels, registers, repeat
   memory, modifiers, gates), speech (event → line). Golden: generated line per
   (event, context, tick, seed).
4. **Data**: export the lexicon (~2,500 words), grammar templates (~1,000),
   29 books, banned list from the web repo to C tables / a romdisk blob.
   Independent of the sim engine; the natural parallel track.
5. **Main-loop glue**: what he says and when (queue, remarks, flyting, diary).
6. **Renderer on PowerVR**: terrain chunks, sprites, text, bubbles, day tint,
   weather, the delighters. No parity constraint; the largest raw effort; can
   proceed against the already-ported map in parallel.
7. **Menu, VMU saves, Hall, persistence.**

## Parallelisation

The sim step and the grammar engine are cohesive and order-sensitive — best
driven sequentially by one owner with golden transcripts. The clean, low-risk
parallel seams are: the data-export pipeline (4), the renderer foundation (6),
and morphology (a leaf of 3). Those can be separate agents; the rest should not
be split, because sub-systems share the world state and the exact draw order.
