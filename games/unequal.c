/*
 * unequal.c
 *
 * Implementation of 'Futoshiki', a puzzle featured in the Guardian.
 *
 * TTD:
   * add multiple-links-on-same-col/row solver nous
   * Optimise set solver to use bit operations instead
 *
 * Guardian puzzles of note:
   * #1: 5:0,0L,0L,0,0,0R,0,0L,0D,0L,0R,0,2,0D,0,0,0,0,0,0,0U,0,0,0,0U,
   * #2: 5:0,0,0,4L,0L,0,2LU,0L,0U,0,0,0U,0,0,0,0,0D,0,3LRUD,0,0R,3,0L,0,0,
   * #3: (reprint of #2)
   * #4: 
   * #5: 5:0,0,0,0,0,0,2,0U,3U,0U,0,0,3,0,0,0,3,0D,4,0,0,0L,0R,0,0,
   * #6: 5:0D,0L,0,0R,0,0,0D,0,3,0D,0,0R,0,0R,0D,0U,0L,0,1,2,0,0,0U,0,0L,
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"
#include "latin.h" /* contains typedef for digit */

/* ----------------------------------------------------------
 * Constant and structure definitions
 */

#define PREFERRED_TILE_SIZE 32

#define TILE_SIZE (ds->tilesize)
#define GAP_SIZE  (TILE_SIZE/2)
#define SQUARE_SIZE (TILE_SIZE + GAP_SIZE)

#define BORDER    (TILE_SIZE / 2)

#define COORD(x)  ( (x) * SQUARE_SIZE + BORDER )
#define FROMCOORD(x)  ( ((x) - BORDER + SQUARE_SIZE) / SQUARE_SIZE - 1 )

#define GRID(p,w,x,y) ((p)->w[((y)*(p)->order)+(x)])
#define GRID3(p,w,x,y,z) ((p)->w[ (((x)*(p)->order+(y))*(p)->order+(z)) ])
#define HINT(p,x,y,n) GRID3(p, hints, x, y, n)

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_TEXT, COL_GUESS, COL_ERROR, COL_PENCIL,
    COL_HIGHLIGHT, COL_LOWLIGHT, COL_SPENT = COL_LOWLIGHT,
    COL_BLACK, COL_WHITE,
    NCOLOURS
};

typedef enum {
    MODE_UNEQUAL,      /* Puzzle indicators are 'greater-than'. */
    MODE_ADJACENT,     /* Puzzle indicators are 'adjacent number'. */
    MODE_KROPKI        /* Puzzle indicators are 'adjacent' and 'double' */
} Mode;

struct game_params {
    int order;          /* Size of latin square */
    int diff;           /* Difficulty */
    Mode mode;
};

#define F_IMMUTABLE     1       /* passed in as game description */
#define F_ADJ_UP        2
#define F_ADJ_RIGHT     4
#define F_ADJ_DOWN      8
#define F_ADJ_LEFT      16
#define F_ERROR         32
#define F_ERROR_UP      64
#define F_ERROR_RIGHT   128
#define F_ERROR_DOWN    256
#define F_ERROR_LEFT    512
#define F_SPENT_UP      1024
#define F_SPENT_RIGHT   2048
#define F_SPENT_DOWN    4096
#define F_SPENT_LEFT    8192
#define F_DBL_UP        16384
#define F_DBL_RIGHT     32768
#define F_DBL_DOWN      65536
#define F_DBL_LEFT      131072

#define ADJ_TO_SPENT(x) ((x) << 9)
#define ADJ_TO_DOUBLE(x) ((x) << 13)

#define F_ERROR_MASK (F_ERROR|F_ERROR_UP|F_ERROR_RIGHT|F_ERROR_DOWN|F_ERROR_LEFT)

struct game_state {
    int order;
    bool completed, cheated;
    Mode mode;
    digit *nums;                 /* actual numbers (size order^2) */
    unsigned char *hints;        /* remaining possiblities (size order^3) */
    unsigned long *flags;         /* flags (size order^2) */
};

/* ----------------------------------------------------------
 * Game parameters and presets
 */

/* Steal the method from map.c for difficulty levels. */
#define DIFFLIST(A)               \
    A(LATIN,Trivial,NULL,t)       \
    A(EASY,Easy,solver_easy, e)   \
    A(NORMAL,Normal,solver_normal, k)   \
    A(HARD,Hard,NULL,x)     \
    A(RECURSIVE,Recursive,NULL,r)

#define ENUM(upper,title,func,lower) DIFF_ ## upper,
#define TITLE(upper,title,func,lower) #title,
#define ENCODE(upper,title,func,lower) #lower
#define CONFIG(upper,title,func,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT, DIFF_IMPOSSIBLE = diff_impossible, DIFF_AMBIGUOUS = diff_ambiguous, DIFF_UNFINISHED = diff_unfinished };
static char const *const unequal_diffnames[] = { DIFFLIST(TITLE) };
static char const unequal_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

#define DEFAULT_PRESET 3

static const struct game_params unequal_presets[] = {
    {  4, DIFF_EASY,   0 },
    {  4, DIFF_EASY,   1 },
    {  4, DIFF_EASY,   2 },
    {  5, DIFF_NORMAL, 0 },
    {  5, DIFF_NORMAL, 1 },
    {  5, DIFF_NORMAL, 2 },
    {  6, DIFF_HARD,   0 },
    {  6, DIFF_HARD,   1 },
    {  6, DIFF_HARD,   2 },
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(unequal_presets))
        return false;

    ret = snew(game_params);
    *ret = unequal_presets[i]; /* structure copy */

    sprintf(buf, "%s: %dx%d %s",
            ret->mode == MODE_KROPKI   ? "Kropki" : 
            ret->mode == MODE_ADJACENT ? "Adjacent" : 
                                         "Unequal",
            ret->order, ret->order,
            unequal_diffnames[ret->diff]);

    *name = dupstr(buf);
    *params = ret;
    return true;
}

static game_params *default_params(void)
{
    game_params *ret;
    char *name;

    if (!game_fetch_preset(DEFAULT_PRESET, &name, &ret)) return NULL;
    sfree(name);
    return ret;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;       /* structure copy */
    return ret;
}

static void decode_params(game_params *ret, char const *string)
{
    char const *p = string;

    ret->order = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;

    if (*p == 'k') {
        p++;
        ret->mode = MODE_KROPKI;
    } else if (*p == 'a') {
        p++;
        ret->mode = MODE_ADJACENT;
    } else
        ret->mode = MODE_UNEQUAL;

    if (*p == 'd') {
        int i;
        p++;
        ret->diff = DIFFCOUNT+1; /* ...which is invalid */
        if (*p) {
            for (i = 0; i < DIFFCOUNT; i++) {
                if (*p == unequal_diffchars[i])
                    ret->diff = i;
            }
            p++;
        }
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char ret[80];

    sprintf(ret, "%d", params->order);
    if (params->mode == MODE_KROPKI)
        sprintf(ret + strlen(ret), "k");
    else if (params->mode == MODE_ADJACENT)
        sprintf(ret + strlen(ret), "a");
    if (full)
        sprintf(ret + strlen(ret), "d%c", unequal_diffchars[params->diff]);

    return dupstr(ret);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];

    ret = snewn(4, config_item);

    ret[0].name = "Mode";
    ret[0].type = C_CHOICES;
    ret[0].u.choices.choicenames = ":Unequal:Adjacent:Kropki";
    ret[0].u.choices.selected = params->mode;

    ret[1].name = "Size (s*s)";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->order);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = "Difficulty";
    ret[2].type = C_CHOICES;
    ret[2].u.choices.choicenames = DIFFCONFIG;
    ret[2].u.choices.selected = params->diff;

    ret[3].name = NULL;
    ret[3].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->mode = cfg[0].u.choices.selected;
    ret->order = atoi(cfg[1].u.string.sval);
    ret->diff = cfg[2].u.choices.selected;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->order < 3 || params->order > 9)
        return "Order must be between 3 and 9";
    if (params->diff >= DIFFCOUNT)
        return "Unknown difficulty rating";
    if (params->order < 5 && params->mode >= MODE_ADJACENT &&
        params->diff >= DIFF_NORMAL)
        return "Order must be at least 5 for puzzles of this type and difficulty.";
    return NULL;
}

/* ----------------------------------------------------------
 * Various utility functions
 */

