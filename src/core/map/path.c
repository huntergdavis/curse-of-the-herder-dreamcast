#include "core/map/path.h"
#include "core/map/terrain.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
static const int DIAG[8] = {0, 0, 0, 0, 1, 1, 1, 1};
#define SQRT2 1.4142135623730951 /* Math.SQRT2 */

/* Binary min-heap over (key=double, val=int), parallel arrays; mirrors path.ts. */
typedef struct { double *keys; int *vals; int size, cap; } Heap;
static void heap_init(Heap *h, int cap) {
    h->cap = cap; h->size = 0;
    h->keys = malloc((size_t)cap * sizeof(double));
    h->vals = malloc((size_t)cap * sizeof(int));
}
static void heap_free(Heap *h) { free(h->keys); free(h->vals); }
static void heap_push(Heap *h, double key, int val) {
    if (h->size == h->cap) {
        h->cap *= 2;
        h->keys = realloc(h->keys, (size_t)h->cap * sizeof(double));
        h->vals = realloc(h->vals, (size_t)h->cap * sizeof(int));
    }
    int i = h->size++;
    while (i > 0) {
        int p = (i - 1) >> 1;
        if (h->keys[p] <= key) break;
        h->keys[i] = h->keys[p]; h->vals[i] = h->vals[p]; i = p;
    }
    h->keys[i] = key; h->vals[i] = val;
}
static int heap_pop(Heap *h) {
    int top = h->vals[0];
    int n = --h->size;
    if (n > 0) {
        double key = h->keys[n]; int val = h->vals[n];
        int i = 0;
        for (;;) {
            int c = 2 * i + 1;
            if (c >= n) break;
            if (c + 1 < n && h->keys[c + 1] < h->keys[c]) c++;
            if (h->keys[c] >= key) break;
            h->keys[i] = h->keys[c]; h->vals[i] = h->vals[c]; i = c;
        }
        h->keys[i] = key; h->vals[i] = val;
    }
    return top;
}

static double tile_cost(const HerderGrid *g, int x, int y) {
    if (x < 0 || y < 0 || x >= g->size || y >= g->size) return INFINITY;
    return HERDER_TERRAIN_COST[g->terrain[y * g->size + x]];
}

double *herder_distance_field(const HerderGrid *g, int sx, int sy,
                              herder_cost_override ov, void *ctx) {
    int n = g->size;
    double *dist = malloc((size_t)n * n * sizeof(double));
    for (int i = 0; i < n * n; i++) dist[i] = INFINITY;
    Heap heap; heap_init(&heap, 4096);
    int start = sy * n + sx;
    dist[start] = 0;
    heap_push(&heap, 0, start);
    while (heap.size > 0) {
        int i = heap_pop(&heap);
        double d = dist[i];
        if (d > dist[i]) continue;
        int x = i % n, y = (i - x) / n;
        for (int k = 0; k < 8; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
            double c = tile_cost(g, nx, ny);
            if (ov) c = ov(ctx, nx, ny, c);
            if (!isfinite(c)) continue;
            if (DIAG[k]) {
                if (!isfinite(tile_cost(g, x + DX[k], y)) || !isfinite(tile_cost(g, x, y + DY[k]))) continue;
                c *= SQRT2;
            }
            int j = ny * n + nx;
            double nd = d + c;
            if (nd < dist[j]) { dist[j] = nd; heap_push(&heap, nd, j); }
        }
    }
    heap_free(&heap);
    return dist;
}

int herder_find_path(const HerderGrid *g, int sx, int sy, int gx, int gy,
                     herder_cost_override ov, void *ctx, int *out_xy) {
    int n = g->size;
    if (sx == gx && sy == gy) return 0;
    double *gc = malloc((size_t)n * n * sizeof(double));
    int *parent = malloc((size_t)n * n * sizeof(int));
    uint8_t *closed = calloc((size_t)n * n, 1);
    for (int i = 0; i < n * n; i++) { gc[i] = INFINITY; parent[i] = -1; }
    Heap heap; heap_init(&heap, 4096);
    int start = sy * n + sx, goal = gy * n + gx;
    const double minCost = 0.7;
    #define HEUR(X, Y) (minCost * ((abs((X)-gx) > abs((Y)-gy) ? abs((X)-gx) : abs((Y)-gy)) + (SQRT2 - 1) * (abs((X)-gx) < abs((Y)-gy) ? abs((X)-gx) : abs((Y)-gy))))
    gc[start] = 0;
    heap_push(&heap, HEUR(sx, sy), start);
    long expansions = 0;
    const long maxExpansions = 400000;
    while (heap.size > 0) {
        int i = heap_pop(&heap);
        if (closed[i]) continue;
        closed[i] = 1;
        if (i == goal) break;
        if (++expansions > maxExpansions) { free(gc); free(parent); free(closed); heap_free(&heap); return -1; }
        int x = i % n, y = (i - x) / n;
        double gi = gc[i];
        for (int k = 0; k < 8; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
            int j = ny * n + nx;
            if (closed[j]) continue;
            double c = tile_cost(g, nx, ny);
            if (ov) c = ov(ctx, nx, ny, c);
            if (!isfinite(c)) continue;
            if (DIAG[k]) {
                if (!isfinite(tile_cost(g, x + DX[k], y)) || !isfinite(tile_cost(g, x, y + DY[k]))) continue;
                c *= SQRT2;
            }
            double ng = gi + c;
            if (ng < gc[j]) { gc[j] = ng; parent[j] = i; heap_push(&heap, ng + HEUR(nx, ny), j); }
        }
    }
    int count = -1;
    if (closed[goal]) {
        /* Reconstruct start..goal, then reverse. */
        int tmp[4096]; int m = 0;
        for (int i = goal; i != start && i != -1; i = parent[i]) { if (m < 4096) tmp[m++] = i; }
        count = 0;
        for (int t = m - 1; t >= 0; t--) { int idx = tmp[t]; out_xy[count*2] = idx % n; out_xy[count*2+1] = idx / n; count++; }
    }
    free(gc); free(parent); free(closed); heap_free(&heap);
    return count;
}
