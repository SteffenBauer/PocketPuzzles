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

/*
bool dottedColor(int colindex) {
    return (fe->colours[3*colindex+0] != fe->colours[3*colindex+1]);
}
*/

/* ----------------------------
   Drawing callbacks
   ---------------------------- */

void ink_draw_text(void *handle, int x, int y, int fonttype, int fontsize,
               int align, int colour, const char *text) {
  ifont *tempfont;
  int sw, sh;
  tempfont = OpenFont(fonttype == FONT_FIXED ? "LiberationMono-Bold" : "LiberationSans-Bold",
                      fontsize, 0);

  SetFont(tempfont, convertColor(colour));
  sw=StringWidth(text);
  sh=TextRectHeight(sw, text, 0);
  if (align & ALIGN_VNORMAL) y -= sh;
  else if (align & ALIGN_VCENTRE) y -= sh/2;
  if (align & ALIGN_HCENTRE) x -= sw/2;
  else if (align & ALIGN_HRIGHT) x -= sw;

  DrawString(fe->xoffset + x, fe->yoffset + y, text);
  CloseFont(tempfont);
}

void ink_draw_rect(void *handle, int x, int y, int w, int h, int colour) {
    int i;
    for (i=0;i<h;i++) DrawLine(fe->xoffset+x,fe->yoffset+y+i,fe->xoffset+x+w-1,fe->yoffset+y+i,convertColor(colour));
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

void ink_draw_polygon(void *handle, int *icoords, int npoints,
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
    /* No updates while redraw is in progress. Update will be done in ink_end_draw */
}

void ink_end_draw(void *handle) {
    SoftUpdate();
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
  fe->statustext = text;
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
        case 199:  /* Exit app */
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
    undo = &fe->gameButton[fe->btnUndoIDX];
    redo = &fe->gameButton[fe->btnRedoIDX];
    if (midend_can_undo(me) && ! undo->active) activate_button(undo);
    if (midend_can_redo(me) && ! redo->active) activate_button(redo);
    if (!midend_can_undo(me) && undo->active) deactivate_button(undo);
    if (!midend_can_redo(me) && redo->active) deactivate_button(redo);
    if (fe->with_rightpointer) {
        swap = &fe->gameButton[fe->btnSwapIDX];
        fe->swapped ? button_to_tapped(swap) : button_to_normal(swap, false);
    }
}

/* ----------------------------
   Screen event callbacks
   ---------------------------- */

void gameTap(int x, int y) {
    int i;
    init_tap_x = x;
    init_tap_y = y;

    for (i=0;i<fe->numGameButtons;i++) {
        if ((fe->gameButton[i].action != ACTION_SWAP) && coord_in_button(x, y, &fe->gameButton[i]))
            button_to_tapped(&fe->gameButton[i]);
    }

    if (coord_in_gamecanvas(x, y) && (fe->current_pointer == 0)) {
        fe->pointerdown_x = x;
        fe->pointerdown_y = y;
        if (fe->with_rightpointer)
            fe->current_pointer = (fe->swapped ? RIGHT_BUTTON : LEFT_BUTTON);
        else {
            fe->current_pointer = LEFT_BUTTON;
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_BUTTON);
        }
    }
}

void gameLongTap(int x, int y) {
    if (coord_in_gamecanvas(x, y) && fe->with_rightpointer) {
        midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), 
                           (fe->swapped ? LEFT_BUTTON : RIGHT_BUTTON));
        fe->current_pointer = (fe->swapped ? LEFT_BUTTON : RIGHT_BUTTON);
    }
}

void gameDrag(int x, int y) {
    if (coord_in_gamecanvas(x, y)) {
        if (fe->current_pointer == LEFT_BUTTON) {
            if (fe->with_rightpointer && !fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), (fe->pointerdown_y)-(fe->yoffset), LEFT_BUTTON);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_DRAG);
            fe->current_pointer = LEFT_DRAG;
        }
        else if (fe->current_pointer == LEFT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_DRAG);
        }
        else if (fe->current_pointer == RIGHT_BUTTON) {
            if (fe->with_rightpointer && fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), (fe->pointerdown_y)-(fe->yoffset), RIGHT_BUTTON);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_DRAG);
            fe->current_pointer = RIGHT_DRAG;
        }
        else if (fe->current_pointer == RIGHT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_DRAG);
            fe->current_pointer = RIGHT_DRAG;
        }
    }
}