static const struct { unsigned long f, fo, fe; int dx, dy; char c, ac; } adjthan[] = {
    { F_ADJ_UP,    F_ADJ_DOWN,  F_ERROR_UP,     0, -1, '^', '-' },
    { F_ADJ_RIGHT, F_ADJ_LEFT,  F_ERROR_RIGHT,  1,  0, '>', '|' },
    { F_ADJ_DOWN,  F_ADJ_UP,    F_ERROR_DOWN,   0,  1, 'v', '-' },
    { F_ADJ_LEFT,  F_ADJ_RIGHT, F_ERROR_LEFT,  -1,  0, '<', '|' }
};

static game_state *blank_game(int order, Mode mode)
{
    game_state *state = snew(game_state);
    int o2 = order*order, o3 = o2*order;

    state->order = order;
    state->mode = mode;
    state->completed = false;
    state->cheated = false;

    state->nums = snewn(o2, digit);
    state->hints = snewn(o3, unsigned char);
    state->flags = snewn(o2, unsigned long);

    memset(state->nums, 0, o2 * sizeof(digit));
    memset(state->hints, 0, o3);
    memset(state->flags, 0, o2 * sizeof(unsigned long));

    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = blank_game(state->order, state->mode);
    int o2 = state->order*state->order, o3 = o2*state->order;

    memcpy(ret->nums, state->nums, o2 * sizeof(digit));
    memcpy(ret->hints, state->hints, o3);
    memcpy(ret->flags, state->flags, o2 * sizeof(unsigned long));
    ret->completed = state->completed;
    ret->cheated = state->cheated;
    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->nums);
    sfree(state->hints);
    sfree(state->flags);
    sfree(state);
}

#define CHECKG(x,y) grid[(y)*o+(x)]

/* Returns false if it finds an error, true if ok. */
static bool check_num_adj(digit *grid, game_state *state,
                          int x, int y, bool me)
{
    unsigned long f = GRID(state, flags, x, y);
    bool ret = true;
    int i, o = state->order;

    for (i = 0; i < 4; i++) {
        int dx = adjthan[i].dx, dy = adjthan[i].dy, n, dn;

        if (x+dx < 0 || x+dx >= o || y+dy < 0 || y+dy >= o)
            continue;

        n = CHECKG(x, y);
        dn = CHECKG(x+dx, y+dy);

        assert (n != 0);
        if (dn == 0) continue;

        if (state->mode == MODE_KROPKI) {
            int gd = abs(n-dn);
            int id = (2*n == dn || 2*dn == n);
            int ot = (n == 1 && dn == 2) || (n == 2 && dn == 1);
            
            if ((f & ADJ_TO_DOUBLE(adjthan[i].f)) && !id) {
                if (me) GRID(state, flags, x, y) |= adjthan[i].fe;
                ret = false;
            }
            else if ((f & adjthan[i].f) && (gd != 1 && !ot)) {
                if (me) GRID(state, flags, x, y) |= adjthan[i].fe;
                ret = false;
            }
            else if (!(f & adjthan[i].f) && 
                     !(f & ADJ_TO_DOUBLE(adjthan[i].f)) && 
                      (id || (gd == 1))) {
                if (me) GRID(state, flags, x, y) |= adjthan[i].fe;
                ret = false;
            }
            
        } else if (state->mode == MODE_ADJACENT) {
            int gd = abs(n-dn);

            if ((f & adjthan[i].f) && (gd != 1)) {
                if (me) GRID(state, flags, x, y) |= adjthan[i].fe;
                ret = false;
            }
            if (!(f & adjthan[i].f) && (gd == 1)) {
                if (me) GRID(state, flags, x, y) |= adjthan[i].fe;
                ret = false;
            }

        } else {
            if ((f & adjthan[i].f) && (n <= dn)) {
                if (me) GRID(state, flags, x, y) |= adjthan[i].fe;
                ret = false;
            }
        }
    }
    return ret;
}

/* Returns false if it finds an error, true if ok. */
static bool check_num_error(digit *grid, game_state *state,
                            int x, int y, bool mark_errors)
{
    int o = state->order;
    int xx, yy, val = CHECKG(x,y);
    bool ret = true;

    assert(val != 0);

    /* check for dups in same column. */
    for (yy = 0; yy < state->order; yy++) {
        if (yy == y) continue;
        if (CHECKG(x,yy) == val) ret = false;
    }

    /* check for dups in same row. */
    for (xx = 0; xx < state->order; xx++) {
        if (xx == x) continue;
        if (CHECKG(xx,y) == val) ret = false;
    }

    if (!ret) {
        if (mark_errors) GRID(state, flags, x, y) |= F_ERROR;
    }
    return ret;
}

/* Returns:     -1 for 'wrong'
 *               0 for 'incomplete'
 *               1 for 'complete and correct'
 */
static int check_complete(digit *grid, game_state *state, bool mark_errors)
{
    int x, y, ret = 1, o = state->order;

    if (mark_errors)
        assert(grid == state->nums);

    for (x = 0; x < state->order; x++) {
        for (y = 0; y < state->order; y++) {
            if (mark_errors)
                GRID(state, flags, x, y) &= ~F_ERROR_MASK;
            if (grid[y*o+x] == 0) {
                ret = 0;
            } else {
                if (!check_num_error(grid, state, x, y, mark_errors)) ret = -1;
                if (!check_num_adj(grid, state, x, y, mark_errors)) ret = -1;
            }
        }
    }
    if (ret == 1 && latin_check(grid, o))
        ret = -1;
    return ret;
}

static char n2c(digit n, int order) {
    if (n == 0)         return ' ';
    if (order < 10) {
        if (n < 10)     return '0' + n;
    } else {
        if (n < 11)     return '0' + n-1;
        n -= 11;
        if (n <= 26)    return 'A' + n;
    }
    return '?';
}

/* should be 'digit', but includes -1 for 'not a digit'.
 * Includes keypresses (0 especially) for interpret_move. */
static int c2n(int c, int order) {
    if (c < 0 || c > 0xff)
        return -1;
    if (c == ' ' || c == '\b')
        return 0;
    if (order < 10) {
        if (c >= '0' && c <= '9')
            return (int)(c - '0');
    } else {
        if (c >= '0' && c <= '9')
            return (int)(c - '0' + 1);
        if (c >= 'A' && c <= 'Z')
            return (int)(c - 'A' + 11);
        if (c >= 'a' && c <= 'z')
            return (int)(c - 'a' + 11);
    }
    return -1;
}


/* ----------------------------------------------------------
 * Solver.
 */

struct solver_link {
    int len, gx, gy, lx, ly;
};

struct solver_ctx {
    game_state *state;

    int nlinks, alinks;
    struct solver_link *links;
};

static void solver_add_link(struct solver_ctx *ctx,
                            int gx, int gy, int lx, int ly, int len)
{
    if (ctx->alinks < ctx->nlinks+1) {
        ctx->alinks = ctx->alinks*2 + 1;
        ctx->links = sresize(ctx->links, ctx->alinks, struct solver_link);
    }
    ctx->links[ctx->nlinks].gx = gx;
    ctx->links[ctx->nlinks].gy = gy;
    ctx->links[ctx->nlinks].lx = lx;
    ctx->links[ctx->nlinks].ly = ly;
    ctx->links[ctx->nlinks].len = len;
    ctx->nlinks++;
}

static struct solver_ctx *new_ctx(game_state *state)
{
    struct solver_ctx *ctx = snew(struct solver_ctx);
    int o = state->order;
    int i, x, y;
    unsigned long f;

    ctx->nlinks = ctx->alinks = 0;
    ctx->links = NULL;
    ctx->state = state;

    if (state->mode != MODE_UNEQUAL)
        return ctx; /* adjacent and kropki mode don't use links. */

    for (x = 0; x < o; x++) {
        for (y = 0; y < o; y++) {
            f = GRID(state, flags, x, y);
            for (i = 0; i < 4; i++) {
                if (f & adjthan[i].f)
                    solver_add_link(ctx, x, y, x+adjthan[i].dx, y+adjthan[i].dy, 1);
            }
        }
    }

    return ctx;
}

static void *clone_ctx(void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    return new_ctx(ctx->state);
}

static void free_ctx(void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    if (ctx->links) sfree(ctx->links);
    sfree(ctx);
}

static void solver_nminmax(struct latin_solver *solver,
                           int x, int y, int *min_r, int *max_r,
                           unsigned char **ns_r)
{
    int o = solver->o, min = o, max = 0, n;
    unsigned char *ns;

    assert(x >= 0 && y >= 0 && x < o && y < o);

    ns = solver->cube + cubepos(x,y,1);

    if (grid(x,y) > 0) {
        min = max = grid(x,y)-1;
    } else {
        for (n = 0; n < o; n++) {
            if (ns[n]) {
                if (n > max) max = n;
                if (n < min) min = n;
            }
        }
    }
    if (min_r) *min_r = min;
    if (max_r) *max_r = max;
    if (ns_r) *ns_r = ns;
}

