# Toolchain

- Docker image `einsteinx2/dcdev-kos-toolchain:latest` (Alpine; KallistiOS; sh-elf-gcc 9.3.0; kos-ports incl. SDL 1.2, libpng, freetype; `cdi4dc`, `makeip`, `genromfs`, `scramble`). Verified 2026-09-07 on this machine.
- `scripts/dc-build.sh` runs `make` inside the image with the repo mounted at `/src` and the KOS environment already set.
- `make cdi` scrambles the binary, stamps `disc/ip.txt` into IP.BIN, builds an ISO and wraps it as a CDI.
- Emulators: Flycast (desktop) loads `.elf` directly; the browser build in `site/` loads the `.cdi`.
