#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "inkview.h"
#include "game.h"
#include "puzzles.h"

#define DOTTED 0xFF000000
int pbSW, pbSH, pbOrient;

int inkcolors[12] = {WHITE, LGRAY, DGRAY, BLACK, LGRAY, LGRAY, DGRAY, DGRAY, DGRAY, DGRAY, LGRAY, BLACK};
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

/* ----------------------------
   Drawing callbacks
   ---------------------------- */

void ink_draw_text(void *handle, int x, int y, int fonttype, int fontsize,
               int align, int colour, const char *text) {

  ifont *tempfont;
  int sw, sh;
  tempfont = OpenFont(fonttype == FONT_FIXED ? "LiberationMono" : "LiberationSans",
                      fontsize, 0);

  SetFont(tempfont, inkcolors[colour]);
  sw=StringWidth(text);
  sh=TextRectHeight(sw, text, 0);
  if (align & ALIGN_VNORMAL) y -= sh;
  else if (align & ALIGN_VCENTRE) y -= sh/2;
  if (align & ALIGN_HCENTRE) x -= sw/2;
  else if (align & ALIGN_HRIGHT) x -= sw;

  DrawString(x, y, text);
  CloseFont(tempfont);
}

void ink_draw_rect(void *handle, int x, int y, int w, int h, int colour) {

  int i;

  if (inkcolors[colour] & DOTTED) {
    for (i=0;i<h;i++) {
      if ((y+i) & 1) DrawLine(x,y+i,x+w-1,y+i,inkcolors[colour]&WHITE);
      else DrawLine(x,y+i,x+w-1,y+i,inkcolors[0]);
    }
  }
  else for (i=0;i<h;i++) DrawLine(x,y+i,x+w-1,y+i,inkcolors[colour]);
}
void ink_draw_rect_outline(void *handle, int x, int y, int w, int h, int colour) {

    DrawRect(x, y, w, h, inkcolors[colour]);
}

void ink_draw_line(void *handle, int x1, int y1, int x2, int y2, int colour) {

  DrawLine(x1, y1, x2, y2, inkcolors[colour]);
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
          if (inkcolors[fillcolour] & DOTTED) {
            if (miny & 1) DrawLine(minx, miny, maxx, miny, inkcolors[fillcolour]&WHITE);
            else DrawLine(minx, miny, maxx, miny, inkcolors[0]);
          }
          else DrawLine(minx, miny, maxx, miny, inkcolors[fillcolour]);
        }
    }
  }

  for (i = 0; i < npoints-1; i++) {
    DrawLine(coords[i].x, coords[i].y, coords[i+1].x, coords[i+1].y, inkcolors[outlinecolour]);
  }
  DrawLine(coords[i].x, coords[i].y, coords[0].x, coords[0].y, inkcolors[outlinecolour]);
}


void ink_draw_circle(void *handle, int cx, int cy, int radius, int fillcolour, int outlinecolour) {

  int i,x,y,yy=0-radius,xx=0;

  for (i=0; i<=2*radius; i++) {
    y=i-radius;
    x=lround(sqrt(radius*radius-y*y));

    DrawLine(cx+xx, cy+yy, cx+x, cy+y, inkcolors[outlinecolour]&WHITE);
    DrawLine(cx-xx, cy+yy, cx-x, cy+y, inkcolors[outlinecolour]&WHITE);

    if (fillcolour!=-1) {
      if (!(inkcolors[fillcolour]&DOTTED)) {
        DrawLine(cx-x, cy+y, cx+x, cy+y, inkcolors[fillcolour]&WHITE);
      }
      else if ((cy+y)%2) DrawLine(cx-x, cy+y, cx+x, cy+y, inkcolors[fillcolour]&WHITE);
    }

    xx=x; yy=y;
  }
}

void ink_clip(void *handle, int x, int y, int w, int h) {

    SetClip(x, y, w, h);
}

void ink_unclip(void *handle) {

    SetClip(0, 0, pbSW, pbSH);
}

void ink_start_draw(void *handle) {
//??
}