static int solver_links(struct latin_solver *solver, void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    int i, j, lmin, gmax, nchanged = 0;
    unsigned char *gns, *lns;
    struct solver_link *link;

    for (i = 0; i < ctx->nlinks; i++) {
        link = &ctx->links[i];
        solver_nminmax(solver, link->gx, link->gy, NULL, &gmax, &gns);
        solver_nminmax(solver, link->lx, link->ly, &lmin, NULL, &lns);

        for (j = 0; j < solver->o; j++) {
            /* For the 'greater' end of the link, discount all numbers
             * too small to satisfy the inequality. */
            if (gns[j]) {
                if (j < (lmin+link->len)) {
                    cube(link->gx, link->gy, j+1) = false;
                    nchanged++;
                }
            }
            /* For the 'lesser' end of the link, discount all numbers
             * too large to satisfy inequality. */
            if (lns[j]) {
                if (j > (gmax-link->len)) {
                    cube(link->lx, link->ly, j+1) = false;
                    nchanged++;
                }
            }
        }
    }
    return nchanged;
}

static int solver_adjacent(struct latin_solver *solver, void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    int nchanged = 0, x, y, i, n, o = solver->o, nx, ny, gd;

    /* Update possible values based on known values and adjacency clues. */

    for (x = 0; x < o; x++) {
        for (y = 0; y < o; y++) {
            if (grid(x, y) == 0) continue;

            /* We have a definite number here. Make sure that any
             * adjacent possibles reflect the adjacent/non-adjacent clue. 
             * For Kropki puzzles, additionally check double/non-double clues. */

            for (i = 0; i < 4; i++) {
                bool isadjacent =
                    (GRID(ctx->state, flags, x, y) & adjthan[i].f);
                bool isdouble   = 
                    (GRID(ctx->state, flags, x, y) & ADJ_TO_DOUBLE(adjthan[i].f));

                nx = x + adjthan[i].dx, ny = y + adjthan[i].dy;
                if (nx < 0 || ny < 0 || nx >= o || ny >= o)
                    continue;

                for (n = 0; n < o; n++) {
                    /* Continue past numbers the adjacent square _could_ be,
                     * given the clue we have. */
                    gd = abs((n+1) - grid(x, y));
                    if (ctx->state->mode == MODE_KROPKI) {
                        int ot = ( (n+1) == 1 && grid(x,y) == 2) || ( (n+1) == 2 && grid(x,y) == 1);    
                        int dbl = ( (n+1)*2 == grid(x,y) || (n+1) == 2*grid(x,y) );
                        if (ot && (isdouble || isadjacent)) continue;
                        if (isdouble && dbl) continue;
                        if (!isdouble && isadjacent && (gd == 1)) continue;
                        if (!isdouble && !isadjacent && !dbl && (gd != 1)) continue;
                    } else if (ctx->state->mode == MODE_ADJACENT) {
                        if (isadjacent && (gd == 1)) continue;
                        if (!isadjacent && (gd != 1)) continue;
                    }
                    if (!cube(nx, ny, n+1))
                        continue; /* already discounted this possibility. */
                    cube(nx, ny, n+1) = false;
                    nchanged++;
                }
            }
        }
    }

    return nchanged;
}

static int solver_adjacent_set(struct latin_solver *solver, void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    int x, y, i, n, nn, o = solver->o, nx, ny, gd;
    int nchanged = 0, *scratch = snewn(o, int);

    /* Update possible values based on other possible values
     * of adjacent squares, and adjacency clues. 
     * and also double clues in kropki mode */

    for (x = 0; x < o; x++) {
        for (y = 0; y < o; y++) {
            for (i = 0; i < 4; i++) {
                bool isadjacent =
                    (GRID(ctx->state, flags, x, y) & adjthan[i].f);
                bool isdouble   = 
                    (GRID(ctx->state, flags, x, y) & ADJ_TO_DOUBLE(adjthan[i].f));

                nx = x + adjthan[i].dx, ny = y + adjthan[i].dy;
                if (nx < 0 || ny < 0 || nx >= o || ny >= o)
                    continue;

                /* We know the current possibles for the square (x,y)
                 * and also the adjacency clue from (x,y) to (nx,ny).
                 * Construct a maximum set of possibles for (nx,ny)
                 * in scratch, based on these constraints... */

                memset(scratch, 0, o*sizeof(int));

                for (n = 0; n < o; n++) {
                    if (!cube(x, y, n+1)) continue;

                    for (nn = 0; nn < o; nn++) {
                        if (n == nn) continue;

                        gd = abs(nn - n);
                        if (ctx->state->mode == MODE_KROPKI) {
                            int ot = (nn == 1 && n == 2) || (nn == 2 && n == 1);
                            int dbl = ( ((nn+1)*2 == (n+1)) || ((nn+1) == 2*(n+1)) );
                            if (ot && !(isdouble || isadjacent)) continue;
                            if (isdouble && !dbl) continue;
                            if (!isdouble && isadjacent && (gd != 1)) continue;
                            if (!isdouble && !isadjacent && (dbl || (gd == 1)) ) continue;
                        } else if (ctx->state->mode == MODE_ADJACENT) {
                            if (isadjacent && (gd != 1)) continue;
                            if (!isadjacent && (gd == 1)) continue;
                        }

                        scratch[nn] = 1;
                    }
                }

                /* ...and remove any possibilities for (nx,ny) that are
                 * currently set but are not indicated in scratch. */
                for (n = 0; n < o; n++) {
                    if (scratch[n] == 1) continue;
                    if (!cube(nx, ny, n+1)) continue;
                    cube(nx, ny, n+1) = false;
                    nchanged++;
                }
            }
        }
    }

    return nchanged;
}

static int solver_easy(struct latin_solver *solver, void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    if (ctx->state->mode >= MODE_ADJACENT)
    return solver_adjacent(solver, vctx);
    else
    return solver_links(solver, vctx);
}

static int solver_normal(struct latin_solver *solver, void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    if (ctx->state->mode >= MODE_ADJACENT)
    return solver_adjacent_set(solver, vctx);
    else
    return 0;
}

#define SOLVER(upper,title,func,lower) func,
static usersolver_t const unequal_solvers[] = { DIFFLIST(SOLVER) };

static bool unequal_valid(struct latin_solver *solver, void *vctx)
{
    struct solver_ctx *ctx = (struct solver_ctx *)vctx;
    if (ctx->state->mode == MODE_ADJACENT) {
        int o = solver->o;
        int x, y, nx, ny, v, nv, i;

        for (x = 0; x+1 < o; x++) {
            for (y = 0; y+1 < o; y++) {
                v = grid(x, y);
                for (i = 0; i < 4; i++) {
                    bool is_adj, should_be_adj;

                    should_be_adj =
                        (GRID(ctx->state, flags, x, y) & adjthan[i].f);

                    nx = x + adjthan[i].dx, ny = y + adjthan[i].dy;
                    if (nx < 0 || ny < 0 || nx >= o || ny >= o)
                        continue;

                    nv = grid(nx, ny);
                    is_adj = (labs(v - nv) == 1);

                    if (is_adj && !should_be_adj) {
                        return false;
                    }

                    if (!is_adj && should_be_adj) {
                        return false;
                    }
                }
            }
        }
    } else if (ctx->state->mode == MODE_UNEQUAL){
        int i;
        for (i = 0; i < ctx->nlinks; i++) {
            struct solver_link *link = &ctx->links[i];
            int gv = grid(link->gx, link->gy);
            int lv = grid(link->lx, link->ly);
            if (gv <= lv) {
                return false;
            }
        }
    }
    return true;
}

static int solver_state(game_state *state, int maxdiff)
{
    struct solver_ctx *ctx = new_ctx(state);
    struct latin_solver solver;
    int diff;

    latin_solver_alloc(&solver, state->nums, state->order);

    diff = latin_solver_main(&solver, maxdiff,
                 DIFF_LATIN, DIFF_NORMAL, DIFF_HARD,
                 DIFF_HARD, DIFF_RECURSIVE,
                 unequal_solvers, unequal_valid, ctx,
                             clone_ctx, free_ctx);

    memcpy(state->hints, solver.cube, state->order*state->order*state->order);

    free_ctx(ctx);

    latin_solver_free(&solver);

    if (diff == DIFF_IMPOSSIBLE)
        return -1;
    if (diff == DIFF_UNFINISHED)
        return 0;
    if (diff == DIFF_AMBIGUOUS)
        return 2;
    return 1;
}

