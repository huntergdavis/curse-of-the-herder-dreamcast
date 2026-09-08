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

## Remaining (in dependency order)

The entire deterministic game logic is done and byte-exact: the simulation and
the whole language pipeline (morphology, ban gate, lexicon+grammar data, the
rule engine, and speech). Given a seed and a wall-clock season, the C port
produces the same day, event for event, and the herder says the same words,
utterance for utterance, as the web. What remains is presentation and platform,
none of it under a byte-exact constraint.

1. **Main-loop glue** (`main.ts`, the non-DOM parts): the utterance queue and
   recent-line memory, idle-curse scheduling (`nextIdleCurseTicks` is already
   ported), flyting/curse replies, the diary. Mostly bookkeeping around the
   already-ported speech calls.
2. **Renderer on PowerVR**: terrain chunks, sprites and their animation, text,
   speech bubbles, day tint, weather, the delighters, HUD. The largest raw
   effort; proceeds against the already-ported map and sim. Can be built in
   parallel now that the state it draws is fixed.
3. **Menu, VMU saves, Hall of Shame, disc boot/packaging.**

## Where this stands (honest)

The hard, exactness-critical two-thirds of the game — the simulation and the
curse-writing brain — is **finished and proven** byte-exact end to end (3287
checks). What is left is drawing it on the Dreamcast's PowerVR and the console
platform layer (saves, menu, disc). That is a large amount of code, but it is
conventional game-rendering and integration work with no parity constraint, and
it can proceed against fixed, verified state. Overall a fully playable 1:1
Dreamcast build is roughly **55%** there, and every remaining piece is
presentation or platform, not logic.
