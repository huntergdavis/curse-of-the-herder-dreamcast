/* Deterministic, stateless keyed RNG. A 1:1 port of src/core/rng.ts from the
 * web game: identical fnv1a, mulberry32 and keyedUnit, so every roll matches.
 * Strings are hashed as bytes; the game only ever feeds ASCII, so this equals
 * the web version's UTF-16-code-unit hashing for all real inputs. */
#ifndef HERDER_RNG_H
#define HERDER_RNG_H
#include <stdint.h>

uint32_t herder_fnv1a(const char *s);
/* One draw of mulberry32 seeded by `seed`, as the raw uint32 numerator. */
uint32_t herder_mulberry32_u32(uint32_t *state);
/* The full unit-interval roll (numerator / 2^32), matching keyedUnit(). */
double herder_unit_from_u32(uint32_t u);

/* keyedUnit(seed, key...) built from a pre-joined "a|b|c" string. */
double herder_keyed_unit_joined(const char *joined);

/* keyedUnit with up to 6 integer keys after the leading string key, joined with '|'.
 * Covers every call shape in the sim (a domain string then small integers). */
double herder_keyed_unit(const char *seed, const char *domain,
                         const int32_t *ints, int nints);

#endif
