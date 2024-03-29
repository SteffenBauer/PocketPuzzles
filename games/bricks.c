/*
 * bricks.c: Implementation for Tawamurenga puzzles.
 * (C) 2021 Lennard Sprong
 * Created for Simon Tatham's Portable Puzzle Collection
 * See LICENCE for licence details
 * 
 * Objective of the game: Shade several cells in the hexagonal grid 
 * while following these rules:
 * 1. Each shaded cell must have at least one shaded cell below it
 *    (unless it's on the bottom row).
 * 2. There can not be 3 or more shaded cells in a horizontal row.
 * 3. A number indicates the amount of shaded cells around it.
 * 4. Cells with numbers cannot be shaded.
 * 
 * This genre was invented by Nikoli.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

enum {
    COL_WHITE,
    COL_GREY,
    COL_BLACK,
    NCOLOURS
};

typedef unsigned int cell;

#define NUM_MASK       0x007
#define F_BOUND        0x008

#define F_SHADE        0x010
#define F_UNSHADE      0x020
#define F_EMPTY        0x030
#define COL_MASK       0x030

#define FE_ERROR       0x040
#define FE_TOPLEFT     0x080
#define FE_TOPRIGHT    0x100
#define FE_LINE_LEFT   0x200
#define FE_LINE_RIGHT  0x400
#define ERROR_MASK     0x7C0

#define F_DONE         0x800

struct game_params {
    int w, h;
    int diff;
};

#define DIFFLIST(A)                        \
    A(EASY,Easy,e)                         \
    A(NORMAL,Normal,n)                     \
    A(TRICKY,Tricky,t)                     \

#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title

#define DIFFENUM(upper,title,lower) DIFF_ ## upper,
enum { DIFFLIST(DIFFENUM) DIFFCOUNT };
static char const *const bricks_diffnames[] = { DIFFLIST(TITLE) };
static char const bricks_diffchars[] = DIFFLIST(ENCODE);

const static struct game_params bricks_presets[] = {
    { 5,  5, DIFF_EASY },
    { 5,  5, DIFF_NORMAL },
    { 7,  7, DIFF_EASY },
    { 7,  7, DIFF_NORMAL },
    { 7,  7, DIFF_TRICKY },
    { 8,  8, DIFF_NORMAL },
    { 8,  8, DIFF_TRICKY },
    { 10, 10, DIFF_TRICKY },
};

#define DEFAULT_PRESET 0

struct game_state {
    /* Original width and height, from parameters */
    int pw, h;

    /* Actual width, which includes grid padding */
    int w;

    cell *grid;

    bool completed, cheated;
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    *ret = bricks_presets[DEFAULT_PRESET];        /* structure copy */
    
    return ret;
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;               /* structure copy */
    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[64];
    
    if(i < 0 || i >= lenof(bricks_presets))
        return false;
    
    ret = snew(game_params);
    *ret = bricks_presets[i]; /* struct copy */
    *params = ret;
    
    sprintf(buf, "%dx%d %s", ret->w, ret->h, bricks_diffnames[ret->diff]);
    *name = dupstr(buf);
    
    return true;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static void decode_params(game_params *params, char const *string)
{
    params->w = params->h = atoi(string);
    while (*string && isdigit((unsigned char) *string)) ++string;
    if (*string == 'x') {
        string++;
        params->h = atoi(string);
        while (*string && isdigit((unsigned char)*string)) string++;
    }
    if (*string == 'd') {
        int i;
        string++;
        params->diff = DIFFCOUNT + 1;   /* ...which is invalid */
        if (*string) {
            for (i = 0; i < DIFFCOUNT; i++) {
                if (*string == bricks_diffchars[i])
                    params->diff = i;
            }
            string++;
        }
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char buf[256];
    char *p = buf;
    p += sprintf(p, "%dx%d", params->w, params->h);
    if (full)
    {
        p += sprintf(p, "d%c", bricks_diffchars[params->diff]);
    }

    *p++ = '\0';
    
    return dupstr(buf);
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
    
    ret[2].name = "Difficulty";
    ret[2].type = C_CHOICES;
    ret[2].u.choices.choicenames = DIFFLIST(CONFIG);
    ret[2].u.choices.selected = params->diff;
    
    ret[3].name = NULL;
    ret[3].type = C_END;
    
    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);
    
    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->diff = cfg[2].u.choices.selected;
    
    return ret;
}


