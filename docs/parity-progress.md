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

Host tests: **1605 checks, 0 failures** (including a full-day, tick-exact event
transcript of 626 events across two seeds). Every file also compiles for the SH-4.

## Remaining (in dependency order)

The entire deterministic simulation is done and byte-exact. What is left is the
language layer and the presentation layer.

1. **Grammar**: morphology, the rule engine (bands × levels, registers, repeat
   memory, modifiers, gates), speech (event → line). Golden: generated line per
   (event, context, tick, seed).
2. **Data**: export the lexicon (~2,500 words), grammar templates (~1,000),
   29 books, banned list from the web repo to C tables / a romdisk blob.
   Independent of the sim engine; the natural parallel track.
3. **Main-loop glue**: what he says and when (queue, remarks, flyting, diary).
4. **Renderer on PowerVR**: terrain chunks, sprites, text, bubbles, day tint,
   weather, the delighters. No parity constraint; the largest raw effort; can
   proceed against the already-ported map in parallel.
5. **Menu, VMU saves, Hall, persistence.**

## Parallelisation

The grammar engine is cohesive and order-sensitive — best driven sequentially
by one owner with golden transcripts. The clean, low-risk parallel seams are:
the data-export pipeline (2), the renderer foundation (4), and morphology (a
leaf of 1). Those can be separate agents; the rest should not be split, because
sub-systems share the world state and the exact draw order.
