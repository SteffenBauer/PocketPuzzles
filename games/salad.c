/*
 * salad.c: Implemention of ABC End View puzzles.
 * (C) 2013 Lennard Sprong
 * Created for Simon Tatham's Portable Puzzle Collection
 * See LICENCE for licence details
 *
 * This puzzle has two different game modes: ABC End View and Number Ball.
 * Objective: Enter each letter once in each row and column. 
 * Some squares remain empty.
 * ABC End View:  The letters on the edge indicate which letter
 * is encountered first when 'looking' into the grid.
 * Number Ball: A circle indicates that a number must be placed here,
 * and a cross indicates a space that remains empty.
 *
 * Number Ball was invented by Inaba Naoki. 
 * I don't know who first designed ABC End View.
 *
 * http://www.janko.at/Raetsel/AbcEndView/index.htm
 * http://www.janko.at/Raetsel/Nanbaboru/index.htm
 */

/*
 * TODO:
 * - Add difficulty levels
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"
#include "latin.h"

enum { GAMEMODE_LETTERS, GAMEMODE_NUMBERS };

enum {
    COL_BACKGROUND,
    COL_HIGHLIGHT,
    COL_LOWLIGHT,
    COL_BORDER,
    COL_BORDERCLUE,
    COL_PENCIL,
    COL_I_NUM, /* Immutable */
    COL_I_BALL,
    COL_I_BALLBG,
    COL_I_HOLE,
    COL_G_NUM, /* Guess */
    COL_G_BALL,
    COL_G_BALLBG,
    COL_G_HOLE,
    COL_E_BG,
    COL_E_BORDERCLUE,
    COL_E_NUM, /* Error */
    COL_E_BALL,
    COL_E_HOLE,
    NCOLOURS
};

#define DIFFLIST(A) \
    A(EASY,Normal,salad_solver_easy, e) \
    A(TRICKY,Tricky,NULL,x)
        
#define ENUM(upper,title,func,lower) DIFF_ ## upper,
#define TITLE(upper,title,func,lower) #title,
#define ENCODE(upper,title,func,lower) #lower
#define CONFIG(upper,title,func,lower) ":" #title
static char const salad_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCOUNT lenof(salad_diffchars)

enum { DIFFLIST(ENUM) DIFF_IMPOSSIBLE = diff_impossible,
    DIFF_AMBIGUOUS = diff_ambiguous, DIFF_UNFINISHED = diff_unfinished };

#define DIFF_HOLESONLY (DIFF_EASY - 1)

struct game_params {
    int order;
    int nums;
    int mode;
    int diff;
};

struct game_state {
    game_params *params;
    
    digit *borderclues;
    digit *gridclues;
    
    digit *grid;
    
    /* Extra map of confirmed holes/characters */
    char *holes;
    
    bool completed, cheated;
    
    unsigned int *marks;
};

#define DEFAULT_PRESET 0

static const struct game_params salad_presets[] = {
    {5, 3, GAMEMODE_LETTERS, DIFF_EASY},
    {5, 3, GAMEMODE_NUMBERS, DIFF_EASY},
    {5, 3, GAMEMODE_LETTERS, DIFF_TRICKY},
    {5, 3, GAMEMODE_NUMBERS, DIFF_TRICKY},
    {5, 4, GAMEMODE_LETTERS, DIFF_TRICKY},
    {5, 4, GAMEMODE_NUMBERS, DIFF_TRICKY},
    {6, 4, GAMEMODE_LETTERS, DIFF_TRICKY},
    {6, 4, GAMEMODE_NUMBERS, DIFF_TRICKY},
    {7, 4, GAMEMODE_LETTERS, DIFF_TRICKY},
    {7, 4, GAMEMODE_NUMBERS, DIFF_TRICKY},
    {8, 5, GAMEMODE_LETTERS, DIFF_TRICKY},
    {8, 5, GAMEMODE_NUMBERS, DIFF_TRICKY},
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    *ret = salad_presets[DEFAULT_PRESET]; /* struct copy */

    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[64];
    
    if(i < 0 || i >= lenof(salad_presets))
        return false;
    
    ret = snew(game_params);
    *ret = salad_presets[i]; /* struct copy */
    *params = ret;
    
    if(ret->mode == GAMEMODE_LETTERS)
        sprintf(buf, "Letters: %dx%d A~%c %s", ret->order, ret->order, ret->nums + 'A' - 1, ret->diff == 0 ? "Easy" : "Tricky");
    else
        sprintf(buf, "Numbers: %dx%d 1~%c %s", ret->order, ret->order, ret->nums + '0', ret->diff == 0 ? "Easy" : "Tricky");
    *name = dupstr(buf);
    
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
    params->order = atoi(string);
    while (*p && isdigit((unsigned char)*p)) ++p;

    if (*p == 'n')
    {
        ++p;
        params->nums = atoi(p);
        while (*p && isdigit((unsigned char)*p)) ++p;
    }
    
    if (*p == 'B')
    {
        params->mode = GAMEMODE_NUMBERS;
    }
    else if(*p == 'L')
    {
        params->mode = GAMEMODE_LETTERS;
    }
    
    if (*p == 'd') {
        int i;
        p++;
        params->diff = DIFFCOUNT + 1;   /* ...which is invalid */
        if (*p) {
            for (i = 0; i < DIFFCOUNT; i++) {
                if (*p == salad_diffchars[i])
                    params->diff = i;
            }
            p++;
        }
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char ret[80];
    sprintf(ret, "%dn%d%c", params->order, params->nums,
        params->mode == GAMEMODE_LETTERS ? 'L' : 'B');
    if (full)
        sprintf(ret + strlen(ret), "d%c", salad_diffchars[params->diff]);
    
    return dupstr(ret);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];
    
    ret = snewn(5, config_item);
    
    ret[0].name = "Game Mode";
    ret[0].type = C_CHOICES;
    ret[0].u.choices.choicenames = ":ABC End View:Number Ball";
    ret[0].u.choices.selected = params->mode;
    
    ret[1].name = "Size (s*s)";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->order);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = "Symbols";
    ret[2].type = C_STRING;
    sprintf(buf, "%d", params->nums);
    ret[2].u.string.sval = dupstr(buf);
    
    ret[3].name = "Difficulty";
    ret[3].type = C_CHOICES;
    ret[3].u.choices.choicenames = DIFFLIST(CONFIG);
    ret[3].u.choices.selected = params->diff;
    
    ret[4].name = NULL;
    ret[4].type = C_END;
    
    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = default_params();
    
    ret->mode = cfg[0].u.choices.selected;
    ret->order = atoi(cfg[1].u.string.sval);
    ret->nums = atoi(cfg[2].u.string.sval);
    ret->diff = cfg[3].u.choices.selected;
    
    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if(params->nums < 2)
        return "Symbols must be at least 2.";
    if(params->nums >= params->order)
        return "Symbols must be lower than the size.";
    if(params->order < 3)
        return "Size must be at least 3.";
    if(params->order > 12)
        return "Size must be at most 12.";
    if(params->nums > 9)
        return "Symbols must be no more than 9.";
    if (params->diff >= DIFFCOUNT)
        return "Unknown difficulty rating";
    
    return NULL;
}

static game_state *blank_game(const game_params *params)
{
    int o = params->order;
    int o2 = o*o;

    game_state *state = snew(game_state);
    
    state->params = snew(game_params);
    *(state->params) = *params;               /* structure copy */
    state->grid = snewn(o2, digit);
    state->holes = snewn(o2, char);
    state->borderclues = snewn(o*4, digit);
    state->gridclues = snewn(o2, digit);
    state->marks = snewn(o2, unsigned int);
    state->completed = state->cheated = false;
    
    memset(state->marks, 0, o2 * sizeof(unsigned int));
    
    return state;
}

static game_state *dup_game(const game_state *state)
{
    int o = state->params->order;
    int o2 = o*o;
    game_state *ret = blank_game(state->params);

    memcpy(ret->grid, state->grid, o2 * sizeof(digit));
    memcpy(ret->holes, state->holes, o2 * sizeof(char));
    memcpy(ret->borderclues, state->borderclues, o*4 * sizeof(digit));
    memcpy(ret->gridclues, state->gridclues, o2 * sizeof(digit));
    memcpy(ret->marks, state->marks, o2 * sizeof(unsigned int));
    
    ret->completed = state->completed;
    ret->cheated = state->cheated;

    return ret;
}

