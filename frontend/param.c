#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/param.h"

static bool coord_in_choice(int x, int y, int i) {
    int xc, yc, xw, yw;

    xc = ScreenWidth()- 5*pa.paramlayout.menubtn_size - 20;
    yc = pa.paramItem[i].y;
    xw = 5*pa.paramlayout.menubtn_size;
    yw = pa.paramlayout.menubtn_size;

    return ((x>=xc) && (x<(xc+xw)) && (y>=yc) && (y<(yc+yw)));
}

void choiceMenuHandler(int index) {
    int cp, i, c;
    if (index >= 100) {
        i = index/100-1;
        c = index%100;
        cp = pa.paramItem[i].item.c.selected%100;
        pa.paramItem[i].item.c.choiceMenu[cp+1].type = ITEM_ACTIVE;
        pa.paramItem[i].item.c.selected = index;
        pa.paramItem[i].item.c.choiceMenu[c+1].type = ITEM_BULLET;
        pa.cfg[i].u.choices.selected = c;
    
        choice_to_normal(i);
    }
    SoftUpdate();
}

void paramTap(int x, int y) {
    int i;
    init_tap_x = x;
    init_tap_y = y;

    for (i=0;i<pa.numParamButtons;i++) {
        if (coord_in_button(x, y, &pa.paramButton[i]))
            button_to_tapped(&pa.paramButton[i], false);
    }
    for (i=0;i<pa.numParams;i++) {
        switch(pa.paramItem[i].type) {
            case C_STRING:
                if (coord_in_button(x, y, &pa.paramItem[i].item.n.decrease))
                    button_to_tapped(&pa.paramItem[i].item.n.decrease, false);
                if (coord_in_button(x, y, &pa.paramItem[i].item.n.increase))
                    button_to_tapped(&pa.paramItem[i].item.n.increase, false);
                break;
            case C_CHOICES:
                if (coord_in_choice(x, y, i))
                    choice_to_tapped(i);
                break;
            case C_BOOLEAN:
                if (coord_in_button(x, y, &pa.paramItem[i].item.b.indicator))
                    button_to_tapped(&pa.paramItem[i].item.b.indicator, false);
                break;
        }
    }
    if (y > pa.paramlayout.buttonpanel.starty) {
        paramDrawPanel(true);
    }
    SoftUpdate();
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
                        paramFree();
                        switchToGameScreen();
                        return;
                }
            }
        }
    }
    for (i=0;i<pa.numParams;i++) {
        switch(pa.paramItem[i].type) {
            case C_STRING:
                if (release_button(init_tap_x, init_tap_y, &pa.paramItem[i].item.n.decrease)) {
                    button_to_normal(&pa.paramItem[i].item.n.decrease, false);
                    if (release_button(x, y, &pa.paramItem[i].item.n.decrease))
                        paramDecreaseItem(i);
                }
                if (release_button(init_tap_x, init_tap_y, &pa.paramItem[i].item.n.increase)) {
                    button_to_normal(&pa.paramItem[i].item.n.increase, false);
                    if (release_button(x, y, &pa.paramItem[i].item.n.increase))
                        paramIncreaseItem(i);
                }
                break;
            case C_CHOICES:
                if (coord_in_choice(init_tap_x, init_tap_y, i)) {
                    choice_to_normal(i);
                    if (coord_in_choice(x, y, i)) {
                        OpenMenu(pa.paramItem[i].item.c.choiceMenu, 
                                 pa.paramItem[i].item.c.selected, 
                                 x, y, choiceMenuHandler);
                    }
                }
                break;
            case C_BOOLEAN:
                if (release_button(init_tap_x, init_tap_y, &pa.paramItem[i].item.b.indicator)) {
                    button_to_normal(&pa.paramItem[i].item.b.indicator, false);
                    if (release_button(x, y, &pa.paramItem[i].item.b.indicator))
                        paramSwapItem(i);
                }
                break;
        }

    }
    if (init_tap_y > pa.paramlayout.buttonpanel.starty) {
        paramDrawPanel(false);
        if (y > pa.paramlayout.buttonpanel.starty) {
            paramSubmitParams();
        }
    }
    SoftUpdate();
}

