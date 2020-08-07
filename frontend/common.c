#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "common.h"

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
    StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap, 0);
    if (update) PartialUpdate(button->posx, button->posy, button->size, button->size);
}
void button_to_tapped(BUTTON *button) {
    if (button->bitmap_tap == NULL)
        InvertArea(button->posx, button->posy, button->size, button->size);
    else
        StretchBitmap(button->posx, button->posy, button->size, button->size, button->bitmap_tap, 0);
    PartialUpdate(button->posx, button->posy, button->size, button->size);
}
void button_to_cleared(BUTTON *button, bool update) {
    FillArea(button->posx, button->posy, button->size, button->size, 0x00FFFFFF);
    if (update) PartialUpdate(button->posx, button->posy, button->size, button->size);
}


