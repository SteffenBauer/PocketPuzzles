#ifndef POCKETPUZZLES_GAME_HEADER
#define POCKETPUZZLES_GAME_HEADER
#include "common.h"
#include "puzzles.h"

bool gameInitialized;

typedef struct {
  int x;
  int y;
} MWPOINT;

struct frontend {
  const struct game *currentgame;
  struct layout gamelayout; /* Dimensions of game panels */
  int width;                /* Width of main game canvas */
  int height;               /* Height of main game canvas */
  int xoffset;              /* Main game canvas offset in X-direction */
  int yoffset;              /* Main game canvas offset in Y-direction */

  int numGameButtons;       /* Number of menu + control buttons */
  BUTTON *gameButton;       /* Array of game buttons */

  int btnSwapIDX;
  int btnUndoIDX;
  int btnRedoIDX;
  int btnBackIDX;
  int btnDrawIDX;
  int btnGameIDX;
  int btnTypeIDX;

  bool with_twoctrllines;   /* Game needs two lines of control buttons */
  bool with_statusbar;      /* Game needs the statusbar */
  bool with_rightpointer;   /* Game operates with right/long-click */
  bool swapped;             /* Indicates if left/right click is swapped */

  int current_pointer;      /* Current pointer type (left/ right) */
  int pointerdown_x;        /* Coordinates of initial pointer down event */
  int pointerdown_y;

  bool finished;            /* Whether game was finished (for showing finish message) */
  irect cliprect;           /* Initial screen clip rectangle upon game init */

  const char *statustext;   /* Currently shown status text (needed for screen redraw) */

  struct timeval last_time;
  int time_int;
  bool isTimer;

  game_params *pparams;
  int ncolours;
  float *colours;
  ifont *gamefont;
};

struct blitter {
  int width;
  int height;
  ibitmap *ibit;
};

static frontend *fe;
static midend *me;
static drawing *dr;
struct preset_menu *presets;

extern ibitmap icon_back, icon_back_tap, icon_redraw, icon_redraw_tap,
               icon_game, icon_game_tap, icon_type, icon_type_tap,
               menu_exit, menu_help, menu_new, menu_restart, menu_solve,
               bt_add, bt_backspace, bt_bridges_g, bt_fill, bt_fill_undead, bt_guess_i,
               bt_redo, bt_redo_d, bt_remove, bt_map_c, bt_map_j, bt_net_shuffle, 
               bt_salad_o, bt_salad_x, bt_net_shuffle, bt_bridges_g,
               bt_west, bt_east, bt_north, bt_south,
               bt_swap, bt_undo, bt_undo_d;

static BUTTON btn_back = { true, BTN_MENU, 0, 0, 0, 0, ACTION_BACK, ' ', &icon_back, &icon_back_tap, NULL};
static BUTTON btn_draw = { true, BTN_MENU, 0, 0, 0, 0, ACTION_DRAW, ' ', &icon_redraw, &icon_redraw_tap, NULL};
static BUTTON btn_game = { true, BTN_MENU, 0, 0, 0, 0, ACTION_GAME, ' ', &icon_game, &icon_game_tap, NULL};
static BUTTON btn_type = { true, BTN_MENU, 0, 0, 0, 0, ACTION_TYPE, ' ', &icon_type, &icon_type_tap, NULL};

static BUTTON btn_backspace = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, '\b', &bt_backspace, NULL, NULL};
static BUTTON btn_add       = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, '+', &bt_add, NULL, NULL};
static BUTTON btn_remove    = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, '-', &bt_remove, NULL, NULL};

static BUTTON btn_swap = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_SWAP, ' ', &bt_swap, NULL, NULL};
static BUTTON btn_undo = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_UNDO, ' ', &bt_undo, NULL, &bt_undo_d};
static BUTTON btn_redo = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_REDO, ' ', &bt_redo, NULL, &bt_redo_d};

