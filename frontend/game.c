#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/game.h"
#include "frontend/state.h"
#include "frontend/gamelist.h"
#include "puzzles.h"

#define DOTTED 0xFF000000

const struct drawing_api ink_drawing = {
    ink_draw_text,
    ink_draw_rect,
    ink_draw_line,
    ink_draw_polygon,
    ink_draw_circle,
    ink_draw_update,
    ink_clip,
    ink_unclip,
    ink_start_draw,
    ink_end_draw,
    ink_status_bar,
    ink_blitter_new,
    ink_blitter_free,
    ink_blitter_save,
    ink_blitter_load,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

int convertColor(int colindex) {
    int col;
    col  = 0x010000 * (int)(255.0 * fe->colours[3*colindex+0]);
    col += 0x000100 * (int)(255.0 * fe->colours[3*colindex+1]);
    col += 0x000001 * (int)(255.0 * fe->colours[3*colindex+2]);
    return col;
}

/* ----------------------------
   Drawing callbacks
   ---------------------------- */

void ink_draw_text(void *handle, int x, int y, int fonttype, int fontsize,
               int align, int colour, const char *text) {
  ifont *tempfont;
  int sw, sh, flags;
  bool is_bold = (fonttype == FONT_FIXED) || (fonttype == FONT_VARIABLE);
  bool is_mono = (fonttype == FONT_FIXED) || (fonttype == FONT_FIXED_NORMAL);

  tempfont = OpenFont( is_mono && is_bold ? "LiberationMono-Bold" : 
                       is_bold            ? "LiberationSans-Bold" :
                       is_mono            ? "LiberationMono" :
                                            "LiberationSans",
                       fontsize, 0);

  flags = 0x000;
  if (align & ALIGN_VNORMAL) flags |= VALIGN_TOP;
  if (align & ALIGN_VCENTRE) flags |= VALIGN_MIDDLE;
  if (align & ALIGN_HLEFT)   flags |= ALIGN_LEFT;
  if (align & ALIGN_HCENTRE) flags |= ALIGN_CENTER;
  if (align & ALIGN_HRIGHT)  flags |= ALIGN_RIGHT;

  SetFont(tempfont, convertColor(colour));
  sw = StringWidth(text);
  sh = TextRectHeight(sw, text, flags);
  if      (align & ALIGN_VNORMAL) y -= sh;
  else if (align & ALIGN_VCENTRE) y -= sh/2;
  if      (align & ALIGN_HCENTRE) x -= sw/2;
  else if (align & ALIGN_HRIGHT)  x -= sw;
  /* Bad hack to fix strange vertical misalign between bold and normal fonts */
  if (!is_bold) y -= fontsize/12;

  DrawString(fe->xoffset + x, fe->yoffset + y, text);

  CloseFont(tempfont);
}

void ink_draw_rect(void *handle, int x, int y, int w, int h, int colour) {
    int i;
    for (i=0;i<h;i++) DrawLine(fe->xoffset+x,fe->yoffset+y+i,
                        fe->xoffset+x+w-1,fe->yoffset+y+i,convertColor(colour));
}
void ink_draw_rect_outline(void *handle, int x, int y, int w, int h, int colour) {
    DrawRect(fe->xoffset+x, fe->yoffset+y, w, h, convertColor(colour));
}

void ink_draw_line(void *handle, int x1, int y1, int x2, int y2, int colour) {
    DrawLine(fe->xoffset+x1, fe->yoffset+y1, fe->xoffset+x2, fe->yoffset+y2, convertColor(colour));
}

static void extendrow(int y, int x1, int y1, int x2, int y2, int *minxptr, int *maxxptr) {
  int x;
  typedef long NUM;
  NUM num;

  if (((y < y1) || (y > y2)) && ((y < y2) || (y > y1)))
    return;

  if (y1 == y2) {
    if (*minxptr > x1) *minxptr = x1;
    if (*minxptr > x2) *minxptr = x2;
    if (*maxxptr < x1) *maxxptr = x1;
    if (*maxxptr < x2) *maxxptr = x2;
    return;
  }

  if (x1 == x2) {
    if (*minxptr > x1) *minxptr = x1;
    if (*maxxptr < x1) *maxxptr = x1;
    return;
  }

  num = ((NUM) (y - y1)) * (x2 - x1);
  x = x1 + num / (y2 - y1);
  if (*minxptr > x) *minxptr = x;
  if (*maxxptr < x) *maxxptr = x;
}

void ink_draw_polygon(void *handle, const int *icoords, int npoints,
                  int fillcolour, int outlinecolour) {
  MWPOINT *coords = (MWPOINT *)icoords;

  MWPOINT *pp;
  int miny;
  int maxy;
  int minx;
  int maxx;
  int i;

  if (fillcolour!=-1) {
    if (npoints <= 0) return;

    pp = coords;
    miny = pp->y;
    maxy = pp->y;
    for (i = npoints; i-- > 0; pp++) {
        if (miny > pp->y) miny = pp->y;
        if (maxy < pp->y) maxy = pp->y;
    }

    for (; miny <= maxy; miny++) {
        minx = 32767;
        maxx = -32768;
        pp = coords;
        for (i = npoints; --i > 0; pp++)
            extendrow(miny, pp[0].x, pp[0].y, pp[1].x, pp[1].y, &minx, &maxx);
        extendrow(miny, pp[0].x, pp[0].y, coords[0].x, coords[0].y, &minx, &maxx);

        if (minx <= maxx) {
            DrawLine(fe->xoffset+minx, fe->yoffset+miny, fe->xoffset+maxx, fe->yoffset+miny, convertColor(fillcolour));
        }
    }
  }

  for (i = 0; i < npoints-1; i++) {
    DrawLine(fe->xoffset+coords[i].x, fe->yoffset+coords[i].y, fe->xoffset+coords[i+1].x, fe->yoffset+coords[i+1].y, convertColor(outlinecolour));
  }
  DrawLine(fe->xoffset+coords[i].x, fe->yoffset+coords[i].y, fe->xoffset+coords[0].x, fe->yoffset+coords[0].y, convertColor(outlinecolour));
}


void ink_draw_circle(void *handle, int cx, int cy, int radius, int fillcolour, int outlinecolour) {
  int i,x,y,yy=0-radius,xx=0;

  for (i=0; i<=2*radius; i++) {
    y=i-radius;
    x=lround(sqrt(radius*radius-y*y));

    DrawLine(fe->xoffset+cx+xx, fe->yoffset+cy+yy, fe->xoffset+cx+x, fe->yoffset+cy+y, convertColor(outlinecolour));
    DrawLine(fe->xoffset+cx-xx, fe->yoffset+cy+yy, fe->xoffset+cx-x, fe->yoffset+cy+y, convertColor(outlinecolour));

    if (fillcolour!=-1) {
        DrawLine(fe->xoffset+cx-x, fe->yoffset+cy+y, fe->xoffset+cx+x, fe->yoffset+cy+y, convertColor(fillcolour));
    }

    xx=x; yy=y;
  }
}

void ink_clip(void *handle, int x, int y, int w, int h) {
    SetClip(fe->xoffset+x, fe->yoffset + y, w, h);
}

void ink_unclip(void *handle) {
    SetClipRect(&fe->cliprect);
}

void ink_start_draw(void *handle) {
}

void ink_draw_update(void *handle, int x, int y, int w, int h) {
    /* No updates while redraw is in progress.
       Update trigger will be flagged in ink_end_draw */
}

void ink_end_draw(void *handle) {
    fe->do_update = true;
}

blitter *ink_blitter_new(void *handle, int w, int h) {
  blitter *bl = snew(blitter);
  bl->width = w;
  bl->height = h;
  bl->ibit = NULL;
  return bl;
}

void ink_blitter_free(void *handle, blitter *bl) {
  sfree(bl->ibit);
  sfree(bl);
}

void ink_blitter_save(void *handle, blitter *bl, int x, int y) {
  bl->ibit = BitmapFromScreen(fe->xoffset+x, fe->yoffset+y, bl->width, bl->height);
}

void ink_blitter_load(void *handle, blitter *bl, int x, int y) {
  DrawBitmap(fe->xoffset+x, fe->yoffset+y, bl->ibit);
}

void ink_status_bar(void *handle, const char *text) {
    if (!fe->gamelayout.with_statusbar) return;
    if (text != NULL) {
        FillArea(0, fe->gamelayout.statusbar.starty+1, ScreenWidth(), fe->gamelayout.statusbar.height-1, 0x00FFFFFF);
        SetFont(fe->gamefont, BLACK);
        DrawString(10, fe->gamelayout.statusbar.starty+12, text);
        fe->do_update = true;
    }
}

/* ----------------------------
   Midend -> Frontend callbacks
   ---------------------------- */

void frontend_default_colour(frontend *fe, float *output) {
    output[0] = 1.0F;
    output[1] = 1.0F;
    output[2] = 1.0F;
}

void get_random_seed(void **randseed, int *randseedsize) {
    struct timeval *tvp = snew(struct timeval);
    gettimeofday(tvp, NULL);
    *randseed = (void *)tvp;
    *randseedsize = sizeof(struct timeval);
}

void tproc() {
  if (fe->isTimer) {
    struct timeval now;
    float elapsed;

    gettimeofday(&now, NULL);
    elapsed = ((now.tv_usec - fe->last_time.tv_usec) * 0.000001F + (now.tv_sec - fe->last_time.tv_sec));
    midend_timer(me, elapsed);
    fe->last_time = now;
    SetWeakTimer("timername", tproc, fe->time_int);
  }
}

void activate_timer(frontend *fe) {
    /* Timed puzzles don't work really well on eInk,
       so timers are deactivated so far */
  return;
  fe->isTimer=true;
  gettimeofday(&fe->last_time, NULL);
  SetWeakTimer("timername", tproc, fe->time_int);
}

void deactivate_timer(frontend *fe) {
  if (fe->isTimer) {
      fe->isTimer=false;
      ClearTimer(tproc);
  }
}

void fatal(const char *fmt, ...) {
    exitApp();
}

/* ------------------------- */

static bool coord_in_gamecanvas(int x, int y) {
    return ((x>=fe->xoffset) && (x<(fe->xoffset+fe->width)) &&
           (y>=fe->yoffset) && (y<(fe->yoffset+fe->height)));
}

void gameMenuHandler(int index) {
    gameMenu_selectedIndex = index;

    button_to_normal(&fe->gameButton[fe->btnGameIDX], true);

    switch (index) {
        case 101:  /* New Game */
            gameStartNewGame();
            gameScreenShow();
            break;
        case 102:  /* Restart Game */
            gameRestartGame();
            break;
        case 103:  /* Solve Game */
            gameSolveGame();
            break;
        case 104:  /* How to Play */
            Dialog(0, "Rules", fe->currentgame->rules, "OK", NULL, NULL);
            break;
        case 105:  /* Exit app */
            exitApp();
            break;
        default:
            break;
    }
}

void typeMenuHandler(int index) {
    typeMenu_selectedIndex = index;
    char buf[256];
    int currentindex = midend_which_preset(me);

    button_to_normal(&fe->gameButton[fe->btnTypeIDX], true);

    if (index == 200) {
        paramPrepare(me);
        switchToParamScreen();
    }
    if (index > 200) {
        if (currentindex >= 0) typeMenu[currentindex+2].type = ITEM_ACTIVE;
        else typeMenu[1].type = ITEM_ACTIVE;
        typeMenu[index-199].type = ITEM_BULLET;
        gameSwitchPreset(index-201);
        gameScreenShow();
    }
}

static void gameBuildGameMenu() {
    int i, np;

    np = fe->currentgame->can_solve ? 7 : 6;

    sfree(gameMenu);
    gameMenu=snewn(np, imenuex);

    for (i=0;i<np;i++) {
        gameMenu[i].submenu = NULL;
        gameMenu[i].reserved = NULL;
        gameMenu[i].font = NULL;
    }

    i = 0;

    gameMenu[i].type = ITEM_HEADER;
    gameMenu[i].index = 0;
    gameMenu[i].text = "Game";
    gameMenu[i++].icon = NULL;

    gameMenu[i].type = ITEM_ACTIVE;
    gameMenu[i].index = 101;
    gameMenu[i].text = "New";
    gameMenu[i++].icon = &menu_new;

    gameMenu[i].type = ITEM_ACTIVE;
    gameMenu[i].index = 102;
    gameMenu[i].text = "Restart";
    gameMenu[i++].icon = &menu_restart;

    if (fe->currentgame->can_solve) {
        gameMenu[i].type = ITEM_ACTIVE;
        gameMenu[i].index = 103;
        gameMenu[i].text = "Show solution";
        gameMenu[i++].icon = &menu_solve;
    }

    gameMenu[i].type = ITEM_ACTIVE;
    gameMenu[i].index = 104;
    gameMenu[i].text = "How to play";
    gameMenu[i++].icon = &menu_help;

    gameMenu[i].type = ITEM_ACTIVE;
    gameMenu[i].index = 105;
    gameMenu[i].text = "Save game and exit";
    gameMenu[i++].icon = &menu_exit;

    gameMenu[i].type = 0;
    gameMenu[i].index = 0;
    gameMenu[i].text = NULL;
    gameMenu[i++].icon = NULL;
}


static void gameBuildTypeMenu() {
    int i, np, chosen;
    presets = midend_get_presets(me, NULL);

    np = presets->n_entries;

    sfree(typeMenu);
    typeMenu=snewn(np+3, imenu);

    typeMenu[0].type = ITEM_HEADER;
    typeMenu[0].index = 0;
    typeMenu[0].text = "Game presets     ";
    typeMenu[0].submenu = NULL;

    typeMenu[1].text = "Custom";
    typeMenu[1].type = ITEM_ACTIVE;
    typeMenu[1].index = 200;
    typeMenu[1].submenu = NULL;

    for (i=2; i<np+2; i++) {
      typeMenu[i].text = presets->entries[i-2].title;
      typeMenu[i].type = ITEM_ACTIVE;
      typeMenu[i].index = 199+i;
      typeMenu[i].submenu = NULL;
    }
    typeMenu[i].type = 0;
    typeMenu[i].index = 0;
    typeMenu[i].text = NULL;
    typeMenu[i].submenu = NULL;

    chosen = midend_which_preset(me);
    typeMenu[(chosen >= 0) ? chosen+2 : 1].type = ITEM_BULLET;
}

static void gameCheckButtonState() {
    BUTTON *undo, *redo, *swap;
    int i;
    undo = &fe->gameButton[fe->btnUndoIDX];
    redo = &fe->gameButton[fe->btnRedoIDX];
    if (midend_can_undo(me) && ! undo->active) activate_button(undo);
    if (midend_can_redo(me) && ! redo->active) activate_button(redo);
    if (!midend_can_undo(me) && undo->active) deactivate_button(undo);
    if (!midend_can_redo(me) && redo->active) deactivate_button(redo);
    if (fe->with_swap) {
        swap = &fe->gameButton[fe->btnSwapIDX];
        fe->swapped ? button_to_tapped(swap, false) : button_to_normal(swap, false);
    }
    for (i=0;i<fe->numGameButtons;i++) {
        if (fe->gameButton[i].action == ACTION_CTRL)
            if (midend_is_key_highlighted(me, fe->gameButton[i].actionParm.c))
                button_to_tapped(&fe->gameButton[i], false);
            else
                button_to_normal(&fe->gameButton[i], false);
    }
}

/* ----------------------------
   Screen event callbacks
   ---------------------------- */

void gameTap(int x, int y) {
    int i;
    bool is_active;
    init_tap_x = x;
    init_tap_y = y;

    for (i=0;i<fe->numGameButtons;i++) {
        if ((fe->gameButton[i].action != ACTION_SWAP) &&
            coord_in_button(x, y, &fe->gameButton[i])) {
            is_active = false;
            if (fe->gameButton[i].type == BTN_CHAR)
                is_active = midend_is_key_highlighted(me, fe->gameButton[i].actionParm.c);
            if (is_active) button_to_normal(&fe->gameButton[i], true);
            else           button_to_tapped(&fe->gameButton[i], true);
        }
        if ((fe->gameButton[i].action == ACTION_SWAP) &&
            coord_in_button(x, y, &fe->gameButton[i])) {
            if (fe->swapped) button_to_normal(&fe->gameButton[i], true);
            else             button_to_tapped(&fe->gameButton[i], true);
        }
    }


    if (coord_in_gamecanvas(x, y) && (fe->current_pointer == 0)) {
        fe->pointerdown_x = x;
        fe->pointerdown_y = y;
        if (fe->with_rightpointer)
            fe->current_pointer = (fe->swapped ? RIGHT_BUTTON : LEFT_BUTTON);
        else {
            fe->current_pointer = LEFT_BUTTON;
            fe->do_update = false;
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_BUTTON, fe->swapped);
            gameDrawFurniture();
        }
    }
}

