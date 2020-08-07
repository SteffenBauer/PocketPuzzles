#ifndef POCKETPUZZLES_COMMON_HEADER
#define POCKETPUZZLES_COMMON_HEADER

ifont *font;
int kFontSize;

int chooser_cols;
int chooser_rows;

typedef struct panel {
    int starty;
    int height;
} PANEL;

struct layout {
    bool with_statusbar;

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
    BTN_CTRL
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
    char *title;
    // struct game *thegame;
} BUTTON;

bool coord_in_button(int x, int y, BUTTON *button);
bool release_button(int x, int y, BUTTON *button);
void button_to_normal(BUTTON *button, bool update);
void button_to_tapped(BUTTON *button);
void button_to_cleared(BUTTON *button, bool update);

int init_tap_x;
int init_tap_y;

extern void switchToChooser();
extern void switchToGame();
extern void exitApp();

#endif

