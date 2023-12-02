#ifndef POCKETPUZZLES_PARAM_HEADER
#define POCKETPUZZLES_PARAM_HEADER
#include "common.h"

bool paramInitialized;

struct item_number {
    BUTTON decrease_more;
    BUTTON decrease;
    BUTTON increase;
    BUTTON increase_more;
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
    bool moreButtons;
    BUTTON *paramButton;

    int numParams;
    PARAMITEM *paramItem;
    config_item *cfg;

    int btnBackIDX;

    ifont *paramfont;
    int pfontsize;

    midend *me;
    int ptype;    /* Parameters or Settings */
    char *title;
} pa;

extern ibitmap cfg_yes, cfg_no, cfg_incr, cfg_incr_tap,
               cfg_decr, cfg_decr_tap,
               cfg_incr_more, cfg_incr_more_tap,
               cfg_decr_more, cfg_decr_more_tap,
               icon_back, icon_back_tap, cfg_difficulty;

static bool coord_in_choice(int x, int y, int i);
static void choice_to_normal(int i);
static void choice_to_tapped(int i);

static void paramSubmitParams();
static void paramSetItemNum(int i, int n);
static void paramDecreaseItem(int i, int a);
static void paramIncreaseItem(int i, int a);
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
extern void gamePrepareFrontend();
extern void gameScreenShow();

#endif

