#include "core/rng.h"
#include <stdio.h>
#include <string.h>

uint32_t herder_fnv1a(const char *s) {
    uint32_t h = 0x811c9dc5u;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= (uint32_t)(*p);
        h *= 0x01000193u; /* 32-bit wrap matches Math.imul on ASCII inputs */
    }
    return h;
}

/* mulberry32: advance the state and return the uint32 numerator. Mirrors the JS,
 * where |0 and >>>0 are 32-bit ops and Math.imul is a 32-bit multiply. */
uint32_t herder_mulberry32_u32(uint32_t *state) {
    uint32_t a = (*state + 0x6d2b79f5u);
    *state = a;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return t ^ (t >> 14);
}

double herder_unit_from_u32(uint32_t u) {
    return (double)u / 4294967296.0;
}

double herder_keyed_unit_joined(const char *joined) {
    uint32_t st = herder_fnv1a(joined);
    return herder_unit_from_u32(herder_mulberry32_u32(&st));
}

double herder_keyed_unit(const char *seed, const char *domain,
                         const int32_t *ints, int nints) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s|%s", seed, domain ? domain : "");
    if (domain == NULL) n = snprintf(buf, sizeof(buf), "%s", seed);
    for (int i = 0; i < nints && n < (int)sizeof(buf); i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "|%d", (int)ints[i]);
    return herder_keyed_unit_joined(buf);
}