static void free_game(game_state *state)
{
    free_params(state->params);
    sfree(state->grid);
    sfree(state->holes);
    sfree(state->borderclues);
    sfree(state->gridclues);
    sfree(state->marks);
    sfree(state);
}

/* *********************** *
 * Latin square with holes *
 * *********************** */

/* This square definitely doesn't contain a character */
#define LATINH_CROSS  'X'
/* This square must contain a character */
#define LATINH_CIRCLE 'O'

struct solver_ctx {
    game_state *state;
    int order;
    int nums;
};

static struct solver_ctx *new_ctx(game_state *state, int order, int nums)
{
    struct solver_ctx *ctx = snew(struct solver_ctx);
    ctx->state = state;
    ctx->order = order;
    ctx->nums = nums;
    
    return ctx;
}

static void *clone_ctx(void *vctx)
{
    struct solver_ctx *octx = (struct solver_ctx *)vctx;
    struct solver_ctx *nctx = new_ctx(octx->state, octx->order, octx->nums);
    
    return nctx;
}

static void free_ctx(void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    sfree(ctx);
}

static int latinholes_solver_sync(struct latin_solver *solver, struct solver_ctx *sctx)
{
    /* Check the marks for each square, and see if it confirms a square
     * being empty or not empty */
    
    int i, n;
    int o = solver->o;
    int o2 = o*o;
    bool match;
    int nums = sctx->nums;
    int nchanged = 0;
    
    if(nums == o)
        return 0;
    
    for(i = 0; i < o2; i++)
    {
        if(sctx->state->holes[i])
            continue;
        
        /* Check for possibilities for numbers */
        match = false;
        for(n = 0; n < nums; n++)
        {
            if(cube(i%o, i/o, n+1))
                match = true;
        }
        if(!match)
        {
            /* This square must be a hole */
            nchanged++;
            sctx->state->holes[i] = LATINH_CROSS;
            continue;
        }
        
        /* Check for possibilities for hole */
        match = false;
        for(n = nums; n < o; n++)
        {
            if(cube(i%o, i/o, n+1))
                match = true;
        }
        if(!match)
        {
            /* This square must be a number */
            nchanged++;
            sctx->state->holes[i] = LATINH_CIRCLE;
        }
    }
    
    return nchanged;
}

static int latinholes_solver_place_cross(struct latin_solver *solver, struct solver_ctx *sctx, int x, int y)
{
    int n;
    int nchanged = 0;
    int nums = sctx->nums;

    for(n = 0; n < nums; n++)
    {
        if(!cube(x, y, n+1))
            continue;
        
        cube(x, y, n+1) = false;
        nchanged++;
    }
    
    return nchanged;
}

static int latinholes_solver_place_circle(struct latin_solver *solver, struct solver_ctx *sctx, int x, int y)
{
    int n;
    int nchanged = 0;
    int o = solver->o;
    int nums = sctx->nums;

    for(n = nums; n < o; n++)
    {
        if(!cube(x, y, n+1))
            continue;
        
        cube(x, y, n+1) = false;
        nchanged++;
    }
    
    return nchanged;
}

static int latinholes_solver_count(struct latin_solver *solver, struct solver_ctx *sctx)
{
    int o = solver->o;
    int nums = sctx->nums;
    int dir, holecount, circlecount;
    int x, y, i, j;
    int nchanged = 0;
    
    x = 0; y = 0;
    
    for(dir = 0; dir < 2; dir++)
    {
        for(i = 0; i < o; i++)
        {
            if(dir) x = i; else y = i;
            
            holecount = 0;
            circlecount = 0;
            for(j = 0; j < o; j++)
            {
                if(dir) y = j; else x = j;
                
                if(sctx->state->holes[y*o+x] == LATINH_CROSS)
                    holecount++;
                if(sctx->state->holes[y*o+x] == LATINH_CIRCLE)
                    circlecount++;
            }
            
            if(holecount == (o-nums))
            {
                for(j = 0; j < o; j++)
                {
                    if(dir) y = j; else x = j;
                    
                    if(!sctx->state->holes[y*o+x])
                        nchanged += latinholes_solver_place_circle(solver, sctx, x, y);
                }
            }
            else if(circlecount == nums)
            {
                for(j = 0; j < o; j++)
                {
                    if(dir) y = j; else x = j;
                    
                    if(!sctx->state->holes[y*o+x])
                        nchanged += latinholes_solver_place_cross(solver, sctx, x, y);
                }
            }
        }
    }
    
    return nchanged;
}

static int latinholes_check(game_state *state)
{
    int o = state->params->order;
    int nums = state->params->nums;
    int od = o*nums;
    int x, y, i;
    bool fail;
    digit d;
    
    fail = false;
    
    int *rows, *cols, *hrows, *hcols;
    rows = snewn(od, int);
    cols = snewn(od, int);
    hrows = snewn(o, int);
    hcols = snewn(o, int);
    memset(rows, 0, od * sizeof(int));
    memset(cols, 0, od * sizeof(int));
    memset(hrows, 0, o * sizeof(int));
    memset(hcols, 0, o * sizeof(int));
    
    for(x = 0; x < o; x++)
    for(y = 0; y < o; y++)
    {
        d = state->grid[y*o+x];
        if(d == 0 || d > nums)
        {
            hrows[y]++;
            hcols[x]++;
        }
        else
        {
            rows[y*nums+d-1]++;
            cols[x*nums+d-1]++;
        }
        
        if(d == 0 && state->holes[y*o+x] == LATINH_CIRCLE)
            fail = true;
    }
    
    for(i = 0; i < o; i++)
    {
        if(hrows[i] != (o-nums) || hcols[i] != (o-nums))
        {
            fail = true;
        }
    }
    
    for(i = 0; i < od; i++)
    {
        if(rows[i] != 1 || cols[i] != 1)
        {
            fail = true;
        }
    }
    
    sfree(rows);
    sfree(cols);
    sfree(hrows);
    sfree(hcols);
    
    return !fail;
}

/* ******************** *
 * Salad Letters solver *
 * ******************** */
static int salad_letters_solver_dir(struct latin_solver *solver, struct solver_ctx *sctx, int si, int di, int ei, int cd)
{
    char clue;
    clue = sctx->state->borderclues[cd];
    if(!clue)
        return 0;
    
    int i, j;
    int o = solver->o;
    int nums = sctx->nums;
    int nchanged = 0;
    
    int dist = 0;
    int maxdist;
    bool found = false;
    bool outofrange = false;
    
    /* 
     * Determine max. distance by counting the holes
     * which can never be used for the clue
     */
    maxdist = o-nums;
    for(i = si + (di*(o-nums)); i != ei; i+=di)
    {
        if(sctx->state->holes[i] == LATINH_CROSS)
            maxdist--;
    }
    
    for(i = si; i != ei; i+=di)
    {
        /* Rule out other possibilities near clue */
        if(!found)
        {
            for(j = 1; j <= nums; j++)
            {
                if(j == clue)
                    continue;
                
                if(cube(i%o, i/o, j))
                {
                    cube(i%o, i/o, j) = false;
                    nchanged++;
                }
            }
        }
        
        if(sctx->state->holes[i] != LATINH_CROSS)
            found = true;
        
        /* Rule out this possibility too far away from clue */
        
        if(outofrange)
        {
            if(cube(i%o, i/o, clue))
            {
                cube(i%o, i/o, clue) = false;
                nchanged++;
            }
        }
        dist++;
        
        if(sctx->state->holes[i] == LATINH_CIRCLE || dist > maxdist)
            outofrange = true;
    }
    
    return nchanged;
}
 
static int salad_letters_solver(struct latin_solver *solver, struct solver_ctx *sctx)
{
    int nchanged = 0;
    int o = solver->o;
    int o2 = o*o;
    int i;
    
    for(i = 0; i < o; i++)
    {
        /* Top */
        nchanged += salad_letters_solver_dir(solver, sctx, i, o, o2+i, i+0);
        /* Left */
        nchanged += salad_letters_solver_dir(solver, sctx, i*o, 1, ((i+1)*o), i+o);
        /* Bottom */
        nchanged += salad_letters_solver_dir(solver, sctx, (o2-o)+i, -o, i-o, i+(o*2));
        /* Right */
        nchanged += salad_letters_solver_dir(solver, sctx, ((i+1)*o)-1, -1, i*o - 1, i+(o*3));
    }
    
    return nchanged;
}