void paramNext() { }
void paramPrev() { }

static void paramSubmitParams() {
    const char *reply;
    reply = midend_set_config(pa.me, CFG_SETTINGS, pa.cfg);
    if (reply == NULL) {
        paramFree();
        gameStartNewGame();
        switchToGameScreen();
    }
    else {
        Message(ICON_WARNING, "", reply, 3000);
    }
}

static void paramSetItemNum(int i, int n) {
    char buf[256];
    int x, y;
    x = ScreenWidth() - 3*pa.paramlayout.menubtn_size - 20;
    y = pa.paramItem[i].y+pa.paramlayout.menubtn_size/2-(pa.pfontsize/2);
    sprintf(buf, "%i", n);
    sfree(pa.cfg[i].u.string.sval);
    pa.cfg[i].u.string.sval = dupstr(buf);
    FillArea(x, y, pa.paramlayout.menubtn_size, pa.pfontsize, 0x00FFFFFF);
    DrawTextRect(x, y, pa.paramlayout.menubtn_size, pa.pfontsize, pa.cfg[i].u.string.sval, ALIGN_CENTER);
}

static void paramDecreaseItem(int i) {
    int n;
    n = atoi(pa.cfg[i].u.string.sval);
    if (n>0) n--;
    paramSetItemNum(i, n);
}

static void paramIncreaseItem(int i) {
    int n;
    n = atoi(pa.cfg[i].u.string.sval);
    n++;
    paramSetItemNum(i, n);
}

static void paramSwapItem(int i) {
    pa.cfg[i].u.boolean.bval = !pa.cfg[i].u.boolean.bval;
    pa.paramItem[i].item.b.indicator.bitmap = pa.cfg[i].u.boolean.bval ? &cfg_yes : &cfg_no;
    button_to_normal(&pa.paramItem[i].item.b.indicator, false);
}

static void paramDrawMenu() {
    FillArea(0, pa.paramlayout.menu.starty, ScreenWidth(), pa.paramlayout.menu.height, 0x00FFFFFF);
    FillArea(0, pa.paramlayout.menu.starty + pa.paramlayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&pa.paramButton[pa.btnBackIDX], false);

    SetFont(pa.paramfont, BLACK);
    DrawTextRect(0, (pa.paramlayout.menubtn_size/2)-(pa.pfontsize/2), ScreenWidth(), pa.pfontsize, pa.title, ALIGN_CENTER);
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
    DrawTextRect(0, pa.paramlayout.buttonpanel.starty+pa.paramlayout.buttonpanel.height/2-(pa.pfontsize/2), ScreenWidth(), pa.pfontsize, "OK", ALIGN_CENTER);
    PartialUpdate(0, pa.paramlayout.buttonpanel.starty, ScreenWidth(), pa.paramlayout.buttonpanel.height);
}

static void paramDrawParams() {
    int i, xn, xi, y, c;
    xn = pa.paramlayout.menubtn_size/2;
    xi = ScreenWidth() - 3*pa.paramlayout.menubtn_size - 20;
    for (i=0;i<pa.numParams;i++) {
        FillArea(0, pa.paramItem[i].y, ScreenWidth(), pa.paramlayout.menubtn_size+3, 0x00FFFFFF);
        FillArea(xn, pa.paramItem[i].y+pa.paramlayout.menubtn_size+1, ScreenWidth()-2*xn, 1, 0x00000000);
        SetFont(pa.paramfont, BLACK);
        y = pa.paramItem[i].y+pa.paramlayout.menubtn_size/2-(pa.pfontsize/2);
        DrawTextRect(xn, y, ScreenWidth()-2*xn, pa.pfontsize, pa.cfg[i].name, ALIGN_LEFT);
        switch(pa.paramItem[i].type) {
            case C_STRING:
                button_to_normal(&pa.paramItem[i].item.n.decrease, false);
                DrawTextRect(xi, y, pa.paramlayout.menubtn_size, pa.pfontsize, pa.cfg[i].u.string.sval, ALIGN_CENTER);
                button_to_normal(&pa.paramItem[i].item.n.increase, false);
                break;
            case C_CHOICES:
                choice_to_normal(i);
                break;
            case C_BOOLEAN:
                button_to_normal(&pa.paramItem[i].item.b.indicator, false);
                break;
        }
    }
}

