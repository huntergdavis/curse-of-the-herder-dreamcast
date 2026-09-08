# Curse of the Herder — Dreamcast

A port of [Curse of the Herder](https://github.com/huntergdavis/curse-of-the-herder)
(play it in a browser at https://hunterdavis.com/curse-of-the-herder/) to the
Sega Dreamcast, built on [KallistiOS](https://github.com/KallistiOS/KallistiOS).

A cursed herder gathers sixty sheep across an island over a nine-hour day,
learns to swear from the books he finds in little free libraries, and retires
to a Hall of Herders with his last words on a stone. No language model: a
deterministic grammar over curated word lists, generated from the web game's
data so the two stay in step.

**Try it in your browser, then download the disc:** the GitHub Pages site for
this repository runs the game inside
[Flycast WASM](https://github.com/nasomers/flycast-wasm), a Dreamcast emulator
compiled to WebAssembly, with the same `.cdi` you can burn or load in
Flycast, Redream or a flash-cart loader.

## Status

**Complete and verified 1:1 with the web version.** The whole game runs on the
Dreamcast: a full nine-hour day, sixty sheep, the little libraries, the Hall of
Herders, and every curse the herder learns to say.

- **Logic and language are byte-identical** to the web original. The simulation
  and the entire language pipeline (morphology, ban filter, ~2,700-word lexicon,
  ~1,650 grammar rules, the rule engine and speech) are a C rewrite whose output
  is asserted byte-for-byte against golden vectors generated from the web repo —
  same day, same events, same words, tick for tick (3,287 automated checks, all
  green; `./scripts/host-test.sh`).
- **The presentation matches the web** screen-for-screen: the boxed HUD, the
  herder with his four facings and poses, the sheep (idle/graze/asleep, named
  ribbons, nemesis crowns, black), the border-collie dog, the rival and his tidy
  flock, cows, hens, frogs, villagers, seasonal terrain and trees, grass texture,
  the pen's rail fence, day/night tint, rain, fog, rainbows, sun-drifting
  shadows, a follow-camera with look-ahead, speech bubbles, the Curse's dry
  banner, the Sad Almanac forecast, the toasts and the diary, and the gravestone
  finale.
- **Verified on the emulator.** Every screen and system has been run under native
  Flycast (reios HLE BIOS) and captured; see the `docs/emulator-*.png` and
  `docs/web-reference*.png` side-by-side captures. `scripts/emulator-shot.sh`
  reproduces the emulator captures.

The only step not done from the build environment is a sign-off on physical
hardware (a real Dreamcast, CRT and VMU), which runs the identical reios/KOS
path the emulator does. See [docs/parity-progress.md](docs/parity-progress.md)
for the module-by-module record and [PLAN.md](PLAN.md) for the original plan.

## Controls

- **D-pad left/right** — choose the pasture (seed) on the title; **Start / A** —
  begin the day.
- **A** (in game) — cycle the speed (×1 real-time … ×300); the HUD shows it.
- **Start** (in game or at the gravestone) — return to the title.
- **Y** (title) — the Hall of Herders.
- A finished day is inducted into the Hall, saved to the VMU, and a new herder
  wakes at dawn — the same ambient loop the web runs by default.

## Building

The toolchain lives in Docker (KallistiOS, GCC 9.3 for SH-4, `cdi4dc`,
`makeip`, `genromfs`), so nothing is installed on the host:

```
./scripts/dc-build.sh        # herder.elf, for emulators and dc-load
./scripts/dc-build.sh cdi    # build/herder.cdi, a self-booting disc image
```

The image is `einsteinx2/dcdev-kos-toolchain`, pulled on first use.

## Running

- **Browser:** the Pages site, or `node scripts/serve-site.mjs` for a local
  copy (it sends the cross-origin isolation headers the emulator needs).
- **Flycast / Redream on a desktop:** open `build/herder.cdi`, or load
  `herder.elf` directly in Flycast.
- **Real hardware:** burn `build/herder.cdi` to CD-R (most consoles boot it
  without modification) or load it from a GDEMU or similar.

## Licence

Our code is MIT, see [LICENSE](LICENSE). The game's words, books and design are
shared with the web original under its licence. Flycast and Flycast WASM are
GPLv2 and are redistributed unmodified in `site/emulator/` with their
[source](https://github.com/nasomers/flycast-wasm); no Sega BIOS is included
or needed, the emulator boots homebrew with its own high-level BIOS.
KallistiOS is under its own permissive licence.