static game_state *load_game(const game_params *params, const char *desc, char **fail)
{
    int o = params->order;
    int nums = params->nums;
    int o2 = o*o;
    int ox4 = o*4;
    int c, pos;
    digit d;
    
    game_state *ret = blank_game(params);
    memset(ret->grid, 0, o2 * sizeof(digit));
    memset(ret->holes, 0, o2 * sizeof(char));
    memset(ret->borderclues, 0, ox4 * sizeof(char));
    memset(ret->gridclues, 0, o2 * sizeof(digit));
    
    const char *p = desc;
    /* Read border clues */
    if(params->mode == GAMEMODE_LETTERS)
    {
        pos = 0;
        while(*p && *p != ',')
        {
            c = *p++;
            d = 0;
            if(pos >= ox4)
            {
                free_game(ret);
                *fail = "Border description is too long.";
                return NULL;
            }
            
            if(c >= 'a' && c <= 'z')
                pos += (c - 'a') + 1;
            else if(c >= '1' && c <= '9')
                d = c - '0';
            else if(c >= 'A' && c <= 'I')
                d = (c - 'A') + 1;
            else
            {
                free_game(ret);
                *fail = "Border description contains invalid characters.";
                return NULL;
            }
            
            if(d > 0 && d <= nums)
            {
                ret->borderclues[pos] = d;
                pos++;
            }
            else if(d > nums)
            {
                free_game(ret);
                *fail = "Border clue is out of range.";
                return NULL;
            }
        }
        
        if(pos < ox4)
        {
            free_game(ret);
            *fail = "Description is too short.";
            return NULL;
        }
        
        if(*p == ',')
            p++;
    }
    /* Read grid clues */
    
    pos = 0;
    while(*p)
    {
        c = *p++;
        d = 0;
        if(pos >= o2)
        {
            free_game(ret);
            *fail = "Grid description is too long.";
            return NULL;
        }
        
        if(c >= 'a' && c <= 'z')
            pos += (c - 'a') + 1;
        else if(c >= '1' && c <= '9')
            d = c - '0';
        else if(c >= 'A' && c <= 'I')
            d = (c - 'A') + 1;
        else if(c == 'O')
        {
            ret->gridclues[pos] = LATINH_CIRCLE;
            ret->holes[pos] = LATINH_CIRCLE;
            pos++;
        }
        else if(c == 'X')
        {
            ret->gridclues[pos] = LATINH_CROSS;
            ret->holes[pos] = LATINH_CROSS;
            pos++;
        }
        else
        {
            free_game(ret);
            *fail = "Grid description contains invalid characters.";
            return NULL;
        }
        
        if(d > 0 && d <= nums)
        {
            ret->gridclues[pos] = d;
            ret->grid[pos] = d;
            ret->holes[pos] = LATINH_CIRCLE;
            pos++;
        }
        else if(d > nums)
        {
            free_game(ret);
            *fail = "Grid clue is out of range.";
            return NULL;
        }
    }
    
    if(pos > 0 && pos < o2)
    {
        free_game(ret);
        *fail = "Description is too short.";
        return NULL;
    }
    
    return ret;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    char *fail = NULL;
    game_state *state = load_game(params, desc, &fail);
    if(state)
        free_game(state);
    
    if(fail)
        return fail;
    
    return NULL;
}

static int salad_solver_easy(struct latin_solver *solver, void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    int nchanged = 0;
    
    nchanged += latinholes_solver_sync(solver, ctx);
    
    if(ctx->state->params->mode == GAMEMODE_LETTERS)
    {
        nchanged += salad_letters_solver(solver, ctx);
    }
    
    nchanged += latinholes_solver_count(solver, ctx);
    
    return nchanged;
}

static digit salad_scan_dir(digit *grid, char *holes, int si, int di, int ei, bool direct)
{
    int i;
    for(i = si; i != ei; i+=di)
    {
        if(direct && grid[i] == 0 && holes[i] != LATINH_CROSS)
            return 0;
        if(grid[i] != 0 && grid[i] != LATINH_CROSS)
            return grid[i];
    }
    
    return 0;
}

static bool salad_checkborders(game_state *state)
{
    int o = state->params->order;
    int o2 = o*o;
    int i; char c;
    
    for(i = 0; i < o; i++)
    {
        /* Top */
        if(state->borderclues[i])
        {        
            c = salad_scan_dir(state->grid, NULL, i, o, o2+i, false);
            if(c != state->borderclues[i])
                return false;
        }
        /* Left */
        if(state->borderclues[i+o])
        {
            c = salad_scan_dir(state->grid, NULL, i*o, 1, ((i+1)*o), false);
            if(c != state->borderclues[i+o])
                return false;
        }
        /* Bottom */
        if(state->borderclues[i+(o*2)])
        {
            c = salad_scan_dir(state->grid, NULL, (o2-o)+i, -o, i-o, false);
            if(c != state->borderclues[i+(o*2)])
                return false;
        }
        /* Right */
        if(state->borderclues[i+(o*3)])
        {
            c = salad_scan_dir(state->grid, NULL, ((i+1)*o)-1, -1, i*o - 1, false);
            if(c != state->borderclues[i+(o*3)])
                return false;
        }
    }
    
    return true;
}

#define SOLVER(upper,title,func,lower) func,
static usersolver_t const salad_solvers[] = { DIFFLIST(SOLVER) };

static bool salad_valid(struct latin_solver *solver, void *vctx) {
    return true;
}
static int salad_solve(game_state *state, int maxdiff)
{
    int o = state->params->order;
    int nums = state->params->nums;
    int o2 = o*o;
    struct solver_ctx *ctx = new_ctx(state, o, nums);
    struct latin_solver *solver = snew(struct latin_solver);
    int diff, i;
    
    latin_solver_alloc(solver, state->grid, o);
    
    for(i = 0; i < o2; i++)
    {
        if(state->gridclues[i] && state->gridclues[i] != LATINH_CROSS && state->gridclues[i] != LATINH_CIRCLE)
        {
            latin_solver_place(solver, i%o, i/o, state->gridclues[i]);
        }
        else if(state->gridclues[i] == LATINH_CROSS)
        {
            latinholes_solver_place_cross(solver, ctx, i%o, i/o);
        }
        else if(state->gridclues[i] == LATINH_CIRCLE)
        {
            latinholes_solver_place_circle(solver, ctx, i%o, i/o);
        }
    }
    if(maxdiff != DIFF_HOLESONLY)
    {
        latin_solver_main(solver, maxdiff,
            DIFF_EASY, DIFF_TRICKY, DIFF_TRICKY,
            DIFF_TRICKY, DIFF_IMPOSSIBLE,
            salad_solvers, salad_valid, ctx, clone_ctx, free_ctx);
        
        diff = latinholes_check(state);
    }
    else
    {
        int holes = 0;
        int nchanged = 1;

        while(nchanged)
        {
            nchanged = 0;
            nchanged += latinholes_solver_sync(solver, ctx);
            nchanged += latinholes_solver_count(solver, ctx);
        }
        
        for(i = 0; i < o2; i++)
        {
            if(state->holes[i] == LATINH_CROSS)
                holes++;
        }
        diff = holes == ((o-nums) * o);
    }
    free_ctx(ctx);
    latin_solver_free(solver);
    sfree(solver);

    if (!diff)
        return 0;
    
    return 1;
}

static game_state *new_game(midend *me, const game_params *params, const char *desc)
{
    char *fail;
    
    game_state *state = load_game(params, desc, &fail);
    assert(state);
    
    return state;
}

