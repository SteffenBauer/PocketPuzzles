#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <malloc.h>

#include "inkview.h"
#include "pocketpuzzles.h"

static void switchToChooser() {
    mainlayout.with_statusbar     = false;
    mainlayout.maincanvas.starty  = mainlayout.maincanvas_nosb.starty;
    mainlayout.maincanvas.height  = mainlayout.maincanvas_nosb.height;
    mainlayout.buttonpanel.starty = mainlayout.buttonpanel_nosb.starty;
    mainlayout.buttonpanel.height = mainlayout.buttonpanel_nosb.height;

    SCREEN.init     = &chooserInit;
    SCREEN.show     = &chooserShowPage;
    SCREEN.tap      = &chooserTap;
    SCREEN.long_tap = &chooserLongTap;
    SCREEN.release  = &chooserRelease;
}

static void setupLayout() {
    kFontSize = 32;
    chooser_cols = 5;
    chooser_rows = 5;

    SetPanelType(PANEL_ENABLED);
    ClearScreen();
    DrawPanel(NULL, "", "", 0);

    mainlayout.menubtn_size = PanelHeight();
    mainlayout.control_size = ScreenWidth()/10;
    mainlayout.chooser_size = ScreenWidth()/8;

    mainlayout.control_padding = (ScreenWidth()-(control_num*mainlayout.control_size))/(control_num+1);
    mainlayout.chooser_padding = (ScreenWidth()-(chooser_cols*mainlayout.chooser_size))/(chooser_cols+1);
    chooser_lastpage = num_games / (chooser_cols * chooser_rows);

    mainlayout.with_statusbar = false;

    mainlayout.menu.starty = 0;
    mainlayout.menu.height = PanelHeight() + 2;

    mainlayout.statusbar.height = kFontSize + 40;
    mainlayout.statusbar.starty = ScreenHeight()-PanelHeight() - mainlayout.statusbar.height;

    mainlayout.buttonpanel_nosb.height = mainlayout.control_size + 5;
    mainlayout.buttonpanel_nosb.starty = ScreenHeight()-PanelHeight() - mainlayout.buttonpanel_nosb.height;

    mainlayout.buttonpanel_sb.height = mainlayout.control_size + 5;
    mainlayout.buttonpanel_sb.starty = mainlayout.statusbar.starty - mainlayout.buttonpanel_sb.height;

    mainlayout.maincanvas_nosb.starty = mainlayout.menu.height + 3;
    mainlayout.maincanvas_nosb.height = mainlayout.buttonpanel_nosb.starty - mainlayout.maincanvas_nosb.starty;

    mainlayout.maincanvas_sb.starty = mainlayout.menu.height + 3;
    mainlayout.maincanvas_sb.height = mainlayout.buttonpanel_sb.starty - mainlayout.maincanvas_sb.starty;
}

static void setupApp() {
    setupLayout();
    switchToChooser();
    SCREEN.init();
    SCREEN.show();
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

