#include "core/sim/flock.h"
#include "core/map/terrain.h"
#include "core/rng.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct { double min, max; int count; } Ring;
static const Ring DEFAULT_RINGS[5] = {
    {28, 62, 14}, {80, 125, 14}, {150, 205, 12}, {220, 285, 12}, {300, 380, 8},
};

static int placeable(const HerderMap *m, int x, int y) {
    int i = y * m->size + x;
    if (!isfinite(m->pen_distance[i])) return 0;
    int t = m->terrain[i];
    if (t == T_Road || t == T_Bridge) return 0;
    int d = m->deco[i];
    return d == D_None || d == D_Tuft || d == D_Flowers || d == D_Boulder;
}

static int splice_at(int *arr, int *len, int idx) {
    int v = arr[idx];
    for (int i = idx; i < *len - 1; i++) arr[i] = arr[i + 1];
    (*len)--;
    return v;
}

static int flock_core(const HerderMap *map, const char *seed, HerderSheep *out, int max, uint32_t *stp) {
    (void)max; (void)seed;
    int n = map->size;
    uint32_t st = *stp;
    #define RND() herder_unit_from_u32(herder_mulberry32_u32(&st))
    double scale = n / 512.0;
    Ring rings[5];
    for (int r = 0; r < 5; r++) { rings[r].min = DEFAULT_RINGS[r].min * scale; rings[r].max = DEFAULT_RINGS[r].max * scale; rings[r].count = DEFAULT_RINGS[r].count; }

    /* Buckets by ring. */
    int cap = n * n;
    int *bucket[5]; int blen[5] = {0,0,0,0,0};
    for (int r = 0; r < 5; r++) bucket[r] = malloc((size_t)cap * sizeof(int));
    for (int y = 2; y < n - 2; y++) for (int x = 2; x < n - 2; x++) {
        if (!placeable(map, x, y)) continue;
        double d = map->pen_distance[y * n + x];
        for (int r = 0; r < 5; r++) if (d >= rings[r].min && d <= rings[r].max) { bucket[r][blen[r]++] = y * n + x; break; }
    }

    int count = 0;
    uint8_t *taken = calloc((size_t)cap, 1);

    int *roofs = malloc((size_t)cap * sizeof(int)); int rooflen = 0;
    for (int i = 0; i < n * n; i++) if ((map->deco[i] == D_House || map->deco[i] == D_HouseRed) && isfinite(map->pen_distance[i - 1])) roofs[rooflen++] = i;
    int roofCount = rooflen < 3 ? rooflen : 3;
    int *shallows = malloc((size_t)cap * sizeof(int)); int shlen = 0;
    for (int y = 2; y < n - 2; y++) for (int x = 2; x < n - 2; x++) {
        int i = y * n + x;
        if (map->terrain[i] != T_Water) continue;
        int nb[4] = {i - 1, i + 1, i - n, i + n}; int bankfirst = -1, bankcnt = 0;
        for (int q = 0; q < 4; q++) { int j = nb[q]; if (isfinite(map->pen_distance[j]) && map->terrain[j] != T_Water && map->terrain[j] != T_Bridge) { if (bankfirst < 0) bankfirst = j; bankcnt++; } }
        double d = map->pen_distance[bankfirst >= 0 ? bankfirst : i];
        if (bankcnt >= 2 && isfinite(d) && d > 60 * scale && d < 300 * scale) shallows[shlen++] = i;
    }
    int riverCount = shlen < 3 ? shlen : 3;
    int *boulders = malloc((size_t)cap * sizeof(int)); int boulen = 0;
    for (int i = 0; i < n * n; i++) if (map->deco[i] == D_Boulder && isfinite(map->pen_distance[i]) && map->pen_distance[i] > 80 * scale) boulders[boulen++] = i;
    int boulderCount = boulen < 3 ? boulen : 3;

    int cntBoulder = 0, cntRiver = 0, cntRoof = 0;
    for (int r = 0; r < 5; r++) {
        int *bk = bucket[r]; int bkl = blen[r];
        int ringCount = rings[r].count;
        if (r >= 2 && r <= 4 && cntBoulder < boulderCount && boulen > 0) {
            int i = splice_at(boulders, &boulen, (int)floor(RND() * boulen));
            int x = i % n, y = i / n;
            HerderSheep *s = &out[count]; memset(s, 0, sizeof(*s));
            s->id = count; s->x = x; s->y = y; s->home_x = x; s->home_y = y; s->tx = x; s->ty = y;
            s->absurd = 1; s->ring = r; s->on_boulder = 1; s->temper = 0; s->skittish = 0;
            count++; cntBoulder++; ringCount -= 1;
        }
        if (r >= 1 && r <= 3 && cntRiver < riverCount && shlen > 0) {
            int i = splice_at(shallows, &shlen, (int)floor(RND() * shlen));
            int x = i % n, y = i / n;
            HerderSheep *s = &out[count]; memset(s, 0, sizeof(*s));
            s->id = count; s->x = x; s->y = y; s->home_x = x; s->home_y = y; s->tx = x; s->ty = y;
            s->absurd = 1; s->ring = r; s->in_river = 1; s->temper = -1;
            count++; cntRiver++; ringCount -= 1;
        }
        if (r >= 1 && r <= 3 && cntRoof < roofCount && rooflen > 0) {
            int i = splice_at(roofs, &rooflen, (int)floor(RND() * rooflen));
            int x = i % n, y = i / n;
            HerderSheep *s = &out[count]; memset(s, 0, sizeof(*s));
            s->id = count; s->x = x; s->y = y; s->home_x = x; s->home_y = y; s->tx = x; s->ty = y;
            s->absurd = 1; s->ring = r; s->on_roof = 1; s->temper = -1;
            count++; cntRoof++; ringCount -= 1;
        }
        for (int k = 1; bkl == 0 && k < 5; k++) { int idx = r - k >= 0 ? r - k : (r + k < 5 ? r + k : r); bk = bucket[idx >= 0 ? idx : 0]; bkl = blen[idx >= 0 ? idx : 0]; }
        for (int c = 0; c < ringCount && bkl > 0; c++) {
            int i = bk[(int)floor(RND() * bkl)];
            int guard = 0;
            while (taken[i] && guard++ < 20) i = bk[(int)floor(RND() * bkl)];
            taken[i] = 1;
            int x = i % n, y = i / n;
            int t = map->terrain[i];
            int nearWater = 0;
            int off[4] = {1, -1, n, -n};
            for (int q = 0; q < 4; q++) { int j = i + off[q]; if (j >= 0 && j < n * n && map->terrain[j] == T_Water) { nearWater = 1; break; } }
            double tr = RND();
            int temper = tr < 0.4 ? 0 : tr < 0.6 ? 1 : tr < 0.75 ? 2 : tr < 0.9 ? 3 : 4;
            HerderSheep *s = &out[count]; memset(s, 0, sizeof(*s));
            s->id = count; s->x = x; s->y = y; s->home_x = x; s->home_y = y; s->tx = x; s->ty = y;
            s->ring = r; s->temper = temper;
            s->skittish = temper == 1 ? 0.6 + RND() * 0.35 : (temper == 3 || temper == 4) ? 0.05 : 0.15 + RND() * 0.4;
            s->absurd = (t == T_Rock || (nearWater && RND() < 0.5)) ? 1 : 0;
            count++;
        }
    }

    /* The black sheep: one of the non-absurd ones. */
    int plainIdx[128]; int plainCount = 0;
    for (int i = 0; i < count; i++) if (!out[i].absurd) plainIdx[plainCount++] = i;
    if (plainCount > 0) { int pick = plainIdx[(int)floor(RND() * plainCount)]; out[pick].black = 1; }

    for (int r = 0; r < 5; r++) free(bucket[r]);
    free(taken); free(roofs); free(shallows); free(boulders);
    *stp = st;
    return count;
}

