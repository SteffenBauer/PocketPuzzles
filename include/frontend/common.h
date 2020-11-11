#ifndef POCKETPUZZLES_COMMON_HEADER
#define POCKETPUZZLES_COMMON_HEADER
#include "puzzles.h"

typedef struct panel {
    int starty;
    int height;
} PANEL;

typedef enum {
    LAYOUT_FULL,
    LAYOUT_STATUSBAR,
    LAYOUT_BUTTONBAR,
    LAYOUT_BOTH,
    LAYOUT_2XBUTTONBAR,
    LAYOUT_2XBOTH
} LAYOUTTYPE;

struct layout {
    bool with_statusbar;
    bool with_buttonbar;
    bool with_2xbuttonbar;

    int menubtn_size;
    int control_size;
    int chooser_size;

    PANEL menu;
    PANEL maincanvas;
    PANEL buttonpanel;
    PANEL statusbar;
};

typedef enum {
    BTN_MENU,
    BTN_CHOOSER,
    BTN_CTRL,
    BTN_CHAR,
    BTN_ITEM,
    BTN_NULL
} BUTTONTYPE;

typedef enum {
    ACTION_HOME,
    ACTION_DRAW,
    ACTION_MENU,
    ACTION_NEXT,
    ACTION_PREV,
    ACTION_LAUNCH,
    ACTION_BACK,
    ACTION_GAME,
    ACTION_TYPE,
    ACTION_UNDO,
    ACTION_REDO,
    ACTION_SWAP,
    ACTION_CTRL
} BUTTONACTION;

typedef struct button {
    bool active;
    BUTTONTYPE type;
    int posx; int posy; int size; int page;
    BUTTONACTION action;
    union {
        char c;
        const struct game *thegame;
    } actionParm;
    ibitmap *bitmap;
    ibitmap *bitmap_tap;
    ibitmap *bitmap_disabled;
} BUTTON;

typedef struct gameinfo {
    ibitmap *bitmap;
    const struct game *thegame;
} GAMEINFO;

bool coord_in_button(int x, int y, BUTTON *button);
bool release_button(int x, int y, BUTTON *button);
void button_to_normal(BUTTON *button, bool update);
void button_to_tapped(BUTTON *button);
void button_to_cleared(BUTTON *button, bool update);
void activate_button(BUTTON *button);
void deactivate_button(BUTTON *button);
static void draw_buttonchar(BUTTON *button);

int init_tap_x;
int init_tap_y;

struct layout getLayout(LAYOUTTYPE screenlayout);

extern void switchToChooserScreen();
extern void switchToGameScreen();
extern void switchToParamScreen();
extern void exitApp();

#endif