void gameLongTap(int x, int y) {
    if (coord_in_gamecanvas(x, y) && fe->with_rightpointer) {
        fe->do_update = false;
        midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), 
                           (fe->swapped ? LEFT_BUTTON : RIGHT_BUTTON), fe->swapped);
        fe->current_pointer = (fe->swapped ? LEFT_BUTTON : RIGHT_BUTTON);
        gameDrawFurniture();
    }
}

void gameDrag(int x, int y) {
    fe->do_update = false;
    if (coord_in_gamecanvas(x, y)) {
        if (fe->current_pointer == LEFT_BUTTON) {
            if (fe->with_rightpointer && !fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), 
                    (fe->pointerdown_y)-(fe->yoffset), LEFT_BUTTON, fe->swapped);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_DRAG, fe->swapped);
            fe->current_pointer = LEFT_DRAG;
        }
        else if (fe->current_pointer == LEFT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_DRAG, fe->swapped);
        }
        else if (fe->current_pointer == RIGHT_BUTTON) {
            if (fe->with_rightpointer && fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset),
                    (fe->pointerdown_y)-(fe->yoffset), RIGHT_BUTTON, fe->swapped);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_DRAG, fe->swapped);
            fe->current_pointer = RIGHT_DRAG;
        }
        else if (fe->current_pointer == RIGHT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_DRAG, fe->swapped);
            fe->current_pointer = RIGHT_DRAG;
        }
    }
    gameDrawFurniture();
}

