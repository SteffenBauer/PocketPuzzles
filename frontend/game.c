#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/game.h"
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
  tempfont = OpenFont(fonttype == FONT_FIXED ? "LiberationMono" : "LiberationSans",
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


  /* if (dottedColor(colour)) {
    for (i=0;i<h;i++) {
      if ((y+i) & 1) DrawLine(fe->xoffset+x,fe->yoffset+y+i,fe->xoffset+x+w-1,fe->yoffset+y+i,convertColor(colour));
      else DrawLine(fe->xoffset+x,fe->yoffset+y+i,fe->xoffset+x+w-1,fe->yoffset+y+i,0x000000);
    }
  }
  else */
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
          /* if (inkcolors[fillcolour] & DOTTED) {
            if (miny & 1) DrawLine(fe->xoffset+minx, fe->yoffset+miny, fe->xoffset+maxx, fe->yoffset+miny, inkcolors[fillcolour]&WHITE);
            else DrawLine(fe->xoffset+minx, fe->yoffset+miny, fe->xoffset+maxx, fe->yoffset+miny, inkcolors[0]);
          }
          else */ DrawLine(fe->xoffset+minx, fe->yoffset+miny, fe->xoffset+maxx, fe->yoffset+miny, convertColor(fillcolour));
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
      /* if (!(inkcolors[fillcolour]&DOTTED)) { */
        DrawLine(fe->xoffset+cx-x, fe->yoffset+cy+y, fe->xoffset+cx+x, fe->yoffset+cy+y, convertColor(fillcolour));
      /* }
      else if ((cy+y)%2) DrawLine(fe->xoffset+cx-x, fe->yoffset+cy+y, fe->xoffset+cx+x, fe->yoffset+cy+y, inkcolors[fillcolour]&WHITE); */
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
  // if (fe->do_partial) PartialUpdate(fe->xoffset+x, fe->yoffset + y, w, h);
}

void ink_end_draw(void *handle) {
    SoftUpdate();
  //if (!fe->do_partial) FullUpdate();
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
    output[0] = 1.0;
    output[1] = 1.0;
    output[2] = 1.0;
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

};

void deactivate_timer(frontend *fe) {
  fe->isTimer=false;
  ClearTimer(tproc);
};

void fatal(const char *fmt, ...) {
    exitApp();
}

/* ------------------------- */

static bool coord_in_gamecanvas(int x, int y) {
    return ((x>=fe->xoffset) && (x<(fe->xoffset+fe->width)) &&
           (y>=fe->yoffset) && (y<(fe->yoffset+fe->height)));
}

void gameMenuHandler(int index) {
    const char *errorMsg;
    gameMenu_selectedIndex = index;

    switch (index) {
        case 101:
            gamePrepare();
            gameShowPage();
            break;
        case 102:
            fe->do_partial = false;
            midend_restart_game(me);
            fe->do_partial = true;
            break;
        case 103:
            fe->do_partial = false;
            errorMsg = midend_solve(me);
            fe->do_partial = true;
            if (errorMsg) Message(ICON_WARNING, "", errorMsg, 3000);
            break;
        case 104:
            Message(ICON_WARNING, "", "Help not implemented yet!", 3000);
            break;
        case 199:
            gameExitPage();
            exitApp();
            break;
        default:
            break;
    }
    button_to_normal(&btn_game, true);
};

void typeMenuHandler(int index) {
    typeMenu_selectedIndex = index;
    char buf[256];
    int currentindex = midend_which_preset(me);

    if (index == 200)
        Message(ICON_WARNING, "", "Custom preset not implemented yet!", 3000);
    if (index > 200) {
        if (currentindex >= 0) typeMenu[currentindex+2].type = ITEM_ACTIVE;
        else typeMenu[1].type = ITEM_ACTIVE;
        presets = midend_get_presets(me, NULL);
        midend_set_params(me, presets->entries[index-201].params);
        typeMenu[index-199].type = ITEM_BULLET;
        gamePrepare();
        gameShowPage();
    }
    button_to_normal(&btn_type, true);
};

