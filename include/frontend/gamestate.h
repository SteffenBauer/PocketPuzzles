#ifndef GAMESTATE_HEADER
#define GAMESTATE_HEADER

#include "common.h"
#include "puzzles.h"

struct serialise_buf {
    char *buf;
    int len, pos;
} game_save;

struct savefile_write {
    FILE *fp;
    int error;
};

void serialise_game(midend *me);
const char *deserialise_game(midend *me);
const char *get_gamesave_name(char **name);
void free_gamestate();

#endif