void gameRelease(int x, int y) {
    int i;

    if (coord_in_gamecanvas(init_tap_x, init_tap_y)) {
        if (fe->current_pointer == LEFT_BUTTON) {
            if (fe->with_rightpointer && !fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), (fe->pointerdown_y)-(fe->yoffset), LEFT_BUTTON);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_RELEASE);
        }
        else if (fe->current_pointer == LEFT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_RELEASE);
        }
        else if (fe->current_pointer == RIGHT_BUTTON) {
            if (fe->with_rightpointer && fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), (fe->pointerdown_y)-(fe->yoffset), RIGHT_BUTTON);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_RELEASE);
        }
        else if (fe->current_pointer == RIGHT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_RELEASE);
        }
        fe->current_pointer = 0;
    }
    for (i=0;i<fe->numGameButtons;i++) {
        if (release_button(init_tap_x, init_tap_y, &fe->gameButton[i])) {
            if (fe->gameButton[i].action != ACTION_SWAP)
                button_to_normal(&fe->gameButton[i], true);
            if (release_button(x, y, &fe->gameButton[i])) {
                switch(fe->gameButton[i].action) {
                    case ACTION_BACK:
                        gameSerialise();
                        switchToChooserScreen();
                        return;
                    case ACTION_DRAW:
                        gameScreenShow();
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
                        midend_process_key(me, 0, 0, fe->gameButton[i].actionParm.c);
                        break;
                    case ACTION_UNDO:
                        if (midend_can_undo(me)) midend_process_key(me, 0, 0 , UI_UNDO);
                        break;
                    case ACTION_REDO:
                        if (midend_can_redo(me)) midend_process_key(me, 0, 0, UI_REDO);
                        break;
                    case ACTION_SWAP:
                        fe->swapped = !fe->swapped;
                        break;
                }
            }
        }
    }
    gameDrawFurniture();
    SoftUpdate();
    checkGameEnd();
}

void gamePrev() {
    if (midend_can_undo(me)) midend_process_key(me, 0, 0 , UI_UNDO);
    gameDrawFurniture();
    SoftUpdate();
}

void gameNext() {
    if (midend_can_redo(me)) midend_process_key(me, 0, 0, UI_REDO);
    gameDrawFurniture();
    SoftUpdate();
}

static void gameDrawStatusBar() {
    char buf[256];
    if (!fe->gamelayout.with_statusbar) return;
    gameSetupStatusBar();
    sprintf(buf, "%s                               ", midend_get_statustext(me));
    SetFont(fe->gamefont, BLACK);
    DrawString(10, fe->gamelayout.statusbar.starty+12, buf);
}