static const char *validate_params(const game_params *params, bool full)
{
    int w = params->w;
    int h = params->h;
    
    if(w < 2) return "Width must be at least 2";
    if(h < 2) return "Height must be at least 2";
    if (params->diff >= DIFFCOUNT)
        return "Unknown difficulty rating";
    
    return NULL;
}

/* ******************** *
 * Validation and Tools *
 * ******************** */

enum { STATUS_COMPLETE, STATUS_UNFINISHED, STATUS_INVALID };

static char bricks_validate_threes(int w, int h, cell *grid)
{
    int x, y;
    char ret = STATUS_COMPLETE;

    /* Check for any three in a row, and mark errors accordingly */
    for (y = 0; y < h; y++) {
        for (x = 1; x < w-1; x++) {
            int i1 = y * w + x-1;
            int i2 = y * w + x;
            int i3 = y * w + x+1;
            if ((grid[i1] & COL_MASK) == F_SHADE &&
                (grid[i2] & COL_MASK) == F_SHADE &&
                (grid[i3] & COL_MASK) == F_SHADE) {
                ret = STATUS_INVALID;
                grid[i1] |= FE_LINE_LEFT;
                grid[i2] |= FE_LINE_LEFT|FE_LINE_RIGHT;
                grid[i3] |= FE_LINE_RIGHT;
            }
        }
    }

    return ret;
}

static char bricks_validate_gravity(int w, int h, cell *grid)
{
    int x, y;
    char ret = STATUS_COMPLETE;

    for (y = 0; y < h-1; y++) {
        for (x = 0; x < w; x++) {
            int i1 = y * w + x;

            if ((grid[i1] & COL_MASK) != F_SHADE)
                continue;

            int i2 = (y+1) * w + x-1;
            int i3 = (y+1) * w + x;

            cell n2 = x == 0 ? F_BOUND : grid[i2];
            if(!(n2 & F_BOUND))
                n2 &= COL_MASK;
            cell n3 = grid[i3];
            if(!(n3 & F_BOUND))
                n3 &= COL_MASK;

            if(n2 != F_SHADE && n3 != F_SHADE)
                ret = max(STATUS_UNFINISHED, ret);

            bool w2 = !n2 || n2 == F_UNSHADE || n2 == F_BOUND;
            bool w3 = !n3 || n3 == F_UNSHADE || n3 == F_BOUND;

            if(w2 && w3) {
                ret = STATUS_INVALID;
                grid[i1] |= FE_ERROR;
                if(n2 != F_BOUND)
                    grid[i2] |= FE_TOPRIGHT;
                if(n3 != F_BOUND)
                    grid[i3] |= FE_TOPLEFT;
            }
        }
    }

    return ret;
}

typedef struct {
    int dx, dy;
} bricks_step;
static bricks_step bricks_steps[6] = {
        {0, -1}, {1,-1},
    {-1, 0},         {1, 0},
        {-1, 1}, {0, 1},
};

static char bricks_validate_counts(int w, int h, cell *grid)
{
    int x, y, x2, y2, s;
    int shade, unshade;
    cell n, n2;
    char ret = STATUS_COMPLETE;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            n = grid[y*w+x];
            if((n & COL_MASK) || n & F_BOUND) continue;
            n &= NUM_MASK;
            if(n == 7) continue;

            shade = unshade = 0;

            for(s = 0; s < 6; s++) {
                x2 = x + bricks_steps[s].dx;
                y2 = y + bricks_steps[s].dy;

                if(x2 < 0 || x2 >= w || y2 < 0 || y2 >= h)
                    unshade++;
                else {
                    n2 = grid[y2*w+x2];
                    if(n2 & COL_MASK) {
                        n2 &= COL_MASK;
                        if(n2 == F_SHADE)
                            shade++;
                        else if(n2 == F_UNSHADE)
                            unshade++;
                    } else
                        unshade++;
                }
            }

            if(shade < n)
                ret =  max(ret, STATUS_UNFINISHED);
            
            if(shade > n || 6 - unshade < n) {
                ret = STATUS_INVALID;
                grid[y*w+x] |= FE_ERROR;
            }
        }
    }

    return ret;
}

