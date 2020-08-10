#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/main.h"

void switchToChooser() {
    SCREEN.currentScreen = SCREEN_CHOOSER;
    SCREEN.tap           = &chooserTap;
    SCREEN.long_tap      = &chooserLongTap;
    SCREEN.drag          = &chooserDrag;
    SCREEN.release       = &chooserRelease;

    chooserShowPage();
}

void switchToGame(const struct game *thegame) {
    SCREEN.currentScreen = SCREEN_GAME;
    SCREEN.tap           = &gameTap;
    SCREEN.long_tap      = &gameLongTap;
    SCREEN.drag          = &gameDrag;
    SCREEN.release       = &gameRelease;

    gameInit(thegame);
    gameShowPage();
}

static void setupApp() {
    SetPanelType(PANEL_ENABLED);
    chooserInit();
    switchToChooser();
}

void exitApp() {
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

