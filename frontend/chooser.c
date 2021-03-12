#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/chooser.h"
#include "frontend/gamelist.h"

void chooserResetDialogHandler(int button) {
    if (button == 1) {
        configDel();
        chooserSetupButtons();
        chooserRefreshCanvas();
    }
}

void chooserMenuHandler(int index) {
    char buf[256];

    button_to_normal(&ca.chooserButton[ca.btnMenuIDX], true);

    switch (index) {
        case 101:  /* Settings */
            Message(ICON_WARNING, "", "Settings not implemented yet!", 3000);
            break;
        case 102:  /* Resume Game */
            if (gameResumeGame())
                switchToGameScreen();
            else
                Message(ICON_WARNING, "", "No game to resume", 2000);
            break;
        case 103: /* Reset presets */
            Dialog(ICON_QUESTION, "Reset presets", "Reset savegame and game presets to defaults?", "OK", "Cancel", chooserResetDialogHandler);
            break;
        case 104:  /* About */
            sprintf(buf, "Simon Tatham's Portable Puzzle Collection\n\nOriginal project by Simon Tatham\n\nPort to PocketBook by Steffen Bauer\n\nSee 'How to play' at each puzzle for individual contributors.\n\nVersion: %s", VERSION);
            Dialog(ICON_INFORMATION, "About", buf, "OK", NULL, NULL);
            break;
        default:
            break;
    }
};

void chooserTap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;
    int i;
    for (i=0;i<ca.numChooserButtons;i++) {
        if (coord_in_button(x, y, &ca.chooserButton[i]))
            button_to_tapped(&ca.chooserButton[i], true);
    }
    for (i=0;i<ca.numPageButtons;i++) {
        if (coord_in_button(x, y, &ca.pageButton[i])) {
            button_to_tapped(&ca.pageButton[i], true);
        }
    }
}

void chooserLongTap(int x, int y) {
    int i;
    for (i=0;i<ca.numChooserButtons;i++) {
        if (release_button(x, y, &ca.chooserButton[i]) &&
           (ca.chooserButton[i].action == ACTION_LAUNCH)) {
            if (ca.chooserButton[i].type == BTN_CHOOSER) {
                stateSetFavorite(ca.chooserButton[i].actionParm.thegame->name);
            }
            else {
                stateUnsetFavorite(ca.chooserButton[i].actionParm.thegame->name);
            }
            init_tap_x = init_tap_y = -1;
            chooserSetupButtons();
            chooserRefreshCanvas();
            break;
        }
    }
}

void chooserDrag(int x, int y) { }

void chooserRelease(int x, int y) {
    int i;

    if (init_tap_y > ca.chooserlayout.maincanvas.starty) {
        if      (((init_tap_x - x) > ScreenWidth()/10) && 
                 (ca.current_chooserpage < ca.chooser_lastpage)) {
            chooserNext();
            return;
        }
        else if (((x - init_tap_x) > ScreenWidth()/10) && 
                 (ca.current_chooserpage > 0)) {
            chooserPrev();
            return;
        }
    }
    if (init_tap_y < ca.chooserlayout.buttonpanel.starty) {
        for (i=0;i<ca.numChooserButtons;i++) {
            if (coord_in_button(init_tap_x, init_tap_y, &ca.chooserButton[i]))
                button_to_normal(&ca.chooserButton[i], true);

            if (release_button(x, y, &ca.chooserButton[i])) {
                switch(ca.chooserButton[i].action) {
                    case ACTION_HOME:
                        exitApp();
                        break;
                    case ACTION_DRAW:
                        chooserScreenShow();
                        break;
                    case ACTION_MENU:
                        OpenMenuEx(chooserMenu, 0, 
                            ScreenWidth()-10-ca.chooserlayout.menubtn_size ,
                            ca.chooserlayout.menubtn_size +2, chooserMenuHandler);
                        break;
                    case ACTION_LAUNCH:
                        gameSetGame(ca.chooserButton[i].actionParm.thegame);
                        gameStartNewGame();
                        switchToGameScreen();
                        break;
                }
            }
        }
        return;
    }
    else if (init_tap_y > ca.chooserlayout.buttonpanel.starty) {
        for (i=0;i<ca.numPageButtons;i++) {
            if (release_button(x, y, &ca.pageButton[i])) {
                chooserSwitch(ca.pageButton[i].page);
                break;
            }
        }
    }
}