static char bricks_validate(int w, int h, cell *grid, bool strict)
{
    int s = w * h;
    int i;
    char ret = STATUS_COMPLETE;
    char newstatus;

    for(i = 0; i < s; i++) {
        grid[i] &= ~ERROR_MASK;
    }

    newstatus = bricks_validate_threes(w, h, grid);
    ret = max(newstatus, ret);

    newstatus = bricks_validate_gravity(w, h, grid);
    ret = max(newstatus, ret);

    newstatus = bricks_validate_counts(w, h, grid);
    ret = max(newstatus, ret);

    if(strict) {
        for(i = 0; i < s; i++) {
            if((grid[i] & COL_MASK) == F_EMPTY)
                return STATUS_UNFINISHED;
        }
    }

    return ret;
}

static void bricks_grid_size(const game_params *params, int *w, int *h)
{
    *w = params->w;
    *h = params->h;

    *w += ((*h + 1) / 2) - 1;
}

static void bricks_apply_bounds(int w, int h, cell *grid)
{
    int i, x, y, extra;
    for(i = 0; i < w*h; i++)
        grid[i] = F_EMPTY;

    for (y = 0; y < h; y++)
    {
        for (x = 0; x < y / 2; x++)
        {
            grid[(y*w)+(w-x-1)] = F_BOUND;
        }
        extra = (h | y) & 1 ? 0 : 1;
        for (x = 0; x + extra < (h-y) / 2; x++)
        {
            grid[y*w + x] = F_BOUND;
        }
    }
}


static const char *validate_desc(const game_params *params, const char *desc)
{
    int w, h;
    bricks_grid_size(params, &w, &h);

    int s = params->w*params->h;
    const char *p = desc;
    cell n;

    int i = 0;
    while(*p)
    {
        if(isdigit((unsigned char) *p))
        {
            n = atoi(p);
            if(n > 7)
                return "Number is out of range";
            while (*p && isdigit((unsigned char) *p)) ++p;
            ++i;
        }
        else if(*p >= 'a' && *p <= 'z')
            i += ((*p++) - 'a') + 1;
        else if(*p >= 'A' && *p <= 'Z')
            i += ((*p++) - 'A') + 1;
        else
            ++p;
    }
    
    if(i < s)
        return "Not enough spaces";
    if(i > s)
        return "Too many spaces";
    
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    int w, h;
    bricks_grid_size(params, &w, &h);

    int i, j;
    
    game_state *state = snew(game_state);

    state->w = w;
    state->h = h;
    state->pw = params->w;
    state->completed = state->cheated = false;
    state->grid = snewn(w*h, cell);
    bricks_apply_bounds(w, h, state->grid);
    
    const char *p = desc;

    i = j = 0;
    while(*p)
    {
        if(state->grid[i] == F_BOUND) {
            ++i;
            continue;
        }

        if(j > 0) {
            ++i;
            --j;
            continue;
        }

        if(isdigit((unsigned char) *p))
        {
            state->grid[i] = atoi(p);
            while (*p && isdigit((unsigned char) *p)) ++p;
            ++j;
        }
        else if(*p >= 'a' && *p <= 'z')
            j += ((*p++) - 'a') + 1;
        else
            ++p;
    }
    
    return state;
}

