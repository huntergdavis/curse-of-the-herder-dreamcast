#include "core/sim/world.h"
#include "core/sim/book.h"
#include "data/lang_data.h"
#include "core/map/terrain.h"
#include "core/map/path.h"
#include "core/progression.h"
#include "core/rng.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TICK_SECONDS 0.25
#define TICKS_PER_HOUR 14400
#define HERDER_BASE_SPEED 1.1
#define CARRY_FACTOR 0.8
#define FLEE_RADIUS 2.5
#define MAX_FLEES 3
#define SEE_RADIUS 12
#define LIBRARY_DETOUR 12
#define LIBRARY_PATH_DETOUR 9
#define GOVERNOR_PERIOD 2400
#define DAY_TICKS (9*3600*4)
#define READ_TICKS_MIN 240
#define READ_TICKS_MAX 480
#define REGISTER_TICKS 2400
#define READ_COOLDOWN (9*60*4)
#define SHAME_RADIUS 4
#define SHAME_COOLDOWN (15*60*4)
#define BREATHER_TICKS 80
#define BREATHER_COOLDOWN (25*60*4)
#define RANT_TICKS 12
#define RANT_COOLDOWN (11*60*4)
#define GAZE_TICKS 10
#define GAZE_COOLDOWN (9*60*4)
#define MISHAP_COOLDOWN (6*60*4)

typedef struct { const char *kind; const int *terrain; int nterr; double chance; int anger; int ticks; } Mishap;
static const int MT_MUD[1] = {T_Mud};
static const int MT_FOREST[1] = {T_Forest};
static const int MT_ROCK[1] = {T_Rock};
static const int MT_GMF[3] = {T_Grass, T_Meadow, T_Farm};
static const int MT_GM[2] = {T_Grass, T_Meadow};
static const Mishap MISHAPS[7] = {
    {"bog", MT_MUD, 1, 0.05, 14, 14},
    {"nettles", MT_FOREST, 1, 0.035, 12, 8},
    {"stub", MT_ROCK, 1, 0.04, 12, 8},
    {"cowpat", MT_GMF, 3, 0.004, 10, 6},
    {"molehill", MT_GM, 2, 0.003, 9, 6},
    {"wasp", NULL, 0, 0.0012, 16, 10},
    {"dogUnderfoot", NULL, 0, 0.0004, 11, 8},
};

/* ---- keyedUnit helpers ---- */
static double kus(const char *seed, const char *d) { return herder_keyed_unit(seed, d, NULL, 0); }
static double kusi(const char *seed, const char *d, int a) { int32_t k[1]={a}; return herder_keyed_unit(seed, d, k, 1); }
static double kusii(const char *seed, const char *d, int a, int b) { int32_t k[2]={a,b}; return herder_keyed_unit(seed, d, k, 2); }
static double kusiii(const char *seed, const char *d, int a, int b, int c) { int32_t k[3]={a,b,c}; return herder_keyed_unit(seed, d, k, 3); }
static double ku_mishap(const char *seed, const char *kind, int tick) {
    char buf[64]; snprintf(buf, sizeof(buf), "%s|mishap|%s|%d", seed, kind, tick);
    return herder_keyed_unit_joined(buf);
}

/* ---- events ---- */
static void push_event(HerderWorld *w, int tick, const char *kind, int sheepId, const char *detail, const char *book) {
    if (w->event_count == w->event_cap) { w->event_cap *= 2; w->events = realloc(w->events, (size_t)w->event_cap * sizeof(HerderEvent)); }
    HerderEvent *e = &w->events[w->event_count++];
    e->seq = w->eventCount++; e->tick = tick; e->sheep_id = sheepId;
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    snprintf(e->detail, sizeof(e->detail), "%s", detail ? detail : "");
    snprintf(e->book, sizeof(e->book), "%s", book ? book : "");
}
static void add_frustration(HerderWorld *w, double amount) { w->frustration = herder_clamp_frustration(w->frustration + amount); }

/* ---- geometry / map ---- */
static int mN(const HerderWorld *w) { return w->map->size; }
static double H_hypot(double a, double b) { return hypot(a, b); }
static int is_walk(const HerderWorld *w, int i) { return herder_is_walkable(w->map->terrain[i]); }
static int tile_at(const HerderWorld *w, double x, double y) {
    int tx = (int)floor(x + 0.5), ty = (int)floor(y + 0.5);
    int n = mN(w);
    if (tx < 0 || ty < 0 || tx >= n || ty >= n) return T_Water;
    return w->map->terrain[ty * n + tx];
}
static double fence_cost(void *vw, int x, int y, double base) {
    HerderWorld *w = vw; return w->map->deco[y * mN(w) + x] == D_Fence ? INFINITY : base;
}

/* ---- mishaps ---- */
static void start_mishap(HerderWorld *w, const char *kind, int anger, int ticks, int sheepId) {
    HerderHerder *h = &w->h;
    w->lastMishapTick = w->tick;
    add_frustration(w, anger);
    if (h->mode != HM_MISHAP) h->rantReturnMode = h->mode;
    h->mode = HM_MISHAP;
    snprintf(h->mishap, sizeof(h->mishap), "%s", kind);
    h->restUntilTick = w->tick + ticks;
    push_event(w, w->tick, "mishap", sheepId, kind, NULL);
}

/* ---- pathing ---- */
static int plan_path(HerderWorld *w, int tx, int ty) {
    HerderHerder *h = &w->h;
    int cnt = herder_find_path(&(HerderGrid){mN(w), w->map->terrain}, (int)floor(h->x + 0.5), (int)floor(h->y + 0.5), tx, ty, fence_cost, w, h->path);
    if (cnt < 0) return 0;
    h->path_len = cnt; h->path_head = 0;
    return 1;
}
static int path_remaining(const HerderHerder *h) { return h->path_len - h->path_head; }