static BUTTON btn_salad_o     = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'O', &bt_salad_o, NULL, NULL};
static BUTTON btn_salad_x     = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'X', &bt_salad_x, NULL, NULL};
static BUTTON btn_net_shuffle = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'J', &bt_net_shuffle, NULL, NULL};
static BUTTON btn_bridges_g   = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'G', &bt_bridges_g, NULL, NULL};
static BUTTON btn_rome_w      = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'L', &bt_west, NULL, NULL};
static BUTTON btn_rome_e      = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'R', &bt_east, NULL, NULL};
static BUTTON btn_rome_n      = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'U', &bt_north, NULL, NULL};
static BUTTON btn_rome_s      = { false, BTN_CTRL, 0, 0, 0, 0, ACTION_CTRL, 'D', &bt_south, NULL, NULL};

static imenuex gameMenu[] = {
    { ITEM_HEADER,   0, "Game",          NULL, NULL,          NULL, NULL },
    { ITEM_ACTIVE, 101, "New",           NULL, &menu_new,     NULL, NULL },
    { ITEM_ACTIVE, 102, "Restart",       NULL, &menu_restart, NULL, NULL },
    { ITEM_ACTIVE, 103, "Show Solution", NULL, &menu_solve,   NULL, NULL },
    { ITEM_ACTIVE, 104, "How to play",   NULL, &menu_help,    NULL, NULL },
    { ITEM_ACTIVE, 199, "Quit app",      NULL, &menu_exit,    NULL, NULL },
    { 0, 0, NULL, NULL, NULL, NULL, NULL }
};
static int gameMenu_selectedIndex = 1;
static imenu *typeMenu;
static int typeMenu_selectedIndex = 1;
static void gameBuildTypeMenu();

static void gameSetupMenuButtons();
static void gameSetupControlButtons();
static void gameSetupStatusBar();

static void gameDrawMenu();
static void gameDrawControlButtons();
static void gameDrawStatusBar();

static void checkGameEnd();
static bool coord_in_gamecanvas(int x, int y);
static void gamePrepareFrontend();
static LAYOUTTYPE gameGetLayout();
static void gameDrawFurniture();
static void gameCheckButtonState();

static void gameStartNewGame();
static void gameRestartGame();
static void gameSolveGame();
static void gameSwitchPreset(int index);

void gameScreenInit();
void gameScreenShow();
void gameScreenFree();
void gameSetGame(const struct game *thegame);

void gameTap(int x, int y);
void gameLongTap(int x, int y);
void gameDrag(int x, int y);
void gameRelease(int x, int y);
void gamePrev();
void gameNext();

void ink_draw_text(void *handle, int x, int y, int fonttype, int fontsize,
               int align, int colour, const char *text);
void ink_draw_rect(void *handle, int x, int y, int w, int h, int colour);
void ink_draw_rect_outline(void *handle, int x, int y, int w, int h, int colour);
void ink_draw_line(void *handle, int x1, int y1, int x2, int y2, int colour);
static void extendrow(int y, int x1, int y1, int x2, int y2, int *minxptr, int *maxxptr);
void ink_draw_polygon(void *handle, int *icoords, int npoints,
                  int fillcolour, int outlinecolour);
void ink_draw_circle(void *handle, int cx, int cy, int radius, int fillcolour, int outlinecolour);
void ink_clip(void *handle, int x, int y, int w, int h);
void ink_unclip(void *handle);
void ink_start_draw(void *handle);
void ink_draw_update(void *handle, int x, int y, int w, int h);
void ink_end_draw(void *handle);
void ink_status_bar(void *handle, const char *text);

blitter *ink_blitter_new(void *handle, int w, int h);
void ink_blitter_free(void *handle, blitter *bl);
void ink_blitter_save(void *handle, blitter *bl, int x, int y);
void ink_blitter_load(void *handle, blitter *bl, int x, int y);

#endif

