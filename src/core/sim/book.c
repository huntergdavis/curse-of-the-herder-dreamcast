#include "core/sim/book.h"
/* Array order matches data/books.ts (the stable tiebreak for equal `when`). */
const char *const HERDER_BOOK_ID[HERDER_BOOK_COUNT] = {
    "first-words","sad-almanac","grumbles","slow-things","manners","cattle-market",
    "vulgar-tongue","bard","drover","french","german","porca","sacres","kvetch",
    "hemingway","crawler-notes","three-men","one-word","boatswain","devils-dictionary",
    "gargantua","cooks-oracle","geology","blacks-law","knitting","grays","fungi","burns","notes",
};
const double HERDER_BOOK_WHEN[HERDER_BOOK_COUNT] = {
    0.02,0.06,0.1,0.16,0.22,0.3,0.36,0.44,0.5,0.56,0.6,0.64,0.68,0.72,0.78,0.55,
    0.6,0.84,0.9,0.8,0.82,0.47,0.66,0.52,0.28,0.6,0.42,0.75,0.97,
};
const int HERDER_BOOK_HAS_PACK[HERDER_BOOK_COUNT] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
};
int herder_book_notes_index(void) { return 28; }
