/* Host parity test: the C RNG must reproduce the web game's golden vectors. */
#include "core/rng.h"
#include "core/noise.h"
#include "core/map/terrain.h"
#include "core/map/path.h"
#include "core/map/generate.h"
#include "core/progression.h"
#include "core/names.h"
#include "core/sim/flock.h"
#include "core/sim/world.h"
#include "core/lang/morphology.h"
#include "core/lang/banned.h"
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
static const char *const TEMPERN[5] = {"plain","skittish","stubborn","dozy","curious"};
static HerderSheep g_flock[80]; static int g_flockn = 0; static char g_flockseed[64] = {0};
static HerderLib g_libs[32]; static int g_libn = 0; static char g_libseed[64] = {0};
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


static HerderWorld *ev_world(const char *seed) {
    static HerderWorld *cache[4]; static char cseed[4][32]; static HerderMap cmap[4]; static int ncache = 0;
    for (int i = 0; i < ncache; i++) if (strcmp(cseed[i], seed) == 0) return cache[i];
    int i = ncache++;
    snprintf(cseed[i], 32, "%s", seed);
    herder_generate_map(cseed[i], 576, &cmap[i]);
    cache[i] = malloc(sizeof(HerderWorld));
    herder_world_init(cache[i], &cmap[i], cseed[i]);
    int guard = 0;
    while (!cache[i]->finished && guard++ < 9*3600*4 + 10000) herder_step(cache[i]);
    return cache[i];
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
        } else if (strcmp(fld[0], "LIBN") == 0 && n >= 3) {
            if (strcmp(g_libseed, fld[1]) != 0) {
                HerderMap m; herder_generate_map(fld[1], 576, &m);
                static HerderSheep sh[80];
                herder_create_world(&m, fld[1], sh, 80, g_libs, &g_libn);
                snprintf(g_libseed, sizeof(g_libseed), "%s", fld[1]);
                herder_map_free(&m);
            }
            checks++; if (g_libn != atoi(fld[2])) { failures++; printf("FAIL LIBN(%s): got %d want %d\n", fld[1], g_libn, atoi(fld[2])); }
        } else if (strcmp(fld[0], "LIBS") == 0 && n >= 5) {
            if (strcmp(g_libseed, fld[1]) != 0) {
                HerderMap m; herder_generate_map(fld[1], 576, &m);
                static HerderSheep sh[80];
                herder_create_world(&m, fld[1], sh, 80, g_libs, &g_libn);
                snprintf(g_libseed, sizeof(g_libseed), "%s", fld[1]);
                herder_map_free(&m);
            }
            int k = atoi(fld[2]);
            checks++;
            if (g_libs[k].x != atoi(fld[3]) || g_libs[k].y != atoi(fld[4])) { failures++; printf("FAIL LIBS(%s,%d): got %d,%d want %s,%s\n", fld[1], k, g_libs[k].x, g_libs[k].y, fld[3], fld[4]); }
        } else if (strcmp(fld[0], "FLOCKN") == 0 && n >= 4) {
            if (strcmp(g_flockseed, fld[1]) != 0) {
                HerderMap m; herder_generate_map(fld[1], atoi(fld[2]), &m);
                g_flockn = herder_create_flock(&m, fld[1], g_flock, 80);
                snprintf(g_flockseed, sizeof(g_flockseed), "%s", fld[1]);
                herder_map_free(&m);
            }
            checks++; if (g_flockn != atoi(fld[3])) { failures++; printf("FAIL FLOCKN(%s): got %d want %d\n", fld[1], g_flockn, atoi(fld[3])); }
        } else if (strcmp(fld[0], "FLOCKS") == 0 && n >= 11) {
            if (strcmp(g_flockseed, fld[1]) != 0) {
                HerderMap m; herder_generate_map(fld[1], 576, &m);
                g_flockn = herder_create_flock(&m, fld[1], g_flock, 80);
                snprintf(g_flockseed, sizeof(g_flockseed), "%s", fld[1]);
                herder_map_free(&m);
            }
            int id = atoi(fld[2]);
            HerderSheep *s = &g_flock[id];
            char flags[4]; int fp = 0;
            if (s->on_roof) flags[fp++] = 'r';
            if (s->in_river) flags[fp++] = 'v';
            if (s->on_boulder) flags[fp++] = 'b';
            if (fp == 0) flags[fp++] = '-';
            flags[fp] = 0;
            const char *tp = s->temper >= 0 ? TEMPERN[s->temper] : "-";
            char got[128], want[128];
            snprintf(got, sizeof(got), "%d\t%d\t%016llx\t%s\t%d\t%d\t%s\t%d", (int)s->x, (int)s->y, (unsigned long long)bits_of(s->skittish), tp, s->absurd, s->ring, flags, s->black);
            snprintf(want, sizeof(want), "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s", fld[3], fld[4], fld[5], fld[6], fld[7], fld[8], fld[9], fld[10]);
            checks++; if (strcmp(got, want) != 0) { failures++; printf("FAIL FLOCKS(%s,%d):\n  got  %s\n  want %s\n", fld[1], id, got, want); }
        } else if (strcmp(fld[0], "PROG_ER") == 0 && n >= 6) {
            double er = herder_erudition(dbl_of(fld[1]), dbl_of(fld[2]), dbl_of(fld[3]));
            expect_bits("PROG_ER", er, fld[4]);
            checks++; if (herder_level_for(er) != atoi(fld[5])) { failures++; printf("FAIL PROG_ER level\n"); }
        } else if (strcmp(fld[0], "PROG_FILTH") == 0 && n >= 3) {
            checks++; if (herder_filth_ceiling(atof(fld[1])) != atoi(fld[2])) { failures++; printf("FAIL FILTH(%s)\n", fld[1]); }
        } else if (strcmp(fld[0], "PROG_CI") == 0 && n >= 4) {
            expect_bits("PROG_CI", herder_curse_interval_seconds(atoi(fld[1]), dbl_of(fld[2])), fld[3]);
        } else if (strcmp(fld[0], "PROG_CLAMP") == 0 && n >= 3) {
            expect_bits("PROG_CLAMP", herder_clamp_frustration(dbl_of(fld[1])), fld[2]);
        } else if (strcmp(fld[0], "PROG_BASE") == 0 && n >= 3) {
            expect_bits("PROG_BASE", herder_frustration_baseline(dbl_of(fld[1])), fld[2]);
        } else if (strcmp(fld[0], "PROG_DRIFT") == 0 && n >= 5) {
            expect_bits("PROG_DRIFT", herder_frustration_drift(dbl_of(fld[1]), dbl_of(fld[2]), dbl_of(fld[3])), fld[4]);
        } else if (strcmp(fld[0], "NAME_HERDER") == 0 && n >= 3) {
            char buf[64]; const char *seed = fld[1];
            snprintf(buf, sizeof(buf), "%s%s %s", herder_name_old(seed) ? "Old " : "",
                     HERDER_FIRST[herder_name_first(seed)], HERDER_EPITHET[herder_name_epithet(seed)]);
            checks++; if (strcmp(buf, fld[2]) != 0) { failures++; printf("FAIL NAME_HERDER(%s): got %s want %s\n", seed, buf, fld[2]); }
        } else if (strcmp(fld[0], "NAME_DOG") == 0 && n >= 3) {
            checks++; if (strcmp(HERDER_DOGS[herder_dog_name(fld[1])], fld[2]) != 0) { failures++; printf("FAIL NAME_DOG(%s)\n", fld[1]); }
        } else if (strcmp(fld[0], "NAME_RIVAL") == 0 && n >= 3) {
            checks++; if (strcmp(HERDER_RIVALS[herder_rival_name(fld[1])], fld[2]) != 0) { failures++; printf("FAIL NAME_RIVAL(%s)\n", fld[1]); }
        } else if (strcmp(fld[0], "NAME_SHEEP") == 0 && n >= 4) {
            checks++; if (strcmp(HERDER_SHEEP[herder_sheep_name(fld[1], atoi(fld[2]))], fld[3]) != 0) { failures++; printf("FAIL NAME_SHEEP(%s,%s): got %s want %s\n", fld[1], fld[2], HERDER_SHEEP[herder_sheep_name(fld[1], atoi(fld[2]))], fld[3]); }
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
        } else if (strcmp(fld[0], "BAN") == 0 && n >= 3) {
            int g = herder_find_banned(fld[1]) ? 1 : 0;
            checks++; if (g != atoi(fld[2])) { failures++; printf("FAIL BAN(%s): got %d want %s\n", fld[1], g, fld[2]); }
        } else if (strcmp(fld[0], "NORM") == 0 && n >= 3) {
            char g[256]; herder_normalise_for_ban(g, sizeof(g), fld[1]);
            checks++; if (strcmp(g, fld[2]) != 0) { failures++; printf("FAIL NORM(%s): got [%s] want [%s]\n", fld[1], g, fld[2]); }
        } else if (strcmp(fld[0], "ART") == 0 && n >= 3) {
            checks++; const char *g = herder_article(fld[1]);
            if (strcmp(g, fld[2]) != 0) { failures++; printf("FAIL ART(%s): got %s want %s\n", fld[1], g, fld[2]); }
        } else if (strcmp(fld[0], "PLUR") == 0 && n >= 4) {
            char g[256]; const char *ov = strcmp(fld[2], "~") == 0 ? NULL : fld[2];
            herder_pluralize(g, sizeof(g), fld[1], ov);
            checks++; if (strcmp(g, fld[3]) != 0) { failures++; printf("FAIL PLUR(%s,%s): got %s want %s\n", fld[1], fld[2], g, fld[3]); }
        } else if (strcmp(fld[0], "VERB") == 0 && n >= 4) {
            char g[128]; herder_verb_form(g, sizeof(g), fld[1], fld[2], NULL, NULL, NULL);
            checks++; if (strcmp(g, fld[3]) != 0) { failures++; printf("FAIL VERB(%s,%s): got %s want %s\n", fld[1], fld[2], g, fld[3]); }
        } else if (strcmp(fld[0], "NUM") == 0 && n >= 3) {
            char g[128]; herder_number_word(g, sizeof(g), atol(fld[1]));
            checks++; if (strcmp(g, fld[2]) != 0) { failures++; printf("FAIL NUM(%s): got %s want %s\n", fld[1], g, fld[2]); }
        } else if (strcmp(fld[0], "ORD") == 0 && n >= 3) {
            char g[128]; herder_ordinal_word(g, sizeof(g), atol(fld[1]));
            checks++; if (strcmp(g, fld[2]) != 0) { failures++; printf("FAIL ORD(%s): got %s want %s\n", fld[1], g, fld[2]); }
        } else if (strcmp(fld[0], "SYL") == 0 && n >= 3) {
            int g = herder_count_syllables(fld[1]);
            checks++; if (g != atoi(fld[2])) { failures++; printf("FAIL SYL(%s): got %d want %s\n", fld[1], g, fld[2]); }
        } else if (strcmp(fld[0], "TIDY") == 0 && n >= 3) {
            char g[8192]; herder_tidy_sentence(g, sizeof(g), fld[1]);
            checks++; if (strcmp(g, fld[2]) != 0) { failures++; if (failures < 40) printf("FAIL TIDY(%s):\n  got  [%s]\n  want [%s]\n", fld[1], g, fld[2]); }
        } else if (strcmp(fld[0], "EV") == 0 && n >= 6) {
            HerderWorld *w = ev_world(fld[1]);
            int seq = atoi(fld[2]);
            checks++;
            if (seq >= w->event_count) { failures++; if (failures < 40) printf("FAIL EV(%s,%d): missing (only %d events)\n", fld[1], seq, w->event_count); }
            else {
                HerderEvent *e = &w->events[seq];
                char got[96], want[96];
                snprintf(got, sizeof(got), "%d\t%s\t%d\t%s\t%s", e->tick, e->kind, e->sheep_id, e->detail, e->book);
                snprintf(want, sizeof(want), "%s\t%s\t%s\t%s\t%s", fld[3], fld[4], fld[5], n>=7?fld[6]:"", n>=8?fld[7]:"");
                if (strcmp(got, want) != 0) { failures++; if (failures < 40) printf("FAIL EV(%s,seq%d):\n  got  %s\n  want %s\n", fld[1], seq, got, want); }
            }
        } else if (strcmp(fld[0], "EVEND") == 0 && n >= 5) {
            HerderWorld *w = ev_world(fld[1]);
            char got[64], want[64];
            snprintf(got, sizeof(got), "%d\t%d\t%d", w->tick, w->finished ? 1 : 0, w->event_count);
            snprintf(want, sizeof(want), "%s\t%s\t%s", fld[2], fld[3], fld[4]);
            checks++;
            if (strcmp(got, want) != 0) { failures++; printf("FAIL EVEND(%s):\n  got  %s\n  want %s\n", fld[1], got, want); }
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