void gameRelease(int x, int y) {
    int i;
    fe->do_update = false;
    if (coord_in_gamecanvas(init_tap_x, init_tap_y)) {
        if (fe->current_pointer == LEFT_BUTTON) {
            if (fe->with_rightpointer && !fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset),
                    (fe->pointerdown_y)-(fe->yoffset), LEFT_BUTTON, fe->swapped);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_RELEASE, fe->swapped);
        }
        else if (fe->current_pointer == LEFT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_RELEASE, fe->swapped);
        }
        else if (fe->current_pointer == RIGHT_BUTTON) {
            if (fe->with_rightpointer && fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset),
                    (fe->pointerdown_y)-(fe->yoffset), RIGHT_BUTTON, fe->swapped);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_RELEASE, fe->swapped);
        }
        else if (fe->current_pointer == RIGHT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_RELEASE, fe->swapped);
        }
        fe->current_pointer = 0;
    }
    for (i=0;i<fe->numGameButtons;i++) {
        if (release_button(init_tap_x, init_tap_y, &fe->gameButton[i])) {
            if ((fe->gameButton[i].action != ACTION_SWAP) && (fe->gameButton[i].action != ACTION_CTRL))
                button_to_normal(&fe->gameButton[i], false);
            if (release_button(x, y, &fe->gameButton[i])) {
                switch(fe->gameButton[i].action) {
                    case ACTION_BACK:
                        gameSerialise();
                        switchToChooserScreen();
                        return;
                    case ACTION_DRAW:
                        FullUpdate();
                        return;
                    case ACTION_GAME:
                        OpenMenuEx(gameMenu, gameMenu_selectedIndex, 
                            ScreenWidth()-20-(2*fe->gamelayout.menubtn_size),
                            fe->gamelayout.menubtn_size+2, gameMenuHandler);
                        return;
                    case ACTION_TYPE:
                        OpenMenu(typeMenu, typeMenu_selectedIndex, 
                            ScreenWidth()-10-fe->gamelayout.menubtn_size,
                            fe->gamelayout.menubtn_size+2, typeMenuHandler);
                        return;
                    case ACTION_CTRL:
                        midend_process_key(me, 0, 0, fe->gameButton[i].actionParm.c, fe->swapped);
                        break;
                    case ACTION_UNDO:
                        if (midend_can_undo(me)) midend_process_key(me, 0, 0 , UI_UNDO, fe->swapped);
                        break;
                    case ACTION_REDO:
                        if (midend_can_redo(me)) midend_process_key(me, 0, 0, UI_REDO, fe->swapped);
                        break;
                    case ACTION_SWAP:
                        fe->swapped = !fe->swapped;
                        break;
                }
            }
        }
    }
    gameDrawFurniture();
    checkGameEnd();
}