static void gameBuildTypeMenu() {
    int i, np, chosen;
    presets = midend_get_presets(me, NULL);

    np = presets->n_entries;

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

};

static void check_button_state() {
    if (midend_can_undo(me) && !btn_undo.active) activate_button(&btn_undo);
    if (midend_can_redo(me) && !btn_redo.active) activate_button(&btn_redo);
    if (!midend_can_undo(me) && btn_undo.active) deactivate_button(&btn_undo);
    if (!midend_can_redo(me) && btn_redo.active) deactivate_button(&btn_redo);
}

void gameTap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;

    if (coord_in_button(x, y, &btn_back)) button_to_tapped(&btn_back);
    if (coord_in_button(x, y, &btn_draw)) button_to_tapped(&btn_draw);
    if (coord_in_button(x, y, &btn_game)) button_to_tapped(&btn_game);
    if (coord_in_button(x, y, &btn_type)) button_to_tapped(&btn_type);

    if (coord_in_button(x, y, &btn_undo)) button_to_tapped(&btn_undo);
    if (coord_in_button(x, y, &btn_redo)) button_to_tapped(&btn_redo);

    if (coord_in_gamecanvas(x, y) && (fe->current_pointer == 0)) {
        fe->current_pointer = (fe->swapped ? RIGHT_BUTTON : LEFT_BUTTON);
        fe->pointerdown_x = x;
        fe->pointerdown_y = y;
    }
}

void gameLongTap(int x, int y) {
    if (coord_in_gamecanvas(x, y)) {
        midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), 
                           (fe->swapped ? LEFT_BUTTON : RIGHT_BUTTON));
        fe->current_pointer = (fe->swapped ? LEFT_BUTTON : RIGHT_BUTTON);
    }
}

void gameDrag(int x, int y) {
    if (coord_in_gamecanvas(x, y)) {
        if (fe->current_pointer == LEFT_BUTTON) {
            if (!fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), (fe->pointerdown_y)-(fe->yoffset), LEFT_BUTTON);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_DRAG);
            fe->current_pointer = LEFT_DRAG;
        }
        else if (fe->current_pointer == LEFT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_DRAG);
        }
        else if (fe->current_pointer == RIGHT_BUTTON) {
            if (fe->swapped)
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
    if (coord_in_button(init_tap_x, init_tap_y, &btn_back)) button_to_normal(&btn_back, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_draw)) button_to_normal(&btn_draw, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_game)) button_to_normal(&btn_game, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_type)) button_to_normal(&btn_type, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_undo)) button_to_normal(&btn_undo, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_redo)) button_to_normal(&btn_redo, true);

    if (coord_in_gamecanvas(x, y)) {
        if (fe->current_pointer == LEFT_BUTTON) {
            if (!fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), (fe->pointerdown_y)-(fe->yoffset), LEFT_BUTTON);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_RELEASE);
        }
        if (fe->current_pointer == LEFT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), LEFT_RELEASE);
        }
        if (fe->current_pointer == RIGHT_BUTTON) {
            if (fe->swapped)
                midend_process_key(me, (fe->pointerdown_x)-(fe->xoffset), (fe->pointerdown_y)-(fe->yoffset), RIGHT_BUTTON);
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_RELEASE);
        }
        if (fe->current_pointer == RIGHT_DRAG) {
            midend_process_key(me, x-(fe->xoffset), y-(fe->yoffset), RIGHT_RELEASE);
        }
        fe->current_pointer = 0;
        checkGameEnd();
    }

    if (release_button(x, y, &btn_back)) {
        gameExitPage();
        switchToChooser();
        return;
    }
    else if (release_button(x, y, &btn_draw)) {
        gameShowPage();
        FullUpdate();
    }
    else if (release_button(x, y, &btn_game)) {
        OpenMenuEx(gameMenu, gameMenu_selectedIndex, ScreenWidth()-20-(2*fe->gamelayout.menubtn_size),
                   fe->gamelayout.menubtn_size+2, gameMenuHandler);
    }
    else if (release_button(x, y, &btn_type)) {
        OpenMenu(typeMenu, typeMenu_selectedIndex, ScreenWidth()-10-fe->gamelayout.menubtn_size,
                 fe->gamelayout.menubtn_size+2, typeMenuHandler);
    }
    else if (release_button(x, y, &btn_undo)) {
        if (midend_can_undo(me)) {
            midend_process_key(me, x-fe->xoffset, y-fe->yoffset, UI_UNDO);
        }
    }
    else if (release_button(x, y, &btn_redo)) {
        if (midend_can_redo(me)) {
            midend_process_key(me, x-fe->xoffset, y-fe->yoffset, UI_REDO);
        }
    }
    else if (release_button(x, y, &btn_swap)) {
        fe->swapped ? button_to_normal(&btn_swap, true) : button_to_tapped(&btn_swap);
        fe->swapped = !fe->swapped;
    }
    check_button_state();
    if (fe->gamelayout.with_statusbar) {
        gameDrawStatusBar();
    }
    SoftUpdate();
}

