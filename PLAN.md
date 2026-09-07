# Porting plan

The web game is the source of truth for content and behaviour. The Dreamcast
build is a rewrite of the engine in C with the data generated from the web
repository, so a word banned there stays banned here and a transcript that
reads well there reads the same here.

## Targets and budgets

| Item | Web | Dreamcast |
| --- | --- | --- |
| CPU | any | SH-4 200 MHz, hardware float32 only |
| RAM | plenty | 16 MB main, 8 MB video |
| Screen | any | 640×480 VGA / 480i, PowerVR |
| Save | IndexedDB | VMU, 200 blocks of 512 bytes per unit |
| Clock | wall clock | none reliable in emulators; count frames |
| Distribution | Pages | `.cdi` disc image and the same image in the browser |

Budget: under 6 MB of data resident, under 30% of a 60 Hz frame for the sim,
fixed-point or float32 everywhere (the Float32 distance-field bug from the web
port is known; the field will be integer costs).

## Phases

0. **Boot proof** (done): a KallistiOS program that clears the screen, draws a
   pen and a blinking sheep, exits on Start; Docker build; `.cdi`.
1. **Try-before-you-download site**: GitHub Pages hosts Flycast WASM with the
   current `.cdi` and a download link. CI builds the disc and publishes both.
   Cross-origin isolation via a service worker; BIOS-less boot via HLE.
2. **Data export**: a script in the web repo emits compact tables (lexicon,
   grammar rules, books, names, seasons) as C sources or a romdisk blob.
   Golden tests: the C grammar must reproduce a web transcript for a seed.
3. **Core in C**: RNG (fnv1a + mulberry32), map generation (value noise,
   rivers, villages, roads via A*, libraries), Dijkstra with integer costs,
   sim step (herder, sheep temperaments, mishaps, weather, streaks, nemesis,
   jailbreaks, lunch, rival, dog moments), progression and frustration.
   Host build first (SDL2 or headless) with a transcript tool, compared line
   by line to the web transcript.
4. **Grammar in C**: template expansion, modifiers (a/an, plurals, thou,
   alliteration, syllables), bands × levels, registers, repeat memory, ban
   list check at build time. Golden transcript parity.
5. **Renderer on PowerVR**: chunked terrain as textured quads (blob autotile
   pre-rasterised into a tile atlas), procedural sprites as small textured
   quads, text from glyph atlases of the two OFL fonts, speech bubbles,
   day tint, weather, the delighters in order of joy per byte.
6. **Menu, saves, Hall**: controller-driven menu (speed, level cap, text size,
   end-of-day), VMU saves for herder-in-progress and the Hall, the VMU LCD
   shows the word of the day.
7. **Polish and release**: attract-mode behaviour, 480i safe areas, real
   hardware test on a CD-R, 1.0 disc.

## Testing

- Host build with a transcript tool and a frame-capture tool; golden files
  from the web repo.
- Flycast headless on CI where possible; the browser emulator for eyes-on.
- Every push builds the ELF and the CDI; the Pages site always runs the last
  green disc.
