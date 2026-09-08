#include "core/map/generate.h"
#include "core/map/terrain.h"
#include "core/map/path.h"
#include "core/noise.h"
#include "core/rng.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* JS Math.round is round-half-up (toward +Inf); floor(x+0.5) matches it. */
static double jsround(double x) { return floor(x + 0.5); }

static uint32_t hash_seed(const char *s) { return herder_fnv1a(s); }
static double rnd(uint32_t *st) { return herder_unit_from_u32(herder_mulberry32_u32(st)); }

/* Road/fence cost hook for A* (roadBias in generate.ts). */
typedef struct { int n; const uint8_t *terrain; const uint8_t *deco; } RoadCtx;
static double road_bias(void *vctx, int x, int y, double base) {
    RoadCtx *c = (RoadCtx *)vctx;
    int i = y * c->n + x;
    int dd = c->deco[i];
    if (dd == D_Fence || dd == D_PenGround) return INFINITY;
    int t = c->terrain[i];
    if (t == T_Road || t == T_Bridge) return 0.25;
    if (t == T_Water) return 9.0;
    return base;
}

static void find_pen_site(const uint8_t *terrain, int n, int *px, int *py) {
    int c = n / 2;
    for (int r = 0; r < n / 2; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if ((abs(dx) > abs(dy) ? abs(dx) : abs(dy)) != r) continue;
                int x = c + dx, y = c + dy;
                if (x < 6 || y < 6 || x >= n - 6 || y >= n - 6) continue;
                int ok = 1;
                for (int yy = -3; yy <= 3 && ok; yy++)
                    for (int xx = -3; xx <= 3; xx++) {
                        int t = terrain[(y + yy) * n + (x + xx)];
                        if (t != T_Grass && t != T_Meadow) { ok = 0; break; }
                    }
                if (ok) { *px = x; *py = y; return; }
            }
        }
    }
    *px = c; *py = c;
}

static const int RDX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
static const int RDY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

