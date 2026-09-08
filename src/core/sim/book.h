/* Book catalogue metadata (id, when, pack presence). 1:1 with data/books.ts,
 * enough for reading order and the reread/book events. */
#ifndef HERDER_BOOK_H
#define HERDER_BOOK_H
#define HERDER_BOOK_COUNT 29
extern const char *const HERDER_BOOK_ID[HERDER_BOOK_COUNT];
extern const double HERDER_BOOK_WHEN[HERDER_BOOK_COUNT];
extern const int HERDER_BOOK_HAS_PACK[HERDER_BOOK_COUNT]; /* 0 only for "notes" */
int herder_book_notes_index(void);
#endif
