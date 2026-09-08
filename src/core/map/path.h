/* 8-connected Dijkstra distance field and A*, a 1:1 port of src/core/map/path.ts.
 * The binary min-heap's tie-breaking and Float64 costs are reproduced exactly,
 * because a single differing tie cascades into a different map. */
#ifndef HERDER_PATH_H
#define HERDER_PATH_H
#include <stdint.h>

typedef struct { int size; const uint8_t *terrain; } HerderGrid;

/* Optional per-tile cost hook (roads, fences): base is the terrain cost. */
typedef double (*herder_cost_override)(void *ctx, int x, int y, double base);

/* Path cost from (sx,sy) to every tile; caller frees. INFINITY = unreachable. */
double *herder_distance_field(const HerderGrid *g, int sx, int sy,
                              herder_cost_override ov, void *ctx);

/* A* from (sx,sy) to (gx,gy). Writes the path (tiles after start..goal) into
 * out_xy as x,y pairs and returns the count, or -1 if unreachable. out_xy must
 * hold at least size*size*2 ints. */
int herder_find_path(const HerderGrid *g, int sx, int sy, int gx, int gy,
                     herder_cost_override ov, void *ctx, int *out_xy);

#endif