static game_state *solver_hint(const game_state *state, int *diff_r,
                               int mindiff, int maxdiff)
{
    game_state *ret = dup_game(state);
    int diff, r = 0;

    for (diff = mindiff; diff <= maxdiff; diff++) {
        r = solver_state(ret, diff);
        if (r != 0) goto done;
    }

done:
    if (diff_r) *diff_r = (r > 0) ? diff : -1;
    return ret;
}

/* ----------------------------------------------------------
 * Game generation.
 */

static char *latin_desc(digit *sq, size_t order)
{
    int o2 = order*order, i;
    char *soln = snewn(o2+2, char);

    soln[0] = 'S';
    for (i = 0; i < o2; i++)
        soln[i+1] = n2c(sq[i], order);
    soln[o2+1] = '\0';

    return soln;
}

/* returns true if it placed (or could have placed) clue. */
static bool gg_place_clue(game_state *state, int ccode, digit *latin, bool checkonly)
{
    int loc = ccode / 5, which = ccode % 5;
    int x = loc % state->order, y = loc / state->order;

    assert(loc < state->order*state->order);

    if (which == 4) {           /* add number */
        if (state->nums[loc] != 0) {
            assert(state->nums[loc] == latin[loc]);
            return false;
        }
        if (!checkonly) {
            state->nums[loc] = latin[loc];
        }
    } else {                    /* add flag */
        int lx, ly, lloc;

        if (state->mode != MODE_UNEQUAL)
            return false; /* never add flag clues in adjacent or kropki mode
                             (they're always all present) */

        if (state->flags[loc] & adjthan[which].f)
            return false; /* already has flag. */

        lx = x + adjthan[which].dx;
        ly = y + adjthan[which].dy;
        if (lx < 0 || ly < 0 || lx >= state->order || ly >= state->order)
            return false; /* flag compares to off grid */

        lloc = loc + adjthan[which].dx + adjthan[which].dy*state->order;
        if (latin[loc] <= latin[lloc])
            return false; /* flag would be incorrect */

        if (!checkonly) {
            state->flags[loc] |= adjthan[which].f;
        }
    }
    return true;
}

/* returns true if it removed (or could have removed) the clue. */
static bool gg_remove_clue(game_state *state, int ccode, bool checkonly)
{
    int loc = ccode / 5, which = ccode % 5;

    assert(loc < state->order*state->order);

    if (which == 4) {           /* remove number. */
        if (state->nums[loc] == 0) return false;
        if (!checkonly) {
            state->nums[loc] = 0;
        }
    } else {                    /* remove flag */
        if (state->mode != MODE_UNEQUAL)
            return false; /* never remove clues in adjacent or kropki mode. */

        if (!(state->flags[loc] & adjthan[which].f)) return false;
        if (!checkonly) {
            state->flags[loc] &= ~adjthan[which].f;
        }
    }
    return true;
}

static int gg_best_clue(game_state *state, int *scratch, digit *latin)
{
    int ls = state->order * state->order * 5;
    int maxposs = 0, minclues = 5, best = -1, i, j;
    int nposs, nclues, loc;

    for (i = ls; i-- > 0 ;) {
        if (!gg_place_clue(state, scratch[i], latin, true)) continue;

        loc = scratch[i] / 5;
        for (j = nposs = 0; j < state->order; j++) {
            if (state->hints[loc*state->order + j]) nposs++;
        }
        for (j = nclues = 0; j < 4; j++) {
            if (state->flags[loc] & adjthan[j].f) nclues++;
        }
        if ((nposs > maxposs) ||
            (nposs == maxposs && nclues < minclues)) {
            best = i; maxposs = nposs; minclues = nclues;
        }
    }
    /* if we didn't solve, we must have 1 clue to place! */
    assert(best != -1);
    return best;
}

#define MAXTRIES 1000
static int gg_solved;

static int game_assemble(game_state *new, int *scratch, digit *latin,
                         int difficulty)
{
    game_state *copy = dup_game(new);
    int best;

    if (difficulty >= DIFF_RECURSIVE) {
        /* We mustn't use any solver that might guess answers;
         * if it guesses wrongly but solves, gg_place_clue will
         * get mighty confused. We will always trim clues down
         * (making it more difficult) in game_strip, which doesn't
         * have this problem. */
        difficulty = DIFF_RECURSIVE-1;
    }

    while(1) {
        gg_solved++;
        if (solver_state(copy, difficulty) == 1) break;

        best = gg_best_clue(copy, scratch, latin);
        gg_place_clue(new, scratch[best], latin, false);
        gg_place_clue(copy, scratch[best], latin, false);
    }
    free_game(copy);
    return 0;
}

static void game_strip(game_state *new, int *scratch, digit *latin,
                       int difficulty)
{
    int o = new->order, o2 = o*o, lscratch = o2*5, i;
    game_state *copy = blank_game(new->order, new->mode);

    /* For each symbol (if it exists in new), try and remove it and
     * solve again; if we couldn't solve without it put it back. */
    for (i = 0; i < lscratch; i++) {
        if (!gg_remove_clue(new, scratch[i], false)) continue;

        memcpy(copy->nums,  new->nums,  o2 * sizeof(digit));
        memcpy(copy->flags, new->flags, o2 * sizeof(unsigned long));
        gg_solved++;
        if (solver_state(copy, difficulty) != 1) {
            /* put clue back, we can't solve without it. */
            gg_place_clue(new, scratch[i], latin, false);
        }
    }
    free_game(copy);
}

static void add_kropki_flags(game_state *state, digit *latin, random_state *rs)
{
    int x, y, o = state->order;
    int lay_double;
    int lay_adjacent;

    /* All clues in kropki mode are always present (the only variables are
     * the numbers). This adds all the flags to state based on the supplied
     * latin square. */

    for (y = 0; y < o; y++) {
        for (x = 0; x < o; x++) {
            int gi, gc;

            gi = latin[y*o+x];
            if (x < (o-1)) {
                gc = latin[y*o+x+1];
                lay_double = (2*gi == gc || 2*gc == gi);
                lay_adjacent = (abs(gi - gc) == 1);
                if ((gi == 1 && gc == 2) || (gi == 2 && gc == 1)) {
                    lay_double = (random_upto(rs, 2) == 0);
                    lay_adjacent = !lay_double;
                }
                if (lay_double) {
                    GRID(state, flags, x, y) |= F_DBL_RIGHT;
                    GRID(state, flags, x+1, y) |= F_DBL_LEFT;
                } else if (lay_adjacent) {
                    GRID(state, flags, x, y) |= F_ADJ_RIGHT;
                    GRID(state, flags, x+1, y) |= F_ADJ_LEFT;
                }
            }

            if (y < (o-1)) {
                gc = latin[(y+1)*o+x];
                lay_double = (2*gi == gc || 2*gc == gi);
                lay_adjacent = (abs(gi - gc) == 1);
                if ((gi == 1 && gc == 2) || (gi == 2 && gc == 1)) {
                    lay_double = (random_upto(rs, 2) == 0);
                    lay_adjacent = !lay_double;
                }
                if (lay_double) {
                    GRID(state, flags, x, y) |= F_DBL_DOWN;
                    GRID(state, flags, x, y+1) |= F_DBL_UP;
                } else if (lay_adjacent) {
                    GRID(state, flags, x, y) |= F_ADJ_DOWN;
                    GRID(state, flags, x, y+1) |= F_ADJ_UP;
                }
            }
        }
    }
}

static void add_adjacent_flags(game_state *state, digit *latin)
{
    int x, y, o = state->order;

    /* All clues in adjacent mode are always present (the only variables are
     * the numbers). This adds all the flags to state based on the supplied
     * latin square. */

    for (y = 0; y < o; y++) {
        for (x = 0; x < o; x++) {
            if (x < (o-1) && (abs(latin[y*o+x] - latin[y*o+x+1]) == 1)) {
                GRID(state, flags, x, y) |= F_ADJ_RIGHT;
                GRID(state, flags, x+1, y) |= F_ADJ_LEFT;
            }
            if (y < (o-1) && (abs(latin[y*o+x] - latin[(y+1)*o+x]) == 1)) {
                GRID(state, flags, x, y) |= F_ADJ_DOWN;
                GRID(state, flags, x, y+1) |= F_ADJ_UP;
            }
        }
    }
}

