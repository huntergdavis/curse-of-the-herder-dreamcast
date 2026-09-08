/* Map generation, a 1:1 port of src/core/map/generate.ts.
 * Deterministic given (seed, size): elevation/moisture, rivers, the pen,
 * villages, roads (A*), decorations, and the pen distance field. */
#ifndef HERDER_GENERATE_H
#define HERDER_GENERATE_H
#include <stdint.h>

typedef struct { int x, y; int name; /* index into VILLAGE_NAMES */ } HerderVillage;

typedef struct {
    int size;
    uint8_t *terrain;   /* size*size */
    uint8_t *deco;      /* size*size */
    double *pen_distance; /* size*size */
    int pen_x, pen_y;
    HerderVillage villages[16];
    int village_count;
    int walkable_count;
} HerderMap;

/* Allocates terrain/deco/pen_distance; free with herder_map_free. */
void herder_generate_map(const char *seed, int size, HerderMap *out);
void herder_map_free(HerderMap *m);

#endif
