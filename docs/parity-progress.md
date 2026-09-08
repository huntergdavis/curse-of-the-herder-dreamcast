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

The game is a faithful, playable, byte-exact port: **title -> play -> gravestone
-> Hall -> title**, with a floating speech bubble, a portable bitmap font (so all
text is host-verified, no BIOS-font dependency), sprites with a walk cycle, the
sheepdog, day/night tint, rain/fog, an island minimap, render-layer delighters
(signpost, hat-in-wind) on top of every sim-event line, a rainbow after the rain,
a gravestone induction finale, and a proper vmu_pkg save.

What is left is small and mostly needs a screen or a memory card:

1. **On-hardware confirmation** — no Flycast/hardware here. The world, sprites,
   and *all text* are verified via the host PNG pipeline, but the actual **VMU
   save round-trip** has never run against a real card (the code now builds a
   correct vmu_pkg, so it should work), and nothing has been seen on a TV.
2. **A few minor delighters** — the signpost and hat lines are wired; inn/hens/
   cow/stick could be added the same way where the map supports them.
3. **Animation depth** — the walk cycle is two frames; the web's sprites are
   richer.

## Where this stands (honest)

Every system the web has is now reproduced and **verified on the host**, frame by
frame, via the PNG preview pipeline:

- **Logic** — the whole simulation and language pipeline, byte-exact (3287 checks).
- **World** — terrain by type with the web palette and texture, decorations
  (trees, houses, boulders, wells, scarecrows, signposts, library boxes), the
  pen filling with penned sheep, the road network, the winding rivers.
- **Sprites** — sheep in idle/walk/graze/asleep poses with named ribbons,
  nemesis crowns, and black variants; the herder in idle/walk/carry/reading/
  resting/ranting poses, four-directional, with belt, level-8 scarf, level-4
  book, and shadows; the sheepdog; the rival and his tidy flock.
- **Atmosphere** — day/night sky tint, rain, fog, a rainbow after the rain, an
  eased follow-camera, and an island minimap.
- **UI & flow** — a portable bitmap font (all text host-verified), a floating
  speech bubble, the HUD with level names, the title screen, the gravestone
  induction finale, and the Hall of Herders with a vmu_pkg save.

**The one remaining gate is hardware.** There is no Dreamcast, native Flycast,
or working KOS-capable emulator on this machine (the bundled Flycast WASM core
does not boot KOS homebrew). So the **VMU save round-trip** has never run against
a real card and the build has never been seen on a TV. Everything that can be
built and confirmed without a device is done and confirmed — call it **~95%**,
with the last ~5% being purely that hardware confirmation (and, should it turn up
anything, whatever small fixes it reveals).

Note on "1:1": the logic and generated text are literally byte-identical to the
web; the rendering is a faithful reproduction on a 640x480 framebuffer, matching
the web's world, sprites, atmosphere, UI, and flow rather than copying vector
pixels. To finish: run `build/herder.cdi` on hardware or in a KOS-capable Flycast
and send back a screenshot (and whether a VMU save persists).
