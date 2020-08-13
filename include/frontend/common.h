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
    BTN_NULL
} BUTTONTYPE;

typedef struct button {
    bool active;
    BUTTONTYPE type;
    int posx;
    int posy;
    int page;
    int size;
    ibitmap *bitmap;
    ibitmap *bitmap_tap;
    ibitmap *bitmap_disabled;
    const struct game *thegame;
} BUTTON;

bool coord_in_button(int x, int y, BUTTON *button);
bool release_button(int x, int y, BUTTON *button);
void button_to_normal(BUTTON *button, bool update);
void button_to_tapped(BUTTON *button);
void button_to_cleared(BUTTON *button, bool update);
void activate_button(BUTTON *button);
void deactivate_button(BUTTON *button);

int init_tap_x;
int init_tap_y;

struct layout getLayout(LAYOUTTYPE screenlayout);

extern void switchToChooser();
extern void switchToGame(const struct game *thegame);
extern void exitApp();

#endif

