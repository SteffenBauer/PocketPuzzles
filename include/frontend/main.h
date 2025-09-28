#ifndef MAIN_HEADER
#define MAIN_HEADER

#include "common.h"

extern void IvSetAppCapability(int caps) __attribute__((weak));

extern void paramScreenInit();
extern void paramScreenShow();
extern void paramScreenFree();

extern void paramTap(int x, int y);
extern void paramLongTap(int x, int y);
extern void paramDrag(int x, int y);
extern void paramRelease(int x, int y);
extern void paramPrev();
extern void paramNext();

extern void chooserScreenInit();
extern void chooserScreenShow();
extern void chooserScreenFree();

extern void chooserTap(int x, int y);
extern void chooserLongTap(int x, int y);
extern void chooserDrag(int x, int y);
extern void chooserRelease(int x, int y);
extern void chooserPrev();
extern void chooserNext();
extern void chooserSerialise();

extern bool gameResumeGame();
extern void gameScreenInit();
extern void gameScreenShow();
extern void gameScreenFree();

extern void gameTap(int x, int y);
extern void gameLongTap(int x, int y);
extern void gameDrag(int x, int y);
extern void gameRelease(int x, int y);
extern void gamePrev();
extern void gameNext();
extern void gameSerialise();

extern void stateInit();
extern void stateFree();
extern char *configGetItem(char *key);

typedef enum {
    SCREEN_CHOOSER,
    SCREEN_GAME,
    SCREEN_HELP,
    SCREEN_PARAMS,
    SCREEN_EXIT
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

void switchToParamScreen();
void switchToChooserScreen();
void switchToGameScreen();

static bool setupAppCapabilities();
static void setupApp();
void exitApp();

#endif

