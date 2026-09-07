#include "core/map/terrain.h"
#include <math.h>

const double HERDER_TERRAIN_COST[TERRAIN_COUNT] = {
    INFINITY, 1.3, 1.0, 1.0, 1.4, 1.6, 2.6, 2.4, INFINITY, 0.7, 0.7,
};
const double HERDER_TERRAIN_SPEED[TERRAIN_COUNT] = {
    0.0, 0.8, 1.0, 1.0, 0.72, 0.62, 0.4, 0.42, 0.0, 1.35, 1.35,
};

bool herder_is_walkable(int t) {
    return t >= 0 && t < TERRAIN_COUNT && isfinite(HERDER_TERRAIN_COST[t]);
}

int herder_classify(double e, double m) {
    if (e < 0.31) return T_Water;
    if (e < 0.345) return T_Sand;
    if (e > 0.855) return T_Snow;
    if (e > 0.745) return T_Rock;
    if (e < 0.40 && m > 0.58) return T_Mud;
    if (m > 0.64) return T_Forest;
    if (m < 0.36) return T_Meadow;
    return T_Grass;
}
