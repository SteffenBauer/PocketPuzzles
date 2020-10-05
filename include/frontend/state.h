#ifndef STATE_HEADER
#define STATE_HEADER

#include "common.h"
#include "puzzles.h"

bool stateInitialized;

typedef struct dict_t_struct {
    char *key;
    char *value;
    struct dict_t_struct *next;
} dict_t;

static dict_t *config = NULL;

struct serialise_buf {
    char *buf;
    int len, pos;
} game_save;

struct savefile_write {
    FILE *fp;
    int error;
};

int configLen();
void configAddItem(char *key, char *value);
void configDel();
char *configGetItem(char *key);
void configDelItem(char *key);
void configSave();
void configLoad();

void stateSerialise(midend *me);
const char *stateDeserialise(midend *me);
const char *stateGamesaveName(char **name);

void stateLoadParams(midend *me, const game *ourgame);
void stateSaveParams(midend *me, const game *ourgame);

void stateInit();
void stateFree();

#endif