void gamePrev() {
    fe->do_update = false;
    if (midend_can_undo(me)) midend_process_key(me, 0, 0 , UI_UNDO, fe->swapped);
    gameDrawFurniture();
}

void gameNext() {
    fe->do_update = false;
    if (midend_can_redo(me)) midend_process_key(me, 0, 0, UI_REDO, fe->swapped);
    gameDrawFurniture();
}

static void gameDrawControlButtons() {
    int i;
    FillArea(0, fe->gamelayout.buttonpanel.starty, ScreenWidth(), fe->gamelayout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

    for (i=0;i<fe->numGameButtons;i++)
        button_to_normal(&fe->gameButton[i], false);

    deactivate_button(&fe->gameButton[fe->btnUndoIDX]);
    deactivate_button(&fe->gameButton[fe->btnRedoIDX]);
    gameCheckButtonState();
}

static void gameDrawMenu() {
    FillArea(0, fe->gamelayout.menu.starty, ScreenWidth(), fe->gamelayout.menu.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.menu.starty + fe->gamelayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    SetFont(fe->gamefont, BLACK);
    DrawTextRect(0, (fe->gamelayout.menubtn_size/2)-(fe->gfontsize/2), ScreenWidth(), fe->gfontsize, fe->currentgame->name, ALIGN_CENTER);

    button_to_normal(&fe->gameButton[fe->btnBackIDX], false);
    button_to_normal(&fe->gameButton[fe->btnDrawIDX], false);
    button_to_normal(&fe->gameButton[fe->btnGameIDX], false);
    button_to_normal(&fe->gameButton[fe->btnTypeIDX], false);
}
static void gameDrawStatusBar() {
    if (!fe->gamelayout.with_statusbar) return;
    FillArea(0, fe->gamelayout.statusbar.starty+1, ScreenWidth(), fe->gamelayout.statusbar.height-1, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.statusbar.starty, ScreenWidth(), 1, 0x00000000);
}

static void gameSetupMenuButtons() {
    fe->gameButton[fe->btnBackIDX].active = true;
    fe->gameButton[fe->btnBackIDX].posx = fe->gamelayout.menubtn_size/4;
    fe->gameButton[fe->btnBackIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnBackIDX].size = fe->gamelayout.menubtn_size;

    fe->gameButton[fe->btnDrawIDX].active = true;
    fe->gameButton[fe->btnDrawIDX].posx = ScreenWidth() - (28*fe->gamelayout.menubtn_size)/8;
    fe->gameButton[fe->btnDrawIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnDrawIDX].size = fe->gamelayout.menubtn_size;

    fe->gameButton[fe->btnGameIDX].active = true;
    fe->gameButton[fe->btnGameIDX].posx = ScreenWidth() - (19*fe->gamelayout.menubtn_size)/8;
    fe->gameButton[fe->btnGameIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnGameIDX].size = fe->gamelayout.menubtn_size;

    fe->gameButton[fe->btnTypeIDX].active = true;
    fe->gameButton[fe->btnTypeIDX].posx = ScreenWidth() - (10*fe->gamelayout.menubtn_size)/8;
    fe->gameButton[fe->btnTypeIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnTypeIDX].size = fe->gamelayout.menubtn_size;
}

static void gameSetupControlButtons() {
    int i;
    int btn_num1, btn_num2;
    int pad1, pad2;

    if (strcmp(fe->currentgame->name, "Rome")==0) {
        btn_num1 = 4; btn_num2 = fe->numGameButtons - btn_num1 - 4;
    }
    else if (fe->with_twoctrllines) {
        btn_num1 = 0;
        for (i=0;i<fe->numGameButtons-4;i++) {
            if (fe->gameButton[i].type == BTN_CHAR) btn_num1++;
        }
        btn_num2 = fe->numGameButtons - btn_num1 - 4;
    }
    else {
        btn_num1 = fe->numGameButtons - 4;
        btn_num2 = 0;
    }
    pad1 = (ScreenWidth()-(btn_num1 * fe->gamelayout.control_size))/(btn_num1+1);
    pad2 = (ScreenWidth()-(btn_num2 * fe->gamelayout.control_size))/(btn_num2+1);

    for (i=0;i<fe->numGameButtons-4;i++) {
        fe->gameButton[i].active = true;
        fe->gameButton[i].page = (i<btn_num1) ? 1 : 2;
        if (i < btn_num1) {
            fe->gameButton[i].posx = i*fe->gamelayout.control_size + (i+1)*pad1;
            fe->gameButton[i].posy = fe->gamelayout.buttonpanel.starty + 2;
        }
        else {
            fe->gameButton[i].posx = (i-btn_num1)*fe->gamelayout.control_size + (i-btn_num1+1)*pad2;
            fe->gameButton[i].posy = fe->gamelayout.buttonpanel.starty + fe->gamelayout.control_size + 4;
        }
        fe->gameButton[i].size = fe->gamelayout.control_size;
    }
}

static void checkGameEnd() {
    int status;
    if (!fe->finished) {
        status = midend_status(me);
        if (status == 1) {
            Message(ICON_INFORMATION, "", "Puzzle is solved!", 2000);
            fe->finished = true;
        }
        else if (status == -1) {
            Message(ICON_WARNING, "", "Puzzle is lost!", 2000);
            fe->finished = true;
        }
    }
}

static void gamePrepareFrontend() {
    char buf[256];

    fe->current_pointer = 0;
    fe->pointerdown_x = 0;
    fe->pointerdown_y = 0;
    fe->swapped = false;
    fe->colours = midend_colours(me, &fe->ncolours);
    fe->finished = false;
    fe->isTimer = false;
    fe->time_int = 20;

    fe->gamelayout = getLayout(gameGetLayout());

    gameSetupMenuButtons();
    gameSetupControlButtons();
    gameBuildGameMenu();
    gameBuildTypeMenu();

    int x, y;
    x = ScreenWidth();
    y = fe->gamelayout.maincanvas.height;
    midend_size(me, &x, &y, true);
    fe->width  = x;
    fe->height = y;
    fe->xoffset = (ScreenWidth() - fe->width)/2;
    fe->yoffset = fe->gamelayout.maincanvas.starty + (fe->gamelayout.maincanvas.height - fe->height) / 2 ;
}

static BUTTON gameGetButton(const char *gameName, char key) {
    if      (key == '\b') return btn_backspace;
    else if (key == '+' && strcmp(gameName, "Map") == 0)      return btn_fill_map;
    else if (key == '+' && strcmp(gameName, "Rome") == 0)     return btn_fill_rome;
    else if (key == '+' && strcmp(gameName, "Undead") == 0)   return btn_fill_marks;
    else if (key == '+' && strcmp(gameName, "ABCD") == 0)     return btn_fill_marks;
    else if (key == '+' && strcmp(gameName, "Group") == 0)    return btn_fill_marks;
    else if (key == '+' && strcmp(gameName, "Salad") == 0)    return btn_fill_marks;
    else if (key == '+' && strcmp(gameName, "CrossNum") == 0) return btn_fill_nums;
    else if (key == '+' && strcmp(gameName, "Keen") == 0)     return btn_fill_nums;
    else if (key == '+' && strcmp(gameName, "Mathrax") == 0)  return btn_fill_nums;
    else if (key == '+' && strcmp(gameName, "Towers") == 0)   return btn_fill_nums;
    else if (key == '+' && strcmp(gameName, "Unequal") == 0)  return btn_fill_nums;

    else if (key == '+')  return btn_add;
    else if (key == '-')  return btn_remove;

    else if (key == 'O' && strcmp(gameName, "Salad")==0)   return btn_salad_o;
    else if (key == 'X' && strcmp(gameName, "Salad")==0)   return btn_salad_x;
    else if (key == 'J' && strcmp(gameName, "Net")==0)     return btn_net_shuffle;
    else if (key == 'G' && strcmp(gameName, "Bridges")==0) return btn_bridges_g;
    else if (key == 'T' && strcmp(gameName, "Rome")==0)    return btn_rome_n;
    else if (key == 'W' && strcmp(gameName, "Rome")==0)    return btn_rome_w;
    else if (key == 'E' && strcmp(gameName, "Rome")==0)    return btn_rome_e;
    else if (key == 'D' && strcmp(gameName, "Rome")==0)    return btn_rome_s;
    return btn_null;
}

static LAYOUTTYPE gameGetLayout() {
    bool wants_statusbar;
    int i, addkeys, nkeys = 0;
    struct key_label *keys;

    fe->with_rightpointer = fe->currentgame->flags & REQUIRE_RBUTTON;
    fe->with_swap = fe->with_rightpointer;
    if (strcmp(fe->currentgame->name, "Ascent")==0 ||
        strcmp(fe->currentgame->name, "Signpost")==0)
        fe->with_swap = false;

    sfree(fe->gameButton);
    keys = midend_request_keys(me, &nkeys);
    if (keys == NULL) nkeys = 0;

    addkeys = fe->with_swap ? 3 : 2;
    fe->numGameButtons = 4 + nkeys + addkeys;
    fe->with_twoctrllines = (nkeys+addkeys) > 9;
    fe->with_statusbar = midend_wants_statusbar(me);

    if (strcmp(fe->currentgame->name, "ABCD")==0)     fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Dominosa")==0) fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Group")==0)    fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "CrossNum")==0) fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Keen")==0)     fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Mathrax")==0)  fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Rome")==0)     fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Salad")==0)    fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Solo")==0)     fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Towers")==0)   fe->with_twoctrllines = true;
    if (strcmp(fe->currentgame->name, "Unequal")==0)  fe->with_twoctrllines = true;

    fe->gameButton = smalloc((fe->numGameButtons) * sizeof(BUTTON));

    for (i=0;i<nkeys;i++) {
        fe->gameButton[i] = gameGetButton(fe->currentgame->name, keys[i].button);
        if (fe->gameButton[i].type == BTN_NULL) {
            fe->gameButton[i].type   = BTN_CHAR;
            fe->gameButton[i].action = ACTION_CTRL;
            fe->gameButton[i].actionParm.c = keys[i].button;
            fe->gameButton[i].bitmap = NULL;
            fe->gameButton[i].bitmap_tap = NULL;
            fe->gameButton[i].bitmap_disabled = NULL;
        }
    }

    if (fe->with_swap)
        fe->gameButton[fe->btnSwapIDX = i++] = btn_swap;
    fe->gameButton[fe->btnUndoIDX = i++] = btn_undo;
    fe->gameButton[fe->btnRedoIDX = i++] = btn_redo;

    fe->gameButton[fe->btnBackIDX = i++] = btn_back;
    fe->gameButton[fe->btnDrawIDX = i++] = btn_draw;
    fe->gameButton[fe->btnGameIDX = i++] = btn_game;
    fe->gameButton[fe->btnTypeIDX = i++] = btn_type;

    sfree(keys);

    if (fe->with_statusbar && !fe->with_twoctrllines)  return LAYOUT_BOTH;
    if (fe->with_statusbar && fe->with_twoctrllines)   return LAYOUT_2XBOTH;
    if (!fe->with_statusbar && !fe->with_twoctrllines) return LAYOUT_BUTTONBAR;
    if (!fe->with_statusbar && fe->with_twoctrllines)  return LAYOUT_2XBUTTONBAR;
    return LAYOUT_2XBOTH; /* default */
}

