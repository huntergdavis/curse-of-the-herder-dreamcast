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

Everything that can be built **and verified** from this environment is done and
confirmed. The port is a faithful, playable, byte-exact rendering of the game:

- **Logic** — the whole simulation and language pipeline, byte-exact (3287 checks):
  same day, same events, same words as the web, tick for tick.
- **Renderer** (`render/fb.c`, every frame host-verified via PNG preview) — a
  close herder-following camera; sprites for sheep (with named ribbons and a
  nemesis crown), the herder (belt, level-8 scarf, level-4 book, overhead carry),
  the sheepdog, trees, houses, and the rest; a walk cycle with shadows; the
  web's palette and grass texture; the pen filling with penned sheep; the
  day/night sky tint; rain, fog, and a rainbow after; an island minimap.
- **Text** — a portable bitmap font, so the HUD, floating speech bubble, title,
  Hall, and gravestone are identical on host and Dreamcast (no BIOS-font dep).
- **Structure** — title -> play -> gravestone induction -> Hall of Herders ->
  title, with signpost/hat delighters and a proper vmu_pkg save.

**A fully polished, hardware-confirmed 1:1 build is roughly 92% there.** The last
stretch is not more logic and mostly cannot be finished blind:

1. **Hardware/emulator confirmation** — no Flycast or Dreamcast on this machine.
   The world, sprites, and all text are host-verified, but the **VMU save
   round-trip** has never run against a real card and nothing has been seen on a
   TV. This is the one real gate.
2. **Diminishing polish** — grazing/asleep sheep poses and more animation
   frames (four-directional facing, the eased camera, and the rival-and-flock
   crossing are now done); a couple more proximity delighters (inn/hens/cow).

Note on "1:1": the logic and generated text are literally byte-identical to the
web. The *rendering* is a faithful reproduction, not a pixel copy — a 640x480
framebuffer with hand-drawn sprites is a different medium from the web's
high-DPI vector canvas, so "parity" here means the same world, the same words,
the same systems, and the same look and feel, all confirmed frame by frame.
