#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/param.h"

void paramTap(int x, int y) {
    int i;
    init_tap_x = x;
    init_tap_y = y;

    for (i=0;i<pa.numParamButtons;i++) {
        if (coord_in_button(x, y, &pa.paramButton[i]))
            button_to_tapped(&pa.paramButton[i]);
    }
    if (y > pa.paramlayout.buttonpanel.starty) {
        paramDrawPanel(true);
    }
}

void paramLongTap(int x, int y) { }
void paramDrag(int x, int y) { }

void paramRelease(int x, int y) {
    int i;
    for (i=0;i<pa.numParamButtons;i++) {
        if (release_button(init_tap_x, init_tap_y, &pa.paramButton[i])) {
            button_to_normal(&pa.paramButton[i], true);
            if (release_button(x, y, &pa.paramButton[i])) {
                switch(pa.paramButton[i].action) {
                    case ACTION_BACK:
                        switchToGameScreen();
                        return;
                    case ACTION_DRAW:
                        paramScreenShow();
                        FullUpdate();
                        return;
                }
            }
        }
    }
    if (init_tap_y > pa.paramlayout.buttonpanel.starty) {
        paramDrawPanel(false);
        if (y > pa.paramlayout.buttonpanel.starty) {
            switchToGameScreen();
        }
    }
}

void paramNext() { }
void paramPrev() { }

static void paramDrawMenu() {
    FillArea(0, pa.paramlayout.menu.starty, ScreenWidth(), pa.paramlayout.menu.height, 0x00FFFFFF);
    FillArea(0, pa.paramlayout.menu.starty + pa.paramlayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&pa.paramButton[pa.btnBackIDX], false);
    button_to_normal(&pa.paramButton[pa.btnDrawIDX], false);

    SetFont(pa.paramfont, BLACK);
    DrawTextRect(0, (pa.paramlayout.menubtn_size/2)-(PFONTSIZE/2), ScreenWidth(), PFONTSIZE, pa.title, ALIGN_CENTER);
}

static void paramDrawPanel(bool inverse) {
    if (!inverse) {
        FillArea(0, pa.paramlayout.buttonpanel.starty, ScreenWidth(), pa.paramlayout.buttonpanel.height, 0x00FFFFFF);
        FillArea(0, pa.paramlayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);
        SetFont(pa.paramfont, BLACK);
    }
    else {
        FillArea(0, pa.paramlayout.buttonpanel.starty, ScreenWidth(), pa.paramlayout.buttonpanel.height, 0x00000000);    
        SetFont(pa.paramfont, WHITE);
    }
    DrawTextRect(0, pa.paramlayout.buttonpanel.starty+pa.paramlayout.buttonpanel.height/2-(PFONTSIZE/2), ScreenWidth(), PFONTSIZE, "OK", ALIGN_CENTER);
    PartialUpdate(0, pa.paramlayout.buttonpanel.starty, ScreenWidth(), pa.paramlayout.buttonpanel.height);
}

static void paramDrawParams() {
    int i, x, y;
    /*int c;
    const char *p, *q;
    char **choices; */
    char buf[256];
    x = pa.paramlayout.menubtn_size;
    for (i=0;i<pa.numParams;i++) {
        FillArea(0, pa.paramItem[i].y, ScreenWidth(), pa.paramlayout.menubtn_size+3, 0x00FFFFFF);
        FillArea(x, pa.paramItem[i].y+pa.paramlayout.menubtn_size+1, ScreenWidth(), 1, 0x00000000);
        SetFont(pa.paramfont, BLACK);
        y = pa.paramItem[i].y+pa.paramlayout.menubtn_size/2-(PFONTSIZE/2);
        DrawTextRect(2*x+10, y, ScreenWidth()-10, PFONTSIZE, pa.cfg[i].name, ALIGN_LEFT);
        switch(pa.cfg[i].type) {
            case C_STRING:
                DrawTextRect(ScreenWidth()/2, y, ScreenWidth()/2,PFONTSIZE, pa.cfg[i].u.string.sval, ALIGN_CENTER);
                break;
            case C_CHOICES:
                /*
                c = pa.cfg[i]->u.choices.choicenames;
                p = pa.cfg[i]->u.choices.choicenames+1;
                while (*p) {
                    q = p;
                    while (*q && *q != c) q++;
                    name = snewn(q-p+1, char);
                    strncpy(name, p, q-p);
                    name[q-p] = '\0';
                    if (*q) q++;
                    p = q;
                } */
                sprintf(buf, "%i", pa.cfg[i].u.choices.selected);
                DrawTextRect(ScreenWidth()/2, y, ScreenWidth()/2,PFONTSIZE, buf, ALIGN_CENTER);
                break;
            case C_BOOLEAN:
                DrawTextRect(ScreenWidth()/2, y, ScreenWidth()/2,PFONTSIZE, pa.cfg[i].u.boolean.bval ? "Yes": "No", ALIGN_CENTER);
                break;
        }
    }
}

void paramPrepare(midend *me) {
    int i = 0;

    pa.me = me;
    if (pa.title) sfree(pa.title);
    if (pa.cfg) free_cfg(pa.cfg);
    if (pa.paramItem) sfree(pa.paramItem);
    
    pa.cfg = midend_get_config(pa.me, CFG_SETTINGS, &pa.title);
    pa.paramlayout = getLayout(LAYOUT_BUTTONBAR); 

    pa.numParams = 0;
    while (pa.cfg[pa.numParams].type != C_END) {
        pa.numParams++;
        if (pa.numParams > 32) break;
    }

    pa.paramItem = smalloc(pa.numParams * sizeof(PARAMITEM));
    for (i=0;i<pa.numParams;i++) {
        pa.paramItem[i].bitmap = NULL;
        pa.paramItem[i].y = pa.paramlayout.maincanvas.starty +
                            i*(pa.paramlayout.menubtn_size+3);
    }

    pa.numParamButtons = 2;
    if (pa.paramButton) sfree(pa.paramButton);
    pa.paramButton = smalloc(pa.numParamButtons*sizeof(BUTTON));
    i=0;
    pa.paramButton[pa.btnBackIDX = i++] = (BUTTON){ true,  BTN_MENU, 
        10, pa.paramlayout.menu.starty, 
        pa.paramlayout.menubtn_size, 0, 
        ACTION_BACK, ' ', &icon_back, &icon_back_tap, NULL};

    pa.paramButton[pa.btnDrawIDX = i++] = (BUTTON){ true,  BTN_MENU, 
        ScreenWidth() - 1*pa.paramlayout.menubtn_size - 20, pa.paramlayout.menu.starty, 
        pa.paramlayout.menubtn_size, 0,
        ACTION_DRAW, ' ', &icon_redraw, &icon_redraw_tap, NULL};
}

void paramScreenShow() {
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    paramDrawMenu();
    paramDrawPanel(false);
    paramDrawParams();
    FullUpdate();
}

void paramScreenInit() {
    pa.paramfont = OpenFont("LiberationSans-Bold", PFONTSIZE, 0);
    pa.paramButton = NULL;
    pa.title = NULL;
    pa.cfg = NULL;
    paramInitialized = true;
}

void paramScreenFree() {
    if (paramInitialized) {
        CloseFont(pa.paramfont);
        if (pa.paramButton) sfree(pa.paramButton);
        if (pa.title)       sfree(pa.title);
        if (pa.cfg)         free_cfg(pa.cfg);
        paramInitialized = false;
    }
}