static char *salad_serialize(const digit *input, int s, char base)
{
    char *ret, *p;
    ret = snewn(s + 1, char);
    p = ret;
    int i, run = 0;
    for (i = 0; i < s; i++)
    {
        if (input[i] != 0)
        {
            if (run)
            {
                *p++ = ('a'-1) + run;
                run = 0;
            }
            
            if (input[i] == LATINH_CROSS)
                *p++ = 'X';
            else if (input[i] == LATINH_CIRCLE)
                *p++ = 'O';
            else
                *p++ = input[i] + base;
        }
        else
        {
            if (run == 26)
            {
                *p++ = ('a'-1) + run;
                run = 0;
            }
            run++;
        }
    }
    if (run) {
        *p++ = ('a'-1) + run;
        run = 0;
    }
    *p = '\0';
    
    return ret;
}

static char *solve_game(const game_state *state, const game_state *currstate,
            const char *aux, const char **error)
{
    game_state *solved = dup_game(state);
    char *ret = NULL;
    int result;
    
    result = salad_solve(solved, DIFF_TRICKY);
    
    if(result)
    {
        int o = solved->params->order;
        int o2 = o*o;
        
        ret = snewn(o2 + 2, char);
        char *p = ret;
        *p++ = 'S';
        
        int i;
        for(i = 0; i < o2; i++)
        {
            if(solved->grid[i] && solved->holes[i] != LATINH_CROSS)
                *p++ = (solved->grid[i] + '0');
            else
                *p++ = 'X';
        }
        
        *p++ = '\0';
    }
    else
        *error = "No solution found.";
    
    free_game(solved);
    return ret;
}

static void salad_strip_clues(game_state *state, random_state *rs, digit *clues, int m, int diff)
{
    int *spaces = snewn(m, int);
    int o = state->params->order;
    int o2 = o*o;
    int i, j;
    digit temp;
    
    for(i = 0; i < m; i++) spaces[i] = i;
    shuffle(spaces, m, sizeof(*spaces), rs);
    
    for(i = 0; i < m; i++)
    {
        j = spaces[i];
        
        temp = clues[j];
        
        /* Space is already empty */
        if(temp == 0)
            continue;
        
        clues[j] = 0;
        
        memset(state->grid, 0, o2 * sizeof(digit));
        memset(state->holes, 0, o2 * sizeof(char));
        
        if(!salad_solve(state, diff))
        {
            clues[j] = temp;
        }
    }
    sfree(spaces);
}

/* ********* *
 * Generator *
 * ********* */
static char *salad_new_numbers_desc(const game_params *params, random_state *rs, char **aux)
{
    int o = params->order;
    int o2 = o*o;
    int nums = params->nums;
    int i, j;
    int diff = params->diff;
    char temp;
    digit *grid = NULL;
    game_state *state = NULL;
    int *spaces = snewn(o2, int);

    while(true)
    {
        /* Generate a solved grid */
        grid = latin_generate(o, rs);
        state = blank_game(params);
        memset(state->borderclues, 0, o * 4 * sizeof(digit));
        
        for(i = 0; i < o2; i++)
        {
            if(grid[i] > nums)
                state->gridclues[i] = LATINH_CROSS;
            else
                state->gridclues[i] = grid[i];
        }
        sfree(grid);
        
        /* Remove grid clues */
        for(i = 0; i < o2; i++) spaces[i] = i;
        shuffle(spaces, o2, sizeof(*spaces), rs);
        
        for(i = 0; i < o2; i++)
        {
            j = spaces[i];
            
            temp = state->gridclues[j];
            
            if(temp == 0)
                continue;
            
            /* Remove the hole or the number on a ball */
            if(temp == LATINH_CROSS || temp == LATINH_CIRCLE)
                state->gridclues[j] = 0;
            else
                state->gridclues[j] = LATINH_CIRCLE;
            
            memset(state->grid, 0, o2 * sizeof(digit));
            memset(state->holes, 0, o2 * sizeof(digit));
            
            if(!salad_solve(state, diff))
            {
                state->gridclues[j] = temp;
                continue;
            }
            
            /* See if we can remove the entire ball */
            temp = state->gridclues[j];
            if(temp == 0)
                continue;
            
            state->gridclues[j] = 0;
            memset(state->grid, 0, o2 * sizeof(digit));
            memset(state->holes, 0, o2 * sizeof(digit));
            
            if(!salad_solve(state, diff))
            {
                state->gridclues[j] = temp;
            }
        }
        
        /* 
         * Quality check: See if all the holes can be placed
         * at the start, without entering numbers. If yes,
         * the puzzle is thrown away.
         */
        memset(state->grid, 0, o2 * sizeof(digit));
        memset(state->holes, 0, o2 * sizeof(digit));
        if(!salad_solve(state, DIFF_HOLESONLY))
            break;
        
        free_game(state);
    }
    
    char *ret = salad_serialize(state->gridclues, o2, '0');
    free_game(state);
    sfree(spaces);
    
    return ret;
}

static char *salad_new_letters_desc(const game_params *params, random_state *rs, char **aux)
{
    int o = params->order;
    int o2 = o*o;
    int ox4 = o*4;
    int nums = params->nums;
    int diff = params->diff;
    int i;
    digit *grid;
    game_state *state;
    bool nogrid = false;
    
    /*
     * Quality check: With certain parameters, force the grid 
     * to contain no clues.
     */
    if(o < 8)
        nogrid = true;
    
    while(true)
    {
        grid = latin_generate(o, rs);
        state = blank_game(params);
        memset(state->borderclues, 0, ox4 * sizeof(char));
        for(i = 0; i < o2; i++)
        {
            if(grid[i] <= nums)
                state->gridclues[i] = grid[i];
            else
                state->gridclues[i] = LATINH_CROSS;
        }
        sfree(grid);
        
        /* Add border clues */
        for(i = 0; i < o; i++)
        {
            /* Top */
            state->borderclues[i] = salad_scan_dir(state->gridclues, NULL, i, o, o2+i, false);
            /* Left */
            state->borderclues[i+o] = salad_scan_dir(state->gridclues, NULL, i*o, 1, ((i+1)*o), false);
            /* Bottom */
            state->borderclues[i+(o*2)] = salad_scan_dir(state->gridclues, NULL, (o2-o)+i, -o, i-o, false);
            /* Right */
            state->borderclues[i+(o*3)] = salad_scan_dir(state->gridclues, NULL, ((i+1)*o)-1, -1, i*o - 1, false);
        }
        
        if(nogrid)
        {
            /* Remove all grid clues, and attempt to solve it */
            memset(state->gridclues, 0, o2 * sizeof(char));
            memset(state->grid, 0, o2 * sizeof(digit));
            memset(state->holes, 0, o2 * sizeof(char));
            if(!salad_solve(state, diff))
            {
                free_game(state);
                continue;
            }
        }
        else
        {
            /* Remove grid clues, with full border clues */
            salad_strip_clues(state, rs, state->gridclues, o2, diff);
        }
        /* Remove border clues */
        salad_strip_clues(state, rs, state->borderclues, ox4, diff);
        
        break;
    }
    /* Encode game */
    char *borderstr = salad_serialize(state->borderclues, ox4, 'A' - 1);
    
    char *gridstr = salad_serialize(state->gridclues, o2, 'A' - 1);
    free_game(state);
    char *ret = snewn(ox4+o2+2, char);
    sprintf(ret, "%s,%s", borderstr, gridstr);
    sfree(borderstr);
    sfree(gridstr);
    
    return ret;
}

static char *new_game_desc(const game_params *params, random_state *rs,
               char **aux, bool interactive)
{
    if(params->mode == GAMEMODE_NUMBERS)
        return salad_new_numbers_desc(params, rs, aux);
    else
        return salad_new_letters_desc(params, rs, aux);
}

/* ************** *
 * User interface *
 * ************** */
struct game_ui {
    int hx, hy;
    bool hpencil;
    bool hshow;
    bool hcursor;
    int hhint;
    bool hdrag;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ret = snew(game_ui);
    