static void choice_to_normal(int i) {
    int c, yf, yt;
    c = pa.cfg[i].u.choices.selected;
    yf = pa.paramItem[i].y;
    yt = pa.paramItem[i].y+pa.paramlayout.menubtn_size/2-(pa.pfontsize/2);
    FillArea(ScreenWidth()- 5*pa.paramlayout.menubtn_size - 20, yf, 
             5*pa.paramlayout.menubtn_size, pa.paramlayout.menubtn_size, 0x00FFFFFF);
    SetFont(pa.paramfont, BLACK);
    DrawTextRect(ScreenWidth()- 5*pa.paramlayout.menubtn_size - 20, yt, 
                 5*pa.paramlayout.menubtn_size, pa.pfontsize, 
                 pa.paramItem[i].item.c.choices[c], ALIGN_CENTER);
}

static void choice_to_tapped(int i) {
    int c, yf, yt;
    c = pa.cfg[i].u.choices.selected;
    yf = pa.paramItem[i].y;
    yt = pa.paramItem[i].y+pa.paramlayout.menubtn_size/2-(pa.pfontsize/2);
    FillArea(ScreenWidth()- 5*pa.paramlayout.menubtn_size - 20, yf, 
             5*pa.paramlayout.menubtn_size, pa.paramlayout.menubtn_size, 0x00000000);
    SetFont(pa.paramfont, WHITE);
    DrawTextRect(ScreenWidth()- 5*pa.paramlayout.menubtn_size - 20, yt, 
                 5*pa.paramlayout.menubtn_size, pa.pfontsize, 
                 pa.paramItem[i].item.c.choices[c], ALIGN_CENTER);
}

static int paramGetChoicesNum(int i) {
    int c, n;
    const char *p, *q;
    c = *pa.cfg[i].u.choices.choicenames;
    p = pa.cfg[i].u.choices.choicenames+1;
    n = 0;
    while (*p) {
        q = p;
        while (*q && *q != c) q++;
        n++;
        if (*q) q++;
        p = q;
    }
    return n;
}

static void paramBuildChoices(int n, int i) {
    int c, j;
    const char *p, *q;
    pa.paramItem[i].item.c.choices = smalloc(n*sizeof(char*));
    c = *pa.cfg[i].u.choices.choicenames;
    p = pa.cfg[i].u.choices.choicenames+1;
    j = 0;
    while (*p) {
        q = p;
        while (*q && *q != c) q++;
        pa.paramItem[i].item.c.choices[j] = smalloc(q-p+1 * sizeof(char));
        strncpy(pa.paramItem[i].item.c.choices[j], p, q-p);
        pa.paramItem[i].item.c.choices[j][q-p] = '\0';
        j++;
        if (*q) q++;
        p = q;
    }
}

static void paramBuildChoiceMenu(int i) {
    int n, j, c;
    n = pa.paramItem[i].item.c.num_choices;
    pa.paramItem[i].item.c.choiceMenu = snewn(n+2, imenu);

    pa.paramItem[i].item.c.choiceMenu[0].type = ITEM_HEADER;
    pa.paramItem[i].item.c.choiceMenu[0].text = (char *)pa.cfg[i].name;
    pa.paramItem[i].item.c.choiceMenu[0].index = 0;
    pa.paramItem[i].item.c.choiceMenu[0].submenu = NULL;

    for (j=1;j<n+1;j++) {
        pa.paramItem[i].item.c.choiceMenu[j].type = ITEM_ACTIVE;
        pa.paramItem[i].item.c.choiceMenu[j].text = pa.paramItem[i].item.c.choices[j-1];
        pa.paramItem[i].item.c.choiceMenu[j].index = 100*(i+1)+j-1;
        pa.paramItem[i].item.c.choiceMenu[j].submenu = NULL;
    }
    pa.paramItem[i].item.c.choiceMenu[j].type = 0;
    pa.paramItem[i].item.c.choiceMenu[j].text = NULL;
    pa.paramItem[i].item.c.choiceMenu[j].index = 0;
    pa.paramItem[i].item.c.choiceMenu[j].submenu = NULL;
    
    c = pa.cfg[i].u.choices.selected;
    pa.paramItem[i].item.c.choiceMenu[c+1].type = ITEM_BULLET;
    pa.paramItem[i].item.c.selected = 100*(i+1)+c;
}

