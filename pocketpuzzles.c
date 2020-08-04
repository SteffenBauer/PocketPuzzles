#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "pocketpuzzles.h"

static bool coord_in_button(int x, int y, BUTTON *button) {
    return ((x>=button->posx) && (x<(button->posx+button->size)) &&
            (y>=button->posy) && (y<(button->posy+button->size)));
}

static void button_to_normal(BUTTON *button) {
    StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}
static void button_to_tapped(BUTTON *button) {
    if (button->bitmap_tap == NULL)
        InvertArea(button->posx, button->posy, button->size, button->size);
    else
        StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap_tap, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}

static void setupMenuButtons() {
    icon_size = PanelHeight();

    bt_home.posx = 10;
    bt_home.posy = start_y_top;
    bt_home.size = icon_size; 
}

static void setupChooserLayout() {
    SetPanelType(PANEL_ENABLED);
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    start_y_top = 0;
    height_top   = PanelHeight()+2;

    start_y_chooser = PanelHeight()+3;
    height_chooser = ScreenHeight() - PanelHeight() - start_y_chooser - 2;
}

static void drawChooserTop() {
    FillArea(0, start_y_top, ScreenWidth(), height_top, 0x00FFFFFF);
    FillArea(0, start_y_top+height_top-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&bt_home);

    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(0, (icon_size/2)-(3*kFontSize/4), ScreenWidth(), kFontSize, "PUZZLES", ALIGN_CENTER);
    CloseFont(font);
}

static void setupChooserButtons() {
    int i;
    int c,r;

    icon_size = PanelHeight();
    button_size = ScreenWidth()/8;
    chooser_padding = (ScreenWidth()-(chooser_x_num*button_size))/(chooser_x_num+1);

    for(i=0;i<num_games;i++) {
        c = i % chooser_x_num;
        r = i / chooser_x_num;
        bt_chooser[i].button.active = true;
        bt_chooser[i].button.posx = (c+1)*chooser_padding + c*button_size;
        bt_chooser[i].button.posy = 10 + start_y_chooser + r*(20+kFontSize+button_size);
        bt_chooser[i].button.size = button_size;
    }
}

static void drawChooserButtons() {
    int i;
    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    for(i=0;i<num_games;i++) {
        CHOOSER chooser = bt_chooser[i];
        button_to_normal(&chooser.button);
        DrawTextRect(chooser.button.posx-(chooser_padding/2),
                     chooser.button.posy+chooser.button.size+5,
                     chooser.button.size+chooser_padding, kFontSize,
                     chooser.title, ALIGN_CENTER);
    }
    CloseFont(font);
}

static void drawStatusBar(char *text) {
    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    int bar_top = ScreenHeight()-PanelHeight()-kFontSize-40;
    
    FillArea(0, bar_top, ScreenWidth(), kFontSize+40, 0x00FFFFFF);
    DrawTextRect(10, bar_top+8, ScreenWidth(), kFontSize, text, ALIGN_LEFT);
    PartialUpdate(0, bar_top, ScreenWidth(), kFontSize+40);
    CloseFont(font);
}

static void tap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;
    int i;
    char buf[256];
    
    snprintf(buf, 200, "Tap at %d %d", x, y);
    drawStatusBar(buf);
    
    if (coord_in_button(x, y, &bt_home)) button_to_tapped(&bt_home);
    for (i=0;i<num_games;i++) {
        if (coord_in_button(x, y, &bt_chooser[i].button)) {
            button_to_tapped(&bt_chooser[i].button);
            break;
        }
    }
}

static void long_tap(int x, int y) {
}

static void release(int x, int y) {
    int i;
    if (coord_in_button(init_tap_x, init_tap_y, &bt_home)) button_to_normal(&bt_home);
    if (coord_in_button(init_tap_x, init_tap_y, &bt_home) &&
        coord_in_button(x, y, &bt_home)) {
        exitApp();
    }
    for (i=0;i<num_games;i++) {
        if (coord_in_button(init_tap_x, init_tap_y, &bt_chooser[i].button)) {
            button_to_normal(&bt_chooser[i].button);
            break;
        }
    }
}

static void setupApp() {
    setupChooserLayout();
    setupMenuButtons();
    drawChooserTop();
    setupChooserButtons();
    drawChooserButtons();
    FullUpdate();
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

