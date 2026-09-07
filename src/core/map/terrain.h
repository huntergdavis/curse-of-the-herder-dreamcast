/* Terrain and decoration ids, costs and the classification thresholds.
 * A 1:1 port of src/core/map/terrain.ts plus the classify() used in generate.ts. */
#ifndef HERDER_TERRAIN_H
#define HERDER_TERRAIN_H
#include <stdbool.h>

enum {
  T_Water = 0, T_Sand, T_Grass, T_Meadow, T_Farm, T_Forest,
  T_Mud, T_Rock, T_Snow, T_Road, T_Bridge, TERRAIN_COUNT
};

enum {
  D_None = 0, D_Tree, D_Tree2, D_Boulder, D_Tuft, D_Flowers, D_House,
  D_HouseRed, D_Well, D_Fence, D_PenGround, D_Library, D_Stump,
  D_Signpost, D_Scarecrow
};

extern const double HERDER_TERRAIN_COST[TERRAIN_COUNT];   /* INFINITY = impassable */
extern const double HERDER_TERRAIN_SPEED[TERRAIN_COUNT];

bool herder_is_walkable(int t);
/* Terrain id from elevation e and moisture m; the exact threshold ladder from generate.ts. */
int herder_classify(double e, double m);

#endif
