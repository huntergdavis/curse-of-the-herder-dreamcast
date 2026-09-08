/* Host parity test: the C RNG must reproduce the web game's golden vectors. */
#include "core/rng.h"
#include "core/noise.h"
#include "core/map/terrain.h"
#include "core/map/path.h"
#include "core/map/generate.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0, checks = 0;
static void expect_u32(const char *what, uint32_t got, uint32_t want) {
    checks++;
    if (got != want) { failures++; printf("FAIL %s: got %u want %u\n", what, got, want); }
}

/* Split `line` into up to `max` fields on single '\t' (empty fields kept). */
static const char *HEXD = "0123456789abcde";
static uint32_t byte_hash(const uint8_t *a, int len) {
    uint32_t h = 0x811c9dc5u;
    for (int i = 0; i < len; i++) { h ^= a[i]; h *= 0x01000193u; }
    return h;
}
static uint64_t bits_of(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double dbl_of(const char *hex16) { uint64_t u = (uint64_t)strtoull(hex16, NULL, 16); double d; memcpy(&d, &u, 8); return d; }
static void expect_bits(const char *what, double got, const char *wanthex) {
    checks++;
    uint64_t g = bits_of(got), w = (uint64_t)strtoull(wanthex, NULL, 16);
    if (g != w) { failures++; printf("FAIL %s: got %016llx want %s\n", what, (unsigned long long)g, wanthex); }
}
static int split_tabs(char *line, char **out, int max) {
    int n = 0; out[n++] = line;
    for (char *p = line; *p && n < max; p++) if (*p == '\t') { *p = 0; out[n++] = p + 1; }
    return n;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "tests/golden/rng.txt";
    FILE *f = fopen(path, "r");
    if (!f) { printf("cannot open %s\n", path); return 2; }
    static char line[65536]; char *fld[12];
    static uint8_t gterr[64*64]; int gn = 0; char gid[32] = {0};
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = 0;
        if (line[0] == 0) continue;
        int n = split_tabs(line, fld, 12);
        if (n < 2) continue;
        if (strcmp(fld[0], "FNV") == 0 && n >= 3) {
            char w[80]; snprintf(w, sizeof(w), "FNV(%s)", fld[1]);
            expect_u32(w, herder_fnv1a(fld[1]), (uint32_t)strtoul(fld[2], NULL, 16));
        } else if (strcmp(fld[0], "MUL") == 0 && n >= 4) {
            uint32_t seed = (uint32_t)strtoul(fld[1], NULL, 10);
            int nn = atoi(fld[2]);
            uint32_t st = seed;
            char *tok = strtok(fld[3], ",");
            for (int i = 0; i < nn && tok; i++) {
                char w[48]; snprintf(w, sizeof(w), "MUL(%u)[%d]", seed, i);
                expect_u32(w, herder_mulberry32_u32(&st), (uint32_t)strtoul(tok, NULL, 10));
                tok = strtok(NULL, ",");
            }
        } else if (strcmp(fld[0], "LATTICE") == 0 && n >= 5) {
            /* lattice is private; recompute value_noise at integer coords would differ, so test it via a dedicated path. */
            /* We test lattice indirectly through value noise; skip direct check. */
            (void)0;
        } else if (strcmp(fld[0], "VNOISE") == 0 && n >= 5) {
            uint32_t seed = (uint32_t)strtoul(fld[1], NULL, 10);
            double x = dbl_of(fld[2]), y = dbl_of(fld[3]);
            char w[64]; snprintf(w, sizeof(w), "VNOISE(%u,%.4f,%.4f)", seed, x, y);
            expect_bits(w, herder_value_noise(seed, x, y), fld[4]);
        } else if (strcmp(fld[0], "FBM") == 0 && n >= 6) {
            uint32_t seed = (uint32_t)strtoul(fld[1], NULL, 10);
            double x = dbl_of(fld[2]), y = dbl_of(fld[3]), sc = dbl_of(fld[4]);
            char w[64]; snprintf(w, sizeof(w), "FBM(%u,%.2f,%.2f,%.1f)", seed, x, y, sc);
            expect_bits(w, herder_fbm(seed, x, y, sc), fld[5]);
        } else if (strcmp(fld[0], "MAP") == 0 && n >= 9) {
            HerderMap m; herder_generate_map(fld[1], atoi(fld[2]), &m);
            char vs[512]; int vp = 0;
            for (int v = 0; v < m.village_count; v++) vp += snprintf(vs + vp, sizeof(vs) - (size_t)vp, "%s%d.%d.%d", v ? ";" : "", m.villages[v].name, m.villages[v].x, m.villages[v].y);
            char got[600];
            snprintf(got, sizeof(got), "%08x\t%08x\t%d\t%d\t%d\t%s",
                     byte_hash(m.terrain, m.size*m.size), byte_hash(m.deco, m.size*m.size),
                     m.pen_x, m.pen_y, m.walkable_count, vs);
            char want[600];
            snprintf(want, sizeof(want), "%s\t%s\t%s\t%s\t%s\t%s", fld[3], fld[4], fld[5], fld[6], fld[7], n >= 9 ? fld[8] : "");
            checks++;
            if (strcmp(got, want) != 0) { failures++; printf("FAIL MAP(%s,%s):\n  got  %s\n  want %s\n", fld[1], fld[2], got, want); }
            herder_map_free(&m);
        } else if (strcmp(fld[0], "MAPGRID") == 0 && n >= 5) {
            HerderMap m; herder_generate_map(fld[1], atoi(fld[2]), &m);
            int len = m.size * m.size, bad_t = 0, bad_d = 0, first = -1;
            for (int i = 0; i < len; i++) {
                if (HEXD[m.terrain[i]] != fld[3][i]) { bad_t++; if (first < 0) first = i; }
                if (HEXD[m.deco[i]] != fld[4][i]) { bad_d++; if (first < 0) first = i; }
            }
            checks++;
            if (bad_t || bad_d) { failures++; printf("FAIL MAPGRID: terrain %d, deco %d differ (first at tile %d = %d,%d)\n", bad_t, bad_d, first, first % m.size, first / m.size); }
            herder_map_free(&m);
        } else if (strcmp(fld[0], "GRID") == 0 && n >= 4) {
            snprintf(gid, sizeof(gid), "%s", fld[1]);
            gn = atoi(fld[2]);
            for (int t = 0; t < gn * gn; t++) { char ch = fld[3][t]; gterr[t] = (uint8_t)(ch <= '9' ? ch - '0' : ch - 'a' + 10); }
        } else if (strcmp(fld[0], "DIST") == 0 && n >= 5) {
            HerderGrid g = { gn, gterr };
            double *df = herder_distance_field(&g, atoi(fld[2]), atoi(fld[3]), NULL, NULL);
            char *tok = strtok(fld[4], ","); int idx = 0; int bad = 0;
            for (; tok && idx < gn * gn; idx++, tok = strtok(NULL, ",")) {
                if (bits_of(df[idx]) != (uint64_t)strtoull(tok, NULL, 16)) bad++;
            }
            checks++; if (bad) { failures++; printf("FAIL DIST(%s): %d/%d tiles differ\n", gid, bad, gn*gn); }
            free(df);
        } else if (strcmp(fld[0], "PATH") == 0 && n >= 6) {
            HerderGrid g = { gn, gterr };
            static int out[64*64*2];
            int cnt = herder_find_path(&g, atoi(fld[2]), atoi(fld[3]), atoi(fld[4]), atoi(fld[5]), NULL, NULL, out);
            char got[4096]; int gp = 0;
            if (cnt < 0) gp += snprintf(got, sizeof(got), "null");
            else for (int t = 0; t < cnt; t++) gp += snprintf(got + gp, sizeof(got) - (size_t)gp, "%s%d.%d", t ? "," : "", out[t*2], out[t*2+1]);
            checks++;
            if (strcmp(got, fld[6]) != 0) { failures++; printf("FAIL PATH(%s): got %s want %s\n", gid, got, fld[6]); }
        } else if (strcmp(fld[0], "CLASSIFY") == 0 && n >= 4) {
            double e = dbl_of(fld[1]), m = dbl_of(fld[2]);
            checks++;
            int got = herder_classify(e, m), want = atoi(fld[3]);
            if (got != want) { failures++; printf("FAIL CLASSIFY(%.3f,%.3f): got %d want %d\n", e, m, got, want); }
        } else if (strcmp(fld[0], "KEYED") == 0 && n >= 3) {
            uint32_t st = herder_fnv1a(fld[1]);
            char w[160]; snprintf(w, sizeof(w), "KEYED(%s)", fld[1]);
            expect_u32(w, herder_mulberry32_u32(&st), (uint32_t)strtoul(fld[2], NULL, 10));
        }
    }
    fclose(f);
    printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
