/*
 * clusters.c: Implementation of Clusters puzzle.
 * (C) 2017 Lennard Sprong
 * Created for Simon Tatham's Portable Puzzle Collection
 * See LICENCE for licence details
 * 
 * Objective: Fill the grid with red and blue tiles, with the following rules:
 * - Tiles which are adjacent to 1 other tile of the same color are denoted
 *   with a dot. All of these tiles are given.
 * - All other tiles must be adjacent to 2 or more tiles of the same color.
 *
 * This puzzle type was invented by Inaba Naoki.
 * Java Applet: http://www.inabapuzzle.com/honkaku/kura.html
 * PDF:         http://www.inabapuzzle.com/honkaku/kura.pdf
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_0,
    COL_1,
    COL_0_DOT,
    COL_1_DOT,
    COL_ERROR,
    COL_CURSOR,
    NCOLOURS
};

enum {
    PREF_CLICK_ACTIONS,
    N_PREF_ITEMS
};

struct game_params {
    int w, h;
};

#define F_COLOR_0    0x01
#define F_COLOR_1    0x02
#define F_SINGLE     0x04
#define F_ERROR      0x08
#define F_CURSOR     0x10

#define COLMASK (F_COLOR_0|F_COLOR_1)

struct game_state {
    int w, h;
    char *grid;

    bool completed, cheated;
};

const static struct game_params clusters_presets[] = {
    { 5, 5 },
    { 7, 7 },
    { 8, 8 },
    { 9, 9 },
    { 10, 10 },
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->w = 7;
    ret->h = 7;

    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(clusters_presets))
        return false;

    ret = snew(game_params);
    *ret = clusters_presets[i]; /* structure copy */

    sprintf(buf, "%dx%d", ret->w, ret->h);

    *name = dupstr(buf);
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
    *ret = *params; /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
    char const *p = string;

    params->w = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;
    if (*p == 'x') {
        p++;
        params->h = atoi(p);
        while (*p && isdigit((unsigned char)*p)) p++;
    }
    else {
        params->h = params->w;
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char buf[80];

    sprintf(buf, "%dx%d", params->w, params->h);

    return dupstr(buf);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[64];

    ret = snewn(3, config_item);

    ret[0].name = "Width";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->w);
    ret[0].u.string.sval = dupstr(buf);

    ret[1].name = "Height";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->h);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = NULL;
    ret[2].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->w * params->h > 150)
        return "Puzzle is too large";

    if (params->w * params->h < 2)
        return "Puzzle is too small";

    return NULL;
}

/* ******************** *
 * Validation and Tools *
 * ******************** */

static int clusters_count(const game_state *state, int x, int y, int col, 
    int *count, int *othercount, int *emptycount)
{
    int w = state->w;
    int h = state->h;
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 0;

    int othercol = state->grid[y*w + x] & COLMASK;
    if (othercol == col) (*count)++;
    else if (!othercol) (*emptycount)++;
    else (*othercount)++;

    return 1;
}

enum { STATUS_COMPLETE, STATUS_UNFINISHED, STATUS_INVALID };

