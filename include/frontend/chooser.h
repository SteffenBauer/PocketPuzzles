#ifndef POCKETPUZZLES_CHOOSER_HEADER
#define POCKETPUZZLES_CHOOSER_HEADER
#include "common.h"

#define CHOOSER_COLS 5
#define CHOOSER_ROWS 5
#define CONTROL_NUM 2

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

    int btnHomeIDX;
    int btnDrawIDX;
    int btnPrevIDX;
    int btnNextIDX;
} ca;

extern ibitmap icon_home, icon_home_tap, icon_redraw, icon_redraw_tap,
               bt_prev, bt_next;

static void chooserSetupButtons();

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
