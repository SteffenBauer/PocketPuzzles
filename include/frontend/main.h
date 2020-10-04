#ifndef MAIN_HEADER
#define MAIN_HEADER

#include "common.h"

extern void chooserScreenInit();
extern void chooserScreenShow();
extern void chooserScreenFree();

extern void chooserTap(int x, int y);
extern void chooserLongTap(int x, int y);
extern void chooserDrag(int x, int y);
extern void chooserRelease(int x, int y);
extern void chooserPrev();
extern void chooserNext();

extern void gameScreenInit();
extern void gameScreenShow();
extern void gameScreenFree();

extern void gameTap(int x, int y);
extern void gameLongTap(int x, int y);
extern void gameDrag(int x, int y);
extern void gameRelease(int x, int y);
extern void gamePrev();
extern void gameNext();

extern void gamestateFree();

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
    void (*drag)(int x, int y);
    void (*release)(int x, int y);
    void (*prev)();
    void (*next)();
} SCREEN;

void switchToChooserScreen();
void switchToGameScreen();

static void setupApp();
void exitApp();
extern void free_gamestate();

#endif