static int clusters_validate(game_state *state)
{
    int w = state->w;
    int h = state->h;
    int x, y, col, count, othercount, emptycount, maxcount;
    bool error;
    char ret = STATUS_COMPLETE;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
        {
            if (!state->grid[y*w + x])
            {
                if(ret == STATUS_COMPLETE) ret = STATUS_UNFINISHED;
                continue;
            }
            col = state->grid[y*w + x] & COLMASK;
            count = othercount = emptycount = maxcount = 0;
            error = false;

            maxcount += clusters_count(state, x - 1, y, col, &count, &othercount, &emptycount);
            maxcount += clusters_count(state, x + 1, y, col, &count, &othercount, &emptycount);
            maxcount += clusters_count(state, x, y - 1, col, &count, &othercount, &emptycount);
            maxcount += clusters_count(state, x, y + 1, col, &count, &othercount, &emptycount);

            if (othercount == maxcount)
                error = true;
            else if (state->grid[y*w + x] & F_SINGLE && count > 1)
                error = true;
            else if (!(state->grid[y*w + x] & F_SINGLE) && othercount == maxcount - 1)
                error = true;

            if (error)
            {
                ret = STATUS_INVALID;
                state->grid[y*w + x] |= F_ERROR;
            }
            else
                state->grid[y*w + x] &= ~F_ERROR;
        }

    return ret;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    int s = params->w * params->h;

    const char *p = desc;
    int pos = 0;

    while (*p) {
        if (*p >= 'a' && *p < 'z')
            pos += 1 + (*p - 'a');
        else if (*p >= 'A' && *p < 'Z')
            pos += 1 + (*p - 'A');
        else if (*p == 'Z' || *p == 'z')
            pos += 25;
        else
            return "Description contains invalid characters";

        ++p;
    }

    if (pos < s + 1)
        return "Description too short";
    if (pos > s + 1)
        return "Description too long";

    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    int w = params->w;
    int h = params->h;
    int s = w*h;
    game_state *state = snew(game_state);

    state->completed = state->cheated = false;
    state->w = w;
    state->h = h;

    state->grid = snewn(w*h, char);
    memset(state->grid, 0, w*h * sizeof(char));

    const char *p = desc;
    int pos = 0;

    while (*p) {
        if (*p >= 'a' && *p < 'z') {
            pos += (*p - 'a');
            if (pos < s) {
                state->grid[pos] = F_COLOR_0 | F_SINGLE;
            }
            pos++;
        }
        else if (*p >= 'A' && *p < 'Z') {
            pos += (*p - 'A');
            if (pos < s) {
                state->grid[pos] = F_COLOR_1 | F_SINGLE;
            }
            pos++;
        }
        else if (*p == 'Z' || *p == 'z') {
            pos += 25;
        }
        else
            assert(!"Description contains invalid characters");

        ++p;
    }
    assert(pos == s + 1);

    return state;
}

static game_state *dup_game(const game_state *state)
{
    int w = state->w;
    int h = state->h;
    game_state *ret = snew(game_state);

    ret->w = w;
    ret->h = h;
    ret->completed = state->completed;
    ret->cheated = state->cheated;

    ret->grid = snewn(w*h, char);
    memcpy(ret->grid, state->grid, w*h * sizeof(char));

    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->grid);
    sfree(state);
}

/* ****** *
 * Solver *
 * ****** */

static int clusters_solver_try(game_state *state)
{
    int s = state->w*state->h;
    int i, d;
    int ret = 0;

    for (i = 0; i < s; i++)
    {
        if (state->grid[i] != 0)
            continue;

        for (d = 0; d <= 1; d++)
        {
            /* See if this leads to an invalid state */
            state->grid[i] = d ? F_COLOR_1 : F_COLOR_0;
            if (clusters_validate(state) == STATUS_INVALID)
            {
                state->grid[i] = d ? F_COLOR_0 : F_COLOR_1;
                ret++;
                break;
            }
            else
                state->grid[i] = 0;
        }
    }

    return ret;
}

static int clusters_solve_game(game_state *state, int maxdiff, char *temp);

static int clusters_solver_recurse(game_state *state, char *temp)
{
    int s = state->w*state->h;
    int i, d;
    int ret = 0, tempresult;

    for (i = 0; i < s; i++)
    {
        if (state->grid[i] != 0)
            continue;

        for (d = 0; d <= 1; d++)
        {
            /* See if this leads to an invalid state */
            memcpy(temp, state->grid, s * sizeof(char));
            state->grid[i] = d ? F_COLOR_1 : F_COLOR_0;
            tempresult = clusters_solve_game(state, 0, temp);
            memcpy(state->grid, temp, s * sizeof(char));
            if (tempresult == STATUS_INVALID)
            {
                state->grid[i] = d ? F_COLOR_0 : F_COLOR_1;
                ret++;
                break;
            }
        }
    }

    return ret;
}

