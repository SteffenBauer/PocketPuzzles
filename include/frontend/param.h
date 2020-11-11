#ifndef POCKETPUZZLES_PARAM_HEADER
#define POCKETPUZZLES_PARAM_HEADER
#include "common.h"

#define PFONTSIZE 36

bool paramInitialized;

struct item_number {
    BUTTON decrease;
    BUTTON increase;
};

struct item_choice {
    int num_choices;
    char **choices;
    imenu *choiceMenu;
    int selected;
};

struct item_bool {
    BUTTON indicator;
};


typedef struct gameparamitem {
    ibitmap *bitmap;
    int y;
    int type;
    union {
        struct item_number n;
        struct item_choice c;
        struct item_bool   b;
    } item;
} PARAMITEM;

struct paramparams {
    struct layout paramlayout; 
    
    int numParamButtons;
    BUTTON *paramButton;

    int numParams;
    PARAMITEM *paramItem;
    config_item *cfg;

    int btnBackIDX;
    int btnDrawIDX;

    ifont *paramfont;

    midend *me;
    char *title;
} pa;

extern ibitmap cfg_yes, cfg_no, cfg_incr, cfg_incr_tap, cfg_decr, cfg_decr_tap,
               icon_back, icon_back_tap, icon_redraw, icon_redraw_tap,
               cfg_difficulty;

static bool coord_in_choice(int x, int y, int i);
static void choice_to_normal(int i);
static void choice_to_tapped(int i);

static void paramSubmitParams();
static void paramSetItemNum(int i, int n);
static void paramDecreaseItem(int i);
static void paramIncreaseItem(int i);
static void paramSwapItem(int i);

static void paramDrawMenu();
static void paramDrawPanel(bool inverse);
static void paramDrawParams();

static void paramFree();

void paramScreenInit();
void paramScreenShow();
void paramScreenFree();

void paramTap(int x, int y);
void paramLongTap(int x, int y);
void paramDrag(int x, int y);
void paramRelease(int x, int y);
void paramPrev();
void paramNext();

extern void gameStartNewGame();
extern void gameScreenShow();

#endif

