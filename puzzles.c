#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"

static const int kFontSize = 32;

static int icon_size;
static int button_size;

extern ibitmap icon_123, icon_123_tap, icon_game, icon_game_tap,
               icon_help, icon_home, icon_home_tap, icon_new,
               icon_redo, icon_redo_tap, icon_redo_disabled, icon_restart, icon_solve,
               icon_swap, icon_swap_tap, icon_type, icon_type_tap,
               icon_undo, icon_undo_tap, icon_undo_disabled;

typedef struct button {
    int posx;
    int posy;
    int size;
    ibitmap *bitmap;
    ibitmap *bitmap_tap;
    ibitmap *bitmap_disabled;
} BUTTON;

BUTTON bt_home;
BUTTON bt_game;
BUTTON bt_type;

BUTTON bt_swap;
BUTTON bt_undo;
BUTTON bt_redo;

static int init_tap_x = 0;
static int init_tap_y = 0;

/*
static void tap(int x, int y);
static void long_tap(int x, int y);
static void release(int x, int y);
static void drawTop();
static void drawStatusBar(char *text);
*/
char key_buffer[64];

static void exitApp();

/*
 * GUI handling
 */

/* Layout */

int start_y_top;
int height_top;
int start_y_game;
int height_game;
int start_y_bt;
int height_bt;
int start_y_stat;
int height_stat;

static void setupLayout() {
    SetPanelType(PANEL_ENABLED);
    ClearScreen();
    DrawPanel(NULL, "", "", 0);

    start_y_top = 0;
    height_top   = PanelHeight()+2;

    start_y_stat = ScreenHeight()-PanelHeight()-kFontSize-40;
    height_stat   = kFontSize+40;

    height_bt = ScreenWidth()/10 + 5;
    start_y_bt = start_y_stat - height_bt;

    start_y_game = PanelHeight()+3;
    height_game = start_y_bt - start_y_game - 2;
}

static void drawGameCanvas() {
    FillArea(0, start_y_game, ScreenWidth(), height_game, 0x00000000);
    PartialUpdate(0, start_y_game, ScreenWidth(), height_game);
}

/* Buttons */

static void setupButtons() {
    int button_padding;
    int button_num = 3;

    icon_size = PanelHeight();
    button_size = ScreenWidth()/10;
    button_padding = (ScreenWidth()-(button_num*button_size))/(button_num+1);

    bt_home.posx = 10;
    bt_home.posy = start_y_top;
    bt_home.size = icon_size;
    bt_home.bitmap          = &icon_home;
    bt_home.bitmap_tap      = &icon_home_tap;
    bt_home.bitmap_disabled = NULL;

    bt_game.posx = ScreenWidth() - 20 - (2*icon_size);
    bt_game.posy = start_y_top;
    bt_game.size = icon_size;
    bt_game.bitmap          = &icon_game;
    bt_game.bitmap_tap      = &icon_game_tap;
    bt_game.bitmap_disabled = NULL;

    bt_type.posx = ScreenWidth() - 10 - icon_size;
    bt_type.posy = start_y_top;
    bt_type.size = icon_size;
    bt_type.bitmap          = &icon_type;
    bt_type.bitmap_tap      = &icon_type_tap;
    bt_type.bitmap_disabled = NULL;

    bt_swap.posx = button_padding;
    bt_swap.posy = start_y_bt+2;
    bt_swap.size = button_size;
    bt_swap.bitmap          = &icon_swap;
    bt_swap.bitmap_tap      = &icon_swap_tap;
    bt_swap.bitmap_disabled = NULL;

    bt_undo.posx = 2*button_padding+button_size;
    bt_undo.posy = start_y_bt+2;
    bt_undo.size = button_size;
    bt_undo.bitmap          = &icon_undo;
    bt_undo.bitmap_tap      = &icon_undo_tap;
    bt_undo.bitmap_disabled = &icon_undo_disabled;

    bt_redo.posx = 3*button_padding+2*button_size;
    bt_redo.posy = start_y_bt+2;
    bt_redo.size = button_size;
    bt_redo.bitmap          = &icon_redo;
    bt_redo.bitmap_tap      = &icon_redo_tap;
    bt_redo.bitmap_disabled = &icon_redo_disabled;
}