static game_state *dup_game(const game_state *state)
{
    int w = state->w;
    int h = state->h;

    game_state *ret = snew(game_state);

    ret->w = w;
    ret->h = h;
    ret->pw = state->pw;
    ret->completed = state->completed;
    ret->cheated = state->cheated;
    ret->grid = snewn(w*h, cell);
    memcpy(ret->grid, state->grid, w*h*sizeof(cell));

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

static int bricks_solver_try(game_state *state)
{
    int w = state->w, h = state->h, s = w * h;
    int i, d;
    int ret = 0;

    for (i = 0; i < s; i++)
    {
        if ((state->grid[i] & COL_MASK) != F_EMPTY)
            continue;

        for (d = 0; d <= 1; d++)
        {
            /* See if this leads to an invalid state */
            state->grid[i] = d ? F_SHADE : F_UNSHADE;
            if (bricks_validate(w, h, state->grid, false) == STATUS_INVALID)
            {
                state->grid[i] = d ? F_UNSHADE : F_SHADE;
                ret++;
                break;
            }
            else
                state->grid[i] = F_EMPTY;
        }
    }

    return ret;
}

static int bricks_solve_game(game_state *state, int maxdiff, cell *temp, bool clear, bool strict);

static int bricks_solver_recurse(game_state *state, int maxdiff, cell *temp)
{
    int s = state->w*state->h;
    int i, d;
    int ret = 0, tempresult;

    for (i = 0; i < s; i++)
    {
        if ((state->grid[i] & COL_MASK) != F_EMPTY)
            continue;

        for (d = 0; d <= 1; d++)
        {
            /* See if this leads to an invalid state */
            memcpy(temp, state->grid, s * sizeof(cell));
            state->grid[i] = d ? F_SHADE : F_UNSHADE;
            tempresult = bricks_solve_game(state, maxdiff - 1, NULL, false, false);
            memcpy(state->grid, temp, s * sizeof(cell));
            if (tempresult == STATUS_INVALID)
            {
                state->grid[i] = d ? F_UNSHADE : F_SHADE;
                ret++;
                break;
            }
        }
    }

    return ret;
}

static int bricks_solve_game(game_state *state, int maxdiff, cell *temp, bool clear, bool strict)
{
    int i;
    int w = state->w, h = state->h, s = w * h;
    int ret = STATUS_UNFINISHED;

    cell hastemp = temp != NULL;
    if (!hastemp && maxdiff >= DIFF_NORMAL)
        temp = snewn(s, cell);
    
    if(clear) {
        for(i = 0; i < s; i++) {
            if(state->grid[i] & COL_MASK)
                state->grid[i] = F_EMPTY;
        }
    }

    while ((ret = bricks_validate(w, h, state->grid, strict)) == STATUS_UNFINISHED)
    {
        if (bricks_solver_try(state))
            continue;

        if (maxdiff < DIFF_NORMAL) break;

        if (bricks_solver_recurse(state, maxdiff, temp))
            continue;

        break;
    }

    if (temp && !hastemp)
        sfree(temp);
    return ret;
}

static char *solve_game(const game_state *state, const game_state *currstate,
    const char *aux, const char **error)
{
    int w = state->w, h = state->h, s = w * h;
    game_state *solved = dup_game(state);
    char *ret = NULL;
    cell n;
    int result;

    bricks_solve_game(solved, DIFF_TRICKY, NULL, true, true);

    result = bricks_validate(w, h, solved->grid, false);

    if (result != STATUS_INVALID) {
        char *p;
        int i;

        ret = snewn(s + 2, char);
        p = ret;
        *p++ = 'S';

        for (i = 0; i < s; i++) {
            n = solved->grid[i] & COL_MASK;
            *p++ = (n == F_SHADE ? '1' : n == F_UNSHADE ? '0' : '-');
        }

        *p++ = '\0';
    }
    else
        *error = "Puzzle is invalid.";

    free_game(solved);
    return ret;
}

/* **************** *
 * Puzzle Generator *
 * **************** */

static void bricks_fill_grid(game_state *state, random_state *rs)
{
    int w = state->w, h = state->h;
    int x, y;
    int run;

    for (y = h-1; y >= 0; y--) {
        run = 0;
        for (x = 0; x < w; x++) {
            int i1 = y * w + x;

            if (state->grid[i1] & F_BOUND)
                continue;

            int i2 = (y+1) * w + x-1;
            int i3 = (y+1) * w + x;

            cell n2 = (x == 0 || y == h-1) ? F_BOUND : state->grid[i2];
            if(!(n2 & F_BOUND))
                n2 &= COL_MASK;
            cell n3 = (y == h-1) ? F_BOUND : state->grid[i3];
            if(!(n3 & F_BOUND))
                n3 &= COL_MASK;

            if(run == 2 || (y != h-1 && n2 != F_SHADE && n3 != F_SHADE) || random_upto(rs, 3) == 0) {
                state->grid[i1] = F_UNSHADE;
                run = 0;
            }
            else {
                state->grid[i1] = F_SHADE;
                run++;
            }
        }
    }
}

static int bricks_build_numbers(game_state *state)
{
    int total = 0;
    int w = state->w, h = state->h;
    int x, y, x2, y2, s;
    int shade;
    cell n, n2;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            n = state->grid[y*w+x];
            if(n == F_SHADE) total++;

            if(n == F_SHADE || n & F_BOUND) continue;

            shade = 0;

            for(s = 0; s < 6; s++) {
                x2 = x + bricks_steps[s].dx;
                y2 = y + bricks_steps[s].dy;

                if(x2 < 0 || x2 >= w || y2 < 0 || y2 >= h)
                    continue;

                n2 = state->grid[y2*w+x2];
                if((n2 & COL_MASK) == F_SHADE)
                    shade++;
            }

            state->grid[y*w+x] = shade;
        }
    }

    return total;
}

