#ifndef POCKETPUZZLES_PARAM_HEADER
#define POCKETPUZZLES_PARAM_HEADER
#include "common.h"

#define PFONTSIZE 36

bool paramInitialized;

typedef struct gameparamitem {
    ibitmap *bitmap;
    int y;
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

extern ibitmap cfg_on, cfg_off, cfg_incr, cfg_incr_tap, cfg_decr, cfg_decr_tap,
               icon_back, icon_back_tap, icon_redraw, icon_redraw_tap;

static void paramDrawMenu();
static void paramDrawPanel(bool inverse);
static void paramDrawParams();

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