static char *new_game_desc(const game_params *params_in, random_state *rs,
               char **aux, bool interactive)
{
    game_params params_copy = *params_in; /* structure copy */
    game_params *params = &params_copy;
    digit *sq = NULL;
    int i, x, y, retlen, k, nsol;
    int o2 = params->order * params->order, ntries = 1;
    int *scratch, lscratch = o2*5;
    char *ret, buf[200];
    game_state *state = blank_game(params->order, params->mode);

    /* Generate a list of 'things to strip' (randomised later) */
    scratch = snewn(lscratch, int);
    /* Put the numbers (4 mod 5) before the inequalities (0-3 mod 5) */
    for (i = 0; i < lscratch; i++) scratch[i] = (i%o2)*5 + 4 - (i/o2);

generate:
    if (sq) sfree(sq);
    sq = latin_generate(params->order, rs);
    latin_debug(sq, params->order);
    /* Separately shuffle the numeric and inequality clues */
    shuffle(scratch, lscratch/5, sizeof(int), rs);
    shuffle(scratch+lscratch/5, 4*lscratch/5, sizeof(int), rs);

    memset(state->nums, 0, o2 * sizeof(digit));
    memset(state->flags, 0, o2 * sizeof(unsigned long));

    if (state->mode == MODE_KROPKI) {
        /* All kropki flags are always present. */
        add_kropki_flags(state, sq, rs);
    }
    else if (state->mode == MODE_ADJACENT) {
        /* All adjacency flags are always present. */
        add_adjacent_flags(state, sq);
    }

    gg_solved = 0;
    if (game_assemble(state, scratch, sq, params->diff) < 0)
        goto generate;
    game_strip(state, scratch, sq, params->diff);

    if (params->diff > 0) {
        game_state *copy = dup_game(state);
        nsol = solver_state(copy, params->diff-1);
        free_game(copy);
        if (nsol > 0) {
            if (ntries < MAXTRIES) {
                ntries++;
                goto generate;
            }
            params->diff--;
        }
    }

    ret = NULL; retlen = 0;
    for (y = 0; y < params->order; y++) {
        for (x = 0; x < params->order; x++) {
            unsigned long f = GRID(state, flags, x, y);
            k = sprintf(buf, "%d%s%s%s%s,",
                        GRID(state, nums, x, y),
                        (f & F_DBL_UP)    ? "*U" : 
                        (f & F_ADJ_UP)    ?  "U" : "",
                        (f & F_DBL_RIGHT) ? "*R" : 
                        (f & F_ADJ_RIGHT) ?  "R" : "",
                        (f & F_DBL_DOWN)  ? "*D" : 
                        (f & F_ADJ_DOWN)  ?  "D" : "",
                        (f & F_DBL_LEFT)  ? "*L" : 
                        (f & F_ADJ_LEFT)  ?  "L" : "");

            ret = sresize(ret, retlen + k + 1, char);
            strcpy(ret + retlen, buf);
            retlen += k;
        }
    }
    *aux = latin_desc(sq, params->order);

    free_game(state);
    sfree(sq);
    sfree(scratch);
    return ret;
}

static game_state *load_game(const game_params *params, const char *desc,
                             const char **why_r)
{
    game_state *state = blank_game(params->order, params->mode);
    const char *p = desc;
    int i = 0, n, o = params->order, x, y;
    const char *why = NULL;
    bool dbl = false;

    while (*p) {
        while (*p >= 'a' && *p <= 'z') {
            i += *p - 'a' + 1;
            p++;
        }
        if (i >= o*o) {
            why = "Too much data to fill grid"; goto fail;
        }

        if (*p < '0' || *p > '9') {
            why = "Expecting number in game description"; goto fail;
        }
        n = atoi(p);
        if (n < 0 || n > o) {
            why = "Out-of-range number in game description"; goto fail;
        }
        state->nums[i] = (digit)n;
        while (*p >= '0' && *p <= '9') p++; /* skip number */

        if (state->nums[i] != 0)
            state->flags[i] |= F_IMMUTABLE; /* === number set by game description */

        while (*p == 'U' || *p == 'R' || *p == 'D' || *p == 'L' || *p == '*') {
            switch (*p) {
            case '*': if (params->mode != MODE_KROPKI) {
                        why = "Double flag * in non-kropki game description";
                        goto fail;
                      } else if (dbl) {
                        why = "Multiple * double flag in kropki game description";
                        goto fail;
                      }
                      dbl = true; break;
            case 'U': state->flags[i] |= dbl ? F_DBL_UP    : F_ADJ_UP;
                      dbl = false; break;
            case 'R': state->flags[i] |= dbl ? F_DBL_RIGHT : F_ADJ_RIGHT;
                      dbl = false; break;
            case 'D': state->flags[i] |= dbl ? F_DBL_DOWN  : F_ADJ_DOWN;
                      dbl = false; break;
            case 'L': state->flags[i] |= dbl ? F_DBL_LEFT  : F_ADJ_LEFT;
                      dbl = false; break;
            default: why = "Expecting flag URDL in game description"; goto fail;
            }
            p++;
        }
        i++;
        if (i < o*o && *p != ',') {
            why = "Missing separator"; goto fail;
        }
        if (*p == ',') p++;
    }
    if (i < o*o) {
        why = "Not enough data to fill grid"; goto fail;
    }
    i = 0;
    for (y = 0; y < o; y++) {
        for (x = 0; x < o; x++) {
            for (n = 0; n < 4; n++) {
                int nx = x + adjthan[n].dx;
                int ny = y + adjthan[n].dy;
                if (GRID(state, flags, x, y) & ADJ_TO_DOUBLE(adjthan[n].f)) {
                    if (params-> mode != MODE_KROPKI) {
                        why = "Double flags are only allowed in Kropki mode"; goto fail;
                    }
                    /* a flag must not point us off the grid. */
                    if (nx < 0 || ny < 0 || nx >= o || ny >= o) {
                        why = "Flags go off grid"; goto fail;
                    }
                    if (!(GRID(state, flags, nx, ny) & ADJ_TO_DOUBLE(adjthan[n].fo))) {
                        why = "Double flags contradicting each other"; goto fail;
                    }
                } else if (GRID(state, flags, x, y) & adjthan[n].f) {
                    /* a flag must not point us off the grid. */
                    if (nx < 0 || ny < 0 || nx >= o || ny >= o) {
                        why = "Flags go off grid"; goto fail;
                    }
                    if (params->mode != MODE_UNEQUAL) {
                        /* if one cell is adjacent to another, the other must
                         * also be adjacent to the first. */
                        if (!(GRID(state, flags, nx, ny) & adjthan[n].fo)) {
                            why = "Flags contradicting each other"; goto fail;
                        }
                    } else {
                        /* if one cell is GT another, the other must _not_ also
                         * be GT the first. */
                        if (GRID(state, flags, nx, ny) & adjthan[n].fo) {
                            why = "Flags contradicting each other"; goto fail;
                        }
                    }
                }
            }
        }
    }

    return state;

fail:
    free_game(state);
    if (why_r) *why_r = why;
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    game_state *state = load_game(params, desc, NULL);
    if (!state) {
        assert("Unable to load ?validated game.");
        return NULL;
    }
    return state;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    const char *why = NULL;
    game_state *dummy = load_game(params, desc, &why);
    if (dummy) {
        free_game(dummy);
        assert(!why);
    } else
        assert(why);
    return why;
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
    game_state *solved;
    int r;
    char *ret = NULL;

    if (aux) return dupstr(aux);

    solved = dup_game(state);
    for (r = 0; r < state->order*state->order; r++) {
        if (!(solved->flags[r] & F_IMMUTABLE))
            solved->nums[r] = 0;
    }
    r = solver_state(solved, DIFFCOUNT-1);   /* always use full solver */
    if (r > 0) ret = latin_desc(solved->nums, solved->order);
    free_game(solved);
    return ret;
}

/* ----------------------------------------------------------
 * Game UI input processing.
 */