static int clusters_solve_game(game_state *state, int maxdiff, char *temp)
{
    int s = state->w * state->h;
    int ret = STATUS_UNFINISHED;

    char hastemp = temp != NULL;
    if (!hastemp)
        temp = snewn(s, char);

    while ((ret = clusters_validate(state)) == STATUS_UNFINISHED)
    {
        if (clusters_solver_try(state))
            continue;

        if (maxdiff < 1) break;

        if (clusters_solver_recurse(state, temp))
            continue;

        break;
    }

    if (!hastemp)
        sfree(temp);
    return ret;
}

static char *solve_game(const game_state *state, const game_state *currstate,
    const char *aux, const char **error)
{
    game_state *solved = dup_game(state);
    char *ret = NULL;
    int result;

    clusters_solve_game(solved, 1, NULL);

    result = clusters_validate(solved);

    if (result != STATUS_INVALID) {
        int s = solved->w*solved->h;
        char *p;
        int i;

        ret = snewn(s + 2, char);
        p = ret;
        *p++ = 'S';

        for (i = 0; i < s; i++)
            *p++ = (solved->grid[i] & F_COLOR_1 ? '1' : solved->grid[i] & F_COLOR_0 ? '0' : '-');

        *p++ = '\0';
    }
    else
        *error = "Puzzle is invalid.";

    free_game(solved);
    return ret;
}

/* ********* *
 * Generator *
 * ********* */

static int clusters_generate(game_state *state, char *temp, random_state *rs, bool force)
{
    int w = state->w;
    int h = state->h;
    int i, x, y, col, count, etc = 0;
    bool reset = true;

    for (i = 0; i < w*h; i++)
    {
        if(force || !state->grid[i])
            state->grid[i] = random_upto(rs, 2) ? F_COLOR_0 : F_COLOR_1;
    }

    while (reset)
    {
        reset = false;
        for (i = 0; i < w*h; i++)
        {
            col = state->grid[i] & COLMASK;
            count = etc = 0;
            x = i%w;
            y = i / w;

            clusters_count(state, x - 1, y, col, &count, &etc, &etc);
            clusters_count(state, x + 1, y, col, &count, &etc, &etc);
            clusters_count(state, x, y - 1, col, &count, &etc, &etc);
            clusters_count(state, x, y + 1, col, &count, &etc, &etc);

            temp[i] = count;
        }

        for (i = 0; i < w*h; i++)
        {
            if (temp[i] == 0)
            {
                state->grid[i] ^= COLMASK;
                reset = true;
                break;
            }
        }

    }

    for (i = 0; i < w*h; i++)
    {
        if (temp[i] == 1)
            state->grid[i] |= F_SINGLE;
        else
            state->grid[i] = 0;
    }
    
    for (i = 0; i < w*h; i++)
    {
        x = i%w;
        y = i/w;
        
        if (x > 0 && state->grid[i] & F_SINGLE && state->grid[i] == state->grid[i-1])
        {
            state->grid[i] = 0;
            state->grid[i-1] = 0;
        }
        else if (y > 0 && state->grid[i] & F_SINGLE && state->grid[i] == state->grid[i-w])
        {
            state->grid[i] = 0;
            state->grid[i-w] = 0;
        }
    }
    
    return clusters_solve_game(state, 1, temp);
}

#define MAX_ATTEMPTS 100
static char *new_game_desc(const game_params *params, random_state *rs,
               char **aux, bool interactive)
{
    int w = params->w;
    int h = params->h;
    int s = w*h;
    game_state *state = snew(game_state);
    char *ret, *p;
    char *temp = snewn(w*h, char);
    int run, i, attempts = 0;
    bool force = false;

    state->w = w;
    state->h = h;
    state->completed = state->cheated = false;

    state->grid = snewn(w*h, char);
    memset(state->grid, 0, w*h * sizeof(char));
    
    while(clusters_generate(state, temp, rs, force) != STATUS_COMPLETE)
    {
        attempts++;
        force = (attempts % MAX_ATTEMPTS == 0);
    }

    /* Encode description */
    ret = snewn(s + 1, char);
    p = ret;
    run = 0;
    for (i = 0; i < s + 1; i++) {
        if (i == s || state->grid[i] == (F_COLOR_0|F_SINGLE)) {
            while (run > 24) {
                *p++ = 'z';
                run -= 25;
            }
            *p++ = 'a' + run;
            run = 0;
        }
        else if (state->grid[i] == (F_COLOR_1|F_SINGLE)) {
            while (run > 24) {
                *p++ = 'Z';
                run -= 25;
            }
            *p++ = 'A' + run;
            run = 0;
        }
        else {
            run++;
        }
    }
    *p = '\0';

    free_game(state);
    sfree(temp);

    return dupstr(ret);
}

