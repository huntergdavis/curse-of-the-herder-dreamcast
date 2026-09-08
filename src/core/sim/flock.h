/* Initial flock placement (createWorld in state.ts), the deterministic part.
 * Same rings, roof/river/boulder swaps and per-sheep draws as the web game. */
#ifndef HERDER_FLOCK_H
#define HERDER_FLOCK_H
#include "core/map/generate.h"

typedef struct {
    int id, x, y, home_x, home_y;
    double skittish;
    int flees, ring;
    int absurd, seen, named;
    int tx, ty;
    double speed;
    int on_roof, in_river, on_boulder, black;
    int temper; /* 0 plain,1 skittish,2 stubborn,3 dozy,4 curious, -1 none */
} HerderSheep;

int herder_create_flock(const HerderMap *map, const char *seed, HerderSheep *out, int max);

#endif