static char bricks_remove_numbers(game_state *state, int maxdiff, cell *tempgrid, random_state *rs)
{
    int w = state->w, h = state->h;
    int *spaces = snewn(w*h, int);
    int i1, j;
    cell temp;

    for (j = 0; j < w*h; j++)
        spaces[j] = j;

    shuffle(spaces, w*h, sizeof(*spaces), rs);
    for(j = 0; j < w*h; j++)
    {
        i1 = spaces[j];
        temp = state->grid[i1];
        if (temp & F_BOUND) continue;
        state->grid[i1] = F_EMPTY;

        if (bricks_solve_game(state, maxdiff, tempgrid, true, true) != STATUS_COMPLETE)
        {
            state->grid[i1] = temp;
        }
    }

    sfree(spaces);

    return true;
}

#define MINIMUM_SHADED 0.4
static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive)
{
    int w, h;
    int spaces = params->w*params->h, total;
    game_state *state = snew(game_state);
    bricks_grid_size(params, &w, &h);
    state->w = w;
    state->h = h;
    state->cheated = state->completed = false;

    int i;
    cell n;
    state->grid = snewn(w*h, cell);
    cell *tempgrid = snewn(w*h, cell);

    while(true)
    {
        bricks_apply_bounds(w, h, state->grid);

        bricks_fill_grid(state, rs);
        bricks_build_numbers(state);

        /* Find ambiguous areas by solving the game, then filling in all unknown squares with a number */
        bricks_solve_game(state, DIFF_EASY, tempgrid, true, false);
        total = bricks_build_numbers(state);

        /* Enforce minimum percentage of shaded squares */
        if((total * 1.0f) / spaces < MINIMUM_SHADED)
            continue;

        bricks_remove_numbers(state, params->diff, tempgrid, rs);

        /* Enforce minimum difficulty */
        if(params->diff > DIFF_EASY && spaces > 6 && bricks_solve_game(state, DIFF_EASY, tempgrid, true, true) == STATUS_COMPLETE)
            continue;

        break;
    }

    char *ret = snewn(w*h*4, char);
    char *p = ret;
    int run = 0;
    enum { RUN_NONE, RUN_BLANK, RUN_NUMBER } runtype = RUN_NONE;
    for(i = 0; i <= w*h; i++)
    {
        n = (i == w*h) ? 0 : state->grid[i];

        if(runtype == RUN_BLANK && (i == w*h || !(n & COL_MASK)))
        {
            while(run >= 26)
            {
                *p++ = 'z';
                run -= 26;
            }
            if(run)
                *p++ = 'a' + run-1;
            run = 0;
        }

        if(i == w*h)
            break;

        if(n >= 0 && n <= 7)
        {
            if(runtype == RUN_NUMBER)
                *p++ = '_';
            p += sprintf(p, "%d", n);
            runtype = RUN_NUMBER;
        }
        else if(n & COL_MASK)
        {
            runtype = RUN_BLANK;
            run++;
        }
    }
    *p++ = '\0';
    ret = sresize(ret, p - ret, char);
    free_game(state);
    sfree(tempgrid);
    return ret;
}

/* ************** *
 * User Interface *
 * ************** */

#define TARGET_SHOW 0x1
#define TARGET_CONNECTED 0x2

struct game_ui
{
    cell dragtype;
    int *drag;
    int ndrags;
    int click_mode;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ret = snew(game_ui);
    ret->ndrags = 0;
    ret->dragtype = 0;
    ret->click_mode = 0;
    ret->drag = NULL;

    if (state) {
        int i, w = state->w, s = w*state->h;
        for (i = 0; i < s; i++)
        {
            if (state->grid[i] != F_BOUND) break;
        }
        ret->drag = snewn(s, int);
    }

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

