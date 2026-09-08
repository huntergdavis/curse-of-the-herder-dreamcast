/* Erudition, level, filth band and frustration curves. 1:1 with src/core/progression.ts. */
#ifndef HERDER_PROGRESSION_H
#define HERDER_PROGRESSION_H

#define HERDER_MAX_LEVEL 12
#define HERDER_ERUDITION_PER_LEVEL 210

extern const char *const HERDER_LEVEL_NAMES[13];

double herder_erudition(double books, double sheep, double hours);
int herder_level_for(double erudition);
int herder_filth_ceiling(double frustration);
double herder_curse_interval_seconds(int level, double frustration);
double herder_clamp_frustration(double v);
double herder_frustration_baseline(double hours);
double herder_frustration_drift(double current, double baseline, double dt_minutes);

/* Frustration deltas (FRUSTRATION in progression.ts). */
extern const double FR_RIVAL, FR_LUNCH_STOLEN, FR_PER_FORTY_CARRY, FR_FLEE,
    FR_ABSURD, FR_ROUGH, FR_RAIN_PER_MIN, FR_DUSK_PER_MIN, FR_WALK_OF_SHAME,
    FR_REPEAT_ESCAPE, FR_PENNED, FR_BOOK, FR_BREATHER, FR_DECAY_PER_MIN;

#endif