static void gameDrawFurniture() {
    if (fe->do_update) {
        gameCheckButtonState();
        SoftUpdate();
    }
    fe->do_update = false;
}

static void gameRestartGame() {
    fe->do_update = true;
    midend_restart_game(me);
    fe->finished = false;
    gameDrawFurniture();
}

static void gameSolveGame() {
    const char *errorMsg;
    fe->do_update = true;
    errorMsg = midend_solve(me);
    if (errorMsg) Message(ICON_WARNING, "", errorMsg, 3000);
    else fe->finished = true;
    gameDrawFurniture();
}

static void gameSwitchPreset(int index) {
    midend_set_params(me, presets->entries[index].params);
    gameStartNewGame();
}

void gameStartNewGame() {
    ShowPureHourglassForce();
    midend_new_game(me);
    HideHourglass();
    gamePrepareFrontend();
}

bool gameResumeGame() {
    char *name;
    const char *result;
    bool validSavegame = false;
    int i = 0;
    result = stateGamesaveName(&name);
    if (result == NULL && name != NULL) {
        while (mygames[i].thegame != NULL) {
            if (strcmp(mygames[i].thegame->name, name) == 0) {
                gameSetGame(mygames[i].thegame);
                if (stateDeserialise(me) == NULL) {
                    fe->finished = (midend_status(me) != 0);
                }
                else {
                    ShowPureHourglassForce();
                    midend_new_game(me);
                    HideHourglass();
                }
                validSavegame = true;
                break;
            }
            i++;
        }
    }
    if (!validSavegame) {
        return false;
    }
    sfree(name);
    gamePrepareFrontend();
    return true;
}

