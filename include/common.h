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
    int control_padding;
    int chooser_padding;

    PANEL menu;
    PANEL maincanvas;
    PANEL maincanvas_sb;
    PANEL maincanvas_nosb;
    PANEL buttonpanel;
    PANEL buttonpanel_sb;
    PANEL buttonpanel_nosb;
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
void button_to_normal(BUTTON *button, bool update);
void button_to_tapped(BUTTON *button);
void button_to_cleared(BUTTON *button, bool update);

void exitApp();

int init_tap_x;
int init_tap_y;

#endif

