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

Host tests: **1931 checks, 0 failures** (including a full-day, tick-exact event
transcript of 626 events across two seeds, and byte-exact morphology and ban
checks). Every file also compiles for the SH-4.

## Remaining (in dependency order)

The entire deterministic simulation is done and byte-exact, and so are the two
self-contained language leaves (morphology and the ban gate). What is left is
the grammar core, the data it reads, and the presentation layer.

1. **Data export**: emit the lexicon (~2,500 entries across the reviewed packs,
   in order CORE_PACKS + PACKS_5_8 + PACKS_9_12), the grammar rules
   (RULES + RULES_5_8 + RULES_9_12 + CALLBACK_RULES, in that order), the
   non-terminals, and the 29 books to C tables. Order is parity-critical
   because it fixes the byPos and rule lists the weighted picks walk. Best done
   by a generator that imports the real TS data. Mechanical; the natural
   parallel track.
2. **Grammar engine** (`grammar.ts`): the rule picker, slot expansion, register
   and band weighting, allit/syllable/own constraints, modifiers, contextual
   symbols, heat. RNG-draw order is unforgiving, exactly as the sim step was.
   Golden: generated line per (event, context, tick, seed).
3. **Speech** (`speech.ts`): world -> context -> line; signature words,
   target selection, timing.
4. **Main-loop glue**: what he says and when (queue, remarks, flyting, diary).
5. **Renderer on PowerVR**: terrain chunks, sprites, text, bubbles, day tint,
   weather, the delighters. No parity constraint; the largest raw effort; can
   proceed against the already-ported map in parallel.
6. **Menu, VMU saves, Hall, persistence.**

## Where this stands (honest)

The deterministic **simulation is 100% ported and proven** byte-exact over a
full day. The **language engine's foundations are done** (morphology, ban
filter). The **grammar rule engine, speech, and the ~3,400 lines of word and
template data** are the next large block — this is the heart of the game, the
part that actually writes the curses, and it will take the same golden-transcript
grind the sim did. Rendering and platform come after. Overall a fully playable
1:1 Dreamcast build is roughly **40%** there; the hard, exactness-critical brain
is the part that is furthest along.