    ret->hx = ret->hy = 0;
    ret->hpencil = ret->hshow = ret->hcursor = false;
    ret->hhint = -1;
    ret->hdrag = false;
    return ret;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static key_label *game_request_keys(const game_params *params, const game_ui *ui, int *nkeys)
{
    int i;
    int n = params->nums;
    int addkeys = params->mode == GAMEMODE_LETTERS ? 3 : 4;
    char base = params->mode == GAMEMODE_LETTERS ? 'A' : '1';

    key_label *keys = snewn(n + addkeys, key_label);
    *nkeys = n + addkeys;

    for (i = 0; i < n; i++)
    {
        keys[i].button = base + i;
        keys[i].label = NULL;
    }
    keys[n].button = 'X';
    keys[n].label = "None";
    if (params->mode == GAMEMODE_LETTERS) {
        keys[n+1].button = '\b';
        keys[n+1].label = NULL;
        keys[n+2].button = '+';
        keys[n+2].label = "Add";
    }
    else {
        keys[n+1].button = 'O';
        keys[n+1].label = "Any";
        keys[n+2].button = '\b';
        keys[n+2].label = NULL;
        keys[n+3].button = '+';
        keys[n+3].label = "Add";
    }
    return keys;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

static const char *current_key_label(const game_ui *ui,
                                     const game_state *state, int button){
    if (button == '\b') return (ui->hhint == 0) ? "H" : "E";
    if (button >= '1' && button <= '9') return ((button-'0') == ui->hhint) ? "H" : "E";
    if (button >= 'A' && button <= 'X') return ((button-'A'+1) == ui->hhint) ? "H" : "E";
    return "";
}


#define FD_CURSOR  0x01
#define FD_PENCIL  0x02
#define FD_ERROR   0x04
#define FD_CIRCLE  0x08
#define FD_CROSS   0x10

#define FD_MASK    0x1f

struct game_drawstate {
    int tilesize;
    bool redraw;
    int *gridfs;
    int *borderfs;
    
    digit *grid;
    int *oldgridfs;
    int *oldborderfs;
    unsigned int *marks;
    
    /* scratch space */
    int *rowcount;
    int *colcount;
};

#define TILE_SIZE (ds->tilesize)
#define DEFAULT_TILE_SIZE 40
#define FROMCOORD(x) ( ((x)/ TILE_SIZE) - 1 )

static char *interpret_move(const game_state *state, game_ui *ui, const game_drawstate *ds,
                int x, int y, int button, bool swapped)
{
    int i, o, nums, pos, gx, gy;
    char buf[80];
    o = state->params->order;
    nums = state->params->nums;
    pos = ui->hx+(o*ui->hy);
    gx = FROMCOORD(x);
    gy = FROMCOORD(y);
    
    button &= ~MOD_MASK;
    
    if(gx >= 0 && gx < o && gy >= 0 && gy < o) {
        if (((button == LEFT_RELEASE && !swapped) || 
             (button == LEFT_BUTTON && swapped)) &&
             (!ui->hdrag && (ui->hhint >= 0)) &&
             (state->gridclues[(gy*o)+gx] == 0 ||
              state->gridclues[(gy*o)+gx] == LATINH_CIRCLE)) {
            sprintf(buf, "R%d,%d,%c", gx, gy, 
                   (char)((ui->hhint == ('X' - 'A' + 1)) ? 'X' :
                          (ui->hhint == ('O' - 'A' + 1)) ? 'O' :
                          (ui->hhint > 0)                ? ui->hhint + '0' :
                                                           '-'));
            return dupstr(buf);
        }
        else if(button == LEFT_BUTTON) {
            if (ui->hhint >= 0) {
                ui->hdrag = false;
            }
            else if (ui->hx == gx && ui->hy == gy && ui->hshow && !ui->hpencil) {
                ui->hshow = false;
            }
            else if (state->gridclues[(gy*o)+gx] == 0 ||
                     state->gridclues[(gy*o)+gx] == LATINH_CIRCLE) {
                ui->hx = gx;
                ui->hy = gy;
                ui->hpencil = false;
                ui->hcursor = false;
                ui->hshow = true;
            }
            ui->hcursor = false;
            return MOVE_UI_UPDATE;
        }
        else if (button == RIGHT_BUTTON) {
            if (ui->hhint >= 0 && 
                (state->gridclues[(gy*o)+gx] == 0 || 
                 state->gridclues[(gy*o)+gx] == LATINH_CIRCLE)) {
    
                sprintf(buf, "P%d,%d,%c", gx, gy, 
                        (char)((ui->hhint == ('X' - 'A' + 1)) ? 'X' :
                               (ui->hhint == ('O' - 'A' + 1)) ? 'O' :
                               (ui->hhint > 0)                ? ui->hhint + '0' :
                                                                '-'));
                return dupstr(buf);
            }
            else if (ui->hx == gx && ui->hy == gy && ui->hshow && ui->hpencil) {
                ui->hshow = false;
            }
            else if (state->grid[gy*o+gx] == 0 && (state->gridclues[(gy*o)+gx] == 0 ||
                     state->gridclues[(gy*o)+gx] == LATINH_CIRCLE)) {
                ui->hx = gx;
                ui->hy = gy;
                ui->hpencil = true;
                ui->hcursor = false;
                ui->hshow = true;
            }
            ui->hhint = -1;
            return MOVE_UI_UPDATE;
        }
        else if (button == LEFT_DRAG) {
            ui->hdrag = true;
        }
    } else if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        ui->hshow = false;
        ui->hpencil = false;
        ui->hhint = -1;
        return MOVE_UI_UPDATE;
    }

    if(ui->hshow && (state->gridclues[pos] == 0 || state->gridclues[pos] == LATINH_CIRCLE)) {
        if ((button >= '1' && button <= '9') || 
            (button >= 'A' && button <= 'I') || 
            button == '\b') {
            digit d = 0;
            
            if (button >= '1' && button <= '9') {
                d = button - '0';
                if(d > nums)
                    return NULL;
            }
            if (button >= 'A' && button <= 'I') {
                d = button - 'A' + 1;
                if(d > nums)
                    return NULL;
            }
            
            sprintf(buf, "%c%d,%d,%c", (char)(ui->hpencil ? 'P':'R'),
                ui->hx, ui->hy, (char)(d ? d + '0' : '-'));
            
            /* When not in keyboard and pencil mode, hide cursor */
            if (!ui->hcursor && !ui->hpencil)
                ui->hshow = false;
                    
            return dupstr(buf);
        }
        
        if(button == 'X') {
            if(state->gridclues[pos] == LATINH_CIRCLE)
                return NULL;
            
            sprintf(buf, "%c%d,%d,%c", (char)(ui->hpencil ? 'P':'R'),
                ui->hx, ui->hy, 'X');
            
            /* When not in keyboard and pencil mode, hide cursor */
            if (!ui->hcursor && !ui->hpencil)
                ui->hshow = false;
                    
            return dupstr(buf);
        }
        
        if(button == 'O') {
            if(state->gridclues[pos] == LATINH_CIRCLE && ui->hpencil)
                return NULL;
            if(state->grid[pos] != 0 && ui->hpencil)
                return NULL;
            
            sprintf(buf, "%c%d,%d,%c", (char)(ui->hpencil ? 'P':'R'),
                ui->hx, ui->hy, 'O');
            
            /* When not in keyboard and pencil mode, hide cursor */
            if (!ui->hcursor && !ui->hpencil)
                ui->hshow = false;
                    
            return dupstr(buf);
        }
    }

    if (!ui->hshow && (
            (button >= '1' && button <= '9') || 
            (button >= 'A' && button <= 'I') ||
            (button == 'O') || (button == 'X') || (button == '\b'))) {
        int n;
        if (button >= '1' && button <= '9')
            n = button - '0';
        else if (button == '\b')
            n = 0;
        else
            n = button - 'A' + 1;
        if (ui->hhint == n) ui->hhint = -1;
        else ui->hhint = n;
        return MOVE_UI_UPDATE;
    }

    if(button == '+') {
        unsigned int allmarks = (1<<(nums+1))-1;
        unsigned int marks = (1<<nums)-1;
        
        for(i = 0; i < o*o; i++)
        {
            if(!state->grid[i] && state->holes[i] != LATINH_CROSS &&
                state->marks[i] != (state->holes[i] == LATINH_CIRCLE ? marks : allmarks))
            return dupstr("M");
        }
    }
    
    return MOVE_UNUSED;
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move)
{
    game_state *ret = NULL;
    int o = state->params->order;
    int nums = state->params->nums;
    int x, y, i;
    char c; digit d;
    
    /* Auto-solve game */
    if(move[0] == 'S')
    {
        const char *p = move + 1;
        ret = dup_game(state);
        
        for (i = 0; i < o*o; i++) {
            
            if(*p >= '1' && *p <= '9')
            {
                ret->grid[i] = *p - '0';
                ret->holes[i] = LATINH_CIRCLE;
            }
            else if(*p == 'X')
            {
                ret->grid[i] = 0;
                ret->holes[i] = LATINH_CROSS;
            }
            else
            {
                free_game(ret);
                return NULL;
            }
            p++;
        }
        
        ret->completed = ret->cheated = true;
        return ret;
    }
    /* Write number or pencil mark in square */
    if((move[0] == 'P' || move[0] == 'R') && sscanf(move+1, "%d,%d,%c", &x, &y, &c) == 3)
    {        
        ret = dup_game(state);
        
        /* Clear square */
        if(c == '-')
        {
            /* If square is already empty, remove pencil marks */
            if(!ret->grid[y*o+x] && ret->holes[y*o+x] != LATINH_CROSS)
                ret->marks[y*o+x] = 0;
            
            ret->grid[y*o+x] = 0;
            if(ret->gridclues[y*o+x] != LATINH_CIRCLE)
                ret->holes[y*o+x] = 0;
        }
        /* Enter number */
        else if(move[0] == 'R' && c != 'X' && c != 'O')
        {
            d = (digit)c - '0';
            if (ret->grid[y*o+x] == d) {
                ret->grid[y*o+x] = 0;
                if(ret->gridclues[y*o+x] != LATINH_CIRCLE)
                    ret->holes[y*o+x] = 0;
            }
            else {
                ret->grid[y*o+x] = d;
                ret->holes[y*o+x] = LATINH_CIRCLE;
            }
        }
        /* Add/remove pencil mark */
        else if(move[0] == 'P' && c != 'X' && c != 'O')
        {
            d = (digit)c - '1';
            ret->marks[y*o+x] ^= 1 << d;
        }
        /* Add hole */
        else if(move[0] == 'R' && c == 'X')
        {
            ret->grid[y*o+x] = 0;
            ret->holes[y*o+x] = ret->holes[y*o+x] == LATINH_CROSS ? 0 : LATINH_CROSS;
        }
        /* Add/remove pencil mark for hole */
        else if(move[0] == 'P' && c == 'X')
        {
            ret->marks[y*o+x] ^= 1 << nums;
        }
        /* Add circle and empty square */
        else if(move[0] == 'R' && c == 'O')
        {
            ret->grid[y*o+x] = 0;
            ret->holes[y*o+x] = ret->holes[y*o+x] == LATINH_CIRCLE ? 0 : LATINH_CIRCLE;
        }
        /* Toggle circle without emptying */
        else if(move[0] == 'P' && c == 'O')
        {
            if(ret->holes[y*o+x] == 0)
                ret->holes[y*o+x] = LATINH_CIRCLE;
            else if(ret->holes[y*o+x] == LATINH_CIRCLE)
                ret->holes[y*o+x] = 0;
        }
        
        /* Check for completion */
        ret->completed = (latinholes_check(ret) && salad_checkborders(ret));
        ret->cheated = false;
        return ret;
    }
    /*
     * Fill in every possible mark. If a square has a circle set,
     * don't include a mark for a cross.
     */
    if(move[0] == 'M')
    {
        unsigned int allmarks = (1<<(nums+1))-1;
        unsigned int marks = (1<<nums)-1;
        ret = dup_game(state);
        
        for(i = 0; i < o*o; i++)
        {
            if(!state->grid[i] && state->holes[i] != LATINH_CROSS)
                ret->marks[i] = (state->holes[i] == LATINH_CIRCLE ? marks : allmarks);
        }
        
        return ret;
    }
    
    return NULL;
}
 
static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    *x = *y = (params->order+2) * tilesize;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
              const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
    ds->redraw = true;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    int i;
    float *ret = snewn(3 * NCOLOURS, float);

