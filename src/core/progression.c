#include "core/progression.h"
#include <math.h>

const char *const HERDER_LEVEL_NAMES[13] = {
    "Grunting", "Two Words", "Simple Sentences", "The Comparative", "Vocative Fury",
    "The Vulgar Tongue", "The Bard", "Polyglot", "Hemingway", "Nautical",
    "The Baroque", "Verse", "Unhinged Laureate",
};

const double FR_RIVAL = 7, FR_LUNCH_STOLEN = 24, FR_PER_FORTY_CARRY = 1, FR_FLEE = 6,
    FR_ABSURD = 8, FR_ROUGH = 2, FR_RAIN_PER_MIN = 0.5, FR_DUSK_PER_MIN = 0.2, FR_WALK_OF_SHAME = 3,
    FR_REPEAT_ESCAPE = 12, FR_PENNED = -5, FR_BOOK = -10, FR_BREATHER = -5, FR_DECAY_PER_MIN = -0.5;

double herder_erudition(double books, double sheep, double hours) {
    return 100 * books + 8 * sheep + 3 * hours;
}
int herder_level_for(double e) {
    double v = floor(fmax(0.0, e) / HERDER_ERUDITION_PER_LEVEL);
    if (v < 0) v = 0;
    if (v > HERDER_MAX_LEVEL) v = HERDER_MAX_LEVEL;
    return (int)v;
}
int herder_filth_ceiling(double f) {
    if (f < 8) return 0;
    if (f < 22) return 1;
    if (f < 42) return 2;
    if (f < 66) return 3;
    return 4;
}
double herder_curse_interval_seconds(int level, double frustration) {
    double base = 42;
    double v = (base * (1.1 - frustration / 100)) / (1 + (double)level / HERDER_MAX_LEVEL);
    return fmax(4.0, v);
}
double herder_clamp_frustration(double v) { return fmax(0.0, fmin(100.0, v)); }
double herder_frustration_baseline(double hours) { return fmax(0.0, fmin(74.0, 10 + hours * 7.5)); }
double herder_frustration_drift(double current, double baseline, double dt) {
    if (current > baseline) return fmax(baseline, current - (0.8 + (current - baseline) / 40) * dt);
    return fmin(baseline, current + 2.0 * dt);
}