    ret = snewn(2, config_item);

    ret[0].name = "Short/Long click actions";
    ret[0].kw = "short-long";
    ret[0].type = C_CHOICES;
    ret[0].u.choices.choicenames = ":Black/White:White/Black";
    ret[0].u.choices.choicekws = ":black:white";
    ret[0].u.choices.selected = ui->click_mode;

    ret[1].name = NULL;
    ret[1].type = C_END;

    return ret;
}

static void set_prefs(game_ui *ui, const config_item *cfg)
{
    ui->click_mode = cfg[0].u.choices.selected;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

struct game_drawstate {
    int tilesize, w, h;
    double thickness;
    int offsetx, offsety;

    cell *oldgrid;
    cell *grid;
    int prevdrags;
};

#define DRAG_RADIUS 0.6F

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int ox, int oy, int button, bool swapped)
{
    int w = state->w, h = state->h;
    int gx, gy;
    int tilesize = ds->tilesize;
    int hx, hy;

    oy -= ds->offsety;
    ox -= ds->offsetx;
    gy = oy < 0 ? -1 : oy / tilesize;
    ox -= gy * tilesize / 2;
    gx = ox < 0 ? -1 : ox / tilesize;

    if (IS_MOUSE_DOWN(button))
    {
        ui->dragtype = 0;
        ui->ndrags = 0;
    }

    if (gx < 0 || gx >= w || gy < 0 || gy >= h) {
        return NULL;
    }

    hx = gx;
    hy = gy;
    if (IS_MOUSE_DOWN(button))
    {
        int i = hy * w + hx;
        cell old = state->grid[i];
        old &= COL_MASK;

        if (button == ((ui->click_mode == 0) ? LEFT_BUTTON : RIGHT_BUTTON))
            ui->dragtype = (old == F_UNSHADE ? F_EMPTY : old == F_SHADE ? F_UNSHADE : F_SHADE);
        else if (button == ((ui->click_mode == 0) ? RIGHT_BUTTON : LEFT_BUTTON))
            ui->dragtype = (old == F_UNSHADE ? F_SHADE : old == F_SHADE ? F_EMPTY : F_UNSHADE);
        else
            ui->dragtype = F_EMPTY;

        ui->ndrags = 0;
        if (ui->dragtype || old)
            ui->drag[ui->ndrags++] = i;

        return MOVE_UI_UPDATE;
    }

    if (IS_MOUSE_DRAG(button) && ui->dragtype)
    {
        int i = hy * w + hx;
        int d;

        if (state->grid[i] == ui->dragtype)
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
        char c = ui->dragtype == F_SHADE ? 'A' : ui->dragtype == F_UNSHADE ? 'B' : 'C';

        if (ui->ndrags == 1 && !(state->grid[ui->drag[0]] & (COL_MASK|F_BOUND))) {
            p += sprintf(p, "D%d;", ui->drag[0]);
        }
        else {
            for (i = 0; i < ui->ndrags; i++)
            {
                j = ui->drag[i];
                if (!(state->grid[j] & COL_MASK)) continue;
                p += sprintf(p, "%c%d;", c, j);
            }
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

                if (!(state->grid[i] & COL_MASK))
                    continue;

                if (*p == '0')
                    ret->grid[i] = F_UNSHADE;
                else if (*p == '1')
                    ret->grid[i] = F_SHADE;
                else
                    ret->grid[i] = F_EMPTY;
            }

            ret->cheated = true;
        }
        else if (sscanf(p, "%c%d", &c, &i) == 2 && i >= 0
            && i < w*h && (c == 'A' || c == 'B'
                || c == 'C' || c == 'D'))
        {
            if (state->grid[i] & COL_MASK)
                ret->grid[i] = (c == 'A' ? F_SHADE : c == 'B' ? F_UNSHADE : F_EMPTY);
            else if (!(state->grid[i] & (COL_MASK|F_BOUND)) && c == 'D')
                ret->grid[i] ^= F_DONE;
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

    ret->completed = bricks_validate(w, h, ret->grid, true) == STATUS_COMPLETE;
    return ret;
}

/* **************** *
 * Drawing routines *
 * **************** */

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    tilesize &= ~1;
    *x = (params->w+1) * tilesize;
    *y = (params->h+1) * tilesize;

    *x += (tilesize / 2);
}

