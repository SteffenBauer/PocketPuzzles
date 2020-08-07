#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "game.h"

static void drawStatusBar(char *text) {
    FillArea(0, mainlayout.statusbar.starty+2, ScreenWidth(), mainlayout.statusbar.height-2, 0x00FFFFFF);
    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(10, mainlayout.statusbar.starty+8, ScreenWidth(), kFontSize, text, ALIGN_LEFT);
    PartialUpdate(0, mainlayout.statusbar.starty+2, ScreenWidth(), mainlayout.statusbar.height-2);
    CloseFont(font);
}

void gameMenuHandler(int index) {
    gameMenu_selectedIndex = index;
    switch (index) {
        case 101:
            drawStatusBar("Selected 'New game'");
            break;
        case 102:
            drawStatusBar("Selected 'Restart game'");
            break;
        case 103:
            drawStatusBar("Selected 'Show solution'");
            break;
        case 104:
            drawStatusBar("Selected 'How to play'");
            break;
        case 199:
            exitApp();
            break;
        default:
            break;
    }
    button_to_normal(&btn_game, true);
};

void typeMenuHandler(int index) {
    typeMenu_selectedIndex = index;
    char buf[256];
    switch (index) {
        case 201 ... 205:
            snprintf(buf, 200, "Selected type: '%s'", typeMenu[index-200].text);
            drawStatusBar(buf);
            break;
        default:
            break;
    }
    button_to_normal(&btn_type, true);
};

static void gameBuildTypeMenu() {
    typeMenu = malloc(7 * sizeof(imenu));

    typeMenu[0].type = ITEM_HEADER;
    typeMenu[0].index = 0;
    typeMenu[0].text = "Game presets";
    typeMenu[0].submenu = NULL;

    typeMenu[1].type = ITEM_ACTIVE;
    typeMenu[1].index = 201;
    typeMenu[1].text = "6x6 Normal";
    typeMenu[1].submenu = NULL;

    typeMenu[2].type = ITEM_ACTIVE;
    typeMenu[2].index = 202;
    typeMenu[2].text = "6x6 Unreasonable";
    typeMenu[2].submenu = NULL;

    typeMenu[3].type = ITEM_ACTIVE;
    typeMenu[3].index = 203;
    typeMenu[3].text = "9x9 Normal";
    typeMenu[3].submenu = NULL;

    typeMenu[4].type = ITEM_ACTIVE;
    typeMenu[4].index = 204;
    typeMenu[4].text = "12x12 Unreasonable";
    typeMenu[4].submenu = NULL;

    typeMenu[5].type = ITEM_ACTIVE;
    typeMenu[5].index = 205;
    typeMenu[5].text = "Custom";
    typeMenu[5].submenu = NULL;

    typeMenu[6].type = 0;
    typeMenu[6].index = 0;
    typeMenu[6].text = NULL;
    typeMenu[6].submenu = NULL;

};

void gameTap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;

    if (coord_in_button(x, y, &btn_back)) button_to_tapped(&btn_back);
    if (coord_in_button(x, y, &btn_game)) button_to_tapped(&btn_game);
    if (coord_in_button(x, y, &btn_type)) button_to_tapped(&btn_type);
    if (coord_in_button(x, y, &btn_swap)) button_to_tapped(&btn_swap);
    if (coord_in_button(x, y, &btn_undo)) button_to_tapped(&btn_undo);
    if (coord_in_button(x, y, &btn_redo)) button_to_tapped(&btn_redo);
}

void gameLongTap(int x, int y) {
}

void gameRelease(int x, int y) {
    int i;
    if (coord_in_button(init_tap_x, init_tap_y, &btn_back)) button_to_normal(&btn_back, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_game)) button_to_normal(&btn_game, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_type)) button_to_normal(&btn_type, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_swap)) button_to_normal(&btn_swap, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_undo)) button_to_normal(&btn_undo, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_redo)) button_to_normal(&btn_redo, true);

    if (release_button(x, y, &btn_back)) {
        gameExitPage();
        switchToChooser();
    }
    if (release_button(x, y, &btn_game)) {
        OpenMenuEx(gameMenu, gameMenu_selectedIndex, ScreenWidth()-20-(2*mainlayout.menubtn_size), mainlayout.menubtn_size+2, gameMenuHandler);
    }
    if (release_button(x, y, &btn_type)) {
        OpenMenu(typeMenu, typeMenu_selectedIndex, ScreenWidth()-10-mainlayout.menubtn_size, mainlayout.menubtn_size+2, typeMenuHandler);
    }
}

static void gameDrawControlButtons() {
    FillArea(0, mainlayout.buttonpanel.starty, ScreenWidth(), mainlayout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, mainlayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

    btn_swap.active = true;
    button_to_normal(&btn_swap, false);
    btn_undo.active = true;
    button_to_normal(&btn_undo, false);
    btn_redo.active = true;
    button_to_normal(&btn_redo, false);
}

static void gameDrawMenu() {
    FillArea(0, mainlayout.menu.starty, ScreenWidth(), mainlayout.menu.height, 0x00FFFFFF);
    FillArea(0, mainlayout.menu.starty + mainlayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&btn_back, false);
    button_to_normal(&btn_game, false);
    button_to_normal(&btn_type, false);

    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(0, (mainlayout.menubtn_size/2)-(kFontSize/2), ScreenWidth(), kFontSize, "GAME", ALIGN_CENTER);
    CloseFont(font);
}

static void gameSetupMenuButtons() {
    btn_back.active = true;
    btn_back.posx = 10;
    btn_back.posy = mainlayout.menu.starty;
    btn_back.size = mainlayout.menubtn_size;

    btn_game.active = true;
    btn_game.posx = ScreenWidth() - 20 - (2*mainlayout.menubtn_size);
    btn_game.posy = mainlayout.menu.starty;
    btn_game.size = mainlayout.menubtn_size;

    btn_type.active = true;
    btn_type.posx = ScreenWidth() - 10 - mainlayout.menubtn_size;
    btn_type.posy = mainlayout.menu.starty;
    btn_type.size = mainlayout.menubtn_size;
}

static void gameSetupControlButtons() {

    btn_swap.active = true;
    btn_swap.posx = gamecontrol_padding;
    btn_swap.posy = mainlayout.buttonpanel.starty + 2;
    btn_swap.size = mainlayout.control_size;

    btn_undo.active = true;
    btn_undo.posx = 2*gamecontrol_padding + mainlayout.control_size;
    btn_undo.posy = mainlayout.buttonpanel.starty + 2;
    btn_undo.size = mainlayout.control_size;

    btn_redo.active = true;
    btn_redo.posx = 3*gamecontrol_padding + 2*mainlayout.control_size;
    btn_redo.posy = mainlayout.buttonpanel.starty + 2;
    btn_redo.size = mainlayout.control_size;
}

static void gameSetupStatusBar() {
    FillArea(0, mainlayout.statusbar.starty, ScreenWidth(), mainlayout.statusbar.height, 0x00FFFFFF);
    FillArea(0, mainlayout.statusbar.starty, ScreenWidth(), 1, 0x00000000);
}

static void gameExitPage() {
    free(typeMenu);
}

void gameShowPage() {
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    gameDrawMenu();
    gameDrawControlButtons();
    gameSetupStatusBar();
    FullUpdate();
}

void gameInit() {
    gamecontrol_num = 3;
    gamecontrol_padding = (ScreenWidth()-(gamecontrol_num*mainlayout.control_size))/(gamecontrol_num+1);
    gameSetupMenuButtons();
    gameSetupControlButtons();
    gameBuildTypeMenu();
}