int herder_create_flock(const HerderMap *map, const char *seed, HerderSheep *out, int max) {
    char fseed[128]; snprintf(fseed, sizeof(fseed), "%s|flock", seed);
    uint32_t st = herder_fnv1a(fseed);
    return flock_core(map, seed, out, max, &st);
}

/* isPlaceable (generate.ts): reachable plain ground, not built on. */
static int lib_placeable(const HerderMap *m, int x, int y) {
    int i = y * m->size + x;
    if (!isfinite(m->pen_distance[i])) return 0;
    int t = m->terrain[i];
    if (t == T_Road || t == T_Bridge) return 0;
    int d = m->deco[i];
    return d == D_None || d == D_Tuft || d == D_Flowers || d == D_Boulder;
}

#define LIBRARY_COUNT 24

int herder_place_libraries(const HerderMap *map, const HerderSheep *sheep, int sheepN,
                           uint32_t *stp, HerderLib *out) {
    int n = map->size;
    uint32_t st = *stp;
    /* byDistance: stable sort of sheep indices by pen distance (ties keep order). */
    int order[128]; for (int i = 0; i < sheepN; i++) order[i] = i;
    for (int i = 1; i < sheepN; i++) { /* insertion sort = stable */
        int key = order[i]; double kd = map->pen_distance[(int)sheep[key].y * n + (int)sheep[key].x];
        int j = i - 1;
        while (j >= 0) { double jd = map->pen_distance[(int)sheep[order[j]].y * n + (int)sheep[order[j]].x]; if (jd <= kd) break; order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    int count = LIBRARY_COUNT < sheepN ? LIBRARY_COUNT : sheepN;
    uint8_t *used = calloc((size_t)n * n, 1);
    int outn = 0;
    for (int k = 0; k < count; k++) {
        double frac = pow((double)k / (double)(sheepN > count ? 1 : (count - 1 < 1 ? 1 : count - 1)), 0.75);
        /* anchor = byDistance[round(pow(k/max(1,count-1),0.75)*(len-1))] */
        double denom = (count - 1) < 1 ? 1 : (count - 1);
        int anchorPos = (int)floor(pow((double)k / denom, 0.75) * (sheepN - 1) + 0.5);
        const HerderSheep *anchor = &sheep[order[anchorPos]];
        (void)frac;
        for (int tries = 0; tries < 60; tries++) {
            double r = 3 + (herder_unit_from_u32(herder_mulberry32_u32(&st))) * 4;
            double a = (herder_unit_from_u32(herder_mulberry32_u32(&st))) * M_PI * 2;
            int x = (int)floor(anchor->x + cos(a) * r + 0.5);
            int y = (int)floor(anchor->y + sin(a) * r + 0.5);
            if (x < 2 || y < 2 || x >= n - 2 || y >= n - 2) continue;
            int i = y * n + x;
            if (used[i] || !lib_placeable(map, x, y)) continue;
            int onSheep = 0; for (int q = 0; q < sheepN; q++) if (sheep[q].x == x && sheep[q].y == y) { onSheep = 1; break; }
            if (onSheep) continue;
            int tooClose = 0; for (int q = 0; q < outn; q++) if (hypot((double)out[q].x - x, (double)out[q].y - y) < 10) { tooClose = 1; break; }
            if (tooClose) continue;
            used[i] = 1; out[outn].x = x; out[outn].y = y; out[outn].taken = 0; outn++;
            break;
        }
    }
    /* sort libraries by pen distance (stable). */
    for (int i = 1; i < outn; i++) {
        HerderLib key = out[i]; double kd = map->pen_distance[key.y * n + key.x];
        int j = i - 1;
        while (j >= 0) { double jd = map->pen_distance[out[j].y * n + out[j].x]; if (jd <= kd) break; out[j+1] = out[j]; j--; }
        out[j+1] = key;
    }
    free(used);
    *stp = st;
    return outn;
}

int herder_create_world(const HerderMap *map, const char *seed,
                        HerderSheep *sheep_out, int sheep_max,
                        HerderLib *lib_out, int *lib_count) {
    char fseed[128]; snprintf(fseed, sizeof(fseed), "%s|flock", seed);
    uint32_t st = herder_fnv1a(fseed);
    int sc = flock_core(map, seed, sheep_out, sheep_max, &st);
    *lib_count = herder_place_libraries(map, sheep_out, sc, &st, lib_out);
    return sc;
}
