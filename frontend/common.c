#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/common.h"

bool coord_in_button(int x, int y, BUTTON *button) {
    return (button->active && 
            (x>=button->posx) && (x<(button->posx+button->size)) &&
            (y>=button->posy) && (y<(button->posy+button->size)));
}
bool release_button(int x, int y, BUTTON *button) {
    return (button->active && 
            coord_in_button(init_tap_x, init_tap_y, button) &&
            coord_in_button(x, y, button));
}
void button_to_normal(BUTTON *button, bool update) {
    if (button->bitmap != NULL) {
        StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap, 0);
    }
    else {
        ifont *font; char buf[2];
        buf[0] = button->c; buf[1] = '\0';
        font = OpenFont("LiberationMono-Bold", button->size/2, 0);
        SetFont(font, BLACK);
        FillArea(button->posx, button->posy, button->size, button->size, 0x00FFFFFF);
        DrawTextRect(button->posx, button->posy, button->size, button->size, buf, ALIGN_CENTER | VALIGN_MIDDLE);
        CloseFont(font);
    }
    if (update) PartialUpdate(button->posx, button->posy, button->size, button->size);
}
void button_to_tapped(BUTTON *button) {
    if (button->bitmap == NULL) {
        ifont *font; char buf[2];
        buf[0] = button->c; buf[1] = '\0';
        font = OpenFont("LiberationMono-Bold", button->size/2, 0);
        SetFont(font, BLACK);
        FillArea(button->posx, button->posy, button->size, button->size, 0x00FFFFFF);
        DrawTextRect(button->posx, button->posy, button->size, button->size, buf, ALIGN_CENTER | VALIGN_MIDDLE);
        InvertArea(button->posx, button->posy, button->size, button->size);
        CloseFont(font);
    }
    else if (button->bitmap_tap == NULL) {
        StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap, 0);
        InvertArea(button->posx, button->posy, button->size, button->size);
    }
    else
        StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap_tap, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}
void button_to_cleared(BUTTON *button, bool update) {
    FillArea(button->posx, button->posy, button->size, button->size, 0x00FFFFFF);
    if (update) PartialUpdate(button->posx, button->posy, button->size, button->size);
}
void activate_button(BUTTON *button) {
    button->active = true;
    button_to_normal(button, true);
}
void deactivate_button(BUTTON *button) {
    button->active = false;
    StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap_disabled, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}


struct layout getLayout(LAYOUTTYPE screenlayout) {
    int bottomy = ScreenHeight()-PanelHeight();
    struct layout requestedLayout;

    requestedLayout.menubtn_size = PanelHeight();
    requestedLayout.control_size = ScreenWidth()/12;
    requestedLayout.chooser_size = ScreenWidth()/8;

    requestedLayout.menu.starty = 0;
    requestedLayout.menu.height = PanelHeight() + 2;
    requestedLayout.statusbar.height = 32 + 40;
    requestedLayout.statusbar.starty = bottomy - requestedLayout.statusbar.height;

    switch (screenlayout) {
        case LAYOUT_FULL:
            requestedLayout.with_statusbar     = false;
            requestedLayout.with_buttonbar     = false;
            requestedLayout.with_2xbuttonbar   = false;
            requestedLayout.buttonpanel.height = 0;
            requestedLayout.buttonpanel.starty = bottomy;
            break;

        case LAYOUT_STATUSBAR:
            requestedLayout.with_statusbar     = true;
            requestedLayout.with_buttonbar     = false;
            requestedLayout.with_2xbuttonbar   = false;
            requestedLayout.buttonpanel.height = 0;
            requestedLayout.buttonpanel.starty = requestedLayout.statusbar.starty - 1;
            break;

        case LAYOUT_BUTTONBAR:
            requestedLayout.with_statusbar     = false;
            requestedLayout.with_buttonbar     = true;
            requestedLayout.with_2xbuttonbar   = false;
            requestedLayout.buttonpanel.height = requestedLayout.control_size + 5;
            requestedLayout.buttonpanel.starty = bottomy - requestedLayout.buttonpanel.height;
            break;

        case LAYOUT_BOTH:
            requestedLayout.with_statusbar     = true;
            requestedLayout.with_buttonbar     = true;
            requestedLayout.with_2xbuttonbar   = false;
            requestedLayout.buttonpanel.height = requestedLayout.control_size + 5;
            requestedLayout.buttonpanel.starty = requestedLayout.statusbar.starty - requestedLayout.buttonpanel.height - 1;
            break;

        case LAYOUT_2XBUTTONBAR:
            requestedLayout.with_statusbar     = false;
            requestedLayout.with_buttonbar     = true;
            requestedLayout.with_2xbuttonbar   = true;
            requestedLayout.buttonpanel.height = 2 * requestedLayout.control_size + 10;
            requestedLayout.buttonpanel.starty = bottomy - requestedLayout.buttonpanel.height - 1;
            break;

        case LAYOUT_2XBOTH:
            requestedLayout.with_statusbar     = true;
            requestedLayout.with_buttonbar     = true;
            requestedLayout.with_2xbuttonbar   = true;
            requestedLayout.buttonpanel.height = 2 * requestedLayout.control_size + 10;
            requestedLayout.buttonpanel.starty = requestedLayout.statusbar.starty - requestedLayout.buttonpanel.height - 1;
            break;
    }
    requestedLayout.maincanvas.starty  = requestedLayout.menu.height + 3;
    requestedLayout.maincanvas.height  = requestedLayout.buttonpanel.starty - requestedLayout.maincanvas.starty - 1;
    return requestedLayout;
}

