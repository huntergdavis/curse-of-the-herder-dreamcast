#!/usr/bin/env bash
# Build the Dreamcast ELF (and, with "cdi", a self-booting disc image) inside the KallistiOS Docker image.
#   ./scripts/dc-build.sh          -> herder.elf
#   ./scripts/dc-build.sh cdi      -> build/herder.cdi
set -euo pipefail
cd "$(dirname "$0")/.."
IMAGE="${DC_IMAGE:-einsteinx2/dcdev-kos-toolchain:latest}"
docker run --rm -v "$PWD":/src -w /src -u "$(id -u):$(id -g)" "$IMAGE" sh -c "make ${1:-all}"
