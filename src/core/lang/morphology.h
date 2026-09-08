/* Small English morphology, a 1:1 port of core/lang/morphology.ts.
 * Functions write into caller buffers and return them (or a count). UTF-8 is
 * emitted for the ellipsis and curly quotes exactly as the web produces them. */
#ifndef HERDER_MORPHOLOGY_H
#define HERDER_MORPHOLOGY_H

const char *herder_article(const char *word);
char *herder_with_article(char *out, int cap, const char *word);
char *herder_pluralize(char *out, int cap, const char *word, const char *override);
char *herder_capitalize(char *out, int cap, const char *s);
char *herder_verb_form(char *out, int cap, const char *base, const char *form,
                       const char *ov_s, const char *ov_ed, const char *ov_en);
char *herder_number_word(char *out, int cap, long n);
char *herder_ordinal_word(char *out, int cap, long n);
int herder_count_syllables(const char *word);
char *herder_tidy_sentence(char *out, int cap, const char *s);

#endif