/* ************** *
 * User Interface *
 * ************** */

struct game_ui {
    int cx, cy;
    bool cursor;

    int *drag;
    int dragtype;
    int ndrags;
    int click_mode;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ret = snew(game_ui);

    ret->cx = ret->cy = 0;
    ret->cursor = false;
    ret->ndrags = 0;
    ret->dragtype = -1;
    ret->drag = state ? snewn(state->w*state->h, int) : NULL;
    ret->click_mode = 0;

    return ret;
}

static void free_ui(game_ui *ui)
{
    sfree(ui->drag);
    sfree(ui);
}

static config_item *get_prefs(game_ui *ui)
{
    config_item *ret;

    ret = snewn(N_PREF_ITEMS+1, config_item);

    ret[PREF_CLICK_ACTIONS].name = "Short/Long click actions";
    ret[PREF_CLICK_ACTIONS].kw = "short-long";
    ret[PREF_CLICK_ACTIONS].type = C_CHOICES;
    ret[PREF_CLICK_ACTIONS].u.choices.choicenames = ":Dark/Light:Light/Dark";
    ret[PREF_CLICK_ACTIONS].u.choices.choicekws = ":dark:light";
    ret[PREF_CLICK_ACTIONS].u.choices.selected = ui->click_mode;

    ret[N_PREF_ITEMS].name = NULL;
    ret[N_PREF_ITEMS].type = C_END;

    return ret;
}