    for (i=0;i<3;i++) {
        ret[COL_BACKGROUND   * 3 + i] = 1.0F;
        ret[COL_HIGHLIGHT    * 3 + i] = 0.75F;
        ret[COL_LOWLIGHT     * 3 + i] = 0.75F;
        ret[COL_BORDER       * 3 + i] = 0.0F;
        ret[COL_BORDERCLUE   * 3 + i] = 0.0F;
        ret[COL_PENCIL       * 3 + i] = 0.0F;
        ret[COL_I_NUM        * 3 + i] = 0.0F;
        ret[COL_I_HOLE       * 3 + i] = 0.0F;
        ret[COL_I_BALL       * 3 + i] = 0.0F;
        ret[COL_I_BALLBG     * 3 + i] = 1.0F;
        ret[COL_G_NUM        * 3 + i] = 0.0F;
        ret[COL_G_HOLE       * 3 + i] = 0.0F;
        ret[COL_G_BALL       * 3 + i] = 0.0F;
        ret[COL_G_BALLBG     * 3 + i] = 0.9F;
        ret[COL_E_BG         * 3 + i] = 0.0F;
        ret[COL_E_BORDERCLUE * 3 + i] = 0.0F;
        ret[COL_E_NUM        * 3 + i] = 1.0F;
        ret[COL_E_BALL       * 3 + i] = 1.0F;
        ret[COL_E_HOLE       * 3 + i] = 0.5F;
    }
    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    int o = state->params->order;
    int o2 = o*o;
    int ox4 = o*4;
    
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = DEFAULT_TILE_SIZE;
    ds->redraw = true;
    ds->gridfs = snewn(o2, int);
    ds->grid = snewn(o2, digit);
    ds->marks = snewn(o2, unsigned int);
    ds->borderfs = snewn(ox4, int);
    
    ds->oldgridfs = snewn(o2, int);
    ds->oldborderfs = snewn(ox4, int);
    ds->rowcount = snewn(o2, int);
    ds->colcount = snewn(o2, int);
    
    memset(ds->gridfs, 0, o2 * sizeof(int));
    memset(ds->grid, 0, o2 * sizeof(digit));
    memset(ds->marks, 0, o2 * sizeof(unsigned int));
    memset(ds->oldgridfs, ~0, o2 * sizeof(int));
    memset(ds->borderfs, 0, ox4 * sizeof(int));
    memset(ds->oldborderfs, ~0, ox4 * sizeof(int));

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->gridfs);
    sfree(ds->borderfs);
    sfree(ds->grid);
    sfree(ds->marks);
    sfree(ds->oldgridfs);
    sfree(ds->oldborderfs);
    sfree(ds->rowcount);
    sfree(ds->colcount);
    sfree(ds);
}

static void salad_draw_pencil(drawing *dr, const game_state *state, int x, int y, char base, int tilesize, int tx, int ty)
{
    /* Draw the entered clues for a square. */
    int o = state->params->order;
    int mmx = state->params->nums + 1;
    int nhints, i, j, hw, hh, hmax, fontsz;
    char str[2];

    /* (can assume square has just been cleared) */

    /* Draw hints; steal ingenious algorithm (basically)
    * from solo.c:draw_number() */
    /*for (i = nhints = 0; i < mmx; i++) {
        if (state->marks[y*o+x] & (1<<i)) nhints++;
    } */
    nhints = mmx;
    for (hw = 1; hw * hw < nhints; hw++);
    
    if (hw < 3) hw = 3;
        hh = (nhints + hw - 1) / hw;
    if (hh < 2) hh = 2;
        hmax = max(hw, hh);
        fontsz = tilesize/(hmax*(11-hmax)/8);

    for (i = j = 0; i < mmx; i++)
    {
        if (state->marks[y*o+x] & (1<<i))
        {
            int hx = i % hw, hy = i / hw;

            str[0] = base+i+1;
            if(i == mmx - 1)
                str[0] = 'X';
            
            str[1] = '\0';
            draw_text(dr,
                            tx + (4*hx+3) * tilesize / (4*hw+2),
                            ty + (4*hy+3) * tilesize / (4*hh+2),
                            FONT_VARIABLE, fontsz,
                            ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL, str);
            j++;
        }
    }
}

