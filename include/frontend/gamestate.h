#ifndef GAMESTATE_HEADER
#define GAMESTATE_HEADER

#include "common.h"
#include "puzzles.h"

bool gamestateInitialized;

struct serialise_buf {
    char *buf;
    int len, pos;
} game_save;

struct savefile_write {
    FILE *fp;
    int error;
};

void gamestateSerialise(midend *me);
const char *gamestateDeserialise(midend *me);
const char *gamestateGamesaveName(char **name);
void gamestateFree();

#endif

