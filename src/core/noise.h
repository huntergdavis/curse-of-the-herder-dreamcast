/* Seeded 2D value noise with fBm. A 1:1 port of src/core/noise.ts: identical
 * IEEE-754 double math and 32-bit lattice hashing, bit-exact with the web game. */
#ifndef HERDER_NOISE_H
#define HERDER_NOISE_H
#include <stdint.h>

double herder_value_noise(uint32_t seed, double x, double y);
double herder_fbm(uint32_t seed, double x, double y, double scale);
/* Full form, for parity with the TS defaults (octaves 5, gain 0.5, lacunarity 2). */
double herder_fbm_full(uint32_t seed, double x, double y, double scale,
                       int octaves, double gain, double lacunarity);

#endif