static void salad_set_drawflags(game_drawstate *ds, const game_ui *ui, const game_state *state, int hshow)
{
    int o = state->params->order;
    int nums = state->params->nums;
    int o2 = o*o;
    int x, y, i;
    char c; digit d;
    
    /* Count numbers */
    memset(ds->rowcount, 0, o2*sizeof(int));
    memset(ds->colcount, 0, o2*sizeof(int));
    for(x = 0; x < o; x++)
    for(y = 0; y < o; y++)
    {
        if(state->holes[y*o+x] == LATINH_CROSS)
        {
            ds->rowcount[y+(nums*o)]++;
            ds->colcount[x+(nums*o)]++;
        }
        else if(state->grid[y*o+x])
        {
            d = state->grid[y*o+x] - 1;
            ds->rowcount[y+(d*o)]++;
            ds->colcount[x+(d*o)]++;
        }
    }
    
    /* Grid flags */
    for(x = 0; x < o; x++)
        for(y = 0; y < o; y++)
        {
            i = y*o+x;
            /* Unset all flags */
            ds->gridfs[i] &= ~FD_MASK;
            
            /* Set cursor flags */
            if(hshow && ui->hx == x && ui->hy == y)
                ds->gridfs[i] |= ui->hpencil ? FD_PENCIL : FD_CURSOR;
            
            /* Mark count errors */
            d = state->grid[i];
            if(state->holes[i] == LATINH_CROSS && 
                (ds->rowcount[y+(nums*o)] > (o-nums) || 
                 ds->colcount[x+(nums*o)] > (o-nums))) {
                ds->gridfs[i] |= FD_ERROR;
            }
            if (state->holes[i] == LATINH_CIRCLE && d == 0) {
                int j;
                int numcount, ballcount;
                numcount = ballcount = 0;
                for (j=0;j<o;j++) {
                    if (state->grid[y*o+j] > 0) numcount++;
                    if (state->holes[y*o+j] == LATINH_CIRCLE && state->grid[y*o+j] == 0) ballcount++;
                }
                if ((nums - numcount) < ballcount) ds->gridfs[i] |= FD_ERROR;
                numcount = ballcount = 0;
                for (j=0;j<o;j++) {
                    if (state->grid[j*o+x] > 0) numcount++;
                    if (state->holes[j*o+x] == LATINH_CIRCLE && state->grid[j*o+x] == 0) ballcount++;
                }
                if ((nums - numcount) < ballcount) ds->gridfs[i] |= FD_ERROR;
            }
            else if(d > 0 && (ds->rowcount[y+((d-1)*o)] > 1 || 
                              ds->colcount[x+((d-1)*o)] > 1)) {
                ds->gridfs[i] |= FD_ERROR;
            }
            
            if(state->holes[i] == LATINH_CROSS)
                ds->gridfs[i] |= FD_CROSS;
            if(state->holes[i] == LATINH_CIRCLE)
                ds->gridfs[i] |= FD_CIRCLE;
        }
    
    /* Check border clues */
    for(i = 0; i < o; i++)
    {
        /* Top */
        if(state->borderclues[i])
        {        
            c = salad_scan_dir(state->grid, state->holes, i, o, o2+i, true);
            if(c && c != state->borderclues[i])
                ds->borderfs[i] |= FD_ERROR;
            else
                ds->borderfs[i] &= ~FD_ERROR;
        }
        /* Left */
        if(state->borderclues[i+o])
        {
            c = salad_scan_dir(state->grid, state->holes, i*o, 1, ((i+1)*o), true);
            if(c && c != state->borderclues[i+o])
                ds->borderfs[i+o] |= FD_ERROR;
            else
                ds->borderfs[i+o] &= ~FD_ERROR;
        }
        /* Bottom */
        if(state->borderclues[i+(o*2)])
        {
            c = salad_scan_dir(state->grid, state->holes, (o2-o)+i, -o, i-o, true);
            if(c && c != state->borderclues[i+(o*2)])
                ds->borderfs[i+(o*2)] |= FD_ERROR;
            else
                ds->borderfs[i+(o*2)] &= ~FD_ERROR;
        }
        /* Right */
        if(state->borderclues[i+(o*3)])
        {
            c = salad_scan_dir(state->grid, state->holes, ((i+1)*o)-1, -1, i*o - 1, true);
            if(c && c != state->borderclues[i+(o*3)])
                ds->borderfs[i+(o*3)] |= FD_ERROR;
            else
                ds->borderfs[i+(o*3)] &= ~FD_ERROR;
        }
    }
}

static void salad_draw_balls(drawing *dr, game_drawstate *ds, int x, int y, const game_state *state)
{
    int mode = state->params->mode;
    int o = state->params->order;
    int tx, ty, bgcolor, color;
    
    int i = x+(y*o);
    if(mode == GAMEMODE_LETTERS && state->grid[i] != 0)
        return;
    if(state->holes[i] != LATINH_CIRCLE)
        return;
    
    tx = (x+1)*TILE_SIZE + (TILE_SIZE/2);
    ty = (y+1)*TILE_SIZE + (TILE_SIZE/2);
    
    /* Draw ball background */
    if(mode == GAMEMODE_LETTERS)
        bgcolor = COL_I_BALLBG;
    else /* Transparent */
        bgcolor = (ds->gridfs[i] & FD_CURSOR ? COL_LOWLIGHT : 
                   ds->gridfs[i] & FD_ERROR  ? COL_E_BG :
                                               COL_BACKGROUND);
    color = (ds->gridfs[i] & FD_CURSOR ? COL_G_BALL :
             ds->gridfs[i] & FD_ERROR  ? COL_E_BALL :
             state->gridclues[i]       ? COL_I_BALL :
                                         COL_G_BALL);
    
    draw_circle(dr, tx, ty, TILE_SIZE*0.4, color, color);
    draw_circle(dr, tx, ty, TILE_SIZE*0.38, bgcolor, color);
}

static void salad_draw_cross(drawing *dr, game_drawstate *ds, int x, int y, double thick, const game_state *state)
{
    int tx, ty, color;
    int i = x+(y*state->params->order);
    if(state->holes[i] != LATINH_CROSS)
        return;
    
    tx = (x+1)*TILE_SIZE;
    ty = (y+1)*TILE_SIZE;
    
    color = (ds->gridfs[i] & FD_ERROR  ? COL_E_NUM : 
             ds->gridfs[i] & FD_CURSOR ? COL_G_NUM :
             state->gridclues[i]       ? COL_I_HOLE : 
                                         COL_G_HOLE);
    draw_thick_line(dr, thick,
        tx + (TILE_SIZE*0.2), ty + (TILE_SIZE*0.2),
        tx + (TILE_SIZE*0.8), ty + (TILE_SIZE*0.8),
        color);
    draw_thick_line(dr, thick,
        tx + (TILE_SIZE*0.2), ty + (TILE_SIZE*0.8),
        tx + (TILE_SIZE*0.8), ty + (TILE_SIZE*0.2),
        color);
}

