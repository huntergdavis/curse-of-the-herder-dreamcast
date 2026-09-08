#include "core/names.h"
#include "core/rng.h"
#include <stddef.h>
#include <math.h>

const char *const HERDER_FIRST[40] = {
    "Fennick","Wulfric","Mags","Dunstan","Hob","Alwin","Tamsin","Godric","Bran","Osric",
    "Edda","Perkin","Wat","Cuthbert","Aldous","Nell","Piers","Hodge","Gilly","Ansel",
    "Bartle","Elric","Maud","Rowan","Silas","Tobin","Ulric","Wynn","Ysolde","Jory",
    "Ebenezer","Hamnet","Lettice","Oswin","Rafe","Sibyl","Thurstan","Cadoc","Idony","Mungo",
};
const char *const HERDER_EPITHET[24] = {
    "the Damp","of the Lower Field","the Unlucky","Twice-Bitten","the Hoarse","of Wether Cross",
    "the Long-Suffering","Mudfoot","the Weary","of No Fixed Temper","the Sodden","Half-Awake",
    "the Perpetual","of the Steep Bit","the Muttering","Crookhand","the Woolly-Minded","Sheepless",
    "the Bewildered","of the Far Pen","the Grumbling","Gatekeeper","the Rained-On","the Late",
};
const char *const HERDER_SHEEP[44] = {
    "Gerald","Beatrix","Lord Fluffington","The Other Gerald","Marjorie","Doreen","Clive","Nigel",
    "Pamela","Barnaby","Eunice","Reginald","Wendy","Horace","Prudence","Montague","Agnes",
    "Percival","Gwendolyn","Cedric","Hilda","Rupert","Mildred","Ambrose","Edith","Cuthbert",
    "Winifred","Bartholomew","Muriel","Humphrey","Ethel","Lancelot","Dorothy","Fitzwilliam",
    "Gladys","Wilberforce","Bernadette","Algernon","Philippa","Cornelius","Maureen","Basil",
    "Donut","Mongo",
};
const char *const HERDER_DOGS[20] = {
    "Biscuit","Nell","Fly","Moss","Cap","Bess","Tam","Glen","Meg","Bracken",
    "Wisp","Jess","Roy","Sweep","Shep","Dot","Bramble","Tweed","Pip","Floss",
};
const char *const HERDER_RIVALS[12] = {
    "Tom","Alfred","Gilbert","Ned","Percy","Wilf","Ambrose","Cuthbert","Bertram","Silas","Humphrey","Clement",
};

static int pick(const char *seed, const char *domain, const int32_t *ints, int nints, int len) {
    return (int)floor(herder_keyed_unit(seed, domain, ints, nints) * len);
}
int herder_name_first(const char *s) { return pick(s, "herder-first", NULL, 0, 40); }
int herder_name_epithet(const char *s) { return pick(s, "herder-epithet", NULL, 0, 24); }
int herder_name_old(const char *s) { return herder_keyed_unit(s, "herder-old", NULL, 0) < 0.4 ? 1 : 0; }
int herder_sheep_name(const char *s, int id) { int32_t k[1] = { id }; return pick(s, "sheep-name", k, 1, 44); }
int herder_dog_name(const char *s) { return pick(s, "dog-name", NULL, 0, 20); }
int herder_rival_name(const char *s) { return pick(s, "rival-name", NULL, 0, 12); }