void chooserPrev() {
    if (ca.current_chooserpage > 0) {
        ca.current_chooserpage -= 1;
        chooserRefreshCanvas();
    }
}

void chooserNext() {
    if (ca.current_chooserpage < ca.chooser_lastpage) {
        ca.current_chooserpage += 1;
        chooserRefreshCanvas();
    }
}

void chooserSwitch(int page) {
    if ((page >= 0) && (page <= ca.chooser_lastpage)) {
        ca.current_chooserpage = page;
        chooserRefreshCanvas();
    }
}

static void chooserDrawChooserButtons(int page) {
    int i;
    FillArea(0, ca.chooserlayout.maincanvas.starty, ScreenWidth(), ca.chooserlayout.maincanvas.height, 0x00FFFFFF);
    SetFont(ca.chooserfont, BLACK);
    for(i=0;i<ca.numChooserButtons;i++) {
        if (ca.chooserButton[i].action == ACTION_LAUNCH && ca.chooserButton[i].page == page) {
            ca.chooserButton[i].active = true;
            button_to_normal(&ca.chooserButton[i], false);
            DrawTextRect(ca.chooserButton[i].posx-(ca.chooser_padding/2),
                         ca.chooserButton[i].posy+ca.chooserButton[i].size+5,
                         ca.chooserButton[i].size+ca.chooser_padding, ca.cfontsize,
                         ca.chooserButton[i].actionParm.thegame->name, ALIGN_CENTER);
        }
        else if (ca.chooserButton[i].action == ACTION_LAUNCH) {
            ca.chooserButton[i].active = false;
        }
    }
}

static void chooserDrawControlButtons(int page) {
    int i;
    FillArea(0, ca.chooserlayout.buttonpanel.starty, ScreenWidth(), ca.chooserlayout.buttonpanel.height, 0x00FFFFFF);

    for (i=0;i<ca.numPageButtons;i++) {
        if (page == ca.pageButton[i].page)
            button_to_tapped(&ca.pageButton[i], false);
        else
            button_to_normal(&ca.pageButton[i], false);
    }
}

static void chooserDrawMenu() {
    FillArea(0, ca.chooserlayout.menu.starty, ScreenWidth(), ca.chooserlayout.menu.height, 0x00FFFFFF);
    FillArea(0, ca.chooserlayout.menu.starty + ca.chooserlayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&ca.chooserButton[ca.btnHomeIDX], false);
    button_to_normal(&ca.chooserButton[ca.btnDrawIDX], false);
    button_to_normal(&ca.chooserButton[ca.btnMenuIDX], false);

    SetFont(ca.chooserfont, BLACK);
    DrawTextRect(0, (ca.chooserlayout.menubtn_size/2)-(ca.cfontsize/2), ScreenWidth(), ca.cfontsize, "PUZZLES", ALIGN_CENTER);
}

