#ifndef POCKETPUZZLES_HEADER
#define POCKETPUZZLES_HEADER

#include "chooser.h"
#include "game.h"
#include "common.h"

typedef enum {
    SCREEN_CHOOSER,
    SCREEN_GAME,
    SCREEN_HELP,
    SCREEN_PARAMS
} SCREENTYPE;

struct screen {
    SCREENTYPE currentScreen;
    void (*tap)(int x, int y);
    void (*long_tap)(int x, int y);
    void (*release)(int x, int y);
    void (*init)();
    LAYOUTTYPE (*getLayout)();
    void (*prepare)();
    void (*show)();
    void (*destroy)();
} SCREEN;

struct layout mainlayout;

static void setLayout(LAYOUTTYPE screenlayout);
void switchToChooser();
void switchToGame(struct game *thegame);

static void setupLayout();
static void setupApp();
void exitApp();

#endif

