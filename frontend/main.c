#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/main.h"

void switchToParamScreen() {
    SCREEN.currentScreen = SCREEN_PARAMS;
    SCREEN.tap           = &paramTap;
    SCREEN.long_tap      = &paramLongTap;
    SCREEN.drag          = &paramDrag;
    SCREEN.release       = &paramRelease;
    SCREEN.prev          = &paramPrev;
    SCREEN.next          = &paramNext;

    paramScreenShow();
}

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
    char *buf;
    SetPanelType(PANEL_ENABLED);
    stateInit();
    paramScreenInit();
    chooserScreenInit();
    gameScreenInit();
    buf = configGetItem("config_resume");
    if (buf && strcmp(buf, "game") == 0 && gameResumeGame())
        switchToGameScreen();
    else
        switchToChooserScreen();
}

void exitApp() {
    switch (SCREEN.currentScreen) {
        case SCREEN_GAME:
        case SCREEN_PARAMS:
            gameSerialise();
            break;
        case SCREEN_CHOOSER:
            chooserSerialise();
            break;
        default:
            break;
    }
    SCREEN.currentScreen = SCREEN_EXIT;
    stateFree();
    paramScreenFree();
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

