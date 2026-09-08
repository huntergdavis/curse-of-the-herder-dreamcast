/* Master ban-list gate, a 1:1 port of core/lang/banned.ts.
 * Returns 1 if the (normalised) text matches any banned pattern, else 0. */
#ifndef HERDER_BANNED_H
#define HERDER_BANNED_H
void herder_normalise_for_ban(char *out, int cap, const char *text);
int herder_find_banned(const char *text);
#endif