static void game_set_offsets(int h, int tilesize, int *offsetx, int *offsety)
{
    *offsetx = tilesize / 2;
    *offsety = tilesize / 2;
    *offsetx -= ((h / 2) - 1) * tilesize;
    if (h & 1)
        *offsetx -= tilesize;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    tilesize &= ~1;
    ds->tilesize = tilesize;
    ds->thickness = max(2.0L, tilesize / 7.0L);
    game_compute_size(params, tilesize, NULL, &ds->w, &ds->h);

    game_set_offsets(params->h, tilesize, &ds->offsetx, &ds->offsety);
}

static float *game_colours(frontend *fe, int *ncolours)
{
    int i;
    float *ret = snewn(3 * NCOLOURS, float);

    for (i=0;i<3;i++) {
        ret[COL_WHITE  * 3 + i] = 1.0F;
        ret[COL_GREY   * 3 + i] = 0.5F;
        ret[COL_BLACK  * 3 + i] = 0.0F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int s = state->w * state->h;
    
    ds->tilesize = 0;
    ds->oldgrid = snewn(s, cell);
    ds->grid = snewn(s, cell);
    ds->prevdrags = 0;

    memset(ds->oldgrid, ~0, s*sizeof(cell));
    memcpy(ds->grid, state->grid, s*sizeof(cell));

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->oldgrid);
    sfree(ds->grid);
    sfree(ds);
}

static void bricks_draw_err_rectangle(drawing *dr, int x, int y, int w, int h,
                                      int tilesize)
{
    double thick = tilesize / 10;
    double margin = tilesize / 20;

    draw_rect(dr, x+margin, y+margin, w-2*margin, thick, COL_GREY);
    draw_rect(dr, x+margin, y+margin, thick, h-2*margin, COL_GREY);
    draw_rect(dr, x+margin, y+h-margin-thick, w-2*margin, thick, COL_GREY);
    draw_rect(dr, x+w-margin-thick, y+margin, thick, h-2*margin, COL_GREY);
}

