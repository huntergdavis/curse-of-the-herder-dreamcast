/* Host parity test: the C RNG must reproduce the web game's golden vectors. */
#include "core/rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0, checks = 0;
static void expect_u32(const char *what, uint32_t got, uint32_t want) {
    checks++;
    if (got != want) { failures++; printf("FAIL %s: got %u want %u\n", what, got, want); }
}

/* Split `line` into up to `max` fields on single '\t' (empty fields kept). */
static int split_tabs(char *line, char **out, int max) {
    int n = 0; out[n++] = line;
    for (char *p = line; *p && n < max; p++) if (*p == '\t') { *p = 0; out[n++] = p + 1; }
    return n;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "tests/golden/rng.txt";
    FILE *f = fopen(path, "r");
    if (!f) { printf("cannot open %s\n", path); return 2; }
    char line[512]; char *fld[8];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = 0;
        if (line[0] == 0) continue;
        int n = split_tabs(line, fld, 8);
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