static void chooserSetupButtons() {
    int i,c,r,p,pi,n;

    for(i=0;i<ca.num_games;i++) {
        p = i / (ca.chooser_cols*ca.chooser_rows);
        pi = i % (ca.chooser_cols*ca.chooser_rows);
        c = pi % ca.chooser_cols;
        r = pi / ca.chooser_cols;
        ca.chooserButton[i].active = true;
        ca.chooserButton[i].posx = (c+1)*ca.chooser_padding + c*ca.chooserlayout.chooser_size;
        ca.chooserButton[i].posy = 50 + ca.chooserlayout.maincanvas.starty +
                                   r*(32+ca.cfontsize+ca.chooserlayout.chooser_size);
        ca.chooserButton[i].size = ca.chooserlayout.chooser_size;
        ca.chooserButton[i].page = p;
        ca.chooserButton[i].action = ACTION_LAUNCH;
        ca.chooserButton[i].bitmap_tap = NULL;
        ca.chooserButton[i].bitmap_disabled = NULL;
    }

    n = 0;
    for (i=0;i<ca.num_games;i++) {
        if (stateIsFavorite(mygames[i].thegame->name)) {
            ca.chooserButton[n].bitmap = mygames[i].bitmap;
            ca.chooserButton[n].actionParm.thegame = mygames[i].thegame;
            ca.chooserButton[n].type = BTN_FAVORITE;
            n++;
        }
    }
    for (i=0;i<ca.num_games;i++) {
        if (!stateIsFavorite(mygames[i].thegame->name)) {
            ca.chooserButton[n].bitmap = mygames[i].bitmap;
            ca.chooserButton[n].actionParm.thegame = mygames[i].thegame;
            ca.chooserButton[n].type = BTN_CHOOSER;
            n++;
        }
    }

    for (n=0;n<ca.numPageButtons;n++) {
        int pos = ScreenWidth()/2 - 
                  (ca.chooserlayout.control_size*ca.numPageButtons)/2 + 
                  n*ca.chooserlayout.control_size;
        ca.pageButton[n] = (BUTTON){
            true, BTN_CTRL, 
            pos, ca.chooserlayout.buttonpanel.starty +ca.chooserlayout.control_size/4, 
            ca.chooserlayout.control_size/2, n, 
            ACTION_SWITCH, 0x00, &bt_page, &bt_page_select, NULL};
    }

    ca.chooserButton[ca.btnHomeIDX = i++] = (BUTTON){ true,  BTN_MENU, 
        10, ca.chooserlayout.menu.starty, 
        ca.chooserlayout.menubtn_size, 0, 
        ACTION_HOME, ' ', &icon_home, &icon_home_tap, NULL};

    ca.chooserButton[ca.btnDrawIDX = i++] = (BUTTON){ true,  BTN_MENU, 
        ScreenWidth() - 2*ca.chooserlayout.menubtn_size - 20, ca.chooserlayout.menu.starty, 
        ca.chooserlayout.menubtn_size, 0,
        ACTION_DRAW, ' ', &icon_redraw, &icon_redraw_tap, NULL};

    ca.chooserButton[ca.btnMenuIDX = i++] = (BUTTON){ true,  BTN_MENU, 
        ScreenWidth() - 1*ca.chooserlayout.menubtn_size - 10, ca.chooserlayout.menu.starty, 
        ca.chooserlayout.menubtn_size, 0,
        ACTION_MENU, ' ', &icon_menu, &icon_menu_tap, NULL};
}

void chooserRefreshCanvas() {
    chooserDrawChooserButtons(ca.current_chooserpage);
    chooserDrawControlButtons(ca.current_chooserpage);
    SoftUpdate();
}

void chooserScreenShow() {
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    chooserDrawMenu();
    chooserDrawChooserButtons(ca.current_chooserpage);
    chooserDrawControlButtons(ca.current_chooserpage);
    FullUpdate();
}

void chooserScreenInit() {
    ca.cfontsize = (int)(ScreenWidth()/30);
    ca.chooserfont = OpenFont("LiberationSans-Bold", ca.cfontsize, 0);

    ca.num_games = 0;
    while (true) {
        if (mygames[ca.num_games].thegame == NULL) break;
        ca.num_games++;
    }
    ca.chooserlayout = getLayout(LAYOUT_BUTTONBAR);

    ca.chooser_cols = CHOOSER_COLS;
    ca.control_padding = (ScreenWidth()-(CONTROL_NUM*ca.chooserlayout.control_size))/(CONTROL_NUM+1);
    ca.chooser_padding = (ScreenWidth()-(ca.chooser_cols*ca.chooserlayout.chooser_size))/(ca.chooser_cols+1);

    ca.chooser_rows = (int)((ca.chooserlayout.maincanvas.height-50) / 
        (32+ca.cfontsize+ca.chooserlayout.chooser_size));

    ca.current_chooserpage = 0;
    ca.chooser_lastpage = (ca.num_games-1) / (ca.chooser_cols * ca.chooser_rows);

    ca.numChooserButtons = ca.num_games + 5;
    ca.chooserButton = smalloc(ca.numChooserButtons*sizeof(BUTTON));

    ca.numPageButtons = ca.chooser_lastpage + 1;
    ca.pageButton = smalloc(ca.numPageButtons*sizeof(BUTTON));

    chooserSetupButtons();
    chooserInitialized = true;
}

void chooserSerialise() {
    configAddItem("config_resume", "chooser");
}

void chooserScreenFree() {
    if (chooserInitialized) {
        CloseFont(ca.chooserfont);
        sfree(ca.chooserButton);
        sfree(ca.pageButton);
        chooserInitialized = false;
    }
}
