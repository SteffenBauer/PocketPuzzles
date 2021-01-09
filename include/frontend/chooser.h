#ifndef POCKETPUZZLES_CHOOSER_HEADER
#define POCKETPUZZLES_CHOOSER_HEADER
#include "common.h"

#define CHOOSER_COLS 4
#define CHOOSER_ROWS 5
#define CONTROL_NUM 2
#define CFONTSIZE 36

bool chooserInitialized;

struct chooserAttributes {
    struct layout chooserlayout;
    int current_chooserpage;
    int chooser_lastpage;

    ifont *chooserfont;

    int num_games;
    int numChooserButtons;
    BUTTON *chooserButton;

    int control_padding;
    int chooser_padding;

    int chooser_cols;
    int chooser_rows;

    int btnHomeIDX;
    int btnDrawIDX;
    int btnMenuIDX;
    int btnPrevIDX;
    int btnNextIDX;
} ca;

extern ibitmap icon_home, icon_home_tap, icon_redraw, icon_redraw_tap,
               bt_west, bt_east, ic_star,
               icon_menu, icon_menu_tap,
               menu_new, menu_star,
               menu_settings, menu_restart, menu_help, menu_reset;

static imenuex chooserMenu[] = {
    { ITEM_HEADER,   0, "Puzzles",            NULL, NULL,           NULL, NULL },
    /* { ITEM_ACTIVE, 101, "Settings",           NULL, &menu_settings, NULL, NULL }, */
    { ITEM_ACTIVE, 102, "Resume last game",   NULL, &menu_restart,  NULL, NULL },
    { ITEM_ACTIVE, 103, "Reset presets",      NULL, &menu_reset,    NULL, NULL },
    { ITEM_ACTIVE, 104, "About",              NULL, &menu_help,     NULL, NULL },
    { 0, 0, NULL, NULL, NULL, NULL, NULL }
};

static void chooserSetupButtons();

static void chooserDrawMenu();
static void chooserDrawChooserButtons(int page);
static void chooserDrawControlButtons(int page);

void chooserRefreshCanvas();
void chooserScreenInit();
void chooserScreenShow();
void chooserScreenFree();

void chooserTap(int x, int y);
void chooserLongTap(int x, int y);
void chooserDrag(int x, int y);
void chooserRelease(int x, int y);
void chooserPrev();
void chooserNext();
void chooserSerialise();

extern void gameSetGame(const struct game *thegame);
extern void gameStartNewGame();
extern bool gameResumeGame();

extern void stateSetFavorite(const char *name);
extern void stateUnsetFavorite(const char *name);
extern bool stateIsFavorite(const char *name);
extern void configAddItem(char *key, char *value);
extern void configDel();

#endif