static void set_prefs(game_ui *ui, const config_item *cfg)
{
    ui->click_mode = cfg[PREF_CLICK_ACTIONS].u.choices.selected;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

#define COORD(x)     ( (x) * tilesize + tilesize/2 )
#define FROMCOORD(x) ( ((x)-(tilesize/2)) / tilesize )

struct game_drawstate {
    int tilesize;
    char *grid;
};

static char *interpret_move(const game_state *state, game_ui *ui,
    const game_drawstate *ds,
    int ox, int oy, int button, bool swapped)
{
    int tilesize = ds->tilesize;
    int hx = ui->cx;
    int hy = ui->cy;

    int gx = FROMCOORD(ox);
    int gy = FROMCOORD(oy);

    int w = state->w, h = state->h;

    button &= ~MOD_MASK;

    /* Mouse click */
    if (IS_MOUSE_DOWN(button))
    {
        ui->dragtype = -1;
        ui->ndrags = 0;
    }

    if (IS_MOUSE_DOWN(button) || IS_MOUSE_DRAG(button))
    {
        if (ox >= (ds->tilesize / 2) && gx < w
            && oy >= (ds->tilesize / 2) && gy < h) {
            hx = gx;
            hy = gy;
            ui->cursor = false;
        }
        else
            return NULL;
    }

    if (IS_MOUSE_DOWN(button))
    {
        int i = hy * w + hx;
        char old = state->grid[i];

        if ((button == LEFT_BUTTON && ui->click_mode == 0) ||
            (button == RIGHT_BUTTON && ui->click_mode == 1))
            ui->dragtype = (old == 0 ? F_COLOR_1 : old & F_COLOR_1 ? F_COLOR_0 : 0);
        else if ((button == RIGHT_BUTTON && ui->click_mode == 0) ||
                 (button == LEFT_BUTTON && ui->click_mode == 1))
            ui->dragtype = (old == 0 ? F_COLOR_0 : old & F_COLOR_0 ? F_COLOR_1 : 0);
        else
            ui->dragtype = 0;

        ui->ndrags = 0;
        if (ui->dragtype || old)
            ui->drag[ui->ndrags++] = i;

        return MOVE_UI_UPDATE;
    }

    if (IS_MOUSE_DRAG(button) && ui->dragtype != -1)
    {
        int i = hy * w + hx;
        int d;

        if (state->grid[i] == 0 && ui->dragtype == 0)
            return NULL;
        if (state->grid[i] & ui->dragtype)
            return NULL;

        for (d = 0; d < ui->ndrags; d++)
        {
            if (i == ui->drag[d])
                return NULL;
        }

        ui->drag[ui->ndrags++] = i;

        return MOVE_UI_UPDATE;
    }

    if (IS_MOUSE_RELEASE(button) && ui->ndrags)
    {
        int i, j;
        char *buf = snewn(ui->ndrags * 7, char);
        char *p = buf;
        char c = ui->dragtype & F_COLOR_0 ? 'A' : ui->dragtype & F_COLOR_1 ? 'B' : 'C';

        for (i = 0; i < ui->ndrags; i++)
        {
            j = ui->drag[i];
            if (state->grid[j] & F_SINGLE) continue;
            p += sprintf(p, "%c%d;", c, j);
        }
        *p++ = '\0';

        buf = sresize(buf, p - buf, char);
        ui->ndrags = 0;

        if (buf[0])
            return buf;
        
        sfree(buf);
        return MOVE_UI_UPDATE;
    }

    return MOVE_UNUSED;
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move)
{
    int w = state->w, h = state->h;
    int s = w * h;
    int i;
    char c;

    game_state *ret = dup_game(state);
    const char *p = move;
    ret->cheated = ret->completed = false;

    while (*p)
    {
        if (*p == 'S')
        {
            for (i = 0; i < s; i++)
            {
                p++;

                if (!*p || !(*p == '1' || *p == '0' || *p == '-')) {
                    free_game(ret);
                    return NULL;
                }

                if (state->grid[i] & F_SINGLE)
                    continue;

                if (*p == '1')
                    ret->grid[i] = F_COLOR_1;
                else if (*p == '0')
                    ret->grid[i] = F_COLOR_0;
                else
                    ret->grid[i] = 0;
            }

            ret->cheated = true;
        }
        else if (sscanf(p, "%c%d", &c, &i) == 2 && i >= 0
            && i < w*h && (c == 'A' || c == 'B'
                || c == 'C'))
        {
            if (!(state->grid[i] & F_SINGLE))
                ret->grid[i] = (c == 'A' ? F_COLOR_0 : c == 'B' ? F_COLOR_1 : 0);
        }
        else
        {
            free_game(ret);
            return NULL;
        }

        while (*p && *p != ';')
            p++;
        if (*p == ';')
            p++;
    }

    ret->completed = clusters_validate(ret) == STATUS_COMPLETE;
    return ret;
}

/* **************** *
 * Drawing routines *
 * **************** */

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    *x = (params->w + 1) * tilesize;
    *y = (params->h + 1) * tilesize;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    int i;
    float *ret = snewn(3 * NCOLOURS, float);

    for (i=0;i<3;i++) {
        ret[COL_BACKGROUND * 3 + i] = 1.0F;
        ret[COL_GRID       * 3 + i] = 0.0F;
        ret[COL_0          * 3 + i] = 0.75F;
        ret[COL_0_DOT      * 3 + i] = 0.0F;
        ret[COL_1          * 3 + i] = 0.25F;
        ret[COL_1_DOT      * 3 + i] = 1.0F;
        ret[COL_ERROR      * 3 + i] = 0.5F;
        ret[COL_CURSOR     * 3 + i] = 0.5F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int s = state->w*state->h;
    
    ds->tilesize = 0;
    ds->grid = snewn(s, char);
    memset(ds->grid, ~0, s * sizeof(char));
    
    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->grid);
    sfree(ds);
}

