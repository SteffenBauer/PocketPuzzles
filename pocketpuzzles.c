#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "pocketpuzzles.h"

static bool coord_in_button(int x, int y, BUTTON *button) {
    return (button->active &&
            (x>=button->posx) && (x<(button->posx+button->size)) &&
            (y>=button->posy) && (y<(button->posy+button->size)));
}
static void button_to_normal(BUTTON *button, bool update) {
    StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap, 0);
    if (update) PartialUpdate(button->posx, button->posy, button->size, button->size);
}
static void button_to_tapped(BUTTON *button) {
    if (button->bitmap_tap == NULL)
        InvertArea(button->posx, button->posy, button->size, button->size);
    else
        StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap_tap, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}
static void button_to_cleared(BUTTON *button, bool update) {
    FillArea(button->posx, button->posy, button->size, button->size, 0x00FFFFFF);
    if (update) PartialUpdate(button->posx, button->posy, button->size, button->size);
}

static void drawChooserTop() {
    FillArea(0, layout.menu.starty, ScreenWidth(), layout.menu.height, 0x00FFFFFF);
    FillArea(0, layout.menu.starty + layout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&btn_home, false);

    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(0, (icon_size/2)-(kFontSize/2), ScreenWidth(), kFontSize, "PUZZLES", ALIGN_CENTER);
    CloseFont(font);
}

static void setupChooserButtons() {
    int i;
    int c,r,p,pi;
    int col = chooser_cols;
    int row = chooser_rows;

    for(i=0;i<num_games;i++) {
        p = i / (col*row);
        pi = i % (col*row);
        c = pi % col;
        r = pi / col;
        btn_chooser[i].posx = (c+1)*chooser_padding + c*chooser_size;
        btn_chooser[i].posy = 50 + layout.maincanvas.starty + r*(20+kFontSize+chooser_size);
        btn_chooser[i].page = p;
        btn_chooser[i].size = chooser_size;
    }
}

static void drawChooserButtons(int page) {
    int i;
    FillArea(0, layout.maincanvas.starty, ScreenWidth(), layout.maincanvas.height, 0x00FFFFFF);
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

static void drawChooserControlButtons(int page) {
    FillArea(0, layout.buttonpanel.starty, ScreenWidth(), layout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, layout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

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

static void setupChooserControlButtons() {
    control_padding = (ScreenWidth()-(control_num*control_size))/(control_num+1);

    btn_prev.posx = control_padding;
    btn_prev.posy = layout.buttonpanel.starty + 2;
    btn_prev.size = control_size;
    btn_next.posx = 2*control_padding + control_size;
    btn_next.posy = layout.buttonpanel.starty + 2;
    btn_next.size = control_size;
}

static void setupMenuButtons() {
    btn_home.active = true;
    btn_home.posx = 10;
    btn_home.posy = layout.menu.starty;
    btn_home.size = icon_size;
}

static void setupLayout() {
    SetPanelType(PANEL_ENABLED);
    ClearScreen();
    DrawPanel(NULL, "", "", 0);

    icon_size = PanelHeight();
    control_size = ScreenWidth()/10;
    chooser_size = ScreenWidth()/8;
    chooser_padding = (ScreenWidth()-(chooser_cols*chooser_size))/(chooser_cols+1);
    chooser_lastpage = num_games / (chooser_cols * chooser_rows);
    current_chooserpage = 0;

    layout.with_statusbar = false;
    layout.menu.starty = 0;
    layout.menu.height = PanelHeight() + 2;

    layout.statusbar.height = kFontSize + 40;
    layout.statusbar.starty = ScreenHeight()-PanelHeight()-layout.statusbar.height;

    layout.buttonpanel.height = control_size + 5;
    layout.buttonpanel.starty = ScreenHeight()-PanelHeight()-layout.buttonpanel.height;

    layout.buttonpanel_sb.height = control_size + 5;
    layout.buttonpanel_sb.starty = layout.statusbar.starty - layout.buttonpanel_sb.height;

    layout.maincanvas.starty = PanelHeight() + 3;
    layout.maincanvas.height = ScreenHeight() - 2*PanelHeight() - 5;

    layout.maincanvas_sb.starty = PanelHeight() + 3;
    layout.maincanvas_sb.height = layout.buttonpanel_sb.starty - layout.maincanvas_sb.starty;
}

static void tap(int x, int y) {
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

static void long_tap(int x, int y) {
}

static void release(int x, int y) {
    int i;
    if (coord_in_button(init_tap_x, init_tap_y, &btn_home)) button_to_normal(&btn_home, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_prev)) button_to_normal(&btn_prev, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_next)) button_to_normal(&btn_next, true);
    for (i=0;i<num_games;i++) {
        if (coord_in_button(init_tap_x, init_tap_y, &btn_chooser[i])) {
            button_to_normal(&btn_chooser[i], true);
            break;
        }
    }

    if (coord_in_button(init_tap_x, init_tap_y, &btn_home) &&
        coord_in_button(x, y, &btn_home)) {
        exitApp();
    }
    if (coord_in_button(init_tap_x, init_tap_y, &btn_prev) &&
        coord_in_button(x, y, &btn_prev) &&
        (current_chooserpage > 0)) {
            current_chooserpage -= 1;
            showChooserPage();
    }
    if (coord_in_button(init_tap_x, init_tap_y, &btn_next) &&
        coord_in_button(x, y, &btn_next) &&
        (current_chooserpage <= chooser_lastpage)) {
            current_chooserpage += 1;
            showChooserPage();
    }
}

static void showChooserPage() {
    drawChooserButtons(current_chooserpage);
    drawChooserControlButtons(current_chooserpage);
    FullUpdate();
}

static void setupApp() {
    setupLayout();
    setupMenuButtons();
    drawChooserTop();
    setupChooserButtons();
    setupChooserControlButtons();
    showChooserPage();
}

static void exitApp() {
    CloseApp();
}

static int main_handler(int event_type, int param_one, int param_two) {
    if (event_type == EVT_INIT) {
        setupApp();
    }
    else if (event_type == EVT_EXIT || (event_type == EVT_HIDE) ||
            (event_type == EVT_KEYPRESS && param_one == IV_KEY_HOME)) {
        exitApp();
    }
    else if (event_type == EVT_POINTERDOWN) {
        tap(param_one, param_two);
    }
    else if (event_type == EVT_POINTERLONG) {
        long_tap(param_one, param_two);
    }
    else if (event_type == EVT_POINTERUP) {
        release(param_one, param_two);
    }
    return 0;
}

int main (int argc, char* argv[]) {
    InkViewMain(main_handler);
    return 0;
}