static void gameDrawControlButtons() {
    int i;
    FillArea(0, fe->gamelayout.buttonpanel.starty, ScreenWidth(), fe->gamelayout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

    for (i=0;i<fe->numGameButtons;i++)
        button_to_normal(&fe->gameButton[i], false);

    deactivate_button(&fe->gameButton[fe->numGameButtons-6]);
    deactivate_button(&fe->gameButton[fe->numGameButtons-5]);
    gameCheckButtonState();
}

static void gameDrawMenu() {
    FillArea(0, fe->gamelayout.menu.starty, ScreenWidth(), fe->gamelayout.menu.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.menu.starty + fe->gamelayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&fe->gameButton[fe->btnBackIDX], false);
    button_to_normal(&fe->gameButton[fe->btnDrawIDX], false);
    button_to_normal(&fe->gameButton[fe->btnGameIDX], false);
    button_to_normal(&fe->gameButton[fe->btnTypeIDX], false);

    SetFont(fe->gamefont, BLACK);
    DrawTextRect(0, (fe->gamelayout.menubtn_size/2)-(GFONTSIZE/2), ScreenWidth(), GFONTSIZE, fe->currentgame->name, ALIGN_CENTER);
}
static void gameSetupStatusBar() {
    if (!fe->gamelayout.with_statusbar) return;
    FillArea(0, fe->gamelayout.statusbar.starty, ScreenWidth(), fe->gamelayout.statusbar.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.statusbar.starty, ScreenWidth(), 1, 0x00000000);
}

static void gameSetupMenuButtons() {
    fe->gameButton[fe->btnBackIDX].active = true;
    fe->gameButton[fe->btnBackIDX].posx = 10;
    fe->gameButton[fe->btnBackIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnBackIDX].size = fe->gamelayout.menubtn_size;

    fe->gameButton[fe->btnDrawIDX].active = true;
    fe->gameButton[fe->btnDrawIDX].posx = ScreenWidth() - 30 - (3*fe->gamelayout.menubtn_size);
    fe->gameButton[fe->btnDrawIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnDrawIDX].size = fe->gamelayout.menubtn_size;

    fe->gameButton[fe->btnGameIDX].active = true;
    fe->gameButton[fe->btnGameIDX].posx = ScreenWidth() - 20 - (2*fe->gamelayout.menubtn_size);
    fe->gameButton[fe->btnGameIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnGameIDX].size = fe->gamelayout.menubtn_size;

    fe->gameButton[fe->btnTypeIDX].active = true;
    fe->gameButton[fe->btnTypeIDX].posx = ScreenWidth() - 10 - fe->gamelayout.menubtn_size;
    fe->gameButton[fe->btnTypeIDX].posy = fe->gamelayout.menu.starty;
    fe->gameButton[fe->btnTypeIDX].size = fe->gamelayout.menubtn_size;
}

static void gameSetupControlButtons() {
    int i;
    int btn_num1, btn_num2;
    int pad1, pad2;

    if (strcmp(fe->currentgame->name, "Rome")==0) {
        btn_num1 = 4;
        btn_num2 = fe->numGameButtons - btn_num1 - 4;
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
    gameSetupStatusBar();
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

static LAYOUTTYPE gameGetLayout() {
    bool wants_statusbar;
    int i, addkeys, nkeys = 0;
    struct key_label *keys;

    fe->with_rightpointer = fe->currentgame->flags & REQUIRE_RBUTTON;

    sfree(fe->gameButton);
    keys = midend_request_keys(me, &nkeys);
    if (keys == NULL) nkeys = 0;

    addkeys = (fe->with_rightpointer) ? 3 : 2; // Left/right switch button 
    fe->numGameButtons = 4 + nkeys + addkeys;
    fe->with_twoctrllines = (nkeys+addkeys) > 9;
    fe->with_statusbar = midend_wants_statusbar(me);

    if (strcmp(fe->currentgame->name, "Rome")==0) fe->with_twoctrllines = true;

    fe->gameButton = smalloc((fe->numGameButtons) * sizeof(BUTTON));

    for (i=0;i<nkeys;i++) {
        fe->gameButton[i].type = BTN_CTRL;
        if      (keys[i].button == '\b') fe->gameButton[i] = btn_backspace;
        else if (keys[i].button == '+')  fe->gameButton[i] = btn_add;
        else if (keys[i].button == '-')  fe->gameButton[i] = btn_remove;
        else if (keys[i].button == 'O' && strcmp(fe->currentgame->name, "Salad")==0)   fe->gameButton[i] = btn_salad_o;
        else if (keys[i].button == 'X' && strcmp(fe->currentgame->name, "Salad")==0)   fe->gameButton[i] = btn_salad_x;
        else if (keys[i].button == 'J' && strcmp(fe->currentgame->name, "Net")==0)     fe->gameButton[i] = btn_net_shuffle;
        else if (keys[i].button == 'G' && strcmp(fe->currentgame->name, "Bridges")==0) fe->gameButton[i] = btn_bridges_g;
        else if (keys[i].button == 'U' && strcmp(fe->currentgame->name, "Rome")==0)    fe->gameButton[i] = btn_rome_n;
        else if (keys[i].button == 'L' && strcmp(fe->currentgame->name, "Rome")==0)    fe->gameButton[i] = btn_rome_w;
        else if (keys[i].button == 'R' && strcmp(fe->currentgame->name, "Rome")==0)    fe->gameButton[i] = btn_rome_e;
        else if (keys[i].button == 'D' && strcmp(fe->currentgame->name, "Rome")==0)    fe->gameButton[i] = btn_rome_s;
        else {
            fe->gameButton[i].type   = BTN_CHAR;
            fe->gameButton[i].action = ACTION_CTRL;
            fe->gameButton[i].actionParm.c = keys[i].button;
            fe->gameButton[i].bitmap = NULL;
            fe->gameButton[i].bitmap_tap = NULL;
        }
    }

    if (fe->with_rightpointer) {
        fe->gameButton[fe->btnSwapIDX = i++] = btn_swap;
        fe->gameButton[fe->btnUndoIDX = i++] = btn_undo;
        fe->gameButton[fe->btnRedoIDX = i++] = btn_redo;
    }
    else {
        fe->gameButton[fe->btnUndoIDX = i++] = btn_undo;
        fe->gameButton[fe->btnRedoIDX = i++] = btn_redo;
    }

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
    gameCheckButtonState();
    gameDrawStatusBar();
}

static void gameRestartGame() {
    fe->statustext = "";
    midend_restart_game(me);
    fe->finished = false;
    gameDrawFurniture();
    SoftUpdate();
}

static void gameSolveGame() {
    const char *errorMsg;
    errorMsg = midend_solve(me);
    if (errorMsg) Message(ICON_WARNING, "", errorMsg, 3000);
    else fe->finished = true;
    gameDrawFurniture();
    SoftUpdate();
}

static void gameSwitchPreset(int index) {
    midend_set_params(me, presets->entries[index].params);
    gameStartNewGame();
}

void gameStartNewGame() {
    ShowPureHourglassForce();
    fe->statustext = "";
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
                    fe->statustext = "";
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
    fe->statustext = "";
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
    FullUpdate();
}

void gameScreenInit() {
    fe = snew(frontend);
    fe->cliprect = GetClipRect();
    fe->gamefont = OpenFont("LiberationSans-Bold", GFONTSIZE, 0);
    fe->gameButton = NULL;
    fe->isTimer = false;
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
        sfree(typeMenu);
        if (me != NULL) midend_free(me);
        gameInitialized = false;
        /* Message(ICON_INFORMATION, "", "Exit app", 1000); */
    }
}

