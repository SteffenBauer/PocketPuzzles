#ifndef POCKETPUZZLES_CHOOSER_HEADER
#define POCKETPUZZLES_CHOOSER_HEADER
#include "common.h"

struct layout chooserlayout;
int current_chooserpage;
int chooser_lastpage;

static int chooser_cols = 5;
static int chooser_rows = 5;

static int control_num = 2;
static int control_padding;
static int chooser_padding;

ifont *chooserfont;
extern ibitmap icon_home, icon_home_tap, icon_redraw, icon_redraw_tap,
               bt_prev, bt_next;

static BUTTON btn_home = { false, BTN_MENU, 0, 0, 0, 0, ' ', &icon_home, &icon_home_tap, NULL, NULL};
static BUTTON btn_draw = { false, BTN_MENU, 0, 0, 0, 0, ' ', &icon_redraw, &icon_redraw_tap, NULL, NULL};
static BUTTON btn_prev = { false, BTN_CTRL, 0, 0, 0, 0, ' ', &bt_prev, NULL, NULL, NULL};
static BUTTON btn_next = { false, BTN_CTRL, 0, 0, 0, 0, ' ', &bt_next, NULL, NULL, NULL};

static int num_games;
static BUTTON *btn_chooser;

static void chooserSetupMenuButtons();
static void chooserSetupChooserButtons();
static void chooserSetupControlButtons();

static void chooserDrawMenu();
static void chooserDrawChooserButtons(int page);
static void chooserDrawControlButtons(int page);

extern void gameInit(const struct game *thegame);

void chooserInit();
void chooserExit();
void chooserShowPage();

void chooserTap(int x, int y);
void chooserLongTap(int x, int y);
void chooserDrag(int x, int y);
void chooserRelease(int x, int y);
void chooserPrev();
void chooserNext();

#endif
