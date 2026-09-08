/* The grammar engine, a 1:1 port of core/lang/grammar.ts. Deterministic given
 * (seed, event, tick, salt) and the Context. */
#ifndef HERDER_GRAMMAR_H
#define HERDER_GRAMMAR_H
#include "data/lang_data.h"

typedef struct {
    int kind;            /* TargetKind index 0..9 (TGT_* bit = 1<<kind) */
    const char *noun;
    const char *name;    /* NULL if unnamed */
    int plural;
} HerderTarget;

typedef struct {
    const char *seed;
    int tick, level;
    int band;            /* 0..4 */
    double heat;         /* 0..1 */
    double hour;
    HerderTarget target;
    const char *const *registers; int registers_n;
    const char *signatureWord;
    int sheepRemaining, sheepPenned, booksRead;
    const char *const *recent; int recent_n;
    const char *const *recentRules; int recentRules_n;
    const char *const *rulesToday; int rulesToday_n;
    const char *const *knownPacks; int knownPacks_n;
    const char *villageName, *dogName, *rivalName;
    const char *season;  /* NULL if none */
    int st_flees, st_absurds, st_shames, st_rains, st_breathers, st_books;
} HerderContext;

typedef struct {
    int ok;
    char text[512];
    const char *ruleId;
    int tier;
    char used[512];      /* unique lexicon words, space-joined */
} HerderGen;

void herder_grammar_init(void);
/* Generate a line for `event`; returns ok=0 if none. minTier<0 to disable. */
HerderGen herder_generate(int event, const HerderContext *ctx, int salt, int minTier);
/* Expand a one-off template (quotation frames). Returns 1 and fills out, or 0. */
int herder_expand_template(char *out, int cap, const char *tmpl, const HerderContext *ctx, int salt);
/* HUD helper. */
int herder_known_words(const HerderContext *ctx);

#endif
