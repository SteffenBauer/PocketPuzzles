#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/main.h"

void switchToChooserScreen() {
    SCREEN.currentScreen = SCREEN_CHOOSER;
    SCREEN.tap           = &chooserTap;
    SCREEN.long_tap      = &chooserLongTap;
    SCREEN.drag          = &chooserDrag;
    SCREEN.release       = &chooserRelease;
    SCREEN.prev          = &chooserPrev;
    SCREEN.next          = &chooserNext;

    chooserScreenShow();
}

void switchToGameScreen() {
    SCREEN.currentScreen = SCREEN_GAME;
    SCREEN.tap           = &gameTap;
    SCREEN.long_tap      = &gameLongTap;
    SCREEN.drag          = &gameDrag;
    SCREEN.release       = &gameRelease;
    SCREEN.prev          = &gamePrev;
    SCREEN.next          = &gameNext;

    gameScreenShow();
}

static void setupApp() {
    SetPanelType(PANEL_ENABLED);
    chooserScreenInit();
    gameScreenInit();
    switchToChooserScreen();
}

void exitApp() {
    /* free_gamestate(); */
    chooserScreenFree();
    gameScreenFree();
    CloseApp();
}

static int main_handler(int event_type, int param_one, int param_two) {
    if (event_type == EVT_INIT) {
        setupApp();
    }
    else if (event_type == EVT_EXIT || (event_type == EVT_HIDE) ||
            (event_type == EVT_KEYPRESS && param_one == IV_KEY_HOME)) {
        exitApp();
    }
    else if (event_type == EVT_KEYPRESS && param_one == IV_KEY_PREV) {
        SCREEN.prev();
    }
    else if (event_type == EVT_KEYPRESS && param_one == IV_KEY_NEXT) {
        SCREEN.next();
    }
    else if (event_type == EVT_POINTERDOWN) {
        SCREEN.tap(param_one, param_two);
    }
    else if (event_type == EVT_POINTERLONG) {
        SCREEN.long_tap(param_one, param_two);
    }
    else if (event_type == EVT_POINTERDRAG) {
        SCREEN.drag(param_one, param_two);
    }
    else if (event_type == EVT_POINTERUP) {
        SCREEN.release(param_one, param_two);
    }
    return 0;
}

int main (int argc, char* argv[]) {
    InkViewMain(main_handler);
    return 0;
}

