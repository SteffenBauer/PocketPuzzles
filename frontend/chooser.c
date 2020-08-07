#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "chooser.h"

void chooserTap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;
    int i;

    if (coord_in_button(x, y, &btn_home)) button_to_tapped(&btn_home);
    if (coord_in_button(x, y, &btn_prev)) button_to_tapped(&btn_prev);
    if (coord_in_button(x, y, &btn_next)) button_to_tapped(&btn_next);
    for (i=0;i<num_games;i++) {
        if (coord_in_button(x, y, &btn_chooser[i])) {
            button_to_tapped(&btn_chooser[i]);
            break;
        }
    }
}

void chooserLongTap(int x, int y) {
}

void chooserRelease(int x, int y) {
    int i;
    if (coord_in_button(init_tap_x, init_tap_y, &btn_home)) button_to_normal(&btn_home, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_prev)) button_to_normal(&btn_prev, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_next)) button_to_normal(&btn_next, true);
    for (i=0;i<num_games;i++) {
        if (coord_in_button(init_tap_x, init_tap_y, &btn_chooser[i])) {
            button_to_normal(&btn_chooser[i], true);
            switchToGame();
            break;
        }
    }

    if (release_button(x, y, &btn_home)) {
        exitApp();
    }
    if (release_button(x, y, &btn_prev) &&
        (current_chooserpage > 0)) {
            current_chooserpage -= 1;
            chooserShowPage();
    }
    if (release_button(x, y, &btn_next) &&
        (current_chooserpage <= chooser_lastpage)) {
            current_chooserpage += 1;
            chooserShowPage();
    }
}

static void chooserDrawChooserButtons(int page) {
    int i;
    FillArea(0, mainlayout.maincanvas.starty, ScreenWidth(), mainlayout.maincanvas.height, 0x00FFFFFF);
    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    for(i=0;i<num_games;i++) {
        if (btn_chooser[i].page == page) {
            btn_chooser[i].active = true;
            button_to_normal(&btn_chooser[i], false);
            DrawTextRect(btn_chooser[i].posx-(chooser_padding/2),
                         btn_chooser[i].posy+btn_chooser[i].size+5,
                         btn_chooser[i].size+chooser_padding, kFontSize,
                         btn_chooser[i].title, ALIGN_CENTER);
        }
        else {
            btn_chooser[i].active = false;
        }
    }
    CloseFont(font);
}

static void chooserDrawControlButtons(int page) {
    FillArea(0, mainlayout.buttonpanel.starty, ScreenWidth(), mainlayout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, mainlayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

    if (page == 0) {
        btn_prev.active = false;
        button_to_cleared(&btn_prev, false);
    }
    else {
        btn_prev.active = true;
        button_to_normal(&btn_prev, false);
    }
    if (page == chooser_lastpage) {
        btn_next.active = false;
        button_to_cleared(&btn_next, false);
    }
    else {
        btn_next.active = true;
        button_to_normal(&btn_next, false);
    }
}

static void chooserDrawMenu() {
    FillArea(0, mainlayout.menu.starty, ScreenWidth(), mainlayout.menu.height, 0x00FFFFFF);
    FillArea(0, mainlayout.menu.starty + mainlayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&btn_home, false);

    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(0, (mainlayout.menubtn_size/2)-(kFontSize/2), ScreenWidth(), kFontSize, "PUZZLES", ALIGN_CENTER);
    CloseFont(font);
}

static void chooserSetupChooserButtons() {
    int i;
    int c,r,p,pi;
    int col = chooser_cols;
    int row = chooser_rows;

    for(i=0;i<num_games;i++) {
        p = i / (col*row);
        pi = i % (col*row);
        c = pi % col;
        r = pi / col;
        btn_chooser[i].posx = (c+1)*chooser_padding + c*mainlayout.chooser_size;
        btn_chooser[i].posy = 50 + mainlayout.maincanvas.starty + r*(20+kFontSize+mainlayout.chooser_size);
        btn_chooser[i].page = p;
        btn_chooser[i].size = mainlayout.chooser_size;
    }
}

static void chooserSetupControlButtons() {
    btn_prev.posx = control_padding;
    btn_prev.posy = mainlayout.buttonpanel.starty + 2;
    btn_prev.size = mainlayout.control_size;
    btn_next.posx = 2*control_padding + mainlayout.control_size;
    btn_next.posy = mainlayout.buttonpanel.starty + 2;
    btn_next.size = mainlayout.control_size;
}

static void chooserSetupMenuButtons() {
    btn_home.active = true;
    btn_home.posx = 10;
    btn_home.posy = mainlayout.menu.starty;
    btn_home.size = mainlayout.menubtn_size; 
}

void chooserShowPage() {
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    chooserDrawMenu();
    chooserDrawChooserButtons(current_chooserpage);
    chooserDrawControlButtons(current_chooserpage);
    FullUpdate();
}

void chooserInit() {
    current_chooserpage = 0;
    control_padding = (ScreenWidth()-(control_num*mainlayout.control_size))/(control_num+1);
    chooser_padding = (ScreenWidth()-(chooser_cols*mainlayout.chooser_size))/(chooser_cols+1);
    chooser_lastpage = num_games / (chooser_cols * chooser_rows);

    chooserSetupMenuButtons();
    chooserSetupControlButtons();
    chooserSetupChooserButtons();
}


