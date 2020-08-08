#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "pocketpuzzles.h"

void switchToChooser() {
    mainlayout.with_statusbar     = false;
    mainlayout.buttonpanel.height = mainlayout.control_size + 5;
    mainlayout.buttonpanel.starty = ScreenHeight()-PanelHeight() - mainlayout.buttonpanel.height;
    mainlayout.maincanvas.starty = mainlayout.menu.height + 3;
    mainlayout.maincanvas.height = mainlayout.buttonpanel.starty - mainlayout.maincanvas.starty - 1;

    SCREEN.init     = &chooserInit;
    SCREEN.show     = &chooserShowPage;
    SCREEN.tap      = &chooserTap;
    SCREEN.long_tap = &chooserLongTap;
    SCREEN.release  = &chooserRelease;

    SCREEN.init();
    SCREEN.show();

}

void switchToGame(struct game *thegame) {
    mainlayout.with_statusbar     = true;
    mainlayout.buttonpanel.height = mainlayout.control_size + 5;
    mainlayout.buttonpanel.starty = mainlayout.statusbar.starty - mainlayout.buttonpanel.height - 1;
    mainlayout.maincanvas.starty = mainlayout.menu.height + 3;
    mainlayout.maincanvas.height = mainlayout.buttonpanel.starty - mainlayout.maincanvas.starty - 1;

    currentgame = thegame;

    SCREEN.init     = &gameInit;
    SCREEN.show     = &gameShowPage;
    SCREEN.tap      = &gameTap;
    SCREEN.long_tap = &gameLongTap;
    SCREEN.release  = &gameRelease;

    SCREEN.init();
    SCREEN.show();
}

static void setupLayout() {
    kFontSize = 32;
    chooser_cols = 5;
    chooser_rows = 5;

    SetPanelType(PANEL_ENABLED);
    ClearScreen();
    DrawPanel(NULL, "", "", 0);

    mainlayout.menubtn_size = PanelHeight();
    mainlayout.control_size = ScreenWidth()/12;
    mainlayout.chooser_size = ScreenWidth()/8;

    mainlayout.with_statusbar = false;

    mainlayout.menu.starty = 0;
    mainlayout.menu.height = PanelHeight() + 2;

    mainlayout.statusbar.height = kFontSize + 40;
    mainlayout.statusbar.starty = ScreenHeight()-PanelHeight() - mainlayout.statusbar.height;

    mainlayout.buttonpanel.height = mainlayout.control_size + 5;
    mainlayout.buttonpanel.starty = mainlayout.statusbar.starty - mainlayout.buttonpanel.height - 1;

    mainlayout.maincanvas.starty = mainlayout.menu.height + 3;
    mainlayout.maincanvas.height = mainlayout.buttonpanel.starty - mainlayout.maincanvas.starty - 1;
}

static void setupApp() {
    setupLayout();
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
    else if (event_type == EVT_POINTERUP) {
        SCREEN.release(param_one, param_two);
    }
    return 0;
}

int main (int argc, char* argv[]) {
    InkViewMain(main_handler);
    return 0;
}