void ink_draw_update(void *handle, int x, int y, int w, int h) {
//??
}

void ink_end_draw(void *handle) {

  PartialUpdate(0, 0, fe->inkSW, fe->inkSH);
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

  bl->ibit = BitmapFromScreen(x, y, bl->width, bl->height);
}

void ink_blitter_load(void *handle, blitter *bl, int x, int y) {

  DrawBitmap(x, y, bl->ibit);
}

void ink_status_bar(void *handle, const char *text) {

  size_t tlen;

  if (fe->statusbar) {
    sfree(fe->statustext);
    tlen=strlen(text)+1;
    fe->statustext = snewn(tlen, char);
    strcpy(fe->statustext, text);
    ink_draw_rect(0, 10, pbSH-30, pbSW-20, 30, 0);
    SetFont(fe->statusfont, BLACK);
    DrawString(10, pbSH-30, fe->statustext);
    PartialUpdateBW(0, pbSH-50, pbSW, 50);
  }
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
  fe->isTimer=1;
  gettimeofday(&fe->last_time, NULL);
  SetWeakTimer("timername", tproc, fe->time_int);

};

void deactivate_timer(frontend *fe) {
  fe->isTimer=0;
  ClearTimer(tproc);
};

void fatal(const char *fmt, ...) {
    exitApp();
}

/* ------------------------- */

static void drawStatusBar(char *text) {
    FillArea(0, mainlayout.statusbar.starty+2, ScreenWidth(), mainlayout.statusbar.height-2, 0x00FFFFFF);
    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(10, mainlayout.statusbar.starty+8, ScreenWidth(), kFontSize, text, ALIGN_LEFT);
    PartialUpdate(0, mainlayout.statusbar.starty+2, ScreenWidth(), mainlayout.statusbar.height-2);
    CloseFont(font);
}

void gameMenuHandler(int index) {
    gameMenu_selectedIndex = index;
    switch (index) {
        case 101:
            drawStatusBar("Selected 'New game'");
            break;
        case 102:
            drawStatusBar("Selected 'Restart game'");
            break;
        case 103:
            drawStatusBar("Selected 'Show solution'");
            break;
        case 104:
            drawStatusBar("Selected 'How to play'");
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
    if (index == 200)
        Message(ICON_WARNING, "Not available", "Custom preset not implemented yet!", 3000);
    if (index > 200) {
        snprintf(buf, 200, "Selected type: '%s'", typeMenu[index-199].text);
        drawStatusBar(buf);
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

void gameTap(int x, int y) {
    init_tap_x = x;
    init_tap_y = y;

    if (coord_in_button(x, y, &btn_back)) button_to_tapped(&btn_back);
    if (coord_in_button(x, y, &btn_game)) button_to_tapped(&btn_game);
    if (coord_in_button(x, y, &btn_type)) button_to_tapped(&btn_type);
    if (coord_in_button(x, y, &btn_swap)) button_to_tapped(&btn_swap);
    if (coord_in_button(x, y, &btn_undo)) button_to_tapped(&btn_undo);
    if (coord_in_button(x, y, &btn_redo)) button_to_tapped(&btn_redo);
}

void gameLongTap(int x, int y) {
}

void gameRelease(int x, int y) {
    int i;
    if (coord_in_button(init_tap_x, init_tap_y, &btn_back)) button_to_normal(&btn_back, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_game)) button_to_normal(&btn_game, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_type)) button_to_normal(&btn_type, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_swap)) button_to_normal(&btn_swap, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_undo)) button_to_normal(&btn_undo, true);
    if (coord_in_button(init_tap_x, init_tap_y, &btn_redo)) button_to_normal(&btn_redo, true);

    if (release_button(x, y, &btn_back)) {
        gameExitPage();
        switchToChooser();
    }
    if (release_button(x, y, &btn_game)) {
        OpenMenuEx(gameMenu, gameMenu_selectedIndex, ScreenWidth()-20-(2*mainlayout.menubtn_size), mainlayout.menubtn_size+2, gameMenuHandler);
    }
    if (release_button(x, y, &btn_type)) {
        OpenMenu(typeMenu, typeMenu_selectedIndex, ScreenWidth()-10-mainlayout.menubtn_size, mainlayout.menubtn_size+2, typeMenuHandler);
    }
}

