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

Boot proof only. See [PLAN.md](PLAN.md) for the phased plan and
[docs/](docs/) for the platform notes.

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