struct game_ui {
    int hx, hy;                         /* as for solo.c, highlight pos */
    bool hshow, hpencil, hcursor;       /* show state, type, and ?cursor. */
    int hhint;
    bool hdrag;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    ui->hx = ui->hy = 0;
    ui->hpencil = false;
    ui->hshow = false;
    ui->hcursor = false;
    ui->hhint = -1;
    ui->hdrag = false;
    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static key_label *game_request_keys(const game_params *params, const game_ui *ui, int *nkeys)
{
    int i;
    int order = params->order;
    char off = (order > 9) ? '0' : '1';
    key_label *keys = snewn(order + 2, key_label);
    *nkeys = order + 2;

    for(i = 0; i < order; i++) {
        if (i==10) off = 'a'-10;
        keys[i].button = i + off;
        keys[i].label = NULL;
    }
    keys[order].button = '\b';
    keys[order].label = NULL;
    keys[order+1].button = '+';
    keys[order+1].label = "Add";

    return keys;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
    /* See solo.c; if we were pencil-mode highlighting and
     * somehow a square has just been properly filled, cancel
     * pencil mode. */
    if (ui->hshow && ui->hpencil && !ui->hcursor &&
        GRID(newstate, nums, ui->hx, ui->hy) != 0) {
        ui->hshow = false;
    }
}

static const char *current_key_label(const game_ui *ui,
                                     const game_state *state, int button){
    if (button == '\b') return (ui->hhint == 0) ? "H" : "E";
    if ((button < '0') || (button > '9')) return "";
    return ((button-'0') == ui->hhint) ? "H" : "E";
}

struct game_drawstate {
    int tilesize, order;
    bool started;
    Mode mode;
    digit *nums;                /* copy of nums, o^2 */
    unsigned char *hints;       /* copy of hints, o^3 */
    unsigned long *flags;        /* o^2 */

    int hx, hy;
    bool hshow, hpencil;        /* as for game_ui. */
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int ox, int oy, int button, bool swapped)
{
    int x = FROMCOORD(ox), y = FROMCOORD(oy), n;
    char buf[80];

    button = STRIP_BUTTON_MODIFIERS(button);

    if (x >= 0 && x < ds->order && y >= 0 && y < ds->order) {

        if ((oy - COORD(y) > TILE_SIZE) || (ox - COORD(x) > TILE_SIZE))
            return NULL;

        if (((button == LEFT_RELEASE && !swapped) || 
             (button == LEFT_BUTTON && swapped)) &&
             (!ui->hdrag && (ui->hhint >= 0)) &&
             !(GRID(state, flags, x, y) & F_IMMUTABLE)) {
            sprintf(buf, "R%d,%d,%d", x, y, ui->hhint);
            return dupstr(buf);
        }
        else if (button == LEFT_BUTTON) {
            /* normal highlighting for non-immutable squares */
            if (GRID(state, flags, x, y) & F_IMMUTABLE) {
                ui->hshow = false;
                ui->hhint = -1;
            }
            else if (ui->hhint >= 0) {
                ui->hdrag = false;
                return NULL;
            }
            else if (x == ui->hx && y == ui->hy &&
                     ui->hshow && !ui->hpencil) {
                ui->hshow = false;
                ui->hhint = -1;
            }
            else {
                ui->hx = x; ui->hy = y; ui->hpencil = false;
                ui->hshow = true;
                ui->hhint = -1;
            }
            ui->hcursor = false;
            ui->hdrag = false;
            return MOVE_UI_UPDATE;
        }
        else if (button == RIGHT_BUTTON) {
            /* pencil highlighting for non-filled squares */
            if (GRID(state, nums, x, y) != 0) {
                ui->hshow = false;
                ui->hhint = -1;
            }
            else if ((ui->hhint >= 0) && (GRID(state, nums, x, y) == 0)) {
                sprintf(buf, "P%d,%d,%d", x, y, ui->hhint);
                return dupstr(buf);
            }
            else if (x == ui->hx && y == ui->hy &&
                     ui->hshow && ui->hpencil)
                ui->hshow = false;
            else {
                ui->hx = x; ui->hy = y; ui->hpencil = true;
                ui->hshow = true;
            }
            ui->hcursor = false;
            ui->hhint = -1;
            ui->hdrag = false;
            return MOVE_UI_UPDATE;
        }
        else if (button == LEFT_DRAG) {
            ui->hdrag = true;
        }
    } else if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        ui->hshow = false;
        ui->hpencil = false;
        ui->hcursor = false;
        ui->hhint = -1;
        ui->hdrag = false;
        return MOVE_UI_UPDATE;
    }

    n = c2n(button, state->order);
    if (ui->hshow && n >= 0 && n <= ds->order) {

        if (GRID(state, flags, ui->hx, ui->hy) & F_IMMUTABLE)
            return NULL;        /* can't edit immutable square (!) */
        if (ui->hpencil && GRID(state, nums, ui->hx, ui->hy) > 0)
            return NULL;        /* can't change hints on filled square (!) */


        sprintf(buf, "%c%d,%d,%d",
                (char)(ui->hpencil && n > 0 ? 'P' : 'R'), ui->hx, ui->hy, n);

        if (!ui->hpencil) ui->hshow = false;

        return dupstr(buf);
    }
    if (!ui->hshow && n >= 0 && n <= ds->order) {
        if (ui->hhint == n) ui->hhint = -1;
        else ui->hhint = n;
        return MOVE_UI_UPDATE;
    }

    if (button == 'H' || button == 'h')
        return dupstr("H");
    if (button == '+')
        return dupstr("M");

    return MOVE_UNUSED;
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move)
{
    game_state *ret = NULL;
    int x, y, n, i;

    if ((move[0] == 'P' || move[0] == 'R') &&
        sscanf(move+1, "%d,%d,%d", &x, &y, &n) == 3 &&
        x >= 0 && x < state->order && y >= 0 && y < state->order &&
        n >= 0 && n <= state->order) {
        ret = dup_game(state);
        if (n == 0 && GRID(ret, nums, x, y) == 0)
            for (i = 0; i < state->order; i++) HINT(ret, x, y, i) = 0;
        if (move[0] == 'P' && n > 0)
            HINT(ret, x, y, n-1) = !HINT(ret, x, y, n-1);
        else {
            GRID(ret, nums, x, y) = GRID(ret, nums, x, y) == n ? 0 : n;
            ret->completed = check_complete(ret->nums, ret, true) > 0;
            ret->cheated = false;
        }
        return ret;
    } else if (move[0] == 'S') {
        const char *p;

        ret = dup_game(state);
        ret->completed = ret->cheated = true;

        p = move+1;
        for (i = 0; i < state->order*state->order; i++) {
            n = c2n((int)*p, state->order);
            if (!*p || n <= 0 || n > state->order)
                goto badmove;
            ret->nums[i] = n;
            p++;
        }
        if (*p) goto badmove;
        check_complete(ret->nums, ret, true);
        return ret;
    } else if (move[0] == 'M') {
        ret = dup_game(state);
        for (x = 0; x < state->order; x++) {
            for (y = 0; y < state->order; y++) {
                for (n = 0; n < state->order; n++) {
                    HINT(ret, x, y, n) = 1;
                }
            }
        }
        return ret;
    } else if (move[0] == 'H') {
        ret = solver_hint(state, NULL, DIFF_EASY, DIFF_EASY);
        check_complete(ret->nums, ret, true);
        return ret;
    } else if (move[0] == 'F' && sscanf(move+1, "%d,%d,%d", &x, &y, &n) == 3 &&
           x >= 0 && x < state->order && y >= 0 && y < state->order) {
        ret = dup_game(state);
        GRID(ret, flags, x, y) ^= n;
        return ret;
    }

badmove:
    if (ret) free_game(ret);
    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing/printing routines.
 */

#define DRAW_SIZE (TILE_SIZE*ds->order + GAP_SIZE*(ds->order-1) + BORDER*2)

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize, order; } ads, *ds = &ads;
    ads.tilesize = tilesize;
    ads.order = params->order;

    *x = *y = DRAW_SIZE;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);
    int i;

    for (i = 0; i < 3; i++) {
        ret[COL_BACKGROUND * 3 + i] = 1.0;
        ret[COL_HIGHLIGHT  * 3 + i] = 0.75;
        ret[COL_LOWLIGHT   * 3 + i] = 0.0;
        ret[COL_TEXT       * 3 + i] = 0.0F;
        ret[COL_GUESS      * 3 + i] = 0.0F;
        ret[COL_ERROR      * 3 + i] = 0.5F;
        ret[COL_GRID       * 3 + i] = 0.25F;
        ret[COL_PENCIL     * 3 + i] = 0.0F;
        ret[COL_BLACK      * 3 + i] = 0.0F;
        ret[COL_WHITE      * 3 + i] = 1.0F;
    }
    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int o2 = state->order*state->order, o3 = o2*state->order;

    ds->tilesize = 0;
    ds->order = state->order;
    ds->mode = state->mode;

    ds->nums = snewn(o2, digit);
    ds->hints = snewn(o3, unsigned char);
    ds->flags = snewn(o2, unsigned long);
    memset(ds->nums, 0, o2*sizeof(digit));
    memset(ds->hints, 0, o3);
    memset(ds->flags, 0, o2*sizeof(unsigned long));

    ds->hx = ds->hy = 0;
    ds->started = false;
    ds->hshow = false;
    ds->hpencil = false;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->nums);
    sfree(ds->hints);
    sfree(ds->flags);
    sfree(ds);
}

