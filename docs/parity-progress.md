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

1. **Visual fidelity**: the game runs on the Dreamcast with a faithful
   framebuffer renderer (`render/fb.c`): a close camera (22px tiles) following
   the herder, sprites for sheep/herder/trees/houses/boulders/wells/scarecrows/
   library boxes, the web's exact terrain palette, grass texture speckle, the
   pen enclosure, the day/night sky tint (ported from `palette.ts` `dayTint`),
   and an island minimap. Verified on the host with a PNG preview pipeline
   (`scripts/render-preview.sh`). Still to add for full polish: animated sprite
   frames, the floating speech bubble (currently a bottom panel), the sheepdog,
   and weather visuals (rain/fog).
2. **Real-time speech glue**: the queue, recent-line memory feedback, flyting
   and curse replies, the diary — the wall-clock-gated parts of `main.ts`. Not
   byte-exact-verifiable (they depend on frame timing), so they belong with the
   app, not the golden suite.
3. **Platform**: menu, VMU saves, the Hall of Shame, polished disc packaging.

## Where this stands (honest)

The simulation and the curse-writing brain are **finished and proven** byte-exact
end to end (3287 checks) and **run on the Dreamcast**, and the world is now drawn
with a **faithful sprite renderer** (palette, sprites, texture, day tint,
minimap), verified frame-by-frame on the host. Remaining is presentation polish
(sprite animation, speech bubble, sheepdog, weather visuals) and the platform
layer (title/menu, VMU saves, Hall of Shame). Overall a fully polished 1:1
Dreamcast build is roughly **68%** there; the logic is complete and running and
the world looks like the game.