void gamePrev() {
    if (midend_can_undo(me)) {
        midend_process_key(me, 0, 0, UI_UNDO);
        check_button_state();
    }
}

void gameNext() {
    if (midend_can_redo(me)) {
        midend_process_key(me, 0, 0, UI_REDO);
        check_button_state();
    }
}

static void gameDrawStatusBar() {
    ifont *font;
    char buf[256];
    gameSetupStatusBar();
    sprintf(buf, "%s                               ", fe->statustext);
    font = OpenFont("LiberationSans-Bold", 32, 0);
    SetFont(font, BLACK);
    DrawString(10, fe->gamelayout.statusbar.starty+8, buf);
    CloseFont(font);
}

static void gameDrawControlButtons() {
    FillArea(0, fe->gamelayout.buttonpanel.starty, ScreenWidth(), fe->gamelayout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

    if (fe->with_rightpointer) {
        if (fe->swapped) button_to_tapped(&btn_swap);
        else button_to_normal(&btn_swap, false);
    }
    button_to_normal(&btn_undo, false);
    button_to_normal(&btn_redo, false);
    deactivate_button(&btn_undo);
    deactivate_button(&btn_redo);
    check_button_state();
}

static void gameDrawMenu() {
    ifont *font;
    FillArea(0, fe->gamelayout.menu.starty, ScreenWidth(), fe->gamelayout.menu.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.menu.starty + fe->gamelayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&btn_back, false);
    button_to_normal(&btn_draw, false);
    button_to_normal(&btn_game, false);
    button_to_normal(&btn_type, false);

    font = OpenFont("LiberationSans-Bold", 32, 0);
    SetFont(font, BLACK);
    DrawTextRect(0, (fe->gamelayout.menubtn_size/2)-(32/2), ScreenWidth(), 32, currentgame->name, ALIGN_CENTER);
    CloseFont(font);
}
static void gameSetupStatusBar() {
    FillArea(0, fe->gamelayout.statusbar.starty, ScreenWidth(), fe->gamelayout.statusbar.height, 0x00FFFFFF);
    FillArea(0, fe->gamelayout.statusbar.starty, ScreenWidth(), 1, 0x00000000);
}

static void gameSetupMenuButtons() {
    btn_back.active = true;
    btn_back.posx = 10;
    btn_back.posy = fe->gamelayout.menu.starty;
    btn_back.size = fe->gamelayout.menubtn_size;

    btn_draw.active = true;
    btn_draw.posx = ScreenWidth() - 30 - (3*fe->gamelayout.menubtn_size);
    btn_draw.posy = fe->gamelayout.menu.starty;
    btn_draw.size = fe->gamelayout.menubtn_size;

    btn_game.active = true;
    btn_game.posx = ScreenWidth() - 20 - (2*fe->gamelayout.menubtn_size);
    btn_game.posy = fe->gamelayout.menu.starty;
    btn_game.size = fe->gamelayout.menubtn_size;

    btn_type.active = true;
    btn_type.posx = ScreenWidth() - 10 - fe->gamelayout.menubtn_size;
    btn_type.posy = fe->gamelayout.menu.starty;
    btn_type.size = fe->gamelayout.menubtn_size;
}

static void gameSetupControlButtons() {

    if (fe->with_rightpointer) {
        btn_swap.active = true;
        btn_swap.posx = gamecontrol_padding;
        btn_swap.posy = fe->gamelayout.buttonpanel.starty + 2;
        btn_swap.size = fe->gamelayout.control_size;
    }

    btn_undo.active = true;
    btn_undo.posx = 2*gamecontrol_padding + fe->gamelayout.control_size;
    btn_undo.posy = fe->gamelayout.buttonpanel.starty + 2;
    btn_undo.size = fe->gamelayout.control_size;

    btn_redo.active = true;
    btn_redo.posx = 3*gamecontrol_padding + 2*fe->gamelayout.control_size;
    btn_redo.posy = fe->gamelayout.buttonpanel.starty + 2;
    btn_redo.size = fe->gamelayout.control_size;
}

static void gameExitPage() {
    SetClipRect(&fe->cliprect);
    deactivate_timer(fe);
    free(typeMenu);
    midend_free(me);
    sfree(fe);
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

void gameShowPage() {
    SetClipRect(&fe->cliprect);
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    gameDrawMenu();
    gameDrawControlButtons();
    fe->do_partial = false;
    midend_force_redraw(me);
    if (fe->gamelayout.with_statusbar) gameDrawStatusBar();
    fe->do_partial = true;
}

void gameInit(const struct game *thegame) {
    currentgame = thegame;
    fe = snew(frontend);
    me = midend_new(fe, thegame, &ink_drawing, fe);
    gamePrepare();
}

void gamePrepare() {
    int x, y;
    char buf[256];

    fe->gamelayout = getLayout(gameGetLayout());
    fe->cliprect = GetClipRect();
    fe->statustext = "";
    fe->current_pointer = 0;
    fe->pointerdown_x = 0;
    fe->pointerdown_y = 0;
    fe->with_rightpointer = currentgame->flags & REQUIRE_RBUTTON;
    fe->swapped = false;
    fe->colours = midend_colours(me, &fe->ncolours);
    fe->finished = false;
    fe->isTimer = false;
    fe->time_int = 20;

    gamecontrol_num = 3;
    gamecontrol_padding = (ScreenWidth()-(gamecontrol_num * fe->gamelayout.control_size))/(gamecontrol_num+1);
    gameSetupMenuButtons();
    gameSetupControlButtons();
    if (fe->gamelayout.with_statusbar) gameSetupStatusBar();
    gameBuildTypeMenu();

    ShowPureHourglassForce();
    midend_new_game(me);
    HideHourglass();

    x = ScreenWidth();
    y = fe->gamelayout.maincanvas.height;
    midend_size(me, &x, &y, true);
    fe->width  = x;
    fe->height = y;
    fe->xoffset = (ScreenWidth() - fe->width)/2;
    fe->yoffset = fe->gamelayout.maincanvas.starty + (fe->gamelayout.maincanvas.height - fe->height) / 2 ;
}

LAYOUTTYPE gameGetLayout() {
    bool wants_statusbar;
    bool wants_2xbuttonbar;
    int nkeys = 0;
    struct key_label *keys;

    keys = midend_request_keys(me, &nkeys);

    wants_statusbar = midend_wants_statusbar(me);
    wants_2xbuttonbar = (keys != NULL) && (nkeys > 0);
    sfree(keys);

    if (wants_statusbar && !wants_2xbuttonbar)  return LAYOUT_BOTH;
    if (wants_statusbar && wants_2xbuttonbar)   return LAYOUT_2XBOTH;
    if (!wants_statusbar && !wants_2xbuttonbar) return LAYOUT_BUTTONBAR;
    if (!wants_statusbar && wants_2xbuttonbar)  return LAYOUT_2XBUTTONBAR;
    return LAYOUT_2XBOTH; /* default */
}