static void draw_gt(drawing *dr, int ox, int oy,
                    int dx1, int dy1, int dx2, int dy2, int col)
{
    int coords[6];
    int xdx = (dx1+dx2 ? 0 : 1), xdy = (dx1+dx2 ? 1 : 0);
    coords[0] = ox + xdx;             coords[1] = oy + xdy;
    coords[2] = ox + xdx + dx1;       coords[3] = oy + xdy + dy1;
    coords[4] = ox + xdx + dx1 + dx2; coords[5] = oy + xdy + dy1 + dy2;
    draw_polygon(dr, coords, 3, col, col);
}

static void draw_gts(drawing *dr, game_drawstate *ds, int ox, int oy,
                     unsigned long f, int bg, int fg)
{
    int g = GAP_SIZE, g2 = (g+1)/2, g4 = (g+1)/4, g6 = (g+1)/6;
    int sx, sy;
    /* Draw all the greater-than signs emanating from this tile. */

    if (f & F_ADJ_UP) {
        if (bg >= 0) draw_rect(dr, ox, oy - g, TILE_SIZE, g, bg);
        sx = ox+g2; sy = oy-g4;
        if (f & F_ERROR_UP) draw_rect(dr, sx-g6, sy-g2-g6, 2*(g2+g6), g2+2*g6, COL_BLACK);
        draw_gt(dr, sx, sy, g2, -g2, g2, g2, (f & F_ERROR_UP) ? COL_BACKGROUND : fg);
        draw_update(dr, ox, oy-g, TILE_SIZE, g);
    }
    if (f & F_ADJ_RIGHT) {
        if (bg >= 0) draw_rect(dr, ox + TILE_SIZE, oy, g, TILE_SIZE, bg);
        sx = ox+TILE_SIZE+g4; sy = oy+g2;
        if (f & F_ERROR_RIGHT) draw_rect(dr, sx-g6, sy-g6, g2+2*g6, 2*g2+2*g6, COL_BLACK);
        draw_gt(dr, sx, sy, g2, g2, -g2, g2, (f & F_ERROR_RIGHT) ? COL_BACKGROUND : fg);
        draw_update(dr, ox+TILE_SIZE, oy, g, TILE_SIZE);
    }
    if (f & F_ADJ_DOWN) {
        if (bg >= 0) draw_rect(dr, ox, oy + TILE_SIZE, TILE_SIZE, g, bg);
        sx = ox+g2; sy = oy+TILE_SIZE+g4;
        if (f & F_ERROR_DOWN) draw_rect(dr, sx-g6, sy-g6, 2*(g2+g6), g2+2*g6, COL_BLACK);
        draw_gt(dr, sx, sy, g2, g2, g2, -g2, (f & F_ERROR_DOWN) ? COL_BACKGROUND : fg);
        draw_update(dr, ox, oy+TILE_SIZE, TILE_SIZE, g);
    }
    if (f & F_ADJ_LEFT) {
        if (bg >= 0) draw_rect(dr, ox - g, oy, g, TILE_SIZE, bg);
        sx = ox-g4; sy = oy+g2;
        if (f & F_ERROR_LEFT) draw_rect(dr, sx-g2-g6, sy-g6, g2+2*g6, 2*g2+2*g6, COL_BLACK);
        draw_gt(dr, sx, sy, -g2, g2, g2, g2, (f & F_ERROR_LEFT) ? COL_BACKGROUND : fg);
        draw_update(dr, ox-g, oy, g, TILE_SIZE);
    }
}

static void draw_adjs(drawing *dr, game_drawstate *ds, int ox, int oy,
                      unsigned long f, int bg, int fg)
{
    int g = GAP_SIZE, g38 = 3*(g+1)/8, g4 = (g+1)/4, g10 = (g+1)/10;

    /* Draw all the adjacency bars relevant to this tile; we only have
     * to worry about F_ADJ_RIGHT and F_ADJ_DOWN.
     *
     * If we _only_ have the error flag set (i.e. it's not supposed to be
     * adjacent, but adjacent numbers were entered) draw an outline red bar.
     */

    if (f & (F_ADJ_RIGHT|F_ERROR_RIGHT)) {
        if ((f & F_ADJ_RIGHT) && (f & F_ERROR_RIGHT)) {
            draw_rect(dr, ox+TILE_SIZE+g38, oy, g4, TILE_SIZE, bg);
            draw_rect(dr, ox+TILE_SIZE+g38, oy, g4, TILE_SIZE/9, COL_BLACK);
            draw_rect(dr, ox+TILE_SIZE+g38, oy+2*TILE_SIZE/9, g4, TILE_SIZE/9, COL_BLACK);
            draw_rect(dr, ox+TILE_SIZE+g38, oy+4*TILE_SIZE/9, g4, TILE_SIZE/9, COL_BLACK);
            draw_rect(dr, ox+TILE_SIZE+g38, oy+6*TILE_SIZE/9, g4, TILE_SIZE/9, COL_BLACK);
            draw_rect(dr, ox+TILE_SIZE+g38, oy+8*TILE_SIZE/9, g4, TILE_SIZE/9, COL_BLACK);
        } 
        else if (f & F_ADJ_RIGHT) {
            draw_rect(dr, ox+TILE_SIZE+g38, oy, g4, TILE_SIZE, COL_TEXT);
        }
        else {
            draw_rect(dr, ox+TILE_SIZE+g38, oy, g4, TILE_SIZE, COL_BLACK);
            draw_rect(dr, ox+TILE_SIZE+g38+g10, oy+g10, g4-2*g10, TILE_SIZE-2*g10, COL_BACKGROUND);
        }
    } else if (bg >= 0) {
        draw_rect(dr, ox+TILE_SIZE+g38, oy, g4, TILE_SIZE, bg);
    }
    draw_update(dr, ox+TILE_SIZE, oy, g, TILE_SIZE);

    if (f & (F_ADJ_DOWN|F_ERROR_DOWN)) {
        if ((f & F_ADJ_DOWN) && (f & F_ERROR_DOWN)) {
            draw_rect(dr, ox, oy+TILE_SIZE+g38, TILE_SIZE, g4, bg);
            draw_rect(dr, ox, oy+TILE_SIZE+g38, TILE_SIZE/9, g4, COL_BLACK);
            draw_rect(dr, ox+2*TILE_SIZE/9, oy+TILE_SIZE+g38, TILE_SIZE/9, g4, COL_BLACK);
            draw_rect(dr, ox+4*TILE_SIZE/9, oy+TILE_SIZE+g38, TILE_SIZE/9, g4, COL_BLACK);
            draw_rect(dr, ox+6*TILE_SIZE/9, oy+TILE_SIZE+g38, TILE_SIZE/9, g4, COL_BLACK);
            draw_rect(dr, ox+8*TILE_SIZE/9, oy+TILE_SIZE+g38, TILE_SIZE/9, g4, COL_BLACK);
        }
        else if (f & F_ADJ_DOWN) {
            draw_rect(dr, ox, oy+TILE_SIZE+g38, TILE_SIZE, g4, COL_TEXT);
        } else {
            draw_rect(dr, ox, oy+TILE_SIZE+g38, TILE_SIZE, g4, COL_BLACK);
            draw_rect(dr, ox+g10, oy+TILE_SIZE+g38+g10, TILE_SIZE-2*g10, g4-2*g10, COL_BACKGROUND);
        }
    } else if (bg >= 0) {
        draw_rect(dr, ox, oy+TILE_SIZE+g38, TILE_SIZE, g4, bg);
    }
    draw_update(dr, ox, oy+TILE_SIZE, TILE_SIZE, g);
}