static void gameDrawControlButtons() {
    FillArea(0, mainlayout.buttonpanel.starty, ScreenWidth(), mainlayout.buttonpanel.height, 0x00FFFFFF);
    FillArea(0, mainlayout.buttonpanel.starty, ScreenWidth(), 1, 0x00000000);

    btn_swap.active = true;
    button_to_normal(&btn_swap, false);
    btn_undo.active = true;
    button_to_normal(&btn_undo, false);
    btn_redo.active = true;
    button_to_normal(&btn_redo, false);
}

static void gameDrawMenu() {
    FillArea(0, mainlayout.menu.starty, ScreenWidth(), mainlayout.menu.height, 0x00FFFFFF);
    FillArea(0, mainlayout.menu.starty + mainlayout.menu.height-2, ScreenWidth(), 1, 0x00000000);

    button_to_normal(&btn_back, false);
    button_to_normal(&btn_game, false);
    button_to_normal(&btn_type, false);

    font = OpenFont("LiberationSans-Bold", kFontSize, 0);
    DrawTextRect(0, (mainlayout.menubtn_size/2)-(kFontSize/2), ScreenWidth(), kFontSize, currentgame->name, ALIGN_CENTER);
    CloseFont(font);
}

static void gameSetupMenuButtons() {
    btn_back.active = true;
    btn_back.posx = 10;
    btn_back.posy = mainlayout.menu.starty;
    btn_back.size = mainlayout.menubtn_size;

    btn_game.active = true;
    btn_game.posx = ScreenWidth() - 20 - (2*mainlayout.menubtn_size);
    btn_game.posy = mainlayout.menu.starty;
    btn_game.size = mainlayout.menubtn_size;

    btn_type.active = true;
    btn_type.posx = ScreenWidth() - 10 - mainlayout.menubtn_size;
    btn_type.posy = mainlayout.menu.starty;
    btn_type.size = mainlayout.menubtn_size;
}

static void gameSetupControlButtons() {

    btn_swap.active = true;
    btn_swap.posx = gamecontrol_padding;
    btn_swap.posy = mainlayout.buttonpanel.starty + 2;
    btn_swap.size = mainlayout.control_size;

    btn_undo.active = true;
    btn_undo.posx = 2*gamecontrol_padding + mainlayout.control_size;
    btn_undo.posy = mainlayout.buttonpanel.starty + 2;
    btn_undo.size = mainlayout.control_size;

    btn_redo.active = true;
    btn_redo.posx = 3*gamecontrol_padding + 2*mainlayout.control_size;
    btn_redo.posy = mainlayout.buttonpanel.starty + 2;
    btn_redo.size = mainlayout.control_size;
}

static void gameSetupStatusBar() {
    FillArea(0, mainlayout.statusbar.starty, ScreenWidth(), mainlayout.statusbar.height, 0x00FFFFFF);
    FillArea(0, mainlayout.statusbar.starty, ScreenWidth(), 1, 0x00000000);
}

static void gameExitPage() {
    free(typeMenu);
    midend_free(me);
    sfree(fe);
}

void gameShowPage() {
    ClearScreen();
    DrawPanel(NULL, "", "", 0);
    gameDrawMenu();
    gameDrawControlButtons();
    if (mainlayout.with_statusbar) gameSetupStatusBar();
    FullUpdate();
}

void gameInit() {
    pbSW=ScreenWidth();
    pbSH=ScreenHeight();
    pbOrient=GetOrientation();

    fe = snew(frontend);
    me = midend_new(fe, currentgame, &ink_drawing, fe);
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

void gamePrepare() {
    gamecontrol_num = 3;
    gamecontrol_padding = (ScreenWidth()-(gamecontrol_num*mainlayout.control_size))/(gamecontrol_num+1);
    gameSetupMenuButtons();
    gameSetupControlButtons();
    gameBuildTypeMenu();
}