static bool coord_in_button(int x, int y, BUTTON *button) {
    return ((x>=button->posx) && (x<(button->posx+button->size)) && (y>=button->posy) && (y<(button->posy+button->size)));
}

static void button_to_normal(BUTTON *button) {
    StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}

static void button_to_tapped(BUTTON *button) {
    StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap_tap, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}

static void button_to_disabled(BUTTON *button) {
    StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap_disabled, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}

void drawButtons() {
    FillArea(0, start_y_bt, ScreenWidth(), height_bt, 0x00FFFFFF);
    FillArea(0, start_y_bt+1, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&bt_swap);
    button_to_normal(&bt_undo);
    button_to_normal(&bt_redo);

    PartialUpdate(0, start_y_bt, ScreenWidth(), height_bt);
}

/* Configuration handling */

static iconfig *guichcfg = NULL;
static char *cfg_difficulties[] = {"Easy", "Normal", "Tricky", NULL};
static iconfigedit guichce[] = {
    {CFG_INDEX,NULL             ,"Difficulty"       ,NULL     ,"game.difficulty","0"        ,cfg_difficulties        ,NULL},
    {0}
};

static void config_done() {
}

/* Status bar */

static void drawStatusBar(char *text) {
    ifont *font;

    FillArea(0, start_y_stat, ScreenWidth(), kFontSize+40, 0x00FFFFFF);
    FillArea(0, ScreenHeight()-PanelHeight()-kFontSize-38, ScreenWidth(), 2, 0x00000000);
    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(10, ScreenHeight()-PanelHeight()-kFontSize-32, ScreenWidth(), kFontSize, text, ALIGN_LEFT);
    PartialUpdate(0, ScreenHeight()-PanelHeight()-kFontSize-40, ScreenWidth(), kFontSize+40);
    CloseFont(font);
}

/* Menus */

static imenuex gameMenu[] = {
    { ITEM_HEADER,   0, "Game", NULL, NULL, NULL, NULL },
    { ITEM_ACTIVE, 101, "New", NULL, &icon_new, NULL, NULL },
    { ITEM_ACTIVE, 102, "Restart", NULL, &icon_restart, NULL, NULL },
    { ITEM_ACTIVE, 103, "Show Solution", NULL, &icon_solve, NULL, NULL },
    { ITEM_ACTIVE, 104, "How to play", NULL, &icon_help, NULL, NULL },
    { ITEM_ACTIVE, 199, "Quit app", NULL, &icon_home, NULL, NULL },
    { 0, 0, NULL, NULL, NULL, NULL, NULL }
};
static int gameMenu_selectedIndex = 1;

static imenu *typeMenu;