void herder_generate_map(const char *seed, int size, HerderMap *out) {
    int n = size;
    int villageCount = 5;
    int riverCount = (int)jsround((double)size / 85.0);
    uint8_t *terrain = calloc((size_t)n * n, 1);
    uint8_t *deco = calloc((size_t)n * n, 1);
    float *elev = malloc((size_t)n * n * sizeof(float));
    uint32_t s = hash_seed(seed);
    #define IDX(X, Y) ((Y) * n + (X))

    double cx = (n - 1) / 2.0, cy = (n - 1) / 2.0;
    double radius = n * 0.52;
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
        double base = herder_fbm_full(s, x, y, n * 0.27, 6, 0.5, 2.0);
        double ridge = herder_fbm_full((uint32_t)(s + 7), x, y, n * 0.09, 4, 0.55, 2.0);
        double dx = (x - cx) / radius, dy = (y - cy) / radius;
        double falloff = fmax(0.0, dx * dx + dy * dy - 0.35) * 0.9;
        double e = base * 0.72 + ridge * 0.28 - falloff;
        double center = exp(-(dx * dx + dy * dy) * 6.0);
        e = e * (1 - center * 0.5) + 0.52 * center * 0.5;
        elev[IDX(x, y)] = (float)e;
    }
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
        double e = (double)elev[IDX(x, y)];
        double m = herder_fbm_full((uint32_t)(s + 99), x, y, n * 0.18, 5, 0.5, 2.0);
        terrain[IDX(x, y)] = (uint8_t)herder_classify(e, m);
    }

    /* Rivers. */
    uint32_t rst = s ^ 0x5eedu;
    int carved = 0;
    int *path = malloc((size_t)(n * 2 + 4) * sizeof(int));
    for (int attempt = 0; attempt < riverCount * 30 && carved < riverCount; attempt++) {
        int x = (int)floor(rnd(&rst) * n);
        int y = (int)floor(rnd(&rst) * n);
        if ((double)elev[IDX(x, y)] < 0.68) continue;
        int plen = 0;
        for (int step = 0; step < n * 2; step++) {
            path[plen++] = IDX(x, y);
            if (terrain[IDX(x, y)] == T_Water) break;
            int bx = x, by = y;
            double be = (double)elev[IDX(x, y)] + 0.004;
            for (int k = 0; k < 8; k++) {
                int nx = x + RDX[k], ny = y + RDY[k];
                if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
                double ne = (double)elev[IDX(nx, ny)] + (rnd(&rst) - 0.5) * 0.01;
                if (ne < be) { be = ne; bx = nx; by = ny; }
            }
            if (bx == x && by == y) break;
            x = bx; y = by;
        }
        if (plen < 25) continue;
        for (int p = 0; p < plen; p++) if (terrain[path[p]] != T_Snow) terrain[path[p]] = T_Water;
        carved++;
    }
    free(path);

    /* The pen. */
    int penx, peny;
    find_pen_site(terrain, n, &penx, &peny);
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        terrain[IDX(penx + dx, peny + dy)] = T_Grass;
        deco[IDX(penx + dx, peny + dy)] = D_PenGround;
    }
    for (int d = -2; d <= 2; d++) {
        int coords[4][2] = { {penx + d, peny - 2}, {penx + d, peny + 2}, {penx - 2, peny + d}, {penx + 2, peny + d} };
        for (int q = 0; q < 4; q++) {
            int x = coords[q][0], y = coords[q][1];
            terrain[IDX(x, y)] = T_Grass;
            deco[IDX(x, y)] = (x == penx && y == peny + 2) ? D_None : D_Fence;
        }
    }

    HerderGrid grid = { n, terrain };
    double *penDistance = herder_distance_field(&grid, penx, peny, NULL, NULL);

    /* Villages. */
    static const double villageRings[8] = {0.16, 0.24, 0.32, 0.4, 0.46, 0.5, 0.55, 0.6};
    int namePool[15]; for (int i = 0; i < 15; i++) namePool[i] = i;
    int namePoolLen = 15;
    HerderVillage villages[16]; int vcount = 0;
    for (int v = 0; v < villageCount; v++) {
        double want = villageRings[v % 8] * n;
        int haveBest = 0; int bx = 0, by = 0; double bscore = 0;
        for (int tries = 0; tries < 400; tries++) {
            double ang = rnd(&rst) * M_PI * 2;
            double r = want * (0.85 + rnd(&rst) * 0.3);
            int x = (int)jsround(cx + cos(ang) * r);
            int y = (int)jsround(cy + sin(ang) * r);
            if (x < 6 || y < 6 || x >= n - 6 || y >= n - 6) continue;
            if (!isfinite(penDistance[IDX(x, y)])) continue;
            int flat = 0;
            for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
                int t = terrain[IDX(x + dx, y + dy)];
                if (t == T_Grass || t == T_Meadow) flat++;
            }
            double spacing = INFINITY;
            for (int o = 0; o < vcount; o++) { double sp = hypot((double)villages[o].x - x, (double)villages[o].y - y); if (sp < spacing) spacing = sp; }
            if (spacing < n * 0.12) continue;
            double score = flat + fmin(spacing, n * 0.3) / n;
            if (!haveBest || score > bscore) { haveBest = 1; bx = x; by = y; bscore = score; }
        }
        if (!haveBest) continue;
        int nameSlot = (int)floor(rnd(&rst) * namePoolLen);
        int name = namePool[nameSlot];
        for (int i = nameSlot; i < namePoolLen - 1; i++) namePool[i] = namePool[i + 1];
        namePoolLen--;
        villages[vcount].x = bx; villages[vcount].y = by; villages[vcount].name = name; vcount++;
        deco[IDX(bx, by)] = D_Well; terrain[IDX(bx, by)] = T_Grass;
        int spots[8][2] = { {-2,-1},{2,-1},{-2,1},{2,1},{0,-2},{0,2},{-1,-3},{3,0} };
        for (int q = 0; q < 8; q++) {
            if (rnd(&rst) < 0.8) {
                int i = IDX(bx + spots[q][0], by + spots[q][1]);
                if (herder_is_walkable(terrain[i])) {
                    terrain[i] = T_Grass;
                    deco[i] = rnd(&rst) < 0.5 ? D_House : D_HouseRed;
                }
            }
        }
        for (int f = 0; f < 2; f++) {
            int fx = bx + (rnd(&rst) < 0.5 ? -9 : 5);
            int fy = by + (rnd(&rst) < 0.5 ? -7 : 4);
            int fw = 4 + (int)floor(rnd(&rst) * 3);
            int fh = 3 + (int)floor(rnd(&rst) * 3);
            for (int y = fy; y < fy + fh; y++) for (int x = fx; x < fx + fw; x++) {
                if (x < 0 || y < 0 || x >= n || y >= n) continue;
                int i = IDX(x, y); int t = terrain[i];
                if (t == T_Grass || t == T_Meadow || t == T_Forest) { terrain[i] = T_Farm; deco[i] = D_None; }
            }
        }
    }

    /* Roads. */
    RoadCtx rc = { n, terrain, deco };
    int *pxy = malloc((size_t)n * n * 2 * sizeof(int));
    for (int v = 0; v < vcount; v++) {
        int cnt = herder_find_path(&grid, penx, peny + 3, villages[v].x, villages[v].y + 1, road_bias, &rc, pxy);
        if (cnt < 0) continue;
        for (int p = 0; p < cnt; p++) {
            int i = IDX(pxy[p*2], pxy[p*2+1]);
            if (deco[i] == D_PenGround || deco[i] == D_Fence || deco[i] == D_Well) continue;
            terrain[i] = terrain[i] == T_Water ? T_Bridge : T_Road;
            deco[i] = D_None;
        }
    }
    free(pxy);
    for (int v = 0; v < vcount; v++) {
        int i = IDX(villages[v].x - 1, villages[v].y + 1);
        if (deco[i] == D_None && terrain[i] != T_Water) deco[i] = D_Signpost;
    }

    /* Decorations. */
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
        int i = IDX(x, y);
        if (deco[i] != D_None) continue;
        if ((abs(x - penx) > abs(y - peny) ? abs(x - penx) : abs(y - peny)) <= 3) continue;
        int t = terrain[i];
        int32_t ints[2] = { x, y };
        double u = herder_keyed_unit(seed, "deco", ints, 2);
        if (t == T_Farm && u < 0.05) deco[i] = D_Scarecrow;
        else if (t == T_Forest) deco[i] = u < 0.3 ? D_Tree : u < 0.5 ? D_Tree2 : u < 0.53 ? D_Stump : u < 0.56 ? D_Boulder : D_None;
        else if (t == T_Rock) deco[i] = u < 0.3 ? D_Boulder : D_None;
        else if (t == T_Grass) deco[i] = u < 0.05 ? D_Tuft : u < 0.07 ? D_Tree : D_None;
        else if (t == T_Meadow) deco[i] = u < 0.08 ? D_Flowers : u < 0.1 ? D_Tuft : D_None;
    }

    free(penDistance);
    penDistance = herder_distance_field(&grid, penx, peny, NULL, NULL);
    int walkable = 0;
    for (int i = 0; i < n * n; i++) if (isfinite(penDistance[i])) walkable++;

    free(elev);
    out->size = n; out->terrain = terrain; out->deco = deco;
    out->pen_distance = penDistance; out->pen_x = penx; out->pen_y = peny;
    out->village_count = vcount;
    for (int i = 0; i < vcount; i++) out->villages[i] = villages[i];
    out->walkable_count = walkable;
}

void herder_map_free(HerderMap *m) {
    free(m->terrain); free(m->deco); free(m->pen_distance);
    m->terrain = m->deco = NULL; m->pen_distance = NULL;
}