static void paramFree() {
    int i, j;
    for (i=0;i<pa.numParams;i++) {
        if (pa.paramItem[i].type == C_CHOICES) {
            for (j=0;j<pa.paramItem[i].item.c.num_choices;j++) 
                sfree(pa.paramItem[i].item.c.choices[j]);
            sfree(pa.paramItem[i].item.c.choices);
            sfree(pa.paramItem[i].item.c.choiceMenu);
        }
    }
    sfree(pa.paramItem);
    sfree(pa.paramButton);
    sfree(pa.title);
    if (pa.cfg != NULL) free_cfg(pa.cfg);
}

void paramPrepare(midend *me) {
    int n, i, j;

    pa.me = me;

    pa.cfg = midend_get_config(pa.me, CFG_SETTINGS, &pa.title);
    pa.paramlayout = getLayout(LAYOUT_BUTTONBAR); 

    for (pa.numParams = 0;
         pa.cfg[pa.numParams].type != C_END;
         pa.numParams++) {}

    pa.paramItem = smalloc(pa.numParams * sizeof(PARAMITEM));
    for (i=0;i<pa.numParams;i++) {
        pa.paramItem[i].bitmap = NULL;
        pa.paramItem[i].type = pa.cfg[i].type;
        pa.paramItem[i].y = pa.paramlayout.maincanvas.starty +
                            i*(pa.paramlayout.menubtn_size+3);
        switch (pa.paramItem[i].type) {
            case C_STRING:
                pa.paramItem[i].item.n.decrease = (BUTTON) { true, BTN_ITEM,
                  ScreenWidth() - 4*pa.paramlayout.menubtn_size - 20, pa.paramItem[i].y, 
                  pa.paramlayout.menubtn_size, 0,
                  ACTION_CTRL, ' ', &cfg_decr, &cfg_decr_tap, NULL};
                pa.paramItem[i].item.n.increase = (BUTTON) { true, BTN_ITEM,
                  ScreenWidth() - 2*pa.paramlayout.menubtn_size - 20, pa.paramItem[i].y, 
                  pa.paramlayout.menubtn_size, 0,
                  ACTION_CTRL, ' ', &cfg_incr, &cfg_incr_tap, NULL};
                break;
            case C_CHOICES:
                n = paramGetChoicesNum(i);
                pa.paramItem[i].item.c.num_choices = n;
                paramBuildChoices(n, i);
                paramBuildChoiceMenu(i);
                break;
            case C_BOOLEAN:
                pa.paramItem[i].item.b.indicator = (BUTTON) { true, BTN_ITEM,
                  ScreenWidth() - 3*pa.paramlayout.menubtn_size - 20, pa.paramItem[i].y, 
                  pa.paramlayout.menubtn_size, 0,
                  ACTION_SWAP, ' ', pa.cfg[i].u.boolean.bval ? &cfg_yes : &cfg_no, NULL, NULL};
                break;
        }
    }

    pa.numParamButtons = 1;
    pa.paramButton = smalloc(pa.numParamButtons*sizeof(BUTTON));
    pa.paramButton[pa.btnBackIDX = 0] = (BUTTON){ true,  BTN_MENU, 
        pa.paramlayout.menubtn_size/4,
        pa.paramlayout.menu.starty,
        pa.paramlayout.menubtn_size, 0, 
        ACTION_BACK, ' ', &icon_back, &icon_back_tap, NULL};
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
    pa.pfontsize = (int)(ScreenWidth()/30);
    pa.paramfont = OpenFont("LiberationSans-Bold", pa.pfontsize, 0);
    pa.paramButton = NULL;
    pa.title = NULL;
    pa.cfg = NULL;
    paramInitialized = true;
}

void paramScreenFree() {
    if (paramInitialized) {
        CloseFont(pa.paramfont);
        paramInitialized = false;
    }
}

