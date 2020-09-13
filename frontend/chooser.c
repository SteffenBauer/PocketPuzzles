#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/chooser.h"
#include "frontend/gamelist.h"

void chooserTap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;
    int i;
    for (i=0;i<ca.numChooserButtons;i++) {
        if (coord_in_button(x, y, &ca.chooserButton[i]))
            button_to_tapped(&ca.chooserButton[i]);
    }
}

void chooserLongTap(int x, int y) { }

void chooserDrag(int x, int y) { }

void chooserRelease(int x, int y) {
    int i;
    for (i=0;i<ca.numChooserButtons;i++) {
        if (coord_in_button(init_tap_x, init_tap_y, &ca.chooserButton[i])) {
            button_to_normal(&ca.chooserButton[i], true);
            if (release_button(x, y, &ca.chooserButton[i])) {
                switch(ca.chooserButton[i].action) {
                    case ACTION_HOME:
                        chooserExit();
                        exitApp();
                        break;
                    case ACTION_DRAW:
                        chooserShowPage();
                        break;
                    case ACTION_PREV:
                        chooserPrev();
                        break;
                    case ACTION_NEXT:
                        chooserNext();
                        break;
                    case ACTION_LAUNCH:
                        gameInit(ca.chooserButton[i].actionParm.thegame);
                        showGameScreen();
                        break;
                }
            }
        }
    }
}

void chooserPrev() {
    if (ca.current_chooserpage > 0) {
        ca.current_chooserpage -= 1;
        chooserShowPage();
    }
}

void chooserNext() {
    if (ca.current_chooserpage <= ca.chooser_lastpage) {
        ca.current_chooserpage += 1;
        chooserShowPage();
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
                         ca.chooserButton[i].size+ca.chooser_padding, 32,
                         ca.chooserButton[i].actionParm.thegame->name, ALIGN_CENTER);
        }
        else if (ca.chooserButton[i].action == ACTION_LAUNCH) {
            ca.chooserButton[i].active = false;
        }
    }
}

static void chooserDrawControlButtons(int page) {
    FillArea(0, ca.chooserlayout.buttonpanel.starty, ScreenWidth(), ca.chooserlayout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, ca.chooserlayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

    if (page == 0) {
        ca.chooserButton[ca.btnPrevIDX].active = false;
        button_to_cleared(&ca.chooserButton[ca.btnPrevIDX], false);
    }
    else {
        ca.chooserButton[ca.btnPrevIDX].active = true;
        button_to_normal(&ca.chooserButton[ca.btnPrevIDX], false);
    }
    if (page == ca.chooser_lastpage) {
        ca.chooserButton[ca.btnNextIDX].active = false;
        button_to_cleared(&ca.chooserButton[ca.btnNextIDX], false);
    }
    else {
        ca.chooserButton[ca.btnNextIDX].active = true;
        button_to_normal(&ca.chooserButton[ca.btnNextIDX], false);
    }
}

static void chooserDrawMenu() {
    FillArea(0, ca.chooserlayout.menu.starty, ScreenWidth(), ca.chooserlayout.menu.height, 0x00FFFFFF);
    FillArea(0, ca.chooserlayout.menu.starty + ca.chooserlayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&ca.chooserButton[ca.btnHomeIDX], false);
    button_to_normal(&ca.chooserButton[ca.btnDrawIDX], false);

    SetFont(ca.chooserfont, BLACK);
    DrawTextRect(0, (ca.chooserlayout.menubtn_size/2)-(32/2), ScreenWidth(), 32, "PUZZLES", ALIGN_CENTER);
}

static void chooserSetupButtons() {
    int i,c,r,p,pi;

    ca.numChooserButtons = ca.num_games + 4;
    ca.chooserButton = malloc(ca.numChooserButtons*sizeof(BUTTON));

    for(i=0;i<ca.num_games;i++) {
        p = i / (CHOOSER_COLS*CHOOSER_ROWS);
        pi = i % (CHOOSER_COLS*CHOOSER_ROWS);
        c = pi % CHOOSER_COLS;
        r = pi / CHOOSER_COLS;
        ca.chooserButton[i].posx = (c+1)*ca.chooser_padding + c*ca.chooserlayout.chooser_size;
        ca.chooserButton[i].posy = 50 + ca.chooserlayout.maincanvas.starty + r*(20+32+ca.chooserlayout.chooser_size);
        ca.chooserButton[i].page = p;
        ca.chooserButton[i].action = ACTION_LAUNCH;
        ca.chooserButton[i].size = ca.chooserlayout.chooser_size;
        ca.chooserButton[i].bitmap = mygames[i].bitmap;
        ca.chooserButton[i].actionParm.thegame = mygames[i].thegame;
    }

    ca.chooserButton[ca.btnHomeIDX = i++] = (BUTTON){ true,  BTN_MENU, 
        10, ca.chooserlayout.menu.starty, 
        ca.chooserlayout.menubtn_size, 0, 
        ACTION_HOME, ' ', &icon_home, &icon_home_tap, NULL};

    ca.chooserButton[ca.btnDrawIDX = i++] = (BUTTON){ true,  BTN_MENU, 
        ScreenWidth() - ca.chooserlayout.menubtn_size - 10, ca.chooserlayout.menu.starty, 
        ca.chooserlayout.menubtn_size, 0,
        ACTION_DRAW, ' ', &icon_redraw, &icon_redraw_tap, NULL};

    ca.chooserButton[ca.btnPrevIDX = i++] = (BUTTON){ false, BTN_CTRL, 
        ca.control_padding, ca.chooserlayout.buttonpanel.starty + 2, 
        ca.chooserlayout.control_size, 0, 
        ACTION_PREV, ' ', &bt_prev, NULL, NULL};

    ca.chooserButton[ca.btnNextIDX = i++] = (BUTTON){ false, BTN_CTRL, 
        2*ca.control_padding + ca.chooserlayout.control_size, 
        ca.chooserlayout.buttonpanel.starty + 2, ca.chooserlayout.control_size, 0, 
        ACTION_NEXT, ' ', &bt_next, NULL, NULL};
}

void chooserShowPage() {
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    chooserDrawMenu();
    chooserDrawChooserButtons(ca.current_chooserpage);
    chooserDrawControlButtons(ca.current_chooserpage);
    FullUpdate();
}

void chooserInit() {
    ca.chooserfont = OpenFont("LiberationSans-Bold", 32, 0);

    ca.num_games = 0;
    while (true) {
        if (mygames[ca.num_games].thegame == NULL) break;
        ca.num_games++;
    }
    ca.current_chooserpage = 0;
    ca.chooser_lastpage = (ca.num_games-1) / (CHOOSER_COLS * CHOOSER_ROWS);

    ca.chooserlayout = getLayout(LAYOUT_BUTTONBAR);
    ca.control_padding = (ScreenWidth()-(CONTROL_NUM*ca.chooserlayout.control_size))/(CONTROL_NUM+1);
    ca.chooser_padding = (ScreenWidth()-(CHOOSER_COLS*ca.chooserlayout.chooser_size))/(CHOOSER_COLS+1);

    chooserSetupButtons();
}

void chooserExit() {
    free(ca.chooserButton);
    CloseFont(ca.chooserfont);
}
