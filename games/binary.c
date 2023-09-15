/*
 * Binary. Implementation of '2048'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#ifdef NO_TGMATH_H
#  include <math.h>
#else
#  include <tgmath.h>
#endif

#include "puzzles.h"

#define PREFERRED_TILE_SIZE 44
#define TILE_SIZE (ds->tilesize)
#define BORDER    (TILE_SIZE / 2)
#define HIGHLIGHT_WIDTH (TILE_SIZE / 10)
#define COORD(x)  ( (x) * TILE_SIZE + BORDER )
#define FROMCOORD(x)  ( ((x) - BORDER + TILE_SIZE) / TILE_SIZE - 1 )

#define X(state, i) ( (i) % (state)->w )
#define Y(state, i) ( (i) / (state)->w )
#define C(state, x, y) ( (y) * (state)->shared->w + (x) )

enum {
    COL_BACKGROUND,
    COL_BOARD,
    COL_TEXT,
    COL_HIGHLIGHT,
    COL_LOWLIGHT,
    NCOLOURS
};

enum {
    BACKGROUND_EMPTY,
    BACKGROUND_CHESS,
    BACKGROUND_LINES
};

struct game_params {
    int w,h;
    int goal;
    int mode;
};

#define GOALLIST(A)                             \
    A(32,32,1)                                  \
    A(64,64,2)                                  \
    A(128,128,3)                                \
    A(256,256,4)                                \
    A(512,512,5)                                \
    A(1024,1024,6)                              \
    A(2048,2048,7)                              \
    A(4096,4096,8)                              \
    A(8192,8192,9)                              \

#define MODELIST(A)                             \
    A(RECT,Rectangular, R)                        \
    A(HEX,Hexagonal, H)                       \

#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
#define GOALENUM(upper,title,lower) GOAL_ ## upper,
#define MODEENUM(upper,title,lower) MODE_ ## upper,

enum { GOALLIST(GOALENUM) GOALCOUNT };
static char const *const binary_goalnames[] = { GOALLIST(TITLE) };
static char const binary_goalchars[] = GOALLIST(ENCODE);
#define GOALCONFIG GOALLIST(CONFIG)

enum { MODELIST(MODEENUM) MODECOUNT };
static char const *const binary_modenames[] = { MODELIST(TITLE) };
static char const binary_modechars[] = MODELIST(ENCODE);
#define MODECONFIG MODELIST(CONFIG)

const int POWER[] = {
    1, 2, 4, 8,
    16, 32, 64, 128,
    256, 512, 1024, 2048,
    4096, 8192, 16384, 32768
};
#define MAXPOWER 0x0f

struct game_state_shared {
    int refcount;
    int w, h;
    int goal;
    int mode;
    random_state *rs;
};

struct game_state {
    struct game_state_shared *shared;
    unsigned char *tiles;
    int score;
    bool lost;
    bool won;
    bool finished;
};

static game_params *default_params(void) {
    game_params *ret = snew(game_params);

    ret->w = 4;
    ret->h = 4;
    ret->goal = GOAL_2048;
    ret->mode = MODE_RECT;

    return ret;
}

static const struct game_params binary_presets[] = {
    { 4,  4, GOAL_512, MODE_RECT},
    { 4,  4, GOAL_2048, MODE_RECT},
    { 5,  5, GOAL_2048, MODE_RECT},
    { 6,  6, GOAL_2048, MODE_RECT},
};

static bool game_fetch_preset(int i, char **name, game_params **params) {
    game_params *ret;
    char str[80];

    if (i < 0 || i >= lenof(binary_presets))
        return false;

    ret = snew(game_params);
    *ret = binary_presets[i];

    sprintf(str, "%dx%d, Goal %s, %s", ret->w, ret->h, binary_goalnames[ret->goal], binary_modenames[ret->mode]);

    *name = dupstr(str);
    *params = ret;
    return true;
}

static void free_params(game_params *params) {
    sfree(params);
}

static game_params *dup_params(const game_params *params) {
    game_params *ret = snew(game_params);
    *ret = *params;            /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string) {
    int i;
    params->w = params->h = atoi(string);
    params->goal = 7;
    params->mode = 0;

    while (*string && isdigit((unsigned char)*string)) string++;
    if (*string == 'x') {
        string++;
        params->h = atoi(string);
        while (*string && isdigit((unsigned char)*string)) string++;
    }
    if (*string && *string == 'g') {
        string++;
        if (*string) {
            for (i = 0; i < GOALCOUNT; i++) {
                if (*string == binary_goalchars[i])
                    params->goal = i;
            }
            string++;
        }
    }
    if (*string == 'm') {
        string++;
        if (*string) {
            for (i = 0; i < MODECOUNT; i++) {
                if (*string == binary_modechars[i])
                    params->mode = i;
            }
            string++;
        }
    }
}

static char *encode_params(const game_params *params, bool full) {
    char data[256];
    sprintf(data, "%dx%dg%cm%c", params->w, params->h, binary_goalchars[params->goal], binary_modechars[params->mode]);
    return dupstr(data);
}

static config_item *game_configure(const game_params *params) {
    config_item *ret;
    char buf[80];

    ret = snewn(5, config_item);

    ret[0].name = "Width";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->w);
    ret[0].u.string.sval = dupstr(buf);

    ret[1].name = "Height";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->h);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = "Goal";
    ret[2].type = C_CHOICES;
    ret[2].u.choices.choicenames = GOALCONFIG;
    ret[2].u.choices.selected = params->goal;

    ret[3].name = "Grid type";
    ret[3].type = C_CHOICES;
    ret[3].u.choices.choicenames = MODECONFIG;
    ret[3].u.choices.selected = params->mode;

    ret[4].name = NULL;
    ret[4].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg) {
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->goal = cfg[2].u.choices.selected;
    ret->mode = cfg[3].u.choices.selected;

    return ret;
}

static const char *validate_params(const game_params *params, bool full) {
    if (params->w < 2 || params->h < 2)
        return "Width and height must both be at least three";
    if (params->w > 8 || params->h > 8)
        return "Width and height must both be at most 8";
    if (params->goal < GOAL_32 || params->goal > GOAL_8192)
        return "Invalid goal number";
    if (params->mode == MODE_HEX)
        return "Hexagonal grid not implemented yet";
    if (params->mode < MODE_RECT || params->mode > MODE_HEX)
        return "Invalid grid type";
    return NULL;
}

static int find_empty_cell(random_state *rs, unsigned char *grid, int size) {
    unsigned char *freecells;
    int found = -1;
    int i, c = 0;

    freecells = snewn(size, unsigned char);
    memset(freecells, 0, size*sizeof(unsigned char));
    for (i=0; i<size; i++)
        if (grid[i] == 0)
            freecells[c++] = i;
    if (c>0)
        found = freecells[random_upto(rs, c)];

    sfree(freecells);
    return found;
}

static bool add_new_number(random_state *rs, unsigned char *grid, int size) {
    int tofill;
    tofill = find_empty_cell(rs, grid, size);
    if (tofill >= 0) {
        grid[tofill] = random_upto(rs, 10) == 0 ? 2 : 1;
        return true;
    }
    return false;
}

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive) {
    int w = params->w, h = params->h;
    char *p;
    char *desc;
    int run, i;
    unsigned char *grid;
    char *rsdesc;

    grid = snewn(w*h, unsigned char);
    memset(grid, 0, w*h*sizeof(unsigned char));

    add_new_number(rs, grid, w*h);
    add_new_number(rs, grid, w*h);

    rsdesc = random_state_encode(rs);

    desc = snewn(w*h+strlen(rsdesc)+2, char);
    p = desc;

    run = 0;
    for(i = 0; i < w*h; i++) {
        if (grid[i] == 0)
            run++;
        else {
            while(run && (run >= 26)) {
                *p++ = 'z';
                run -= 26;
            }
            if(run)
                *p++ = 'a' + run-1;
            run = 0;
            if (grid[i] < 10)
                *p++ = '0' + grid[i];
            else
                *p++ = 'A' + (grid[i]-10);
        }
    }
    while(run && (run >= 26)) {
        *p++ = 'z';
        run -= 26;
    }
    if(run)
        *p++ = 'a' + run-1;
    *p++ = ',';
    for (i=0;i<strlen(rsdesc);i++)
        *p++ = rsdesc[i];
    *p++ = '\0';

    desc = sresize(desc, p - desc, char);
    sfree(rsdesc);
    sfree(grid);
    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc) {
    int w = params->w, h = params->h;
    int i = 0;
    const char *p = desc;

    while (*p != ',') {
        if(*p >= 'a' && *p <= 'z')
            i += ((*p) - 'a') + 1;
        else if (*p >= '1' && *p <= '9')
            i++;
        else if (*p >= 'A' && *p <= 'F')
            i++;
        else
            return "Wrong character in game description";
        p++;
    }
    if (i != w*h)
        return "Game description does not match grid size";

    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc) {
    int i, wh;
    game_state *state = snew(game_state);
    state->score = 0;
    state->lost = state->won = state->finished = false;

    printf("Create new shared state\n");
    state->shared = snew(struct game_state_shared);
    state->shared->refcount = 1;

    state->shared->w = params->w;
    state->shared->h = params->h;
    state->shared->goal = params->goal;
    state->shared->mode = params->mode;

    wh = state->shared->w * state->shared->h;
    state->tiles = snewn(wh, unsigned char);
    memset(state->tiles, 0, wh*sizeof(unsigned char));
    const char *p = desc;
    i = 0;
    while(*p != ',') {
        if(*p >= 'a' && *p <= 'z')
            i += ((*p++) - 'a') + 1;
        else if (*p >= '1' && *p <= '9')
            state->tiles[i++] = (*p++) - '0';
        else if (*p >= 'A' && *p <= 'F')
            state->tiles[i++] = 10 + (*p++) - 'A';
        else p++;
    }
    p++;
    state->shared->rs = random_state_decode(p);

    return state;
}

static game_state *dup_game(const game_state *state) {
    int wh;
    game_state *ret = snew(game_state);
    ret->score = state->score;
    ret->lost = state->lost;
    ret->won = state->won;
    ret->finished = state->finished;
    ret->shared = state->shared;
    ret->shared->refcount++;

    wh = state->shared->w * state->shared->h;
    ret->tiles = snewn(wh, unsigned char);
    memcpy(ret->tiles, state->tiles, wh*sizeof(unsigned char));

    return ret;
}

static void free_game(game_state *state) {
    if (--state->shared->refcount <= 0) {
        if (state->shared->rs)
            random_free(state->shared->rs);
        sfree(state->shared);
        printf("Free shared state\n");
    }
    sfree(state->tiles);
    sfree(state);
}

struct game_ui {
    int background;
};

static game_ui *new_ui(const game_state *state) {
    game_ui *ui = snew(game_ui);
    ui->background = BACKGROUND_EMPTY;
    return ui;
}

static void free_ui(game_ui *ui) {
    sfree(ui);
}

static config_item *get_prefs(game_ui *ui)
{
    config_item *ret;

    ret = snewn(2, config_item);

    ret[0].name = "Game background";
    ret[0].kw = "background";
    ret[0].type = C_CHOICES;
    ret[0].u.choices.choicenames = ":Empty:Chessboard:Lines";
    ret[0].u.choices.choicekws = ":empty:chess:lines";
    ret[0].u.choices.selected = ui->background;

    ret[1].name = NULL;
    ret[1].type = C_END;

    return ret;
}

static void set_prefs(game_ui *ui, const config_item *cfg)
{
    ui->background = cfg[0].u.choices.selected;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate) {
}

static void compress_line(unsigned char *line, int length) {
    int i,j;
    unsigned char t;

    for (i=1;i<length;i++) {
        if (line[i] == 0)
            continue;
        t = line[i];
        line[i] = 0;
        for (j=i;j>=0;j--) {
            if (j==0 && line[0] == 0) {
                line[0] = t;
                break;
            }
            if (line[j] == t) {
                line[j] += 1;
                break;
            }
            if (j<(length-1) && line[j] != 0) {
                line[j+1] = t;
                break;
            }
        }
    }
}

static void move_board(game_state *state, char direction) {
    unsigned char *line, *p;
    int w = state->shared->w, h = state->shared->h;
    int x,y;

    if (direction == 'L' || direction == 'R') {
        line = snewn(w, unsigned char);
        for (y=0;y<h;y++) {
            if (direction == 'L') {
                for (x=0,p=line;x<w;x++)
                    *p++ = state->tiles[x+y*w];
                compress_line(line, w);
                for (x=0,p=line;x<w;x++)
                    state->tiles[x+y*w] = *p++;
            }
            else {
                for (x=w-1,p=line;x>=0;x--)
                    *p++ = state->tiles[x+y*w];
                compress_line(line, w);
                for (x=w-1,p=line;x>=0;x--)
                    state->tiles[x+y*w] = *p++;
            }
        }
        sfree(line);
    }
    else if (direction == 'D' || direction == 'U') {
        line = snewn(h, unsigned char);
        for (x=0;x<w;x++) {
            if (direction == 'U') {
                for (y=0,p=line;y<h;y++)
                    *p++ = state->tiles[x+y*w];
                compress_line(line, h);
                for (y=0,p=line;y<h;y++)
                    state->tiles[x+y*w] = *p++;
            }
            else {
                for (y=h-1,p=line;y>=0;y--)
                    *p++ = state->tiles[x+y*w];
                compress_line(line, h);
                for (y=h-1,p=line;y>=0;y--)
                    state->tiles[x+y*w] = *p++;
            }
        }
        sfree(line);
    }
}

static bool check_change(const game_state *state, char direction) {
    int i, w = state->shared->w, h = state->shared->h;
    bool result = false;
    game_state *test = dup_game(state);
    move_board(test, direction);
    for (i=0;i<w*h;i++)
        if (test->tiles[i] != state->tiles[i]) {
            result = true;
            break;
        }
    sfree(test);
    return result;
}

static bool moves_possible(const game_state *state) {
    if (check_change(state, 'L')) return true;
    if (check_change(state, 'R')) return true;
    if (check_change(state, 'U')) return true;
    if (check_change(state, 'D')) return true;
    return false;
}

static bool goal_reached(const game_state *state) {
    int i, w = state->shared->w, h = state->shared->h;
    for (i=0;i<w*h;i++)
        if (state->tiles[i] >= state->shared->goal + 5)
            return true;
    return false;
}

static bool highest_reached(const game_state *state) {
    int i, w = state->shared->w, h = state->shared->h;
    for (i=0;i<w*h;i++)
        if (state->tiles[i] >= 0x0f)
            return true;
    return false;
}

struct game_drawstate {
    bool started;
    int tilesize;
    int w, h;
    unsigned char *tiles;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button, bool swapped) {
    char buf[80];
    int nx, ny;

    if (state->finished)
        return MOVE_NO_EFFECT;

    if (button == LEFT_BUTTON) {
        nx = FROMCOORD(x);
        ny = FROMCOORD(y);
        if (nx < 0) {
            if (!check_change(state, 'L'))
                return MOVE_NO_EFFECT;
            sprintf(buf, "L");
            return dupstr(buf);
        }
        if (nx >= state->shared->w) {
            if (!check_change(state, 'R'))
                return MOVE_NO_EFFECT;
            sprintf(buf, "R");
            return dupstr(buf);
        }
        if (ny < 0) {
            if (!check_change(state, 'U'))
                return MOVE_NO_EFFECT;
            sprintf(buf, "U");
            return dupstr(buf);
        }
        if (ny >= state->shared->w) {
            if (!check_change(state, 'D'))
                return MOVE_NO_EFFECT;
            sprintf(buf, "D");
            return dupstr(buf);
        }
    }
    return MOVE_UNUSED;
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move) {
    game_state *ret = NULL;
    int w = state->shared->w, h = state->shared->h;
    if (move) {
        ret = dup_game(state);
        move_board(ret, move[0]);
        add_new_number(ret->shared->rs, ret->tiles, w*h);
        if (!moves_possible(ret)) ret->lost = ret->finished = true;
        if (goal_reached(ret)) ret->won = true;
        if (highest_reached(ret)) ret->finished = true;
        return ret;
    }
    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y) {
    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = TILE_SIZE * params->w + 2 * BORDER;
    *y = TILE_SIZE * params->h + 2 * BORDER;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize) {
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours) {
    float *ret = snewn(3 * NCOLOURS, float);
    int i;

    for (i = 0; i < 3; i++) {
        ret[COL_BACKGROUND * 3 + i] = 1.0f;
        ret[COL_BOARD      * 3 + i] = 0.9f;
        ret[COL_HIGHLIGHT  * 3 + i] = 0.75f;
        ret[COL_LOWLIGHT   * 3 + i] = 0.25f;
        ret[COL_TEXT       * 3 + i] = 0.0f;
    }
    *ncolours = NCOLOURS;

    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state) {
    struct game_drawstate *ds = snew(struct game_drawstate);
    int i;

    ds->started = false;
    ds->tilesize = 0;
    ds->w = state->shared->w;
    ds->h = state->shared->h;
    ds->tiles = snewn(ds->w*ds->h, unsigned char);
    for (i = 0; i < ds->w*ds->h; i++)
        ds->tiles[i] = 0;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds) {
    sfree(ds->tiles);
    sfree(ds);
}

static void draw_cell(drawing *dr, game_drawstate *ds, const game_ui *ui, const game_state *state,
                      int x, int y, unsigned char tile, int colour)
{
    if (tile == 0) {
        draw_rect(dr, x, y, TILE_SIZE, TILE_SIZE,
                  ui->background == BACKGROUND_CHESS ? colour : COL_BACKGROUND);
        if (ui->background == BACKGROUND_LINES) {
            draw_rect(dr, x+TILE_SIZE-2, y, 2, TILE_SIZE, COL_TEXT);
            draw_rect(dr, x, y+TILE_SIZE-2, TILE_SIZE, 2, COL_TEXT);
        }
    } else {
        int coords[6];
        char str[40];
        int textsize;

        coords[0] = x + TILE_SIZE - 1;
        coords[1] = y + TILE_SIZE - 1;
        coords[2] = x + TILE_SIZE - 1;
        coords[3] = y;
        coords[4] = x;
        coords[5] = y + TILE_SIZE - 1;
        draw_polygon(dr, coords, 3, COL_LOWLIGHT, COL_LOWLIGHT);

        coords[0] = x;
        coords[1] = y;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);

        draw_rect(dr, x + HIGHLIGHT_WIDTH, y + HIGHLIGHT_WIDTH,
                  TILE_SIZE - 2*HIGHLIGHT_WIDTH, TILE_SIZE - 2*HIGHLIGHT_WIDTH,
                  COL_BACKGROUND);

        sprintf(str, "%d", POWER[tile]);
        textsize = (tile >= state->shared->goal+5) ? 
                        (tile < 10) ? TILE_SIZE/3 : TILE_SIZE/4
                    :   (tile < 10) ? TILE_SIZE/4 : TILE_SIZE/5;
        draw_text(dr, x + TILE_SIZE/2, y + TILE_SIZE/2,
                  FONT_VARIABLE, textsize, ALIGN_VCENTRE | ALIGN_HCENTRE,
                  COL_TEXT, str);
    }
    draw_update(dr, x, y, TILE_SIZE, TILE_SIZE);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime) {
    int i, w = state->shared->w, h = state->shared->h;
    char statusbuf[256];

    if (!ds->started) {
        int coords[10];

        /*
         * Recessed area containing the whole puzzle.
         */
        coords[0] = COORD(w) + HIGHLIGHT_WIDTH - 1;
        coords[1] = COORD(h) + HIGHLIGHT_WIDTH - 1;
        coords[2] = COORD(w) + HIGHLIGHT_WIDTH - 1;
        coords[3] = COORD(0) - HIGHLIGHT_WIDTH;
        coords[4] = coords[2] - TILE_SIZE;
        coords[5] = coords[3] + TILE_SIZE;
        coords[8] = COORD(0) - HIGHLIGHT_WIDTH;
        coords[9] = COORD(h) + HIGHLIGHT_WIDTH - 1;
        coords[6] = coords[8] + TILE_SIZE;
        coords[7] = coords[9] - TILE_SIZE;
        draw_polygon(dr, coords, 5, COL_HIGHLIGHT, COL_HIGHLIGHT);

        coords[1] = COORD(0) - HIGHLIGHT_WIDTH;
        coords[0] = COORD(0) - HIGHLIGHT_WIDTH;
        draw_polygon(dr, coords, 5, COL_LOWLIGHT, COL_LOWLIGHT);

    }

    for (i = 0; i < w*h; i++) {
        int t, x, y;
        t = state->tiles[i];
        if (!ds->started || (t != ds->tiles[i])) {
            x = COORD(i%w);
            y = COORD(i/w);
            draw_cell(dr, ds, ui, state, x, y, t, ((i/w)+(i%w))%2 ? COL_BACKGROUND : COL_BOARD);
            ds->tiles[i] = t;
        }
    }

    ds->started = true;
    sprintf(statusbuf,
            "%sScore %d", state->lost ? "NO MORE MOVES " :
                          state->finished ? "HIGHEST NUMBER REACHED " :
                          state->won  ? "WON! " : "",
                          state->score);
    status_bar(dr, statusbuf);
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui) {
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui) {
    return 0.0F;
}