static int plan_to_sheep(HerderWorld *w, HerderSheep *s) {
    int tx = (int)floor(s->tx + 0.5), ty = (int)floor(s->ty + 0.5);
    int n = mN(w);
    if (!s->on_roof && !s->in_river && !s->on_boulder) return plan_path(w, tx, ty);
    static const int off[8][2] = {{0,1},{1,0},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
    for (int q = 0; q < 8; q++) {
        int dx = off[q][0], dy = off[q][1];
        int i = (ty + dy) * n + (tx + dx);
        if (w->map->deco[i] == D_House || w->map->deco[i] == D_HouseRed || w->map->deco[i] == D_Fence) continue;
        if (!is_walk(w, i)) continue;
        if (plan_path(w, tx + dx, ty + dy)) return 1;
    }
    return 0;
}

static HerderSheep *choose_target(HerderWorld *w) {
    HerderSheep *best = NULL; double bestScore = INFINITY;
    HerderHerder *h = &w->h; int n = mN(w);
    int atPen = H_hypot(h->x - w->map->pen_x, h->y - w->map->pen_y) < 5;
    for (int q = 0; q < w->sheep_count; q++) {
        HerderSheep *s = &w->sheep[q];
        if (s->mode != 0) continue;
        double byPath = w->map->pen_distance[(int)floor(s->y + 0.5) * n + (int)floor(s->x + 0.5)];
        double score = (atPen && isfinite(byPath)) ? byPath : H_hypot(s->x - h->x, s->y - h->y) * (1 + s->ring * 0.05);
        if (!isfinite(score)) score = 1e9;
        if (score < bestScore) { bestScore = score; best = s; }
    }
    return best;
}

/* ---- walk ---- */
static double walk(HerderWorld *w, double budget) {
    HerderHerder *h = &w->h; double moved = 0;
    while (budget > 0 && path_remaining(h) > 0) {
        int nx = h->path[h->path_head * 2], ny = h->path[h->path_head * 2 + 1];
        double dx = nx - h->x, dy = ny - h->y;
        double dist = H_hypot(dx, dy);
        if (dist < 1e-6) { h->path_head++; continue; }
        double step = dist < budget ? dist : budget;
        h->x += (dx / dist) * step; h->y += (dy / dist) * step;
        budget -= step; moved += step;
        if (fabs(dx) > 1e-6) h->facing = dx > 0 ? 0 : 2;
        if (step >= dist - 1e-6) { h->x = nx; h->y = ny; h->path_head++; }
    }
    return moved;
}

static void flee_to(HerderWorld *w, HerderSheep *s) {
    HerderHerder *h = &w->h; int n = mN(w);
    double ax = s->x - h->x, ay = s->y - h->y;
    for (int attempt = 0; attempt < 24; attempt++) {
        double spread = (kusiii(w->seed, "flee-dir", s->id, s->flees, attempt) - 0.5) * M_PI * 0.9;
        double r = 9 - floor(attempt / 6.0) * 1.5;
        double ang = atan2(ay, ax) + spread;
        int x = (int)floor(s->x + cos(ang) * r + 0.5);
        int y = (int)floor(s->y + sin(ang) * r + 0.5);
        if (x < 1 || y < 1 || x >= n - 1 || y >= n - 1) continue;
        int i = y * n + x;
        if (!is_walk(w, i) || !isfinite(w->map->pen_distance[i])) continue;
        int d = w->map->deco[i];
        if (d == D_Fence || d == D_PenGround || d == D_House || d == D_HouseRed) continue;
        s->tx = x; s->ty = y; s->speed = 3.2; return;
    }
}

static void step_sheep(HerderWorld *w) {
    HerderHerder *h = &w->h; int n = mN(w);
    for (int q = 0; q < w->sheep_count; q++) {
        HerderSheep *s = &w->sheep[q];
        if (s->mode != 0) continue;
        double d = H_hypot(s->x - h->x, s->y - h->y);
        if (!s->seen && d < SEE_RADIUS) s->seen = 1;
        double dx = s->tx - s->x, dy = s->ty - s->y;
        double dist = H_hypot(dx, dy);
        if (dist > 1e-3) {
            double sp = (s->speed != 0 ? s->speed : 0.6) * TICK_SECONDS;
            double stepLen = dist < sp ? dist : sp;
            s->x += (dx / dist) * stepLen; s->y += (dy / dist) * stepLen;
            if (stepLen >= dist - 1e-6) { s->x = s->tx; s->y = s->ty; s->speed = 0; }
            continue;
        }
        if (s->absurd) continue;
        if (s->temper == 3) continue; /* dozy */
        if (s->temper == 4 && d < 8 && d > 1.2 && w->tick % 12 == s->id % 12) {
            int nx = (int)floor(s->x + 0.5) + (h->x > s->x ? 1 : h->x < s->x ? -1 : 0);
            int ny = (int)floor(s->y + 0.5) + (h->y > s->y ? 1 : h->y < s->y ? -1 : 0);
            int i = ny * n + nx;
            if (nx > 0 && ny > 0 && nx < n - 1 && ny < n - 1 && is_walk(w, i) && w->map->deco[i] != D_Fence && w->map->deco[i] != D_House && w->map->deco[i] != D_HouseRed) {
                s->tx = nx; s->ty = ny; s->speed = 0.9;
            }
            if (!s->greeted && d < 3) { s->greeted = 1; push_event(w, w->tick, "curious", s->id, NULL, NULL); }
            continue;
        }
        if (h->mode == HM_READING && d < 9 && d > 2.5 && w->tick % 8 == s->id % 8) {
            int rx = (int)floor(s->x + 0.5), ry = (int)floor(s->y + 0.5);
            int hx = (int)floor(h->x + 0.5), hy = (int)floor(h->y + 0.5);
            int nx = rx + (hx > rx ? 1 : hx < rx ? -1 : 0);
            int ny = ry + (hy > ry ? 1 : hy < ry ? -1 : 0);
            int i = ny * n + nx;
            if (nx > 0 && ny > 0 && nx < n - 1 && ny < n - 1 && is_walk(w, i) && w->map->deco[i] != D_Fence && w->map->deco[i] != D_House && w->map->deco[i] != D_HouseRed && w->map->deco[i] != D_Library) {
                s->tx = nx; s->ty = ny; s->speed = 0.45;
            }
        }
        if (w->tick % 40 == s->id % 40 && kusii(w->seed, "wander", s->id, w->tick) < 0.5 && d > 3) {
            int dir = (int)floor(kusii(w->seed, "wander-dir", s->id, w->tick) * 4);
            static const int WDX[4] = {1,-1,0,0}, WDY[4] = {0,0,1,-1};
            int nx = (int)floor(s->x + 0.5) + WDX[dir], ny = (int)floor(s->y + 0.5) + WDY[dir];
            if (fabs(nx - s->home_x) <= 2 && fabs(ny - s->home_y) <= 2 && nx > 0 && ny > 0 && nx < n - 1 && ny < n - 1) {
                int i = ny * n + nx;
                if (is_walk(w, i) && w->map->deco[i] != D_Fence && w->map->deco[i] != D_House && w->map->deco[i] != D_HouseRed && w->map->deco[i] != D_Library) {
                    s->tx = nx; s->ty = ny; s->speed = 0.6;
                }
            }
        }
    }
}

static int nearby_library(HerderWorld *w) {
    HerderHerder *h = &w->h; int best = -1; double bestD = LIBRARY_DETOUR;
    for (int i = 0; i < w->lib_count; i++) {
        if (w->lib[i].taken) continue;
        double d = H_hypot(w->lib[i].x - h->x, w->lib[i].y - h->y);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}
static int library_along_path(HerderWorld *w) {
    HerderHerder *h = &w->h;
    for (int i = 0; i < w->lib_count; i++) {
        if (w->lib[i].taken) continue;
        for (int k = 0; k < path_remaining(h); k += 4) {
            int px = h->path[(h->path_head + k) * 2], py = h->path[(h->path_head + k) * 2 + 1];
            if (abs(px - w->lib[i].x) <= LIBRARY_PATH_DETOUR && abs(py - w->lib[i].y) <= LIBRARY_PATH_DETOUR) return i;
        }
    }
    return -1;
}
static int go_to_library(HerderWorld *w, int lib) {
    HerderHerder *h = &w->h;
    static const int off[4][2] = {{0,1},{1,0},{-1,0},{0,-1}};
    for (int q = 0; q < 4; q++) {
        if (plan_path(w, w->lib[lib].x + off[q][0], w->lib[lib].y + off[q][1])) { h->targetLibrary = lib; h->mode = HM_TOLIBRARY; return 1; }
    }
    w->lib[lib].taken = 1;
    return 0;
}

static void govern(HerderWorld *w) {
    if (w->tick % GOVERNOR_PERIOD != 0 || w->totalWork <= 0) return;
    int done = 0;
    for (int q = 0; q < w->sheep_count; q++) if (w->sheep[q].mode == 2) done += w->sheep[q].home_x >= 0 ? 1 : 0;
    double workFrac = w->sheepPenned == 0 ? 0 : (double)done / w->sheep_count;
    double timeFrac = (double)w->tick / DAY_TICKS;
    double ahead = workFrac - timeFrac;
    double target = ahead > 0.01 ? fmax(0.4, 1 - ahead * 8.0) : 1;
    w->speedScale += (target - w->speedScale) / 3;
}

/* nextBookId: catalogue order by `when` (stable), notes saved for last, then bonus. */
static int next_book_id(HerderWorld *w) {
    int order[HERDER_BOOK_COUNT];
    for (int i = 0; i < HERDER_BOOK_COUNT; i++) order[i] = i;
    for (int i = 1; i < HERDER_BOOK_COUNT; i++) { /* insertion sort = stable */
        int key = order[i]; double kw = HERDER_BOOK_WHEN[key]; int j = i - 1;
        while (j >= 0 && HERDER_BOOK_WHEN[order[j]] > kw) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    int read[HERDER_BOOK_COUNT]; memset(read, 0, sizeof(read));
    int remainingBoxes = 0;
    for (int i = 0; i < w->lib_count; i++) { if (w->lib[i].taken) read[w->lib[i].book] = 1; else remainingBoxes++; }
    int unread[HERDER_BOOK_COUNT], un = 0;
    for (int i = 0; i < HERDER_BOOK_COUNT; i++) if (!read[order[i]]) unread[un++] = order[i];
    int notesIdx = herder_book_notes_index();
    int hasNotes = 0; for (int i = 0; i < un; i++) if (unread[i] == notesIdx) hasNotes = 1;
    /* pool */
    if (remainingBoxes > 1 && hasNotes && un > 1) {
        for (int i = 0; i < un; i++) if (unread[i] != notesIdx) return unread[i];
    } else if (un > 0) return unread[0];
    /* bonus */
    int bonus[HERDER_BOOK_COUNT], bn = 0;
    for (int i = 0; i < HERDER_BOOK_COUNT; i++) if (HERDER_BOOK_HAS_PACK[order[i]] && order[i] != notesIdx) bonus[bn++] = order[i];
    if (bn > 0) return bonus[(int)floor(kusi(w->seed, "bonus-book", w->tick) * bn)];
    return order[0];
}

static void start_reading(HerderWorld *w, int libIndex) {
    HerderHerder *h = &w->h;
    int book = next_book_id(w);
    w->lib[libIndex].book = book;
    for (int i = 0; i < w->readList_count; i++) if (w->readList[i] == book) { push_event(w, w->tick, "reread", -1, NULL, HERDER_BOOK_ID[book]); break; }
    int len = READ_TICKS_MIN + (int)floor(kusi(w->seed, "read-len", libIndex) * (READ_TICKS_MAX - READ_TICKS_MIN));
    w->reading_active = 1; w->reading_book = book; w->reading_startTick = w->tick; w->reading_untilTick = w->tick + len;
    h->mode = HM_READING; h->path_len = h->path_head = 0;
    push_event(w, w->tick, "bookFound", -1, NULL, HERDER_BOOK_ID[book]);
}
static void finish_reading(HerderWorld *w) {
    HerderHerder *h = &w->h;
    if (w->reading_active) {
        if (h->targetLibrary >= 0) w->lib[h->targetLibrary].taken = 1;
        int book = w->reading_book;
        w->booksRead++;
        w->st_books++;
        w->readList[w->readList_count++] = book;
        add_frustration(w, FR_BOOK);
        /* unlock pack and register from the book catalogue */
        { const char *bid = HERDER_BOOK_ID[book]; const Book *bk = NULL;
          for (int i = 0; i < HERDER_BOOKS_N; i++) if (strcmp(HERDER_BOOKS[i].id, bid) == 0) { bk = &HERDER_BOOKS[i]; break; }
          if (bk && bk->pack) { int have = 0; for (int i = 0; i < w->knownPacks_n; i++) if (strcmp(w->knownPacks[i], bk->pack) == 0) have = 1; if (!have && w->knownPacks_n < 32) w->knownPacks[w->knownPacks_n++] = bk->pack; }
          if (bk && bk->reg) { int o = 0; for (int i = 0; i < w->registers_n; i++) if (strcmp(w->registers[i].reg, bk->reg) != 0) w->registers[o++] = w->registers[i]; w->registers_n = o; if (w->registers_n < 16) { w->registers[w->registers_n].reg = bk->reg; w->registers[w->registers_n].untilTick = w->tick + 2400; w->registers_n++; } }
        }
        push_event(w, w->tick, "book", -1, NULL, HERDER_BOOK_ID[book]);
    }
    w->reading_active = 0; w->lastReadTick = w->tick; h->targetLibrary = -1; h->mode = HM_IDLE;
}
static int reading_allowed(HerderWorld *w) {
    if (w->tick - w->lastReadTick > READ_COOLDOWN) return 1;
    int untaken = 0; for (int i = 0; i < w->lib_count; i++) if (!w->lib[i].taken) untaken++;
    return untaken <= 1;
}

static void step_weather(HerderWorld *w) {
    if (w->rainUntilTick && w->tick >= w->rainUntilTick) { w->rainUntilTick = 0; push_event(w, w->tick, "rainStops", -1, NULL, NULL); }
    if (w->fogUntilTick && w->tick >= w->fogUntilTick) { w->fogUntilTick = 0; push_event(w, w->tick, "fogLifts", -1, NULL, NULL); }
    if (w->windUntilTick && w->tick >= w->windUntilTick) { w->windUntilTick = 0; push_event(w, w->tick, "windDrops", -1, NULL, NULL); }
    if (w->tick >= w->nextWeatherTick) {
        double u = kusi(w->seed, "weather", w->tick);
        if (u < 0.45 && !w->rainUntilTick) {
            w->rainUntilTick = w->tick + 8*60*4 + (int)floor(kusi(w->seed, "rain-len", w->tick) * 14*60*4);
            w->st_rains++; push_event(w, w->tick, "rain", -1, NULL, NULL);
        } else if (u < 0.62 && !w->fogUntilTick && !w->rainUntilTick) {
            w->fogUntilTick = w->tick + 6*60*4 + (int)floor(kusi(w->seed, "fog-len", w->tick) * 10*60*4);
            push_event(w, w->tick, "fog", -1, NULL, NULL);
        } else if (u < 0.8 && !w->windUntilTick && !w->fogUntilTick) {
            w->windUntilTick = w->tick + 5*60*4 + (int)floor(kusi(w->seed, "wind-len", w->tick) * 9*60*4);
            push_event(w, w->tick, "wind", -1, NULL, NULL);
        }
        w->nextWeatherTick = w->tick + 30*60*4 + (int)floor(kusi(w->seed, "weather-gap", w->tick) * 50*60*4);
    }
    if (w->rainUntilTick && w->tick % 4 == 0) add_frustration(w, FR_RAIN_PER_MIN / 60);
    if (w->windUntilTick && w->tick % 4 == 0) add_frustration(w, FR_RAIN_PER_MIN / 90);
}

/* forward */
static void step_herder(HerderWorld *w);
static void step_rival(HerderWorld *w);
static void step_drink(HerderWorld *w);
static void step_dog_helps(HerderWorld *w);
static void step_jailbreak(HerderWorld *w);

void herder_step(HerderWorld *w) {
    w->tick++;
    w->frustration = herder_clamp_frustration(herder_frustration_drift(w->frustration, herder_frustration_baseline((double)w->tick / TICKS_PER_HOUR), TICK_SECONDS / 60));
    if (w->registers_n && w->tick % 40 == 0) { int o = 0; for (int i = 0; i < w->registers_n; i++) if (w->registers[i].untilTick > w->tick) w->registers[o++] = w->registers[i]; w->registers_n = o; }
    step_weather(w);
    step_jailbreak(w);
    step_rival(w);
    step_dog_helps(w);
    step_drink(w);
    govern(w);
    step_sheep(w);
    step_herder(w);
}

static void step_herder(HerderWorld *w) {
    HerderHerder *h = &w->h; int n = mN(w);
    if (h->mode == HM_DONE) return;
    if (h->mode == HM_RESTING) {
        if (w->has_lunchTick && !w->has_lunchStolenTick && w->tick == w->lunchTick + 70 && kus(w->seed, "lunch-thief") < 0.75) {
            /* nearest loose non-absurd sheep (stable by index) */
            int bi = -1; double bd = INFINITY;
            for (int q = 0; q < w->sheep_count; q++) {
                HerderSheep *s = &w->sheep[q];
                if (s->mode != 0 || s->absurd) continue;
                double dd = H_hypot(s->x - h->x, s->y - h->y);
                if (dd < bd) { bd = dd; bi = q; }
            }
            int dx = 0, found = 0;
            int cand[2] = {1, -1};
            for (int c = 0; c < 2; c++) { if (is_walk(w, ((int)floor(h->y+0.5)) * n + ((int)floor(h->x+0.5) + cand[c]))) { dx = cand[c]; found = 1; break; } }
            if (bi >= 0 && found) {
                HerderSheep *thief = &w->sheep[bi];
                thief->x = (int)floor(h->x + 0.5) + dx; thief->y = (int)floor(h->y + 0.5);
                thief->tx = thief->x; thief->ty = thief->y; thief->thief = 1; thief->named = 1;
                w->has_lunchStolenTick = 1; w->lunchStolenTick = w->tick;
                h->restUntilTick = w->tick + 12;
                add_frustration(w, FR_LUNCH_STOLEN);
                push_event(w, w->tick, "lunchStolen", thief->id, NULL, NULL);
            }
        }
        if (w->tick >= h->restUntilTick) h->mode = HM_IDLE;
        return;
    }
    if (h->mode == HM_READING) { if (!w->reading_active || w->tick >= w->reading_untilTick) finish_reading(w); return; }
    if (h->mode == HM_RANTING || h->mode == HM_GAZING || h->mode == HM_MISHAP) {
        if (w->tick >= h->restUntilTick) { h->mode = h->rantReturnMode; h->mishap[0] = 0; }
        return;
    }
    if (h->mode == HM_IDLE) {
        if (!w->hadLunch && w->tick > 3.5 * TICKS_PER_HOUR && h->carrying < 0) {
            w->hadLunch = 1; w->has_lunchTick = 1; w->lunchTick = w->tick;
            h->mode = HM_RESTING; h->restUntilTick = w->tick + 160;
            add_frustration(w, -12); push_event(w, w->tick, "lunch", -1, NULL, NULL); return;
        }
        if (w->frustration >= 60 && w->tick - w->lastBreatherTick > BREATHER_COOLDOWN && kusi(w->seed, "breather", w->tick) < 0.5) {
            w->lastBreatherTick = w->tick; h->mode = HM_RESTING; h->restUntilTick = w->tick + BREATHER_TICKS;
            w->st_breathers++; add_frustration(w, FR_BREATHER); push_event(w, w->tick, "breather", -1, NULL, NULL); return;
        }
        int lib = reading_allowed(w) ? nearby_library(w) : -1;
        if (lib >= 0 && go_to_library(w, lib)) return;
        HerderSheep *target = choose_target(w);
        if (!target) { h->mode = HM_DONE; w->finished = 1; w->finishedTick = w->tick; push_event(w, w->tick, "finished", -1, NULL, NULL); return; }
        if (!plan_to_sheep(w, target)) { target->mode = 2; w->sheepPenned++; return; }
        h->targetSheep = target->id; h->approachCount = 0; h->mode = HM_TOSHEEP;
        int onWay = reading_allowed(w) ? library_along_path(w) : -1;
        if (onWay >= 0 && go_to_library(w, onWay)) return;
    }

    if ((h->mode == HM_TOSHEEP || h->mode == HM_TOPEN) && w->frustration < 28 && w->tick - w->lastGazeTick > GAZE_COOLDOWN && kusi(w->seed, "gaze", w->tick) < 0.0025) {
        w->lastGazeTick = w->tick; h->restUntilTick = w->tick + GAZE_TICKS; h->rantReturnMode = h->mode; h->mode = HM_GAZING;
        push_event(w, w->tick, "gaze", -1, NULL, NULL); return;
    }
    if ((h->mode == HM_TOSHEEP || h->mode == HM_TOPEN) && w->frustration >= 75 && w->tick - w->lastRantTick > RANT_COOLDOWN && kusi(w->seed, "rant", w->tick) < 0.002) {
        w->lastRantTick = w->tick; h->restUntilTick = w->tick + RANT_TICKS; h->rantReturnMode = h->mode; h->mode = HM_RANTING;
        push_event(w, w->tick, "rant", -1, NULL, NULL); return;
    }

    int tile = tile_at(w, h->x, h->y);
    double terrainSpeed = HERDER_TERRAIN_SPEED[tile];
    double rainFactor = w->rainUntilTick > w->tick ? 0.85 : 1;
    double speed = HERDER_BASE_SPEED * (terrainSpeed != 0 ? terrainSpeed : 0.4) * (h->carrying >= 0 ? CARRY_FACTOR : 1) * w->speedScale * rainFactor * (1 + (w->frustration >= 80 ? 0.1 : 0));
    walk(w, speed * TICK_SECONDS);

    int tx = (int)floor(h->x + 0.5), ty = (int)floor(h->y + 0.5);
    if (tx != h->lastTileX || ty != h->lastTileY) {
        h->lastTileX = tx; h->lastTileY = ty; h->tripTiles++;
        if (w->tick - w->lastScarecrowTick > 15*60*4) {
            int hit = 0;
            for (int dy = -4; dy <= 4 && !hit; dy++) for (int dx = -4; dx <= 4; dx++) {
                int i = (ty + dy) * n + (tx + dx);
                if (i >= 0 && i < n*n && w->map->deco[i] == D_Scarecrow) { w->lastScarecrowTick = w->tick; push_event(w, w->tick, "scarecrow", -1, NULL, NULL); hit = 1; break; }
            }
        }
        if (!w->crookBroken && w->tick > 5 * TICKS_PER_HOUR && h->carrying < 0 && w->tick - w->lastMishapTick > MISHAP_COOLDOWN && kusi(w->seed, "crook", w->tick) < 0.0015) {
            w->crookBroken = 1; start_mishap(w, "crook", 22, 14, -1); return;
        }
        if (w->tick - w->lastMishapTick > MISHAP_COOLDOWN) {
            int t = w->map->terrain[ty * n + tx];
            for (int mi = 0; mi < 7; mi++) {
                const Mishap *m = &MISHAPS[mi];
                if (m->terrain) { int ok = 0; for (int z = 0; z < m->nterr; z++) if (m->terrain[z] == t) ok = 1; if (!ok) continue; }
                if (ku_mishap(w->seed, m->kind, w->tick) < m->chance) { start_mishap(w, m->kind, m->anger, m->ticks, -1); return; }
            }
        }
        if (h->carrying < 0 && h->mode == HM_TOSHEEP && h->tripTiles > 30 && w->tick - w->lastShameTick > SHAME_COOLDOWN && H_hypot(tx - w->map->pen_x, ty - w->map->pen_y) <= SHAME_RADIUS && path_remaining(h) > 12) {
            w->lastShameTick = w->tick; w->st_shames++; add_frustration(w, FR_WALK_OF_SHAME); push_event(w, w->tick, "walkOfShame", -1, NULL, NULL);
        }
        if (h->carrying >= 0) {
            h->carryOdometer += 1;
            if (h->carryOdometer >= 25) { h->carryOdometer = 0; add_frustration(w, FR_PER_FORTY_CARRY); }
            int t = w->map->terrain[ty * n + tx];
            if (t == T_Mud || t == T_Rock) add_frustration(w, FR_ROUGH * 0.25);
        }
    }

    if (h->mode == HM_TOLIBRARY) {
        if (h->targetLibrary < 0 || w->lib[h->targetLibrary].taken) { h->mode = HM_IDLE; return; }
        if (path_remaining(h) == 0) {
            if (H_hypot(w->lib[h->targetLibrary].x - h->x, w->lib[h->targetLibrary].y - h->y) <= 1.6) start_reading(w, h->targetLibrary);
            else h->mode = HM_IDLE;
        }
        return;
    }

    if (h->mode == HM_TOSHEEP) {
        if (h->targetSheep < 0 || h->targetSheep >= w->sheep_count || w->sheep[h->targetSheep].mode != 0) { h->mode = HM_IDLE; return; }
        HerderSheep *s = &w->sheep[h->targetSheep];
        double d = H_hypot(s->x - h->x, s->y - h->y);
        if (d < FLEE_RADIUS && h->approachCount == 0) {
            h->approachCount = 1;
            double hourFactor = 1 + (double)w->tick / (4*3600*4) * 0.3;
            double grudgeFactor = s->nemesis ? 1.6 : 1;
            int cap = s->nemesis ? MAX_FLEES + 2 : MAX_FLEES;
            if (s->flees < cap && kusiii(w->seed, "flee", s->id, s->flees, w->tick) < s->skittish * 0.5 * hourFactor * grudgeFactor) {
                s->flees++;
                w->st_flees++;
                if (w->streak >= 5) { char db[16]; snprintf(db, sizeof(db), "%d", w->streak); push_event(w, w->tick, "streakBroken", s->id, db, NULL); }
                int anyNem = 0; for (int q = 0; q < w->sheep_count; q++) if (w->sheep[q].nemesis) anyNem = 1;
                if (s->flees == 3 && !anyNem) { s->nemesis = 1; push_event(w, w->tick, "nemesis", s->id, NULL, NULL); }
                w->streak = 0;
                flee_to(w, s);
                if (s->flees >= 2) { add_frustration(w, FR_REPEAT_ESCAPE); s->named = 1; push_event(w, w->tick, "repeatEscape", s->id, NULL, NULL); }
                else { add_frustration(w, FR_FLEE); push_event(w, w->tick, "flee", s->id, NULL, NULL); }
                h->approachCount = 0;
                if (!plan_to_sheep(w, s)) h->mode = HM_IDLE;
                return;
            }
        }
        if (path_remaining(h) == 0) {
            if (d <= ((s->on_roof || s->in_river || s->on_boulder) ? 1.6 : 0.75)) {
                s->on_roof = s->in_river = s->on_boulder = 0;
                s->mode = 1; h->carrying = s->id; h->carryOdometer = 0;
                if (s->absurd) { w->st_absurds++; add_frustration(w, FR_ABSURD); push_event(w, w->tick, "absurd", s->id, NULL, NULL); }
                else if (s->nemesis) { char db[16]; snprintf(db, sizeof(db), "%d", s->flees); push_event(w, w->tick, "nemesisCaught", s->id, db, NULL); }
                else if (s->thief) push_event(w, w->tick, "thiefCaught", s->id, NULL, NULL);
                else if (s->escapee) push_event(w, w->tick, "recaptured", s->id, NULL, NULL);
                else if (s->black) push_event(w, w->tick, "black", s->id, NULL, NULL);
                else if (s->temper == 3) push_event(w, w->tick, "dozy", s->id, NULL, NULL);
                else push_event(w, w->tick, "caught", s->id, NULL, NULL);
                if (s->temper == 2 && kusi(w->seed, "heave", s->id) < 0.6) {
                    if (plan_path(w, w->map->pen_x, w->map->pen_y)) h->mode = HM_TOPEN; else h->mode = HM_IDLE;
                    start_mishap(w, "heave", 7, 10, s->id); return;
                }
                if (plan_path(w, w->map->pen_x, w->map->pen_y)) h->mode = HM_TOPEN; else h->mode = HM_IDLE;
                if (w->tick - w->lastMishapTick > MISHAP_COOLDOWN && kusi(w->seed, "bite", s->id) < 0.09) start_mishap(w, "bite", 15, 8, s->id);
            } else if (!plan_to_sheep(w, s)) h->mode = HM_IDLE;
        }
    } else if (h->mode == HM_TOPEN) {
        HerderSheep *s = h->carrying >= 0 ? &w->sheep[h->carrying] : NULL;
        if (s) { s->x = h->x; s->y = h->y; s->tx = h->x; s->ty = h->y; }
        if (w->tick % 8 == 0 && w->tick - w->lastBookPassTick > 10*60*4) {
            int lib = nearby_library(w);
            if (lib >= 0 && H_hypot(w->lib[lib].x - h->x, w->lib[lib].y - h->y) < 6) { w->lastBookPassTick = w->tick; push_event(w, w->tick, "bookPassed", -1, NULL, NULL); }
        }
        if (path_remaining(h) == 0 && H_hypot(h->x - w->map->pen_x, h->y - w->map->pen_y) < 0.75) {
            if (w->tick - w->lastMishapTick > MISHAP_COOLDOWN && !h->gateJammed && kusi(w->seed, "gate", w->sheepPenned) < 0.07) {
                h->gateJammed = 1; start_mishap(w, "gate", 12, 12, -1); return;
            }
            h->gateJammed = 0;
            if (s) {
                s->mode = 2;
                int k = w->sheepPenned;
                s->x = w->map->pen_x - 1.2 + (k % 4) * 0.8 + kusi(w->seed, "pen-x", k) * 0.3;
                s->y = w->map->pen_y - 1.1 + ((k / 4) % 4) * 0.7 + kusi(w->seed, "pen-y", k) * 0.3;
                s->tx = s->x; s->ty = s->y;
                w->sheepPenned++;
                push_event(w, w->tick, "penned", s->id, NULL, NULL);
                w->streak = s->absurd ? 0 : w->streak + 1;
                if (w->streak == 5 || w->streak == 8) { char db[16]; snprintf(db, sizeof(db), "%d", w->streak); push_event(w, w->tick, "streak", -1, db, NULL); }
                if (w->sheepPenned == w->sheep_count / 2) push_event(w, w->tick, "milestone", -1, "halfway", NULL);
                else if (w->sheepPenned == w->sheep_count - 10) push_event(w, w->tick, "milestone", -1, "tentogo", NULL);
                else if (w->sheepPenned == w->sheep_count - 1) push_event(w, w->tick, "milestone", -1, "lastone", NULL);
            }
            h->carrying = -1; h->tripTiles = 0;
            add_frustration(w, -fmax(2, fabs(FR_PENNED) * (w->frustration / 40)));
            h->mode = HM_RESTING; h->restUntilTick = w->tick + 8;
        } else if (path_remaining(h) == 0) {
            if (!plan_path(w, w->map->pen_x, w->map->pen_y)) h->mode = HM_IDLE;
        }
    }
}

static void break_out(HerderWorld *w, HerderSheep *s) {
    int gx = w->map->pen_x, gy = w->map->pen_y + 3;
    s->mode = 0; s->x = gx; s->y = gy;
    s->tx = gx + (kusi(w->seed, "jb-dx", w->tick) < 0.5 ? -4 : 4);
    s->ty = gy + 3; s->speed = 2.5;
    s->home_x = (int)floor(s->tx + 0.5); s->home_y = (int)floor(s->ty + 0.5);
    s->flees++; w->st_flees++; s->named = 1; s->escapee = 1;
    w->sheepPenned--; w->jailbreaks++; w->lastJailbreakTick = w->tick;
    add_frustration(w, 18);
    push_event(w, w->tick, "jailbreak", s->id, NULL, NULL);
}

static void step_jailbreak(HerderWorld *w) {
    if (w->finished) return;
    HerderHerder *h = &w->h;
    if (w->has_jailbreakPlan) {
        int sid = w->jailbreakPlan_sheep;
        HerderSheep *s = (sid >= 0 && sid < w->sheep_count) ? &w->sheep[sid] : NULL;
        if (!s || s->mode != 2 || H_hypot(h->x - w->map->pen_x, h->y - w->map->pen_y) < 6) { w->has_jailbreakPlan = 0; return; }
        if (w->tick < w->jailbreakPlan_tick) return;
        w->has_jailbreakPlan = 0; break_out(w, s); return;
    }
    if (w->jailbreaks >= 2 || w->sheepPenned < 12) return;
    if (w->tick - w->lastJailbreakTick < 90*60*4) return;
    if (w->tick < 3 * TICKS_PER_HOUR) return;
    if (kusi(w->seed, "jailbreak", w->tick) > 0.0003) return;
    if (H_hypot(h->x - w->map->pen_x, h->y - w->map->pen_y) < 6) return;
    int pcount = 0; int pidx[128];
    for (int q = 0; q < w->sheep_count; q++) if (w->sheep[q].mode == 2 && !w->sheep[q].black) pidx[pcount++] = q;
    if (pcount == 0) return;
    int who = pidx[(int)floor(kusi(w->seed, "jailbreak-who", w->tick) * pcount)];
    w->has_jailbreakPlan = 1; w->jailbreakPlan_tick = w->tick + 120; w->jailbreakPlan_sheep = w->sheep[who].id;
}

static void step_rival(HerderWorld *w) {
    HerderHerder *h = &w->h;
    if (w->has_rival) {
        if (w->has_rival_pauseUntil && w->tick < w->rival_pauseUntil) return;
        w->rival_x += w->rival_dx; w->rival_ticksLeft--;
        if (w->rivalsSeen == 3 && !w->has_rival_boltTick && w->rival_ticksLeft == 112) {
            w->has_rival_boltTick = 1; w->rival_boltTick = w->tick;
            w->has_rival_pauseUntil = 1; w->rival_pauseUntil = w->tick + 24;
            add_frustration(w, -10); push_event(w, w->tick, "rivalBolt", -1, NULL, NULL);
        }
        if (w->rival_ticksLeft <= 0) w->has_rival = 0;
        return;
    }
    if (h->mode == HM_DONE || h->mode == HM_READING || w->finished) return;
    if (w->tick % 40 != 0 || w->tick < TICKS_PER_HOUR * 0.75) return;
    if (w->tick - w->rivalLastTick < TICKS_PER_HOUR * 1.75) return;
    if (kusi(w->seed, "rival", w->tick) > 0.12) return;
    int y = (int)floor(h->y + 0.5) + 3;
    int cx = (int)floor(h->x + 0.5);
    for (int x = cx - 13; x <= cx + 13; x++) if (!herder_is_walkable(tile_at(w, x, y))) return;
    int ahead = h->facing == 0 ? 1 : h->facing == 2 ? -1 : (kusi(w->seed, "rival-side", w->tick) < 0.5 ? 1 : -1);
    double speed = 0.16;
    w->has_rival = 1; w->rival_x = cx + ahead * 11; w->rival_y = y; w->rival_dx = -ahead * speed;
    w->rival_ticksLeft = (int)ceil(22 / speed);
    w->has_rival_boltTick = 0; w->has_rival_pauseUntil = 0;
    w->rivalLastTick = w->tick; w->rivalsSeen++;
    add_frustration(w, FR_RIVAL);
    { char db[16]; snprintf(db, sizeof(db), "%d", w->rivalsSeen); push_event(w, w->tick, "rival", -1, db, NULL); }
}

static void step_drink(HerderWorld *w) {
    if (w->has_drankTick || w->finished || w->tick < 5 * TICKS_PER_HOUR) return;
    HerderHerder *h = &w->h; int n = mN(w);
    if (h->mode != HM_TOSHEEP || h->carrying >= 0 || w->tick % 4 != 0) return;
    int hx = (int)floor(h->x + 0.5), hy = (int)floor(h->y + 0.5);
    for (int oy = -2; oy <= 2; oy++) for (int ox = -2; ox <= 2; ox++) {
        int x = hx + ox, y = hy + oy;
        if (x < 0 || y < 0 || x >= n || y >= n) continue;
        if (w->map->deco[y * n + x] != D_Well) continue;
        w->has_drankTick = 1; w->drankTick = w->tick;
        h->mode = HM_RESTING; h->restUntilTick = w->tick + 36; h->path_len = h->path_head = 0;
        add_frustration(w, -6); push_event(w, w->tick, "drink", -1, NULL, NULL); return;
    }
}

static void step_dog_helps(HerderWorld *w) {
    if (w->dogHelped || w->finished || w->tick < 5 * TICKS_PER_HOUR) return;
    HerderHerder *h = &w->h;
    if (h->mode != HM_TOSHEEP || h->carrying >= 0) return;
    if (h->targetSheep < 0 || h->targetSheep >= w->sheep_count) return;
    HerderSheep *s = &w->sheep[h->targetSheep];
    if (s->mode != 0 || s->absurd || s->nemesis) return;
    double d = H_hypot(s->x - h->x, s->y - h->y);
    if (d < 4 || d > 9) return;
    if (kusi(w->seed, "dog-helps", w->tick) > 0.01) return;
    w->dogHelped = 1; w->dogHelpedTick = w->tick;
    s->tx = (int)floor(h->x + 0.5) + (s->x < h->x ? -1 : 1); s->ty = (int)floor(h->y + 0.5);
    s->speed = 2.2; s->home_x = s->tx; s->home_y = s->ty;
    add_frustration(w, -8); push_event(w, w->tick, "dogHelps", s->id, NULL, NULL);
}

void herder_world_init(HerderWorld *w, const HerderMap *map, const char *seed) {
    memset(w, 0, sizeof(*w));
    w->seed = seed; w->map = map;
    w->sheep = malloc(80 * sizeof(HerderSheep));
    w->lib = malloc(32 * sizeof(HerderLib));
    w->sheep_count = herder_create_world(map, seed, w->sheep, 80, w->lib, &w->lib_count);
    for (int i = 0; i < w->lib_count; i++) w->lib[i].book = 0; /* placeholder first-words */
    w->tick = 0; w->frustration = 10; w->speedScale = 1;
    w->h.x = map->pen_x; w->h.y = map->pen_y + 3; w->h.facing = 1; w->h.mode = HM_IDLE;
    w->h.path_cap = 8192; w->h.path = malloc((size_t)w->h.path_cap * 2 * sizeof(int));
    w->h.path_len = w->h.path_head = 0;
    w->h.targetSheep = -1; w->h.carrying = -1; w->h.targetLibrary = -1;
    w->h.lastTileX = map->pen_x; w->h.lastTileY = map->pen_y + 3;
    w->h.rantReturnMode = HM_IDLE;
    w->booksRead = 0; w->sheepPenned = 0;
    w->totalWork = 0; for (int i = 0; i < w->sheep_count; i++) w->totalWork += map->pen_distance[(int)w->sheep[i].y * map->size + (int)w->sheep[i].x];
    w->lastReadTick = w->lastShameTick = w->lastBreatherTick = w->lastBookPassTick = -100000;
    w->lastRantTick = w->lastGazeTick = w->lastMishapTick = w->lastScarecrowTick = w->lastJailbreakTick = -100000;
    w->nextWeatherTick = (int)(4 * TICKS_PER_HOUR * 0.6);
    w->rivalLastTick = -100000; w->rivalsSeen = 0;
    w->readList = malloc(64 * sizeof(int)); w->readList_count = 0;
    w->season = "winter"; /* overridable; web derives from wall clock */
    w->finished = 0; w->finishedTick = -1;
    w->event_cap = 1024; w->events = malloc((size_t)w->event_cap * sizeof(HerderEvent)); w->event_count = 0;
    w->eventCount = 0;
    push_event(w, 0, "started", -1, NULL, NULL); /* seq 0, eventCount -> 1 */
}

void herder_world_free(HerderWorld *w) {
    free(w->sheep); free(w->lib); free(w->h.path); free(w->readList); free(w->events);
}
