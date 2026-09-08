/* Initial flock placement (createWorld in state.ts), the deterministic part.
 * Same rings, roof/river/boulder swaps and per-sheep draws as the web game. */
#ifndef HERDER_FLOCK_H
#define HERDER_FLOCK_H
#include "core/map/generate.h"

typedef struct {
    int id; double x, y, home_x, home_y;
    double skittish;
    int flees, ring;
    int absurd, seen, named;
    double tx, ty;
    double speed;
    int on_roof, in_river, on_boulder, black;
    int temper; /* 0 plain,1 skittish,2 stubborn,3 dozy,4 curious, -1 none */
    int mode;   /* 0 loose, 1 carried, 2 penned */
    int thief, escapee, greeted, nemesis;
} HerderSheep;

int herder_create_flock(const HerderMap *map, const char *seed, HerderSheep *out, int max);

typedef struct { int x, y; int taken; int book; /* catalog index */ } HerderLib;
/* Full deterministic world setup: flock then libraries, sharing one rng.
 * Returns sheep count; writes library count to *lib_count. */
int herder_place_libraries(const HerderMap *map, const HerderSheep *sheep, int sheepN,
                           uint32_t *stp, HerderLib *out);
int herder_create_world(const HerderMap *map, const char *seed,
                        HerderSheep *sheep_out, int sheep_max,
                        HerderLib *lib_out, int *lib_count);

#endif