static void game_get_cursor_location(const game_ui *ui,
                                     const game_drawstate *ds,
                                     const game_state *state,
                                     const game_params *params,
                                     int *x, int *y, int *w, int *h) {
}

static int game_status(const game_state *state) {
    return 0;
}

#ifdef COMBINED
#define thegame binary
#endif

static const char rules[] = "2048";


const struct game thegame = {
    "Binary", "games.binary", "binary", rules,
    default_params,
    game_fetch_preset, NULL,
    decode_params,
    encode_params,
    free_params,
    dup_params,
    true, game_configure, custom_params,
    validate_params,
    new_game_desc,
    validate_desc,
    new_game,
    dup_game,
    free_game,
    false, NULL, /* solve */
    false, NULL, NULL, /* can_format_as_text_now, text_format */
    true, get_prefs, set_prefs,
    new_ui,
    free_ui,
    NULL, /* encode_ui */
    NULL, /* decode_ui */
    NULL, /* game_request_keys */
    game_changed_state,
    NULL, /* current_key_label */
    interpret_move,
    execute_move,
    PREFERRED_TILE_SIZE, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    game_get_cursor_location,
    game_status,
    false, false, NULL, NULL,          /* print_size, print */
    true,                              /* wants_statusbar */
    false, NULL,                       /* timing_state */
    0,                                 /* flags */
};

