/* The herder's mouth: world -> context -> line. A 1:1 port of speech.ts. */
#ifndef HERDER_SPEECH_H
#define HERDER_SPEECH_H
#include "core/sim/world.h"
#include "core/lang/grammar.h"

typedef struct {
    int ok;
    char text[512];
    double heat;
    double seconds;
    const char *ruleId;
    char used[512];
    int sheepId;      /* -1 if none */
    char targetLabel[80];
} HerderUtterance;

const char *herder_signature_word(const char *seed);
void herder_build_context(HerderContext *c, const HerderWorld *w, const HerderEvent *e, int bandCap);
HerderUtterance herder_speak_for_event(const HerderWorld *w, const HerderEvent *e, int bandCap);
HerderUtterance herder_speak_idle(const HerderWorld *w, int bandCap);
HerderUtterance herder_speak_kind(const HerderWorld *w, int ev, int bandCap, double heatBump);
HerderUtterance herder_speak_epitaph(const HerderWorld *w, int bandCap);
int herder_next_idle_curse_ticks(const HerderWorld *w);

#endif
