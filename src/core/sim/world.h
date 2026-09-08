/* The simulation state and one-tick step, a 1:1 port of sim/state.ts + step.ts.
 * Only fields that affect the event stream and control flow are modelled. */
#ifndef HERDER_WORLD_H
#define HERDER_WORLD_H
#include "core/sim/flock.h"
#include "core/map/generate.h"

typedef struct { int seq, tick; char kind[16]; int sheep_id; char detail[24]; char book[24]; } HerderEvent;

enum { HM_IDLE, HM_TOSHEEP, HM_TOPEN, HM_RESTING, HM_DONE, HM_TOLIBRARY, HM_READING, HM_RANTING, HM_GAZING, HM_MISHAP };

typedef struct {
    double x, y;
    int facing, mode;
    int *path;      /* x,y pairs */
    int path_len, path_head, path_cap;
    int targetSheep, carrying, targetLibrary;
    int restUntilTick, carryOdometer, approachCount, lastTileX, lastTileY, tripTiles;
    int rantReturnMode;
    char mishap[16];
    int gateJammed;
} HerderHerder;

typedef struct {
    const char *seed;
    const HerderMap *map;
    int tick;
    double frustration, speedScale, totalWork;
    HerderHerder h;
    HerderSheep *sheep; int sheep_count;
    HerderLib *lib; int lib_count;
    int booksRead, sheepPenned;
    /* reading */
    int reading_active, reading_book, reading_startTick, reading_untilTick;
    /* weather */
    int rainUntilTick, fogUntilTick, windUntilTick, nextWeatherTick;
    /* cooldowns (init -100000) */
    int lastReadTick, lastShameTick, lastBreatherTick, lastBookPassTick,
        lastRantTick, lastGazeTick, lastMishapTick, lastScarecrowTick, lastJailbreakTick;
    int crookBroken, hadLunch;
    int has_lunchTick, lunchTick, has_lunchStolenTick, lunchStolenTick;
    int has_jailbreakPlan, jailbreakPlan_tick, jailbreakPlan_sheep;
    int streak, jailbreaks;
    /* rival */
    int has_rival; double rival_x, rival_y, rival_dx; int rival_ticksLeft;
    int has_rival_boltTick, rival_boltTick, has_rival_pauseUntil, rival_pauseUntil;
    int rivalLastTick, rivalsSeen;
    int has_drankTick, drankTick, dogHelped, dogHelpedTick;
    /* readingList as book indices (for reread) */
    int *readList; int readList_count;
    /* speech-facing state (no effect on the event stream) */
    int st_flees, st_absurds, st_shames, st_rains, st_breathers, st_books, st_mishaps;
    const char *knownPacks[32]; int knownPacks_n;
    struct { const char *reg; int untilTick; } registers[16]; int registers_n;
    const char *season;
    int finished, finishedTick, eventCount;
    /* event log (unbounded; the web reads its 16-ring every tick, same seq order) */
    HerderEvent *events; int event_count, event_cap;
} HerderWorld;

/* Build the world (map, flock, libraries, initial fields). */
void herder_world_init(HerderWorld *w, const HerderMap *map, const char *seed);
void herder_world_free(HerderWorld *w);
/* Advance one tick. */
void herder_step(HerderWorld *w);

#endif