void buildTypeMenu() {
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
static int typeMenu_selectedIndex = 1;

void mykeyboardHandler(char *text) {
    char buf[256];
    sprintf(buf, "Entered value: '%s'", text);
    drawStatusBar(buf);
}

void gameMenuHandler(int index) {
    gameMenu_selectedIndex = index;
    switch (index) {
        case 101:
            /* OpenConfigEditor("Configuration", guichcfg, guichce, config_done, NULL); */
            /* OpenKeyboard("Enter value", key_buffer, 3, KBD_NUMERIC, mykeyboardHandler); */
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
    button_to_normal(&bt_game);
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
    button_to_normal(&bt_type);
};

static void tap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;
    if (coord_in_button(x, y, &bt_home)) button_to_tapped(&bt_home);
    if (coord_in_button(x, y, &bt_game)) button_to_tapped(&bt_game);
    if (coord_in_button(x, y, &bt_type)) button_to_tapped(&bt_type);

    if (coord_in_button(x, y, &bt_swap)) button_to_tapped(&bt_swap);
    if (coord_in_button(x, y, &bt_undo)) button_to_tapped(&bt_undo);
    if (coord_in_button(x, y, &bt_redo)) button_to_tapped(&bt_redo);

}

static void long_tap(int x, int y) {
}

static void release(int x, int y) {
    if (coord_in_button(init_tap_x, init_tap_y, &bt_home)) button_to_normal(&bt_home);
    if (coord_in_button(init_tap_x, init_tap_y, &bt_game)) button_to_normal(&bt_game);
    if (coord_in_button(init_tap_x, init_tap_y, &bt_type)) button_to_normal(&bt_type);

    if (coord_in_button(init_tap_x, init_tap_y, &bt_swap)) button_to_normal(&bt_swap);
    if (coord_in_button(init_tap_x, init_tap_y, &bt_undo)) button_to_normal(&bt_undo);
    if (coord_in_button(init_tap_x, init_tap_y, &bt_redo)) button_to_normal(&bt_redo);

    if (coord_in_button(init_tap_x, init_tap_y, &bt_home) &&
        coord_in_button(x, y, &bt_home)) {
        exitApp();
    }

    if (coord_in_button(init_tap_x, init_tap_y, &bt_game) &&
        coord_in_button(x, y, &bt_game)) {
        OpenMenuEx(gameMenu, gameMenu_selectedIndex, ScreenWidth()-20-(2*icon_size), icon_size+2, gameMenuHandler);
    }

    if (coord_in_button(init_tap_x, init_tap_y, &bt_type) &&
        coord_in_button(x, y, &bt_type)) {
        OpenMenu(typeMenu, typeMenu_selectedIndex, ScreenWidth()-10-icon_size, icon_size+2, typeMenuHandler);
    }

    if (coord_in_button(init_tap_x, init_tap_y, &bt_swap) &&
        coord_in_button(x, y, &bt_swap)) {
        drawStatusBar("Swap short/long");
    }
    if (coord_in_button(init_tap_x, init_tap_y, &bt_undo) &&
        coord_in_button(x, y, &bt_undo)) {
        drawStatusBar("Undo");
    }
    if (coord_in_button(init_tap_x, init_tap_y, &bt_redo) &&
        coord_in_button(x, y, &bt_redo)) {
        drawStatusBar("Redo");
    }

}

static void drawTop() {
    ifont *font;

    FillArea(0, start_y_top, ScreenWidth(), height_top, 0x00FFFFFF);
    FillArea(0, start_y_top+height_top-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&bt_home);
    button_to_normal(&bt_game);
    button_to_normal(&bt_type);

    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(20+icon_size, (icon_size/2)-(3*kFontSize/4), ScreenWidth()-40-(2*icon_size), kFontSize, "PUZZLES", ALIGN_LEFT);
    CloseFont(font);
}

static void setupApp() {
    setupLayout();
    setupButtons();

    /* guichcfg = OpenConfig(USERDATA "/guidemo.cfg", guichce); */


    buildTypeMenu();
    drawTop();

    // FillArea(50, icon_size+3+50, 120, 120, 0x00A0A0A0);

    drawStatusBar("");
    drawButtons();
    drawGameCanvas();

    FullUpdate();
}

static void exitApp() {
    free(typeMenu);
    CloseApp();
}

static int main_handler(int event_type, int param_one, int param_two) {
    if (event_type == EVT_INIT) {
        setupApp();
    }
    else if (event_type == EVT_EXIT || (event_type == EVT_HIDE)) {
        exitApp();
    }
    else if (event_type == EVT_POINTERDOWN) {
        tap(param_one, param_two);
    }
    else if (event_type == EVT_POINTERLONG) {
        long_tap(param_one, param_two);
    }
    else if (event_type == EVT_POINTERUP) {
        release(param_one, param_two);
    }
    return 0;
}

int main (int argc, char* argv[]) {
    InkViewMain(main_handler);
    return 0;
}
