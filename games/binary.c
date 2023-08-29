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

#define PREFERRED_TILE_SIZE 48

enum {
    COL_BACKGROUND,
    COL_TEXT,
    COL_HIGHLIGHT,
    COL_LOWLIGHT,
    NCOLOURS
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

struct game_state {
    int w, h;
    unsigned char *tiles;
    int goal;
    int mode;
};

static game_params *default_params(void)
{
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

static bool game_fetch_preset(int i, char **name, game_params **params)
{
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

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;            /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
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
    printf("%s --- %d %d %d %d\n", string, params->w, params->h, params->goal, params->mode);
    
}

static char *encode_params(const game_params *params, bool full)
{
    char data[256];
    sprintf(data, "%dx%dg%cm%c", params->w, params->h, binary_goalchars[params->goal], binary_modechars[params->mode]);
    return dupstr(data);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];

    ret = snewn(4, config_item);

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

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->goal = cfg[2].u.choices.selected;
    ret->mode = cfg[3].u.choices.selected;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->w < 3 || params->h < 3)
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

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive)
{
    int w = params->w, h = params->h, W = w+1, H = h+1;
    char *p;
    char *desc;


    desc = snewn(W*H+1, char);
    p = desc;


    *p++ = '\0';
    desc = sresize(desc, p - desc, char);
    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    int wh;
    game_state *state = snew(game_state);

    state->w = params->w;
    state->h = params->h;
    state->goal = params->goal;
    state->mode = params->mode;

    wh = state->w * state->h;
    state->tiles = snewn(wh, unsigned char);
    memset(state->tiles, 0, wh);

    return state;
}

static game_state *dup_game(const game_state *state)
{
    int wh;
    game_state *ret = snew(game_state);

    ret->w = state->w;
    ret->h = state->w;
    ret->goal = state->goal;
    ret->mode = state->mode;

    wh = state->w * state->h;
    ret->tiles = snewn(wh, unsigned char);
    memcpy(ret->tiles, state->tiles, wh*sizeof(unsigned char));

    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->tiles);
    sfree(state);
}

static game_ui *new_ui(const game_state *state)
{
    return NULL;
}

static void free_ui(game_ui *ui)
{
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

struct game_drawstate {
    int tilesize;
    int FIXME;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button, bool swapped)
{
    return NULL;
}

static game_state *execute_move(const game_state *state, const char *move)
{
    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    *x = *y = 10 * tilesize;	       /* FIXME */
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->FIXME = 0;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui)
{
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui)
{
    return 0.0F;
}

static void game_get_cursor_location(const game_ui *ui,
                                     const game_drawstate *ds,
                                     const game_state *state,
                                     const game_params *params,
                                     int *x, int *y, int *w, int *h)
{
}

static int game_status(const game_state *state)
{
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
    false, NULL, NULL, /* get_prefs, set_prefs */
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
    false,                             /* wants_statusbar */
    false, NULL,                       /* timing_state */
    0,                                 /* flags */
};