static void clusters_draw_err_rectangle(drawing *dr, int x, int y, int tilesize)
{
    double thick = tilesize / 7;
    double margin = tilesize / 20;
    int s = tilesize - 1;

    draw_rect(dr, x + margin, y + margin, s - 2 * margin, thick, COL_ERROR);
    draw_rect(dr, x + margin, y + margin, thick, s - 2 * margin, COL_ERROR);
    draw_rect(dr, x + margin, y + s - margin - thick, s - 2 * margin, thick, COL_ERROR);
    draw_rect(dr, x + s - margin - thick, y + margin, thick, s - 2 * margin, COL_ERROR);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
    const game_state *oldstate, const game_state *state,
    int dir, const game_ui *ui,
    float animtime, float flashtime)
{
    int w = state->w;
    int h = state->h;
    int x, y, d;
    int tilesize = ds->tilesize;
    char buf[48];

    sprintf(buf, "%s",
            state->cheated   ? "Auto-solved." :
            state->completed ? "COMPLETED!" : "");
    status_bar(dr, buf);

    if(ds->grid[0] == ~0) {
        draw_rect(dr, COORD(0) - tilesize / 10, COORD(0) - tilesize / 10,
            tilesize*w + 2 * (tilesize / 10) - 1,
            tilesize*h + 2 * (tilesize / 10) - 1, COL_GRID);
    }
        
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int tile = state->grid[y*w + x];

            if (!(tile & F_SINGLE))
            {
                for (d = 0; d < ui->ndrags; d++)
                {
                    if (ui->drag[d] == y*w + x)
                        tile = ui->dragtype;
                }
            }

            if (ui->cursor && ui->cx == x && ui->cy == y) tile |= F_CURSOR;

            if(ds->grid[y*w + x] == tile)
                continue;
            
            ds->grid[y*w + x] = tile;
            draw_update(dr, COORD(x), COORD(y), tilesize, tilesize);
            
            int color = tile & F_COLOR_1 ? COL_1 : tile & F_COLOR_0 ? COL_0 : COL_BACKGROUND;
            draw_rect(dr, COORD(x), COORD(y), tilesize, tilesize, COL_GRID);
            draw_rect(dr, COORD(x), COORD(y), tilesize - 1, tilesize - 1, color);

            if (tile & F_SINGLE)
            {
                int dot = tile & F_COLOR_1 ? COL_1_DOT : COL_0_DOT;
                draw_circle(dr, COORD(x + 0.5), COORD(y + 0.5), tilesize / 5, dot, dot);
            }

            if (tile & F_ERROR)
                clusters_draw_err_rectangle(dr, COORD(x), COORD(y), tilesize);

            if (tile & F_CURSOR)
            {
                draw_rect(dr, COORD(x), COORD(y), tilesize / 12, tilesize - 1, COL_CURSOR);
                draw_rect(dr, COORD(x), COORD(y), tilesize - 1, tilesize / 12, COL_CURSOR);
                draw_rect(dr, COORD(x) + tilesize - 1 - tilesize / 12, COORD(y), tilesize / 12, tilesize - 1,
                    COL_CURSOR);
                draw_rect(dr, COORD(x), COORD(y) + tilesize - 1 - tilesize / 12, tilesize - 1, tilesize / 12,
                    COL_CURSOR);
            }
        }
    }
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

static int game_status(const game_state *state)
{
    return state->completed ? +1 : 0;
}

#ifdef COMBINED
#define thegame clusters
#endif

static const char rules[] = "Fill in the grid with light and dark clusters.\n\n"
"- Tiles with a dot are adjacent to exactly 1 other tile of the same shade.\n"
"- All other tiles are adjacent to at least 2 tiles of the same shade.\n\n\n"
"This puzzle was implemented by Lennard Sprong.";

const struct game thegame = {
    "Clusters", NULL, NULL, rules,
    default_params,
    game_fetch_preset, NULL,  /* preset_menu */
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
    true, solve_game,
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
    32, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    NULL,  /* game_get_cursor_location */
    game_status,
    false, false, NULL, NULL,  /* print_size, print */
    true,                      /* wants_statusbar */
    false, NULL,               /* timing_state */
    REQUIRE_RBUTTON,           /* flags */
};

