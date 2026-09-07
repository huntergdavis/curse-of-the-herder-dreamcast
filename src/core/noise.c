#include "core/noise.h"
#include <math.h>

/* JS `a ^ b` and Math.imul are 32-bit; `x >>> 0` is uint32; `n / 2^32` is double.
 * The products ix*K and iy*K are computed in JS as doubles (exact < 2^53) then
 * coerced to int32 by `^`, i.e. taken mod 2^32 — reproduced here in 64-bit. */
static double lattice(uint32_t seed, int ix, int iy) {
    uint32_t h = seed ^ 0x9e3779b9u;
    uint32_t px = (uint32_t)((int64_t)ix * (int64_t)0x27d4eb2d);
    h = (h ^ px) * 0x165667b1u;
    uint32_t py = (uint32_t)((int64_t)iy * (int64_t)0x85ebca6b);
    h = (h ^ py) * 0xc2b2ae35u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return (double)h / 4294967296.0;
}

static double smooth(double t) { return t * t * (3.0 - 2.0 * t); }

double herder_value_noise(uint32_t seed, double x, double y) {
    double fx0 = floor(x), fy0 = floor(y);
    int x0 = (int)fx0, y0 = (int)fy0;
    double tx = smooth(x - fx0);
    double ty = smooth(y - fy0);
    double a = lattice(seed, x0, y0);
    double b = lattice(seed, x0 + 1, y0);
    double c = lattice(seed, x0, y0 + 1);
    double d = lattice(seed, x0 + 1, y0 + 1);
    return (a * (1.0 - tx) + b * tx) * (1.0 - ty) + (c * (1.0 - tx) + d * tx) * ty;
}

double herder_fbm_full(uint32_t seed, double x, double y, double scale,
                       int octaves, double gain, double lacunarity) {
    double amp = 1.0;
    double freq = 1.0 / scale;
    double sum = 0.0, norm = 0.0;
    for (int o = 0; o < octaves; o++) {
        /* seed + o*101 is a JS double coerced to uint32 by the `^` in lattice();
         * modular uint32 addition reproduces that for our seed range. */
        uint32_t s = seed + (uint32_t)(o * 101);
        sum += amp * herder_value_noise(s, x * freq + o * 17.3, y * freq - o * 9.1);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return sum / norm;
}

double herder_fbm(uint32_t seed, double x, double y, double scale) {
    return herder_fbm_full(seed, x, y, scale, 5, 0.5, 2.0);
}