void gameSetGame(const struct game *thegame) {
    fe->currentgame = thegame;
    if (me != NULL) midend_free(me);
    me = midend_new(fe, thegame, &ink_drawing, fe);
    stateLoadParams(me, thegame);
}

void gameScreenShow() {
    SetClipRect(&fe->cliprect);
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    gameDrawMenu();
    midend_force_redraw(me);
    gameDrawControlButtons();
    gameDrawStatusBar();
    ink_status_bar(NULL, midend_get_statustext(me));
    FullUpdate();
}

void gameScreenInit() {
    fe = snew(frontend);
    fe->cliprect = GetClipRect();
    fe->gfontsize = (int)(ScreenWidth()/30);
    fe->gamefont = OpenFont("LiberationSans-Bold", fe->gfontsize, 0);
    fe->gameButton = NULL;
    fe->do_update = false;
    fe->isTimer = false;
    gameMenu = NULL;
    typeMenu = NULL;
    me = NULL;
    gameInitialized = true;
}

void gameSerialise() {
    deactivate_timer(fe);
    stateSerialise(me);
    stateSaveParams(me, fe->currentgame);
    configAddItem("config_resume", "game");
}

void gameScreenFree() {
    if (gameInitialized) {
        CloseFont(fe->gamefont);
        SetClipRect(&fe->cliprect);
        deactivate_timer(fe);
        sfree(fe->gameButton);
        sfree(fe);
        sfree(gameMenu);
        sfree(typeMenu);
        if (me != NULL) midend_free(me);
        gameInitialized = false;
        /* Message(ICON_INFORMATION, "", "Exit app", 1000); */
    }
}

