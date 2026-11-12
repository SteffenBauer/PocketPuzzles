#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "inkview.h"
#include "frontend/main.h"

#ifdef PB_FW4
// Firmware 4.x compatibility: SDK 4.x headers lack these identifiers.
#define IV_KEY_HOME KEY_HOME
#define IV_KEY_PREV KEY_PREV
#define IV_KEY_NEXT KEY_NEXT
#define APP_CAPABILITY_SUPPORT_SCREEN_INVERSION 0   // unused: requires FW >= 6.8
#define EVT_SCREEN_INVERSION_MODE_CHANGED (-1)      // never sent on FW 4
#define MIN_FW_MAJOR 4
#else
#define MIN_FW_MAJOR 5
#endif

#define STR_(x) #x
#define STR(x) STR_(x)

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

static bool setupAppCapabilities(bool *canInvertScreen) {
    const char *firmware_version = GetSoftwareVersion();
    char *ptr;
    int major, minor;

    ptr = strchr(firmware_version, '.');
    sscanf(ptr+1, "%i.%i", &major, &minor);
    if (major < MIN_FW_MAJOR) {
        Message(ICON_WARNING, "", "This app only runs under firmware version STR(MIN_FW_MAJOR) or higher!", 2000);
        return false;
    }
    if ((major >= 6) && (minor >= 8) && IvSetAppCapability) {
        IvSetAppCapability(APP_CAPABILITY_SUPPORT_SCREEN_INVERSION);
        *canInvertScreen = true;
    }
    else {
        *canInvertScreen = false;
    }
    return true;
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
    bool canInvertScreen;

    if (event_type == EVT_INIT) {
        if (setupAppCapabilities(&canInvertScreen))
            setupApp();
        else
            CloseApp();
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
    else if (canInvertScreen && (event_type == EVT_SCREEN_INVERSION_MODE_CHANGED)) {
        FullUpdate();
    }
    return 0;
}

int main (int argc, char* argv[]) {
    InkViewMain(main_handler);
    return 0;
}