static void game_redraw(drawing *dr, game_drawstate *ds, const game_state *oldstate,
            const game_state *state, int dir, const game_ui *ui,
            float animtime, float flashtime)
{
    int mode = state->params->mode;
    int o = state->params->order;
    int x, y, i, j, tx, ty, color;
    char base = (mode == GAMEMODE_LETTERS ? 'A' - 1 : '0');
    char buf[80];
    bool hshow = ui->hshow;
    double thick = (TILE_SIZE <= 21 ? 1 : 2.5);

    /* Draw status bar */
    sprintf(buf, "%s",
            state->cheated   ? "Auto-solved." :
            state->completed ? "COMPLETED!" : "");
    status_bar(dr, buf);

    salad_set_drawflags(ds, ui, state, hshow);

    buf[1] = '\0';
    for(x = 0; x < o; x++)
        for(y = 0; y < o; y++)
        {
            tx = (x+1)*TILE_SIZE;
            ty = (y+1)*TILE_SIZE;
            i = x+(y*o);
            
            if(!ds->redraw && ds->oldgridfs[i] == ds->gridfs[i] && 
                    ds->grid[i] == state->grid[i] && 
                    ds->marks[i] == state->marks[i])
                continue;
            
            ds->oldgridfs[i] = ds->gridfs[i];
            ds->grid[i] = state->grid[i];
            ds->marks[i] = state->marks[i];
            
            draw_update(dr, tx, ty, TILE_SIZE, TILE_SIZE);
            
            draw_rect(dr, tx, ty, TILE_SIZE, TILE_SIZE, 
                ds->gridfs[i] & FD_ERROR ? COL_E_BG : COL_BACKGROUND);
            
            if(ds->gridfs[y*o+x] & FD_PENCIL)
            {
                int coords[6];
                coords[0] = tx;
                coords[1] = ty;
                coords[2] = tx+(TILE_SIZE/2);
                coords[3] = ty;
                coords[4] = tx;
                coords[5] = ty+(TILE_SIZE/2);
                draw_polygon(dr, coords, 3, COL_LOWLIGHT, COL_LOWLIGHT);
            }
            else if(ds->gridfs[y*o+x] & FD_CURSOR)
            {
                draw_rect(dr, tx, ty, TILE_SIZE, TILE_SIZE, COL_LOWLIGHT);
            }
            
            /* Define a square */
            int sqc[8];
            sqc[0] = tx;
            sqc[1] = ty - 1;
            sqc[2] = tx + TILE_SIZE;
            sqc[3] = ty - 1;
            sqc[4] = tx + TILE_SIZE;
            sqc[5] = ty + TILE_SIZE - 1;
            sqc[6] = tx;
            sqc[7] = ty + TILE_SIZE - 1;
            draw_polygon(dr, sqc, 4, -1, COL_BORDER);
            
            if(ds->gridfs[i] & FD_CIRCLE)
                salad_draw_balls(dr, ds, x, y, state);
            else if(ds->gridfs[i] & FD_CROSS)
                salad_draw_cross(dr, ds, x, y, thick, state);
            
            /* Draw pencil marks */
            if(state->grid[i] == 0 && state->holes[i] != LATINH_CROSS)
            {
                if(state->holes[i] == LATINH_CIRCLE)
                {
                    /* Draw the clues smaller */
                    salad_draw_pencil(dr, state, x, y, base, TILE_SIZE * 0.8F, 
                        (x+1.1F)*TILE_SIZE, (y+1.1F)*TILE_SIZE);
                }
                else
                {
                    salad_draw_pencil(dr, state, x, y, base, TILE_SIZE, tx, ty);
                }
            }
            
            /* Draw number/letter */
            else if(state->grid[i] != 0)
            {
                color = (
                    ds->gridfs[i] & FD_ERROR ?                            COL_E_NUM : 
                    ds->gridfs[i] & FD_CURSOR ?                           COL_G_NUM : 
                    state->gridclues[i] > 0 && state->gridclues[i] <= o ? COL_I_NUM :
                                                                          COL_G_NUM);
                buf[0] = state->grid[i] + base;
                
                draw_text(dr, tx + TILE_SIZE/2, ty + TILE_SIZE/2,
                    state->gridclues[i]>0 && state->gridclues[i] <= o ? 
                        FONT_VARIABLE : FONT_VARIABLE_NORMAL, 
                    TILE_SIZE/2, ALIGN_HCENTRE|ALIGN_VCENTRE, color, buf);
            }
        }
    
    /* Draw border clues */
    for(i = 0; i < o; i++)
    {
        /* Top */
        j = i;
        if(state->borderclues[j] != 0 && ds->borderfs[j] != ds->oldborderfs[j])
        {
            color = ds->borderfs[j] & FD_ERROR ? COL_E_BORDERCLUE : COL_BACKGROUND;
            tx = (i+1)*TILE_SIZE;
            ty = 0;
            draw_rect(dr, tx, ty, TILE_SIZE-1, TILE_SIZE-1, color);
            draw_update(dr, tx, ty, tx+TILE_SIZE-1, ty+TILE_SIZE-1);
            buf[0] = state->borderclues[j] + base;
            draw_text(dr, tx + TILE_SIZE/2, ty + TILE_SIZE/2,
                FONT_VARIABLE, TILE_SIZE/2, ALIGN_HCENTRE|ALIGN_VCENTRE, 
                ds->borderfs[j] & FD_ERROR ? COL_E_NUM : COL_BORDERCLUE,
                buf);
            ds->oldborderfs[j] = ds->borderfs[j];
        }
        /* Left */
        j = i+o;
        if(state->borderclues[j] != 0 && ds->borderfs[j] != ds->oldborderfs[j])
        {
            color = ds->borderfs[j] & FD_ERROR ? COL_E_BORDERCLUE : COL_BACKGROUND;
            tx = 0;
            ty = (i+1)*TILE_SIZE;
            draw_rect(dr, tx, ty, TILE_SIZE-1, TILE_SIZE-1, color);
            draw_update(dr, tx, ty, tx+TILE_SIZE-1, ty+TILE_SIZE-1);
            buf[0] = state->borderclues[j] + base;
            draw_text(dr, tx + TILE_SIZE/2, ty + TILE_SIZE/2,
                FONT_VARIABLE, TILE_SIZE/2, ALIGN_HCENTRE|ALIGN_VCENTRE, 
                ds->borderfs[j] & FD_ERROR ? COL_E_NUM : COL_BORDERCLUE,
                buf);
            ds->oldborderfs[j] = ds->borderfs[j];
        }
        /* Bottom */
        j = i+(o*2);
        if(state->borderclues[j] != 0 && ds->borderfs[j] != ds->oldborderfs[j])
        {
            color = ds->borderfs[j] & FD_ERROR ? COL_E_BORDERCLUE : COL_BACKGROUND;
            tx = (i+1)*TILE_SIZE;
            ty = (o+1)*TILE_SIZE;
            draw_rect(dr, tx, ty, TILE_SIZE-1, TILE_SIZE-1, color);
            draw_update(dr, tx, ty, tx+TILE_SIZE-1, ty+TILE_SIZE-1);
            buf[0] = state->borderclues[j] + base;
            draw_text(dr, tx + TILE_SIZE/2, ty + TILE_SIZE/2,
                FONT_VARIABLE, TILE_SIZE/2, ALIGN_HCENTRE|ALIGN_VCENTRE, 
                ds->borderfs[j] & FD_ERROR ? COL_E_NUM : COL_BORDERCLUE,
                buf);
            ds->oldborderfs[j] = ds->borderfs[j];
        }
        /* Right */
        j = i+(o*3);
        if(state->borderclues[j] != 0 && ds->borderfs[j] != ds->oldborderfs[j])
        {
            color = ds->borderfs[j] & FD_ERROR ? COL_E_BORDERCLUE : COL_BACKGROUND;
            tx = (o+1)*TILE_SIZE;
            ty = (i+1)*TILE_SIZE;
            draw_rect(dr, tx+1, ty+1, TILE_SIZE-2, TILE_SIZE-2, color);
            draw_update(dr, tx+1, ty+1, tx+TILE_SIZE-2, ty+TILE_SIZE-2);
            buf[0] = state->borderclues[j] + base;
            draw_text(dr, tx + TILE_SIZE/2, ty + TILE_SIZE/2,
                FONT_VARIABLE, TILE_SIZE/2, ALIGN_HCENTRE|ALIGN_VCENTRE, 
                ds->borderfs[j] & FD_ERROR ? COL_E_NUM : COL_BORDERCLUE,
                buf);
            ds->oldborderfs[j] = ds->borderfs[j];
        }
    }
    
    ds->redraw = false;
}

static float game_anim_length(const game_state *oldstate, const game_state *newstate,
                  int dir, game_ui *ui)
{
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate, const game_state *newstate,
                   int dir, game_ui *ui)
{
    return 0.0F;
}

static int game_status(const game_state *state)
{
    return state->completed ? +1 : 0;
}

#ifdef COMBINED
#define thegame salad
#endif

static const char rules[] = "Place each given character once in every row and column. Some squares remain empty. This puzzle contains two modes:\n\n"
"- ABC End View: Letters on the side show which letter appears first when 'looking' into the grid.\n"
"- Number Ball: Squares with a ball must contain a number.\n\n\n"
"This puzzle was implemented by Lennard Sprong.";

const struct game thegame = {
    "Salad", NULL, NULL, rules,
    default_params,
    game_fetch_preset, NULL, /* preset_menu */
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
    false, NULL, NULL, /* get_prefs, set_prefs */
    new_ui,
    free_ui,
    NULL, /* encode_ui */
    NULL, /* decode_ui */
    game_request_keys,
    game_changed_state,
    current_key_label,
    interpret_move,
    execute_move,
    DEFAULT_TILE_SIZE, game_compute_size, game_set_size,
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

