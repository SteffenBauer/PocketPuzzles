#ifndef POCKETPUZZLES_GAME_HEADER
#define POCKETPUZZLES_GAME_HEADER
#include "common.h"
#include "puzzles.h"

typedef struct {
  int x;
  int y;
} MWPOINT;

struct frontend {
  struct layout gamelayout;
  int width;
  int height;
  int xoffset;
  int yoffset;
  bool do_partial;
  irect cliprect;
  const char *statustext;
  struct timeval last_time;
  int time_int;
  int isTimer;
  game_params *pparams;
  int ncolours;
  float *colours;
};

struct blitter {
  int width;
  int height;
  ibitmap *ibit;
};

const struct game *currentgame;
frontend *fe;
midend *me;
drawing *dr;
struct preset_menu *presets;

static int gamecontrol_padding;
static int gamecontrol_num;

extern ibitmap icon_back, icon_back_tap, icon_redraw, icon_redraw_tap,
               icon_game, icon_game_tap, icon_type, icon_type_tap,
               menu_exit, menu_help, menu_new, menu_restart, menu_solve,
               bt_add, bt_backspace, bt_bridges_g, bt_fill, bt_fill_undead, bt_guess_i,
               bt_redo, bt_redo_d, bt_remove, bt_map_c, bt_map_j, bt_net_shuffle, 
               bt_salad_o, bt_salad_x, bt_undead_g, bt_undead_v, bt_undead_z,
               bt_swap, bt_undo, bt_undo_d;

static BUTTON btn_back = { false, BTN_MENU, 0, 0, 0, 0, &icon_back, &icon_back_tap, NULL, NULL};
static BUTTON btn_draw = { false, BTN_MENU, 0, 0, 0, 0, &icon_redraw, &icon_redraw_tap, NULL, NULL};
static BUTTON btn_game = { false, BTN_MENU, 0, 0, 0, 0, &icon_game, &icon_game_tap, NULL, NULL};
static BUTTON btn_type = { false, BTN_MENU, 0, 0, 0, 0, &icon_type, &icon_type_tap, NULL, NULL};

static BUTTON btn_swap = { false, BTN_CTRL, 0, 0, 0, 0, &bt_swap, NULL, NULL, NULL};
static BUTTON btn_undo = { false, BTN_CTRL, 0, 0, 0, 0, &bt_undo, NULL, &bt_undo_d, NULL};
static BUTTON btn_redo = { false, BTN_CTRL, 0, 0, 0, 0, &bt_redo, NULL, &bt_redo_d, NULL};

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

static void gameExitPage();

void gameInit(const struct game *thegame);
void gameShowPage();
void gamePrepare();
LAYOUTTYPE gameGetLayout();

void gameTap(int x, int y);
void gameLongTap(int x, int y);
void gameDrag(int x, int y);
void gameRelease(int x, int y);


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