/* Copied from tents.c */
static void bricks_draw_err_gravity(drawing *dr, int tilesize, int x, int y)
{
    int coords[8];
    int yext, xext;

    /*
    * Draw a diamond.
    */
    coords[0] = x - tilesize*2/5;
    coords[1] = y;
    coords[2] = x;
    coords[3] = y - tilesize*2/5;
    coords[4] = x + tilesize*2/5;
    coords[5] = y;
    coords[6] = x;
    coords[7] = y + tilesize*2/5;
    draw_polygon(dr, coords, 4, COL_GREY, COL_GREY);

    /*
    * Draw an exclamation mark in the diamond. This turns out to
    * look unpleasantly off-centre if done via draw_text, so I do
    * it by hand on the basis that exclamation marks aren't that
    * difficult to draw...
    */
    xext = tilesize/16;
    yext = tilesize*2/5 - (xext*2+2);
    draw_rect(dr, x-xext, y-yext, xext*2+1, yext*2+1 - (xext*3), COL_WHITE);
    draw_rect(dr, x-xext, y+yext-xext*2+1, xext*2+1, xext*2, COL_WHITE);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int w = state->w;
    int h = state->h;
    int tilesize = ds->tilesize;
    int tx, ty, tx1, ty1, clipw;
    int i;
    cell n;
    char buf[48];
    int colour;

    sprintf(buf, "%s",
            state->cheated   ? "Auto-solved." :
            state->completed ? "COMPLETED!" : "");
    status_bar(dr, buf);

    if(ds->prevdrags >= ui->ndrags)
        memcpy(ds->grid, state->grid, w*h*sizeof(cell));

    for(i = ds->prevdrags; i < ui->ndrags; i++)
    {
        if(ds->grid[ui->drag[i]] & COL_MASK)
            ds->grid[ui->drag[i]] = ui->dragtype;
    }

    ds->prevdrags = ui->ndrags;
    if(ds->prevdrags > 0)
        bricks_validate(w, h, ds->grid, false);
    
    /* Draw squares */
    for(i = 0; i < w*h; i++)
    {
        n = ds->grid[i];
        if(n & F_BOUND) continue;

        tx = (i%w) * tilesize + ds->offsetx;
        ty = (i/w) * tilesize + ds->offsety;

        tx += (i/w) * tilesize / 2;

        if(ds->oldgrid[i] == n) continue;

        colour = n & FE_ERROR                                  ? COL_BLACK : 
                (n & COL_MASK) == F_SHADE                      ? COL_BLACK :
                (n & COL_MASK) == F_UNSHADE || !(n & COL_MASK) ? COL_WHITE :
                                                                 COL_GREY;
        
        tx1 = tx;
        clipw = tilesize+1;

        /* Expand clip size near horizontal edges to draw error diamonds */
        if(i%w == 0 || ds->grid[i-1] & F_BOUND) {
            tx1 -= tilesize;
            clipw += tilesize;
            draw_update(dr, tx1+1, ty, tilesize+1, tilesize+1);
            draw_rect(dr, tx1+1, ty+1, tilesize-1, tilesize-1, COL_WHITE);
        }

        if(i%w == w-1 || ds->grid[i+1] & F_BOUND) {
            clipw += tilesize;
            draw_update(dr, tx+tilesize+1, ty, tilesize+1, tilesize+1);
            draw_rect(dr, tx+tilesize+1, ty+1, tilesize-1, tilesize-1, COL_WHITE);
        }
        
        /* Draw tile background */
        clip(dr, tx1, ty, clipw, tilesize+1);
        draw_update(dr, tx, ty, tilesize+1, tilesize+1);
        draw_rect(dr, tx+1, ty+1, tilesize-1, tilesize-1, colour);
        ds->oldgrid[i] = n;
        
        /* Draw square border */
        if(!(n & F_BOUND)) {
            int sqc[8];
            sqc[0] = tx;
            sqc[1] = ty;
            sqc[2] = tx + tilesize;
            sqc[3] = ty;
            sqc[4] = tx + tilesize;
            sqc[5] = ty + tilesize;
            sqc[6] = tx;
            sqc[7] = ty + tilesize;
            draw_polygon(dr, sqc, 4, -1, COL_BLACK);
        }
        
        /* Draw 3-in-a-row errors */
        if (n & (FE_LINE_LEFT | FE_LINE_RIGHT)) {
            int left = tx + 1, right = tx + tilesize - 1;
            if (n & FE_LINE_LEFT)
                right += tilesize/2;
            if (n & FE_LINE_RIGHT)
                left -= tilesize/2;
            bricks_draw_err_rectangle(dr, left, ty+1, right-left, tilesize-1, tilesize);
        }

        tx1 = tx + (tilesize/2), ty1 = ty + (tilesize/2);

        /* Draw the number */
        if(!(n & (COL_MASK|F_BOUND)))
        {
            if((n & NUM_MASK) == 7)
                sprintf(buf, "?");
            else
                sprintf(buf, "%d", n & NUM_MASK);
            
            draw_text(dr, tx1, ty1,
                    FONT_VARIABLE, tilesize/2, ALIGN_HCENTRE|ALIGN_VCENTRE,
                    n & FE_ERROR ? COL_WHITE : n & F_DONE ? COL_GREY : COL_BLACK, buf);
            if (n & FE_ERROR)
                bricks_draw_err_rectangle(dr, tx, ty, tilesize, tilesize, tilesize);
        } else if(n & FE_ERROR) {
            bricks_draw_err_gravity(dr, tilesize, tx1, ty + tilesize);
        }

        if(n & FE_TOPLEFT) {
            bricks_draw_err_gravity(dr, tilesize, tx, ty);
        }
        if(n & FE_TOPRIGHT) {
            bricks_draw_err_gravity(dr, tilesize, tx + tilesize, ty);
        }

        unclip(dr);
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
#define thegame bricks
#endif

static const char rules[] = "Shade several cells in the grid while following these rules:\n\n"
"- Each shaded cell must have at least one shaded cell below it (unless it's on the bottom row).\n"
"- A maximum of two cells can be connected horizontally.\n"
"- A number indicates the amount of shaded cells around it.\n"
"- Cells with numbers cannot be shaded.\n\n\n"
"This puzzle was implemented by Lennard Sprong.";

const struct game thegame = {
    "Bricks", "games.bricks", "bricks", rules,
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
    48, game_compute_size, game_set_size,
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
    REQUIRE_RBUTTON, /* flags */
};

