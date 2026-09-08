/* Herder/sheep/dog/rival names. 1:1 with src/core/names.ts (keyed picks). */
#ifndef HERDER_NAMES_H
#define HERDER_NAMES_H
extern const char *const HERDER_FIRST[40];
extern const char *const HERDER_EPITHET[24];
extern const char *const HERDER_SHEEP[44];
extern const char *const HERDER_DOGS[20];
extern const char *const HERDER_RIVALS[12];
/* Chosen indices / flags, for parity and for building the display names. */
int herder_name_first(const char *seed);
int herder_name_epithet(const char *seed);
int herder_name_old(const char *seed);   /* 1 if "Old " prefix */
int herder_sheep_name(const char *seed, int sheep_id);
int herder_dog_name(const char *seed);
int herder_rival_name(const char *seed);
#endif