static void draw_krps(drawing *dr, game_drawstate *ds, int ox, int oy,
                      unsigned long f, int bg, int fg, int fg2)
{
    int g = GAP_SIZE, g4 = (g+1)/4, g2=(g+1)/2, g6=(g+1)/6, g10=(g+1)/10;

    if (bg >= 0) {
        draw_rect(dr, ox+TILE_SIZE, oy, g, TILE_SIZE, bg);
    }
    if (f & F_ERROR_RIGHT) {
        draw_rect(dr, ox+TILE_SIZE+g4-g6, oy, g2+2*g6, TILE_SIZE, COL_ERROR);
    }
    if (f & F_ADJ_RIGHT) {
        draw_circle(dr, ox+TILE_SIZE+g2, oy+TILE_SIZE/2, g4, COL_TEXT, COL_TEXT);
        draw_circle(dr, ox+TILE_SIZE+g2, oy+TILE_SIZE/2, g4-g10, COL_BACKGROUND, COL_TEXT);
    }
    if (f & F_DBL_RIGHT) {
        draw_circle(dr, ox+TILE_SIZE+g2, oy+TILE_SIZE/2, g4, COL_TEXT, COL_TEXT);
    }
    draw_update(dr, ox+TILE_SIZE, oy, g, TILE_SIZE);

    if (bg >= 0) {
        draw_rect(dr, ox, oy+TILE_SIZE, TILE_SIZE, g, bg);
    }
    if (f & F_ERROR_DOWN) {
        draw_rect(dr, ox, oy+TILE_SIZE+g4-g6, TILE_SIZE, g2+2*g6, COL_ERROR);
    }
    if (f & F_ADJ_DOWN) {
        draw_circle(dr, ox+TILE_SIZE/2, oy+TILE_SIZE+g2, g4, COL_TEXT, COL_TEXT);
        draw_circle(dr, ox+TILE_SIZE/2, oy+TILE_SIZE+g2, g4-g10, COL_BACKGROUND, COL_TEXT);
    }
    if (f & F_DBL_DOWN) {
        draw_circle(dr, ox+TILE_SIZE/2, oy+TILE_SIZE+g2, g4, COL_TEXT, COL_TEXT);
    }
    draw_update(dr, ox, oy+TILE_SIZE, TILE_SIZE, g);
}

static void draw_furniture(drawing *dr, game_drawstate *ds,
                           const game_state *state, const game_ui *ui,
                           int x, int y)
{
    int ox = COORD(x), oy = COORD(y), bg;
    bool hon;
    unsigned long f = GRID(state, flags, x, y);

    bg = ((f & F_ERROR)) ? COL_LOWLIGHT : COL_BACKGROUND;

    hon = (ui->hshow && x == ui->hx && y == ui->hy);

    /* Clear square. */
    draw_rect(dr, ox, oy, TILE_SIZE, TILE_SIZE,
              (hon && !ui->hpencil) ? COL_HIGHLIGHT : bg);

    /* Draw the highlight (pencil or full), if we're the highlight */
    if (hon && ui->hpencil) {
        int coords[6];
        coords[0] = ox;
        coords[1] = oy;
        coords[2] = ox + TILE_SIZE/2;
        coords[3] = oy;
        coords[4] = ox;
        coords[5] = oy + TILE_SIZE/2;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);
    }

    /* Draw the square outline (which is the cursor, if we're the cursor). */
    draw_rect_outline(dr, ox, oy, TILE_SIZE, TILE_SIZE, COL_GRID);

    draw_update(dr, ox, oy, TILE_SIZE, TILE_SIZE);

    /* Draw the adjacent clue signs. */
    if (ds->mode == MODE_KROPKI)
        draw_krps(dr, ds, ox, oy, f, COL_BACKGROUND, COL_BLACK, COL_WHITE);
    else if (ds->mode == MODE_ADJACENT)
        draw_adjs(dr, ds, ox, oy, f, COL_BACKGROUND, COL_GRID);
    else
        draw_gts(dr, ds, ox, oy, f, COL_BACKGROUND, COL_TEXT);
}

static void draw_num(drawing *dr, game_drawstate *ds, const game_ui *ui, int x, int y)
{
    int ox = COORD(x), oy = COORD(y);
    unsigned long f = GRID(ds,flags,x,y);
    char str[2];
    bool hon;

    hon = (ui->hshow && x == ui->hx && y == ui->hy);
    /* (can assume square has just been cleared) */

    /* Draw number, choosing appropriate colour */
    str[0] = n2c(GRID(ds, nums, x, y), ds->order);
    str[1] = '\0';
    draw_text(dr, ox + TILE_SIZE/2, oy + TILE_SIZE/2,
              (f & F_IMMUTABLE) ? FONT_VARIABLE : FONT_VARIABLE_NORMAL, 
              2*TILE_SIZE/3, ALIGN_VCENTRE | ALIGN_HCENTRE,
              (f & F_ERROR) ? COL_WHITE : 
              (f & F_IMMUTABLE) ? COL_TEXT : 
              (hon && !ui->hpencil) ? COL_GUESS :
              COL_GUESS, str);
}

static void draw_hints(drawing *dr, game_drawstate *ds, int x, int y)
{
    int ox = COORD(x), oy = COORD(y);
    int nhints, i, j, hw, hh, hmax, fontsz;
    char str[2];

    /* (can assume square has just been cleared) */

    /* Draw hints; steal ingenious algorithm (basically)
     * from solo.c:draw_number() */
    /* for (i = nhints = 0; i < ds->order; i++) {
        if (HINT(ds, x, y, i)) nhints++;
    } */
    nhints = ds->order;
    for (hw = 1; hw * hw < nhints; hw++);
    if (hw < 3) hw = 3;
    hh = (nhints + hw - 1) / hw;
    if (hh < 2) hh = 2;
    hmax = max(hw, hh);
    fontsz = TILE_SIZE/(hmax*(11-hmax)/8);

    for (i = j = 0; i < ds->order; i++) {
        if (HINT(ds,x,y,i)) {
            int hx = i % hw, hy = i / hw;

            str[0] = n2c(i+1, ds->order);
            str[1] = '\0';
            draw_text(dr,
                      ox + (4*hx+3) * TILE_SIZE / (4*hw+2),
                      oy + (4*hy+3) * TILE_SIZE / (4*hh+2),
                      FONT_VARIABLE, fontsz,
                      ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL, str);
            j++;
        }
    }
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int x, y, i;
    bool hchanged = false, stale;

    char buf[48];
    /* Draw status bar */
    sprintf(buf, "%s",
            state->cheated   ? "Auto-solved." :
            state->completed ? "COMPLETED!" : "");
    status_bar(dr, buf);

    if (ds->hx != ui->hx || ds->hy != ui->hy ||
        ds->hshow != ui->hshow || ds->hpencil != ui->hpencil)
        hchanged = true;

    for (x = 0; x < ds->order; x++) {
        for (y = 0; y < ds->order; y++) {
            stale = (!ds->started);

            if (hchanged) {
                if ((x == ui->hx && y == ui->hy) ||
                    (x == ds->hx && y == ds->hy))
                    stale = true;
            }

            if (GRID(state, nums, x, y) != GRID(ds, nums, x, y)) {
                GRID(ds, nums, x, y) = GRID(state, nums, x, y);
                stale = true;
            }
            if (GRID(state, flags, x, y) != GRID(ds, flags, x, y)) {
                GRID(ds, flags, x, y) = GRID(state, flags, x, y);
                stale = true;
            }
            if (GRID(ds, nums, x, y) == 0) {
                /* We're not a number square (therefore we might
                 * display hints); do we need to update? */
                for (i = 0; i < ds->order; i++) {
                    if (HINT(state, x, y, i) != HINT(ds, x, y, i)) {
                        HINT(ds, x, y, i) = HINT(state, x, y, i);
                        stale = true;
                    }
                }
            }
            if (stale) {
                draw_furniture(dr, ds, state, ui, x, y);
                if (GRID(ds, nums, x, y) > 0)
                    draw_num(dr, ds, ui, x, y);
                else
                    draw_hints(dr, ds, x, y);
            }
        }
    }
    ds->hx = ui->hx; ds->hy = ui->hy;
    ds->hshow = ui->hshow;
    ds->hpencil = ui->hpencil;

    ds->started = true;
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
#define thegame unequal
#endif

static const char rules[] = "Fill a latin square such that the given clues are satisfied. The clues are different according to the different puzzle modes:\n\n"
"- Unequal: The clue signs are greater-than symbols indicating one square's value is greater than its neighbour's.\n\n"
"- Adjacent: The clue signs are bars indicating one square's value is numerically adjacent (i.e. one higher or one lower) than its neighbour. All clues are always visible: absence of a bar means that a square's value is NOT numerically adjacent to that neighbour.\n\n"
"- Kropki: The clues are either black circles (indicating that one number is the double of the other), white circles (the numbers are numerically adjacent) or empty (none of both conditions apply). In the case of the numbers '1' and '2' both conditions apply; the clue color is then chosen by random.\n\n\n"
"This puzzle was contributed by James Harvey.\n"
"The 'Kropki' variation was contributed by Steffen Bauer.";

const struct game thegame = {
    "Unequal", "games.unequal", "unequal", rules,
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
    PREFERRED_TILE_SIZE, game_compute_size, game_set_size,
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

