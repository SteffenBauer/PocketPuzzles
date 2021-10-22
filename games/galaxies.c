/*
 * galaxies.c: implementation of 'Tentai Show' from Nikoli,
 *             also sometimes called 'Spiral Galaxies'.
 *
 * Notes:
 *
 * Grid is stored as size (2n-1), holding edges as well as spaces
 * (and thus vertices too, at edge intersections).
 *
 * Any dot will thus be positioned at one of our grid points,
 * which saves any faffing with half-of-a-square stuff.
 *
 * Edges have on/off state; obviously the actual edges of the
 * board are fixed to on, and everything else starts as off.
 *
 * TTD:
   * Cleverer solver
   * Think about how to display remote groups of tiles?
 *
 * Bugs:
 *
 * Notable puzzle IDs:
 *
 * Nikoli's example [web site has wrong highlighting]
 * (at http://www.nikoli.co.jp/en/puzzles/astronomical_show/):
 *  5x5:eBbbMlaBbOEnf
 *
 * The 'spiral galaxies puzzles are NP-complete' paper
 * (at http://www.stetson.edu/~efriedma/papers/spiral.pdf):
 *  7x7:chpgdqqqoezdddki
 *
 * Puzzle competition pdf examples
 * (at http://www.puzzleratings.org/Yurekli2006puz.pdf):
 *  6x6:EDbaMucCohbrecEi
 *  10x10:beFbufEEzowDlxldibMHezBQzCdcFzjlci
 *  13x13:dCemIHFFkJajjgDfdbdBzdzEgjccoPOcztHjBczLDjczqktJjmpreivvNcggFi
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

enum {
    COL_BACKGROUND,
    COL_WHITEBG,
    COL_BLACKBG,
    COL_WHITEDOT,
    COL_BLACKDOT,
    COL_GRID,
    COL_EDGE,
    COL_ARROW,
    COL_CURSOR,
    NCOLOURS
};

#define DIFFLIST(A)             \
    A(NORMAL,Normal,n)          \
    A(UNREASONABLE,Unreasonable,u)

#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM)
    DIFF_IMPOSSIBLE, DIFF_AMBIGUOUS, DIFF_UNFINISHED, DIFF_MAX };
static char const *const galaxies_diffnames[] = {
    DIFFLIST(TITLE) "Impossible", "Ambiguous", "Unfinished" };
static char const galaxies_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

struct game_params {
    /* X and Y is the area of the board as seen by
     * the user, not the (2n+1) area the game uses. */
    int w, h, diff;
};

enum { s_tile, s_edge, s_vertex };

#define F_DOT           1       /* there's a dot here */
#define F_EDGE_SET      2       /* the edge is set */
#define F_TILE_ASSOC    4       /* this tile is associated with a dot. */
#define F_DOT_BLACK     8       /* (ui only) dot is black. */
#define F_MARK          16      /* scratch flag */
#define F_REACHABLE     32
#define F_SCRATCH       64
#define F_MULTIPLE      128
#define F_DOT_HOLD      256
#define F_GOOD          512

typedef struct space {
    int x, y;           /* its position */
    int type;
    unsigned int flags;
    int dotx, doty;     /* if flags & F_TILE_ASSOC */
    int nassoc;         /* if flags & F_DOT */
} space;

#define INGRID(s,x,y) ((x) >= 0 && (y) >= 0 &&                  \
                       (x) < (state)->sx && (y) < (state)->sy)
#define INUI(s,x,y)   ((x) > 0 && (y) > 0 &&                  \
                       (x) < ((state)->sx-1) && (y) < ((state)->sy-1))

#define GRID(s,g,x,y) ((s)->g[((y)*(s)->sx)+(x)])
#define SPACE(s,x,y) GRID(s,grid,x,y)

struct game_state {
    int w, h;           /* size from params */
    int sx, sy;         /* allocated size, (2x-1)*(2y-1) */
    space *grid;
    bool completed, used_solve;
    int ndots;
    space **dots;

    midend *me;         /* to call supersede_game_desc */
    int cdiff;          /* difficulty of current puzzle (for status bar),
                           or -1 if stale. */
};

static bool check_complete(const game_state *state, int *dsf, int *colours);
static int solver_state(game_state *state, int maxdiff);
static int solver_obvious(game_state *state);
static int solver_obvious_dot(game_state *state, space *dot);
static space *space_opposite_dot(const game_state *state, const space *sp,
                                 const space *dot);
static space *tile_opposite(const game_state *state, const space *sp);
static game_state *execute_move(const game_state *state, const char *move);

/* ----------------------------------------------------------
 * Game parameters and presets
 */

/* make up some sensible default sizes */

#define DEFAULT_PRESET 1

static const game_params galaxies_presets[] = {
    {  7,  7, DIFF_NORMAL },
    {  7,  7, DIFF_UNREASONABLE },
    { 10, 10, DIFF_UNREASONABLE },
    { 12, 12, DIFF_UNREASONABLE },
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(galaxies_presets))
        return false;

    ret = snew(game_params);
    *ret = galaxies_presets[i]; /* structure copy */

    sprintf(buf, "%dx%d %s", ret->w, ret->h,
            galaxies_diffnames[ret->diff]);

    if (name) *name = dupstr(buf);
    *params = ret;
    return true;
}

static game_params *default_params(void)
{
    game_params *ret;
    game_fetch_preset(DEFAULT_PRESET, NULL, &ret);
    return ret;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;		       /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
    params->h = params->w = atoi(string);
    params->diff = DIFF_NORMAL;
    while (*string && isdigit((unsigned char)*string)) string++;
    if (*string == 'x') {
        string++;
        params->h = atoi(string);
        while (*string && isdigit((unsigned char)*string)) string++;
    }
    if (*string == 'd') {
        int i;
        string++;
        for (i = 0; i <= DIFF_UNREASONABLE; i++)
            if (*string == galaxies_diffchars[i])
                params->diff = i;
        if (*string) string++;
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char str[80];
    sprintf(str, "%dx%d", params->w, params->h);
    if (full)
        sprintf(str + strlen(str), "d%c", galaxies_diffchars[params->diff]);
    return dupstr(str);
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
    ret[2].u.choices.choicenames = DIFFCONFIG;
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
    if (params->w < 3 || params->h < 3)
        return "Width and height must both be at least 3";
    if (params->w > 15 || params->h > 15)
        return "Width and height must both be at most 15";

    return NULL;
}

/* ----------------------------------------------------------
 * Game utility functions.
 */

static void add_dot(space *space) {
    space->flags |= F_DOT;
    space->nassoc = 0;
}

static void remove_dot(space *space) {
    space->flags &= ~F_DOT;
}

static void remove_assoc(const game_state *state, space *tile) {
    if (tile->flags & F_TILE_ASSOC) {
        SPACE(state, tile->dotx, tile->doty).nassoc--;
        tile->flags &= ~F_TILE_ASSOC;
        tile->dotx = -1;
        tile->doty = -1;
    }
}

static void remove_assoc_with_opposite(game_state *state, space *tile) {
    space *opposite;

    if (!(tile->flags & F_TILE_ASSOC)) {
        return;
    }

    opposite = tile_opposite(state, tile);
    remove_assoc(state, tile);

    if (opposite != NULL && opposite != tile) {
        remove_assoc(state, opposite);
    }
}

static void add_assoc(const game_state *state, space *tile, space *dot) {
    remove_assoc(state, tile);

    tile->flags |= F_TILE_ASSOC;
    tile->dotx = dot->x;
    tile->doty = dot->y;
    dot->nassoc++;
}

static bool ok_to_add_assoc_with_opposite_internal(
    const game_state *state, space *tile, space *opposite)
{
    int *colors;
    bool toret;

    if (tile->flags & F_DOT)
        return false;
    if (opposite == NULL)
        return false;
    if (opposite->flags & F_DOT)
        return false;

    toret = true;
    colors = snewn(state->w * state->h, int);
    check_complete(state, NULL, colors);

    if (colors[(tile->y - 1)/2 * state->w + (tile->x - 1)/2])
        toret = false;
    if (colors[(opposite->y - 1)/2 * state->w + (opposite->x - 1)/2])
        toret = false;

    sfree(colors);
    return toret;
}

static bool ok_to_add_assoc_with_opposite(
    const game_state *state, space *tile, space *dot)
{
    space *opposite = space_opposite_dot(state, tile, dot);
    return ok_to_add_assoc_with_opposite_internal(state, tile, opposite);
}

static void add_assoc_with_opposite(game_state *state, space *tile, space *dot) {
    space *opposite = space_opposite_dot(state, tile, dot);

    if(opposite && ok_to_add_assoc_with_opposite_internal(
           state, tile, opposite))
    {
        remove_assoc_with_opposite(state, tile);
        add_assoc(state, tile, dot);
        remove_assoc_with_opposite(state, opposite);
        add_assoc(state, opposite, dot);
    }
}

#define IS_VERTICAL_EDGE(x) ((x % 2) == 0)

/* Space-enumeration callbacks should all return 1 for 'progress made',
 * -1 for 'impossible', and 0 otherwise. */
typedef int (*space_cb)(game_state *state, space *sp, void *ctx);

#define IMPOSSIBLE_QUITS        1

static int foreach_sub(game_state *state, space_cb cb, unsigned int f,
                       void *ctx, int startx, int starty)
{
    int x, y, ret;
    bool progress = false, impossible = false;
    space *sp;

    for (y = starty; y < state->sy; y += 2) {
        sp = &SPACE(state, startx, y);
        for (x = startx; x < state->sx; x += 2) {
            ret = cb(state, sp, ctx);
            if (ret == -1) {
                if (f & IMPOSSIBLE_QUITS) return -1;
                impossible = true;
            } else if (ret == 1) {
                progress = true;
            }
            sp += 2;
        }
    }
    return impossible ? -1 : progress ? 1 : 0;
}

static int foreach_tile(game_state *state, space_cb cb, unsigned int f,
                        void *ctx)
{
    return foreach_sub(state, cb, f, ctx, 1, 1);
}

static int foreach_edge(game_state *state, space_cb cb, unsigned int f,
                        void *ctx)
{
    int ret1, ret2;

    ret1 = foreach_sub(state, cb, f, ctx, 0, 1);
    ret2 = foreach_sub(state, cb, f, ctx, 1, 0);

    if (ret1 == -1 || ret2 == -1) return -1;
    return (ret1 || ret2) ? 1 : 0;
}

static space *space_opposite_dot(const game_state *state, const space *sp,
                                 const space *dot)
{
    int dx, dy, tx, ty;
    space *sp2;

    dx = sp->x - dot->x;
    dy = sp->y - dot->y;
    tx = dot->x - dx;
    ty = dot->y - dy;
    if (!INGRID(state, tx, ty)) return NULL;

    sp2 = &SPACE(state, tx, ty);
    return sp2;
}

static space *tile_opposite(const game_state *state, const space *sp)
{
    space *dot;
    dot = &SPACE(state, sp->dotx, sp->doty);
    return space_opposite_dot(state, sp, dot);
}

static bool dotfortile(game_state *state, space *tile, space *dot)
{
    space *tile_opp = space_opposite_dot(state, tile, dot);

    if (!tile_opp) return false; /* opposite would be off grid */
    if (tile_opp->flags & F_TILE_ASSOC &&
            (tile_opp->dotx != dot->x || tile_opp->doty != dot->y))
            return false; /* opposite already associated with diff. dot */
    return true;
}

static void adjacencies(game_state *state, space *sp, space **a1s, space **a2s)
{
    int dxs[4] = {-1, 1, 0, 0}, dys[4] = {0, 0, -1, 1};
    int n, x, y;

    /* this function needs optimising. */

    for (n = 0; n < 4; n++) {
        x = sp->x+dxs[n];
        y = sp->y+dys[n];

        if (INGRID(state, x, y)) {
            a1s[n] = &SPACE(state, x, y);

            x += dxs[n]; y += dys[n];

            if (INGRID(state, x, y))
                a2s[n] = &SPACE(state, x, y);
            else
                a2s[n] = NULL;
        } else {
            a1s[n] = a2s[n] = NULL;
        }
    }
}

static bool outline_tile_fordot(game_state *state, space *tile, bool mark)
{
    space *tadj[4], *eadj[4];
    int i;
    bool didsth = false, edge, same;

    adjacencies(state, tile, eadj, tadj);
    for (i = 0; i < 4; i++) {
        if (!eadj[i]) continue;

        edge = eadj[i]->flags & F_EDGE_SET;
        if (tadj[i]) {
            if (!(tile->flags & F_TILE_ASSOC))
                same = !(tadj[i]->flags & F_TILE_ASSOC);
            else
                same = ((tadj[i]->flags & F_TILE_ASSOC) &&
                    tile->dotx == tadj[i]->dotx &&
                    tile->doty == tadj[i]->doty);
        } else
            same = false;

        if (!edge && !same) {
            if (mark) eadj[i]->flags |= F_EDGE_SET;
            didsth = true;
        } else if (edge && same) {
            if (mark) eadj[i]->flags &= ~F_EDGE_SET;
            didsth = true;
        }
    }
    return didsth;
}

static void tiles_from_edge(game_state *state, space *sp, space **ts)
{
    int xs[2], ys[2];

    if (IS_VERTICAL_EDGE(sp->x)) {
        xs[0] = sp->x-1; ys[0] = sp->y;
        xs[1] = sp->x+1; ys[1] = sp->y;
    } else {
        xs[0] = sp->x; ys[0] = sp->y-1;
        xs[1] = sp->x; ys[1] = sp->y+1;
    }
    ts[0] = INGRID(state, xs[0], ys[0]) ? &SPACE(state, xs[0], ys[0]) : NULL;
    ts[1] = INGRID(state, xs[1], ys[1]) ? &SPACE(state, xs[1], ys[1]) : NULL;
}

/* Returns a move string for use by 'solve', including the initial
 * 'S' if issolve is true. */
static char *diff_game(const game_state *src, const game_state *dest,
                       bool issolve)
{
    int movelen = 0, movesize = 256, x, y, len;
    char *move = snewn(movesize, char), buf[80];
    const char *sep = "";
    char achar = issolve ? 'a' : 'A';
    space *sps, *spd;

    if (issolve) {
        move[movelen++] = 'S';
        sep = ";";
    }
    move[movelen] = '\0';
    for (x = 0; x < src->sx; x++) {
        for (y = 0; y < src->sy; y++) {
            sps = &SPACE(src, x, y);
            spd = &SPACE(dest, x, y);

            len = 0;
            if (sps->type == s_tile) {
                if ((sps->flags & F_TILE_ASSOC) &&
                    (spd->flags & F_TILE_ASSOC)) {
                    if (sps->dotx != spd->dotx ||
                        sps->doty != spd->doty)
                    /* Both associated; change association, if different */
                        len = sprintf(buf, "%s%c%d,%d,%d,%d", sep,
                                      (int)achar, x, y, spd->dotx, spd->doty);
                } else if (sps->flags & F_TILE_ASSOC)
                    /* Only src associated; remove. */
                    len = sprintf(buf, "%sU%d,%d", sep, x, y);
                else if (spd->flags & F_TILE_ASSOC)
                    /* Only dest associated; add. */
                    len = sprintf(buf, "%s%c%d,%d,%d,%d", sep,
                                  (int)achar, x, y, spd->dotx, spd->doty);
            } else if (sps->type == s_edge) {
                if ((sps->flags & F_EDGE_SET) != (spd->flags & F_EDGE_SET))
                    /* edge flags are different; flip them. */
                    len = sprintf(buf, "%sE%d,%d", sep, x, y);
            }
            if (len) {
                if (movelen + len >= movesize) {
                    movesize = movelen + len + 256;
                    move = sresize(move, movesize, char);
                }
                strcpy(move + movelen, buf);
                movelen += len;
                sep = ";";
            }
        }
    }
    return move;
}

/* Returns true if a dot here would not be too close to any other dots
 * (and would avoid other game furniture). */
static bool dot_is_possible(const game_state *state, space *sp, bool allow_assoc)
{
    int bx = 0, by = 0, dx, dy;
    space *adj;

    switch (sp->type) {
    case s_tile:
        bx = by = 1; break;
    case s_edge:
        if (IS_VERTICAL_EDGE(sp->x)) {
            bx = 2; by = 1;
        } else {
            bx = 1; by = 2;
        }
        break;
    case s_vertex:
        bx = by = 2; break;
    }

    for (dx = -bx; dx <= bx; dx++) {
        for (dy = -by; dy <= by; dy++) {
            if (!INGRID(state, sp->x+dx, sp->y+dy)) continue;

            adj = &SPACE(state, sp->x+dx, sp->y+dy);

            if (!allow_assoc && (adj->flags & F_TILE_ASSOC))
                return false;

            if (dx != 0 || dy != 0) {
                /* Other than our own square, no dots nearby. */
                if (adj->flags & (F_DOT))
                    return false;
            }

            /* We don't want edges within our rectangle
             * (but don't care about edges on the edge) */
            if (abs(dx) < bx && abs(dy) < by &&
                adj->flags & F_EDGE_SET)
                return false;
        }
    }
    return true;
}

/* ----------------------------------------------------------
 * Game generation, structure creation, and descriptions.
 */

static game_state *blank_game(int w, int h)
{
    game_state *state = snew(game_state);
    int x, y;

    state->w = w;
    state->h = h;

    state->sx = (w*2)+1;
    state->sy = (h*2)+1;
    state->grid = snewn(state->sx * state->sy, space);
    state->completed = false;
    state->used_solve = false;

    for (x = 0; x < state->sx; x++) {
        for (y = 0; y < state->sy; y++) {
            space *sp = &SPACE(state, x, y);
            memset(sp, 0, sizeof(space));
            sp->x = x;
            sp->y = y;
            if ((x % 2) == 0 && (y % 2) == 0)
                sp->type = s_vertex;
            else if ((x % 2) == 0 || (y % 2) == 0) {
                sp->type = s_edge;
                if (x == 0 || y == 0 || x == state->sx-1 || y == state->sy-1)
                    sp->flags |= F_EDGE_SET;
            } else
                sp->type = s_tile;
        }
    }

    state->ndots = 0;
    state->dots = NULL;

    state->me = NULL; /* filled in by new_game. */
    state->cdiff = -1;

    return state;
}

static void game_update_dots(game_state *state)
{
    int i, n, sz = state->sx * state->sy;

    if (state->dots) sfree(state->dots);
    state->ndots = 0;

    for (i = 0; i < sz; i++) {
        if (state->grid[i].flags & F_DOT) state->ndots++;
    }
    state->dots = snewn(state->ndots, space *);
    n = 0;
    for (i = 0; i < sz; i++) {
        if (state->grid[i].flags & F_DOT)
            state->dots[n++] = &state->grid[i];
    }
}

static void clear_game(game_state *state, bool cleardots)
{
    int x, y;

    /* don't erase edge flags around outline! */
    for (x = 1; x < state->sx-1; x++) {
        for (y = 1; y < state->sy-1; y++) {
            if (cleardots)
                SPACE(state, x, y).flags = 0;
            else
                SPACE(state, x, y).flags &= (F_DOT|F_DOT_BLACK);
        }
    }
    if (cleardots) game_update_dots(state);
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = blank_game(state->w, state->h);

    ret->completed = state->completed;
    ret->used_solve = state->used_solve;

    memcpy(ret->grid, state->grid,
           ret->sx*ret->sy*sizeof(space));

    game_update_dots(ret);

    ret->me = state->me;
    ret->cdiff = state->cdiff;

    return ret;
}

static void free_game(game_state *state)
{
    if (state->dots) sfree(state->dots);
    sfree(state->grid);
    sfree(state);
}

/* Game description is a sequence of letters representing the number
 * of spaces (a = 0, y = 24) before the next dot; a-y for a white dot,
 * and A-Y for a black dot. 'z' is 25 spaces (and no dot).
 *
 * I know it's a bitch to generate by hand, so we provide
 * an edit mode.
 */

static char *encode_game(const game_state *state)
{
    char *desc, *p;
    int run, x, y, area;
    unsigned int f;

    area = (state->sx-2) * (state->sy-2);

    desc = snewn(area, char);
    p = desc;
    run = 0;
    for (y = 1; y < state->sy-1; y++) {
        for (x = 1; x < state->sx-1; x++) {
            f = SPACE(state, x, y).flags;

            /* a/A is 0 spaces between, b/B is 1 space, ...
             * y/Y is 24 spaces, za/zA is 25 spaces, ...
             * It's easier to count from 0 because we then
             * don't have to special-case the top left-hand corner
             * (which could be a dot with 0 spaces before it). */
            if (!(f & F_DOT))
                run++;
            else {
                while (run > 24) {
                    *p++ = 'z';
                    run -= 25;
                }
                *p++ = ((f & F_DOT_BLACK) ? 'A' : 'a') + run;
                run = 0;
            }
        }
    }
    *p++ = '\0';
    desc = sresize(desc, p - desc, char);

    return desc;
}

struct movedot {
    int op;
    space *olddot, *newdot;
};

enum { MD_CHECK, MD_MOVE };

static int movedot_cb(game_state *state, space *tile, void *vctx)
{
   struct movedot *md = (struct movedot *)vctx;
   space *newopp = NULL;

   if (!(tile->flags & F_TILE_ASSOC)) return 0;
   if (tile->dotx != md->olddot->x || tile->doty != md->olddot->y)
       return 0;

   newopp = space_opposite_dot(state, tile, md->newdot);

   switch (md->op) {
   case MD_CHECK:
       /* If the tile is associated with the old dot, check its
        * opposite wrt the _new_ dot is empty or same assoc. */
       if (!newopp) return -1; /* no new opposite */
       if (newopp->flags & F_TILE_ASSOC) {
           if (newopp->dotx != md->olddot->x ||
               newopp->doty != md->olddot->y)
               return -1; /* associated, but wrong dot. */
       }
       break;

   case MD_MOVE:
       /* Move dot associations: anything that was associated
        * with the old dot, and its opposite wrt the new dot,
        * become associated with the new dot. */
       add_assoc(state, tile, md->newdot);
       add_assoc(state, newopp, md->newdot);
       return 1; /* we did something! */
   }
   return 0;
}

/* For the given dot, first see if we could expand it into all the given
 * extra spaces (by checking for empty spaces on the far side), and then
 * see if we can move the dot to shift the CoG to include the new spaces.
 */
static bool dot_expand_or_move(game_state *state, space *dot,
                               space **toadd, int nadd)
{
    space *tileopp;
    int i, ret, nnew, cx, cy;
    struct movedot md;

    /* First off, could we just expand the current dot's tile to cover
     * the space(s) passed in and their opposites? */
    for (i = 0; i < nadd; i++) {
        tileopp = space_opposite_dot(state, toadd[i], dot);
        if (!tileopp) goto noexpand;
        if (tileopp->flags & F_TILE_ASSOC) goto noexpand;
    }
    /* OK, all spaces have valid empty opposites: associate spaces and
     * opposites with our dot. */
    for (i = 0; i < nadd; i++) {
        tileopp = space_opposite_dot(state, toadd[i], dot);
        add_assoc(state, toadd[i], dot);
        add_assoc(state, tileopp, dot);
    }
    return true;

noexpand:
    /* Otherwise, try to move dot so as to encompass given spaces: */
    /* first, calculate the 'centre of gravity' of the new dot. */
    nnew = dot->nassoc + nadd; /* number of tiles assoc. with new dot. */
    cx = dot->x * dot->nassoc;
    cy = dot->y * dot->nassoc;
    for (i = 0; i < nadd; i++) {
        cx += toadd[i]->x;
        cy += toadd[i]->y;
    }
    /* If the CoG isn't a whole number, it's not possible. */
    if ((cx % nnew) != 0 || (cy % nnew) != 0) {
        return false;
    }
    cx /= nnew; cy /= nnew;

    /* Check whether all spaces in the old tile would have a good
     * opposite wrt the new dot. */
    md.olddot = dot;
    md.newdot = &SPACE(state, cx, cy);
    md.op = MD_CHECK;
    ret = foreach_tile(state, movedot_cb, IMPOSSIBLE_QUITS, &md);
    if (ret == -1) {
        return false;
    }
    /* Also check whether all spaces we're adding would have a good
     * opposite wrt the new dot. */
    for (i = 0; i < nadd; i++) {
        tileopp = space_opposite_dot(state, toadd[i], md.newdot);
        if (tileopp && (tileopp->flags & F_TILE_ASSOC) &&
            (tileopp->dotx != dot->x || tileopp->doty != dot->y)) {
            tileopp = NULL;
        }
        if (!tileopp) {
            return false;
        }
    }

    /* If we've got here, we're ok. First, associate all of 'toadd'
     * with the _old_ dot (so they'll get fixed up, with their opposites,
     * in the next step). */
    for (i = 0; i < nadd; i++) {
        add_assoc(state, toadd[i], dot);
    }

    /* Finally, move the dot and fix up all the old associations. */
    {
        remove_dot(dot);
        add_dot(md.newdot);
    }

    md.op = MD_MOVE;
    ret = foreach_tile(state, movedot_cb, 0, &md);

    return true;
}

/* Hard-code to a max. of 2x2 squares, for speed (less malloc) */
#define MAX_TOADD 4
#define MAX_OUTSIDE 8

#define MAX_TILE_PERC 20

static bool generate_try_block(game_state *state, random_state *rs,
                               int x1, int y1, int x2, int y2)
{
    int x, y, nadd = 0, nout = 0, i, maxsz;
    space *sp, *toadd[MAX_TOADD], *outside[MAX_OUTSIDE], *dot;

    if (!INGRID(state, x1, y1) || !INGRID(state, x2, y2)) return false;

    /* We limit the maximum size of tiles to be ~2*sqrt(area); so,
     * a 5x5 grid shouldn't have anything >10 tiles, a 20x20 grid
     * nothing >40 tiles. */
    maxsz = (int)sqrt((double)(state->w * state->h)) * 2;

    /* Make a static list of the spaces; if any space is already
     * associated then quit immediately. */
    for (x = x1; x <= x2; x += 2) {
        for (y = y1; y <= y2; y += 2) {
            sp = &SPACE(state, x, y);
            if (sp->flags & F_TILE_ASSOC) return false;
            toadd[nadd++] = sp;
        }
    }

    /* Make a list of the spaces outside of our block, and shuffle it. */
#define OUTSIDE(x, y) do {                              \
    if (INGRID(state, (x), (y))) {                      \
        outside[nout++] = &SPACE(state, (x), (y));      \
    }                                                   \
} while(0)
    for (x = x1; x <= x2; x += 2) {
        OUTSIDE(x, y1-2);
        OUTSIDE(x, y2+2);
    }
    for (y = y1; y <= y2; y += 2) {
        OUTSIDE(x1-2, y);
        OUTSIDE(x2+2, y);
    }
    shuffle(outside, nout, sizeof(space *), rs);

    for (i = 0; i < nout; i++) {
        if (!(outside[i]->flags & F_TILE_ASSOC)) continue;
        dot = &SPACE(state, outside[i]->dotx, outside[i]->doty);
        if (dot->nassoc >= maxsz) {
            continue;
        }
        if (dot_expand_or_move(state, dot, toadd, nadd)) return true;
    }
    return false;
}

#define MAXTRIES 50
#define GP_DOTS   1

static void generate_pass(game_state *state, random_state *rs, int *scratch,
                         int perc, unsigned int flags)
{
    int sz = state->sx*state->sy, nspc, i;

    shuffle(scratch, sz, sizeof(int), rs);

    /* This bug took me a, er, little while to track down. On PalmOS,
     * which has 16-bit signed ints, puzzles over about 9x9 started
     * failing to generate because the nspc calculation would start
     * to overflow, causing the dots not to be filled in properly. */
    nspc = (int)(((long)perc * (long)sz) / 100L);

    for (i = 0; i < nspc; i++) {
        space *sp = &state->grid[scratch[i]];
        int x1 = sp->x, y1 = sp->y, x2 = sp->x, y2 = sp->y;

        if (sp->type == s_edge) {
            if (IS_VERTICAL_EDGE(sp->x)) {
                x1--; x2++;
            } else {
                y1--; y2++;
            }
        }
        if (sp->type != s_vertex) {
            /* heuristic; expanding from vertices tends to generate lots of
             * too-big regions of tiles. */
            if (generate_try_block(state, rs, x1, y1, x2, y2))
                continue; /* we expanded successfully. */
        }

        if (!(flags & GP_DOTS)) continue;

        if ((sp->type == s_edge) && (i % 2)) {
            continue;
        }

        /* If we've got here we might want to put a dot down. Check
         * if we can, and add one if so. */
        if (dot_is_possible(state, sp, false)) {
            add_dot(sp);
            solver_obvious_dot(state, sp);
        }
    }
}

static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
    game_state *state = blank_game(params->w, params->h), *copy;
    char *desc;
    int *scratch, sz = state->sx*state->sy, i;
    int diff, ntries = 0;

    /* Random list of squares to try and process, one-by-one. */
    scratch = snewn(sz, int);
    for (i = 0; i < sz; i++) scratch[i] = i;

generate:
    clear_game(state, true);
    ntries++;

    /* generate_pass(state, rs, scratch, 10, GP_DOTS); */
    /* generate_pass(state, rs, scratch, 100, 0); */
    generate_pass(state, rs, scratch, 100, GP_DOTS);

    game_update_dots(state);

    if (state->ndots == 1) goto generate;

    for (i = 0; i < state->sx*state->sy; i++)
        if (state->grid[i].type == s_tile)
            outline_tile_fordot(state, &state->grid[i], true);
    check_complete(state, NULL, NULL);

    copy = dup_game(state);
    clear_game(copy, false);
    diff = solver_state(copy, params->diff);
    free_game(copy);

    if (diff != params->diff) {
        /*
         * We'll grudgingly accept a too-easy puzzle, but we must
         * _not_ permit a too-hard one (one which the solver
         * couldn't handle at all).
         */
        if (diff > params->diff ||
            ntries < MAXTRIES) goto generate;
    }

    desc = encode_game(state);

    game_state *blank = blank_game(params->w, params->h);
    *aux = diff_game(blank, state, true);
    free_game(blank);

    free_game(state);
    sfree(scratch);

    return desc;
}

static bool dots_too_close(game_state *state)
{
    /* Quick-and-dirty check, using half the solver:
     * solver_obvious will only fail if the dots are
     * too close together, so dot-proximity associations
     * overlap. */
    game_state *tmp = dup_game(state);
    int ret = solver_obvious(tmp);
    free_game(tmp);
    return ret == -1;
}

static game_state *load_game(const game_params *params, const char *desc,
                             const char **why_r)
{
    game_state *state = blank_game(params->w, params->h);
    const char *why = NULL;
    int i, x, y, n;
    unsigned int df;

    i = 0;
    while (*desc) {
        n = *desc++;
        if (n == 'z') {
            i += 25;
            continue;
        }
        if (n >= 'a' && n <= 'y') {
            i += n - 'a';
            df = 0;
        } else if (n >= 'A' && n <= 'Y') {
            i += n - 'A';
            df = F_DOT_BLACK;
        } else {
            why = "Invalid characters in game description"; goto fail;
        }
        /* if we got here we incremented i and have a dot to add. */
        y = (i / (state->sx-2)) + 1;
        x = (i % (state->sx-2)) + 1;
        if (!INUI(state, x, y)) {
            why = "Too much data to fit in grid"; goto fail;
        }
        add_dot(&SPACE(state, x, y));
        SPACE(state, x, y).flags |= df;
        i++;
    }
    game_update_dots(state);

    if (dots_too_close(state)) {
        why = "Dots too close together"; goto fail;
    }

    return state;

fail:
    free_game(state);
    if (why_r) *why_r = why;
    return NULL;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    const char *why = NULL;
    game_state *dummy = load_game(params, desc, &why);
    if (dummy) {
        free_game(dummy);
    }
    return why;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    game_state *state = load_game(params, desc, NULL);
    if (!state) {
        return NULL;
    }
    return state;
}

/* ----------------------------------------------------------
 * Solver and all its little wizards.
 */

static int solver_recurse_depth;

typedef struct solver_ctx {
    game_state *state;
    int sz;             /* state->sx * state->sy */
    space **scratch;    /* size sz */

} solver_ctx;

static solver_ctx *new_solver(game_state *state)
{
    solver_ctx *sctx = snew(solver_ctx);
    sctx->state = state;
    sctx->sz = state->sx*state->sy;
    sctx->scratch = snewn(sctx->sz, space *);
    return sctx;
}

static void free_solver(solver_ctx *sctx)
{
    sfree(sctx->scratch);
    sfree(sctx);
}

    /* Solver ideas so far:
     *
     * For any empty space, work out how many dots it could associate
     * with:
       * it needs line-of-sight
       * it needs an empty space on the far side
       * any adjacent lines need corresponding line possibilities.
     */

/* The solver_ctx should keep a list of dot positions, for quicker looping.
 *
 * Solver techniques, in order of difficulty:
   * obvious adjacency to dots
   * transferring tiles to opposite side
   * transferring lines to opposite side
   * one possible dot for a given tile based on opposite availability
   * tile with 3 definite edges next to an associated tile must associate
      with same dot.
   *
   * one possible dot for a given tile based on line-of-sight
 */

static int solver_add_assoc(game_state *state, space *tile, int dx, int dy,
                            const char *why)
{
    space *dot, *tile_opp;

    dot = &SPACE(state, dx, dy);
    tile_opp = space_opposite_dot(state, tile, dot);

    if (tile->flags & F_TILE_ASSOC) {
        if ((tile->dotx != dx) || (tile->doty != dy)) {
            return -1;
        }
        return 0; /* no-op */
    }
    if (!tile_opp) {
        return -1;
    }
    if (tile_opp->flags & F_TILE_ASSOC &&
        (tile_opp->dotx != dx || tile_opp->doty != dy)) {
        return -1;
    }

    add_assoc(state, tile, dot);
    add_assoc(state, tile_opp, dot);
    return 1;
}

static int solver_obvious_dot(game_state *state, space *dot)
{
    int dx, dy, ret, didsth = 0;
    space *tile;

    for (dx = -1; dx <= 1; dx++) {
        for (dy = -1; dy <= 1; dy++) {
            if (!INGRID(state, dot->x+dx, dot->y+dy)) continue;

            tile = &SPACE(state, dot->x+dx, dot->y+dy);
            if (tile->type == s_tile) {
                ret = solver_add_assoc(state, tile, dot->x, dot->y,
                                       "next to dot");
                if (ret < 0) return -1;
                if (ret > 0) didsth = 1;
            }
        }
    }
    return didsth;
}

static int solver_obvious(game_state *state)
{
    int i, didsth = 0, ret;

    for (i = 0; i < state->ndots; i++) {
        ret = solver_obvious_dot(state, state->dots[i]);
        if (ret < 0) return -1;
        if (ret > 0) didsth = 1;
    }
    return didsth;
}

static int solver_lines_opposite_cb(game_state *state, space *edge, void *ctx)
{
    int didsth = 0, n, dx, dy;
    space *tiles[2], *tile_opp, *edge_opp;

    tiles_from_edge(state, edge, tiles);

    /* if tiles[0] && tiles[1] && they're both associated
     * and they're both associated with different dots,
     * ensure the line is set. */
    if (!(edge->flags & F_EDGE_SET) &&
        tiles[0] && tiles[1] &&
        (tiles[0]->flags & F_TILE_ASSOC) &&
        (tiles[1]->flags & F_TILE_ASSOC) &&
        (tiles[0]->dotx != tiles[1]->dotx ||
         tiles[0]->doty != tiles[1]->doty)) {
        /* No edge, but the two adjacent tiles are both
         * associated with different dots; add the edge. */
        edge->flags |= F_EDGE_SET;
        didsth = 1;
    }

    if (!(edge->flags & F_EDGE_SET)) return didsth;
    for (n = 0; n < 2; n++) {
        if (!tiles[n]) continue;
        if (!(tiles[n]->flags & F_TILE_ASSOC)) continue;

        tile_opp = tile_opposite(state, tiles[n]);
        if (!tile_opp) {
            /* edge of tile has no opposite edge (off grid?);
             * this is impossible. */
            return -1;
        }

        dx = tiles[n]->x - edge->x;
        dy = tiles[n]->y - edge->y;
        edge_opp = &SPACE(state, tile_opp->x+dx, tile_opp->y+dy);
        if (!(edge_opp->flags & F_EDGE_SET)) {
            edge_opp->flags |= F_EDGE_SET;
            didsth = 1;
        }
    }
    return didsth;
}

static int solver_spaces_oneposs_cb(game_state *state, space *tile, void *ctx)
{
    int n, eset, ret;
    space *edgeadj[4], *tileadj[4];
    int dotx, doty;

    if (tile->flags & F_TILE_ASSOC) return 0;

    adjacencies(state, tile, edgeadj, tileadj);

    /* Empty tile. If each edge is either set, or associated with
     * the same dot, we must also associate with dot. */
    eset = 0; dotx = -1; doty = -1;
    for (n = 0; n < 4; n++) {
        if (edgeadj[n]->flags & F_EDGE_SET) {
            eset++;
        } else {
            /* If an adjacent tile is empty we can't make any deductions.*/
            if (!(tileadj[n]->flags & F_TILE_ASSOC))
                return 0;

            /* If an adjacent tile is assoc. with a different dot
             * we can't make any deductions. */
            if (dotx != -1 && doty != -1 &&
                (tileadj[n]->dotx != dotx ||
                 tileadj[n]->doty != doty))
                return 0;

            dotx = tileadj[n]->dotx;
            doty = tileadj[n]->doty;
        }
    }
    if (eset == 4) {
        return -1;
    }

    ret = solver_add_assoc(state, tile, dotx, doty, "rest are edges");
    if (ret == -1) return -1;

    return 1;
}

/* Improved algorithm for tracking line-of-sight from dots, and not spaces.
 *
 * The solver_ctx already stores a list of dots: the algorithm proceeds by
 * expanding outwards from each dot in turn, expanding first to the boundary
 * of its currently-connected tile and then to all empty tiles that could see
 * it. Empty tiles will be flagged with a 'can see dot <x,y>' sticker.
 *
 * Expansion will happen by (symmetrically opposite) pairs of squares; if
 * a square hasn't an opposite number there's no point trying to expand through
 * it. Empty tiles will therefore also be tagged in pairs.
 *
 * If an empty tile already has a 'can see dot <x,y>' tag from a previous dot,
 * it (and its partner) gets untagged (or, rather, a 'can see two dots' tag)
 * because we're looking for single-dot possibilities.
 *
 * Once we've gone through all the dots, any which still have a 'can see dot'
 * tag get associated with that dot (because it must have been the only one);
 * any without any tag (i.e. that could see _no_ dots) cause an impossibility
 * marked.
 *
 * The expansion will happen each time with a stored list of (space *) pairs,
 * rather than a mark-and-sweep idea; that's horrifically inefficient.
 *
 * expansion algorithm:
 *
 * * allocate list of (space *) the size of s->sx*s->sy.
 * * allocate second grid for (flags, dotx, doty) size of sx*sy.
 *
 * clear second grid (flags = 0, all dotx and doty = 0)
 * flags: F_REACHABLE, F_MULTIPLE
 *
 *
 * * for each dot, start with one pair of tiles that are associated with it --
 *   * vertex --> (dx+1, dy+1), (dx-1, dy-1)
 *   * edge --> (adj1, adj2)
 *   * tile --> (tile, tile) ???
 * * mark that pair of tiles with F_MARK, clear all other F_MARKs.
 * * add two tiles to start of list.
 *
 * set start = 0, end = next = 2
 *
 * from (start to end-1, step 2) {
 * * we have two tiles (t1, t2), opposites wrt our dot.
 * * for each (at1) sensible adjacent tile to t1 (i.e. not past an edge):
 *   * work out at2 as the opposite to at1
 *   * assert at1 and at2 have the same F_MARK values.
 *   * if at1 & F_MARK ignore it (we've been there on a previous sweep)
 *   * if either are associated with a different dot
 *     * mark both with F_MARK (so we ignore them later)
 *   * otherwise (assoc. with our dot, or empty):
 *     * mark both with F_MARK
 *     * add their space * values to the end of the list, set next += 2.
 * }
 *
 * if (end == next)
 * * we didn't add any new squares; exit the loop.
 * else
 * * set start = next+1, end = next. go round again
 *
 * We've finished expanding from the dot. Now, for each square we have
 * in our list (--> each square with F_MARK):
 * * if the tile is empty:
 *   * if F_REACHABLE was already set
 *     * set F_MULTIPLE
 *   * otherwise
 *     * set F_REACHABLE, set dotx and doty to our dot.
 *
 * Then, continue the whole thing for each dot in turn.
 *
 * Once we've done for each dot, go through the entire grid looking for
 * empty tiles: for each empty tile:
   * if F_REACHABLE and not F_MULTIPLE, set that dot (and its double)
   * if !F_REACHABLE, return as impossible.
 *
 */

/* Returns true if this tile is either already associated with this dot,
 * or blank. */
static bool solver_expand_checkdot(space *tile, space *dot)
{
    if (!(tile->flags & F_TILE_ASSOC)) return true;
    if (tile->dotx == dot->x && tile->doty == dot->y) return true;
    return false;
}

static void solver_expand_fromdot(game_state *state, space *dot, solver_ctx *sctx)
{
    int i, j, x, y, start, end, next;
    space *sp;

    /* Clear the grid of the (space) flags we'll use. */

    /* This is well optimised; analysis showed that:
        for (i = 0; i < sctx->sz; i++) state->grid[i].flags &= ~F_MARK;
       took up ~85% of the total function time! */
    for (y = 1; y < state->sy; y += 2) {
        sp = &SPACE(state, 1, y);
        for (x = 1; x < state->sx; x += 2, sp += 2)
            sp->flags &= ~F_MARK;
    }

    /* Seed the list of marked squares with two that must be associated
     * with our dot (possibly the same space) */
    if (dot->type == s_tile) {
        sctx->scratch[0] = sctx->scratch[1] = dot;
    } else if (dot->type == s_edge) {
        tiles_from_edge(state, dot, sctx->scratch);
    } else if (dot->type == s_vertex) {
        /* pick two of the opposite ones arbitrarily. */
        sctx->scratch[0] = &SPACE(state, dot->x-1, dot->y-1);
        sctx->scratch[1] = &SPACE(state, dot->x+1, dot->y+1);
    }

    sctx->scratch[0]->flags |= F_MARK;
    sctx->scratch[1]->flags |= F_MARK;

    start = 0; end = 2; next = 2;

expand:
    for (i = start; i < end; i += 2) {
        space *t1 = sctx->scratch[i]/*, *t2 = sctx->scratch[i+1]*/;
        space *edges[4], *tileadj[4], *tileadj2;

        adjacencies(state, t1, edges, tileadj);

        for (j = 0; j < 4; j++) {
            if (edges[j]->flags & F_EDGE_SET) continue;

            if (tileadj[j]->flags & F_MARK) continue; /* seen before. */

            /* We have a tile adjacent to t1; find its opposite. */
            tileadj2 = space_opposite_dot(state, tileadj[j], dot);
            if (!tileadj2) {
                tileadj[j]->flags |= F_MARK;
                continue; /* no opposite, so mark for next time. */
            }
            /* If the tile had an opposite we should have either seen both of
             * these, or neither of these, before. */

            if (solver_expand_checkdot(tileadj[j], dot) &&
                solver_expand_checkdot(tileadj2, dot)) {
                /* Both tiles could associate with this dot; add them to
                 * our list. */
                sctx->scratch[next++] = tileadj[j];
                sctx->scratch[next++] = tileadj2;
            }
            /* Either way, we've seen these tiles already so mark them. */
            tileadj[j]->flags |= F_MARK;
            tileadj2->flags |= F_MARK;
        }
    }
    if (next > end) {
        /* We added more squares; go back and try again. */
        start = end; end = next; goto expand;
    }

    /* We've expanded as far as we can go. Now we update the main flags
     * on all tiles we've expanded into -- if they were empty, we have
     * found possible associations for this dot. */
    for (i = 0; i < end; i++) {
        if (sctx->scratch[i]->flags & F_TILE_ASSOC) continue;
        if (sctx->scratch[i]->flags & F_REACHABLE) {
            /* This is (at least) the second dot this tile could
             * associate with. */
            sctx->scratch[i]->flags |= F_MULTIPLE;
        } else {
            /* This is the first (possibly only) dot. */
            sctx->scratch[i]->flags |= F_REACHABLE;
            sctx->scratch[i]->dotx = dot->x;
            sctx->scratch[i]->doty = dot->y;
        }
    }
}

static int solver_expand_postcb(game_state *state, space *tile, void *ctx)
{
    if (tile->flags & F_TILE_ASSOC) return 0;

    if (!(tile->flags & F_REACHABLE)) {
        return -1;
    }
    if (tile->flags & F_MULTIPLE) return 0;

    return solver_add_assoc(state, tile, tile->dotx, tile->doty,
                            "single possible dot after expansion");
}

static int solver_expand_dots(game_state *state, solver_ctx *sctx)
{
    int i;

    for (i = 0; i < sctx->sz; i++)
        state->grid[i].flags &= ~(F_REACHABLE|F_MULTIPLE);

    for (i = 0; i < state->ndots; i++)
        solver_expand_fromdot(state, state->dots[i], sctx);

    return foreach_tile(state, solver_expand_postcb, IMPOSSIBLE_QUITS, sctx);
}

struct recurse_ctx {
    space *best;
    int bestn;
};

static int solver_recurse_cb(game_state *state, space *tile, void *ctx)
{
    struct recurse_ctx *rctx = (struct recurse_ctx *)ctx;
    int i, n = 0;

    if (tile->flags & F_TILE_ASSOC) return 0;

    /* We're unassociated: count up all the dots we could associate with. */
    for (i = 0; i < state->ndots; i++) {
        if (dotfortile(state, tile, state->dots[i]))
            n++;
    }
    if (n > rctx->bestn) {
        rctx->bestn = n;
        rctx->best = tile;
    }
    return 0;
}

#define MAXRECURSE 5

static int solver_recurse(game_state *state, int maxdiff)
{
    int diff = DIFF_IMPOSSIBLE, ret, n, gsz = state->sx * state->sy;
    space *ingrid, *outgrid = NULL, *bestopp;
    struct recurse_ctx rctx;

    if (solver_recurse_depth >= MAXRECURSE) {
        return DIFF_UNFINISHED;
    }

    /* Work out the cell to recurse on; go through all unassociated tiles
     * and find which one has the most possible dots it could associate
     * with. */
    rctx.best = NULL;
    rctx.bestn = 0;

    foreach_tile(state, solver_recurse_cb, 0, &rctx);
    if (rctx.bestn == 0) return DIFF_IMPOSSIBLE;

    ingrid = snewn(gsz, space);
    memcpy(ingrid, state->grid, gsz * sizeof(space));

    for (n = 0; n < state->ndots; n++) {
        memcpy(state->grid, ingrid, gsz * sizeof(space));

        if (!dotfortile(state, rctx.best, state->dots[n])) continue;

        /* set cell (temporarily) pointing to that dot. */
        solver_add_assoc(state, rctx.best,
                         state->dots[n]->x, state->dots[n]->y,
                         "Attempting for recursion");

        ret = solver_state(state, maxdiff);

        if (diff == DIFF_IMPOSSIBLE && ret != DIFF_IMPOSSIBLE) {
            /* we found our first solved grid; copy it away. */
            outgrid = snewn(gsz, space);
            memcpy(outgrid, state->grid, gsz * sizeof(space));
        }
        /* reset cell back to unassociated. */
        bestopp = tile_opposite(state, rctx.best);

        remove_assoc(state, rctx.best);
        remove_assoc(state, bestopp);

        if (ret == DIFF_AMBIGUOUS || ret == DIFF_UNFINISHED)
            diff = ret;
        else if (ret == DIFF_IMPOSSIBLE)
            /* no change */;
        else {
            /* precisely one solution */
            if (diff == DIFF_IMPOSSIBLE)
                diff = DIFF_UNREASONABLE;
            else
                diff = DIFF_AMBIGUOUS;
        }
        /* if we've found >1 solution, or ran out of recursion,
         * give up immediately. */
        if (diff == DIFF_AMBIGUOUS || diff == DIFF_UNFINISHED)
            break;
    }

    if (outgrid) {
        /* we found (at least one) soln; copy it back to state */
        memcpy(state->grid, outgrid, gsz * sizeof(space));
        sfree(outgrid);
    }
    sfree(ingrid);
    return diff;
}

static int solver_state(game_state *state, int maxdiff)
{
    solver_ctx *sctx = new_solver(state);
    int ret, diff = DIFF_NORMAL;

    ret = solver_obvious(state);
    if (ret < 0) {
        diff = DIFF_IMPOSSIBLE;
        goto got_result;
    }

#define CHECKRET(d) do {                                        \
    if (ret < 0) { diff = DIFF_IMPOSSIBLE; goto got_result; }   \
    if (ret > 0) { diff = max(diff, (d)); goto cont; }          \
} while(0)

    while (1) {
cont:
        ret = foreach_edge(state, solver_lines_opposite_cb,
                           IMPOSSIBLE_QUITS, sctx);
        CHECKRET(DIFF_NORMAL);

        ret = foreach_tile(state, solver_spaces_oneposs_cb,
                           IMPOSSIBLE_QUITS, sctx);
        CHECKRET(DIFF_NORMAL);

        ret = solver_expand_dots(state, sctx);
        CHECKRET(DIFF_NORMAL);

        if (maxdiff <= DIFF_NORMAL)
            break;

        /* harder still? */

        /* if we reach here, we've made no deductions, so we terminate. */
        break;
    }

    if (check_complete(state, NULL, NULL)) goto got_result;

    diff = (maxdiff >= DIFF_UNREASONABLE) ?
        solver_recurse(state, maxdiff) : DIFF_UNFINISHED;

got_result:
    free_solver(sctx);

    return diff;
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
    game_state *tosolve;
    char *ret;
    int i;
    int diff;

    if (aux) {
        tosolve = execute_move(state, aux);
         goto solved;
    } else {
        tosolve = dup_game(currstate);
        diff = solver_state(tosolve, DIFF_UNREASONABLE);
        if (diff != DIFF_UNFINISHED && diff != DIFF_IMPOSSIBLE) {
            goto solved;
        }
        free_game(tosolve);

        tosolve = dup_game(state);
        diff = solver_state(tosolve, DIFF_UNREASONABLE);
        if (diff != DIFF_UNFINISHED && diff != DIFF_IMPOSSIBLE) {
            goto solved;
        }
        free_game(tosolve);
    }

    return NULL;

solved:
    /*
     * Clear tile associations: the solution will only include the
     * edges.
     */
    for (i = 0; i < tosolve->sx*tosolve->sy; i++)
        tosolve->grid[i].flags &= ~F_TILE_ASSOC;
    ret = diff_game(currstate, tosolve, true);
    free_game(tosolve);
    return ret;
}

/* ----------------------------------------------------------
 * User interface.
 */

struct game_ui {
    bool dragging;
    int dx, dy;         /* pixel coords of drag pos. */
    int dotx, doty;     /* grid coords of dot we're dragging from. */
    int srcx, srcy;     /* grid coords of drag start */
    int cur_x, cur_y;
    bool cur_visible;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);
    ui->dragging = false;
    ui->cur_x = ui->cur_y = 1;
    ui->cur_visible = false;
    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static char *encode_ui(const game_ui *ui)
{
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding)
{
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

#define PREFERRED_TILE_SIZE 32
#define TILE_SIZE (ds->tilesize)
#define DOT_SIZE        (TILE_SIZE / 4)
#define EDGE_THICKNESS (max(TILE_SIZE / 16, 2))
#define BORDER (TILE_SIZE / 2)

#define COORD(x) ( (x) * TILE_SIZE + BORDER )
#define SCOORD(x) ( ((x) * TILE_SIZE)/2 + BORDER )
#define FROMCOORD(x) ( ((x) - BORDER) / TILE_SIZE )

#define DRAW_WIDTH      (BORDER * 2 + ds->w * TILE_SIZE)
#define DRAW_HEIGHT     (BORDER * 2 + ds->h * TILE_SIZE)

#define CURSOR_SIZE DOT_SIZE

struct game_drawstate {
    bool started;
    int w, h;
    int tilesize;
    unsigned long *grid;
    int *dx, *dy;
    blitter *bl;
    blitter *blmirror;

    bool dragging;
    int dragx, dragy, oppx, oppy;

    int *colour_scratch;

    int cx, cy;
    bool cur_visible;
    blitter *cur_bl;
};

#define CORNER_TOLERANCE 0.15F
#define CENTRE_TOLERANCE 0.15F

/*
 * Round FP coordinates to the centre of the nearest edge.
 */
static void coord_round_to_edge(float x, float y, int *xr, int *yr)
{
    float xs, ys, xv, yv, dx, dy;

    /*
     * Find the nearest square-centre.
     */
    xs = (float)floor(x) + 0.5F;
    ys = (float)floor(y) + 0.5F;

    /*
     * Find the nearest grid vertex.
     */
    xv = (float)floor(x + 0.5F);
    yv = (float)floor(y + 0.5F);

    /*
     * Determine whether the horizontal or vertical edge from that
     * vertex alongside that square is closer to us, by comparing
     * distances from the square cente.
     */
    dx = (float)fabs(x - xs);
    dy = (float)fabs(y - ys);
    if (dx > dy) {
        /* Vertical edge: x-coord of corner,
         * y-coord of square centre. */
        *xr = 2 * (int)xv;
        *yr = 1 + 2 * (int)floor(ys);
    } else {
        /* Horizontal edge: x-coord of square centre,
         * y-coord of corner. */
        *xr = 1 + 2 * (int)floor(xs);
        *yr = 2 * (int)yv;
    }
}

static bool edge_placement_legal(const game_state *state, int x, int y)
{
    space *sp = &SPACE(state, x, y);
    if (sp->type != s_edge)
        return false;   /* this is a face-centre or a grid vertex */

    /* Check this line doesn't actually intersect a dot */
    unsigned int flags = (GRID(state, grid, x, y).flags |
                          GRID(state, grid, x & ~1U, y & ~1U).flags |
                          GRID(state, grid, (x+1) & ~1U, (y+1) & ~1U).flags);
    if (flags & F_DOT)
        return false;
    return true;
}

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button, bool swapped)
{
    /* UI operations (play mode):
     *
     * Toggle edge (set/unset) (left-click on edge)
     * Associate space with dot (left-drag from dot)
     * Unassociate space (left-drag from space off grid)
     * Autofill lines around shape? (right-click?)
     *
     * (edit mode; will clear all lines/associations)
     *
     * Add or remove dot (left-click)
     */
    char buf[80];
    const char *sep = "";
    int px, py;
    space *sp, *dot;

    buf[0] = '\0';

    if (button == 'H' || button == 'h') {
        char *ret;
        game_state *tmp = dup_game(state);
        solver_obvious(tmp);
        ret = diff_game(state, tmp, false);
        free_game(tmp);
        return ret;
    }

    if (button == LEFT_BUTTON) {
        ui->cur_visible = false;
        coord_round_to_edge(FROMCOORD((float)x), FROMCOORD((float)y),
                            &px, &py);

        if (!INUI(state, px, py)) return NULL;
        if (!edge_placement_legal(state, px, py))
            return NULL;

        sprintf(buf, "E%d,%d", px, py);
        return dupstr(buf);
    } else if (button == RIGHT_BUTTON) {
        int px1, py1;

        ui->cur_visible = false;

        px = (int)(2*FROMCOORD((float)x) + 0.5);
        py = (int)(2*FROMCOORD((float)y) + 0.5);

        dot = NULL;

        /*
         * If there's a dot anywhere nearby, we pick up an arrow
         * pointing at that dot.
         */
        for (py1 = py-1; py1 <= py+1; py1++)
            for (px1 = px-1; px1 <= px+1; px1++) {
                if (px1 >= 0 && px1 < state->sx &&
                    py1 >= 0 && py1 < state->sy &&
                    x >= SCOORD(px1-1) && x < SCOORD(px1+1) &&
                    y >= SCOORD(py1-1) && y < SCOORD(py1+1) &&
                    SPACE(state, px1, py1).flags & F_DOT) {
                    /*
                     * Found a dot. Begin a drag from it.
                     */
                    dot = &SPACE(state, px1, py1);
                    ui->srcx = px1;
                    ui->srcy = py1;
                    goto done;         /* multi-level break */
                }
            }

        /*
         * Otherwise, find the nearest _square_, and pick up the
         * same arrow as it's got on it, if any.
         */
        if (!dot) {
            px = 2*FROMCOORD(x+TILE_SIZE) - 1;
            py = 2*FROMCOORD(y+TILE_SIZE) - 1;
            if (px >= 0 && px < state->sx && py >= 0 && py < state->sy) {
                sp = &SPACE(state, px, py);
                if (sp->flags & F_TILE_ASSOC) {
                    dot = &SPACE(state, sp->dotx, sp->doty);
                    ui->srcx = px;
                    ui->srcy = py;
                }
            }
        }

        done:
        /*
         * Now, if we've managed to find a dot, begin a drag.
         */
        if (dot) {
            ui->dragging = true;
            ui->dx = x;
            ui->dy = y;
            ui->dotx = dot->x;
            ui->doty = dot->y;
            return UI_UPDATE;
        }
    } else if (button == RIGHT_DRAG && ui->dragging) {
        /* just move the drag coords. */
        ui->dx = x;
        ui->dy = y;
        return UI_UPDATE;
    } else if (button == RIGHT_RELEASE && ui->dragging) {
        ui->dragging = false;

        /*
         * Drags are always targeted at a single square.
         */
        px = 2*FROMCOORD(x+TILE_SIZE) - 1;
        py = 2*FROMCOORD(y+TILE_SIZE) - 1;

	/*
	 * Dragging an arrow on to the same square it started from
	 * is a null move; just update the ui and finish.
	 */
	if (px == ui->srcx && py == ui->srcy)
	    return UI_UPDATE;

	/*
	 * Otherwise, we remove the arrow from its starting
	 * square if we didn't start from a dot...
	 */
	if ((ui->srcx != ui->dotx || ui->srcy != ui->doty) &&
	    SPACE(state, ui->srcx, ui->srcy).flags & F_TILE_ASSOC) {
	    sprintf(buf + strlen(buf), "%sU%d,%d", sep, ui->srcx, ui->srcy);
	    sep = ";";
	}

	/*
	 * ... and if the square we're moving it _to_ is valid, we
	 * add one there instead.
	 */
        if (INUI(state, px, py)) {
            sp = &SPACE(state, px, py);
            dot = &SPACE(state, ui->dotx, ui->doty);

            /*
             * Exception: if it's not actually legal to add an arrow
             * and its opposite at this position, we don't try,
             * because otherwise we'd append an empty entry to the
             * undo chain.
             */
            if (ok_to_add_assoc_with_opposite(state, sp, dot))
		sprintf(buf + strlen(buf), "%sA%d,%d,%d,%d",
			sep, px, py, ui->dotx, ui->doty);
	}

	if (buf[0])
	    return dupstr(buf);
	else
	    return UI_UPDATE;
    } else if (IS_CURSOR_MOVE(button)) {
        move_cursor(button, &ui->cur_x, &ui->cur_y, state->sx-1, state->sy-1, false);
        if (ui->cur_x < 1) ui->cur_x = 1;
        if (ui->cur_y < 1) ui->cur_y = 1;
        ui->cur_visible = true;
        if (ui->dragging) {
            ui->dx = SCOORD(ui->cur_x);
            ui->dy = SCOORD(ui->cur_y);
        }
        return UI_UPDATE;
    } else if (IS_CURSOR_SELECT(button)) {
        if (!ui->cur_visible) {
            ui->cur_visible = true;
            return UI_UPDATE;
        }
        sp = &SPACE(state, ui->cur_x, ui->cur_y);
        if (ui->dragging) {
            ui->dragging = false;

            if ((ui->srcx != ui->dotx || ui->srcy != ui->doty) &&
                SPACE(state, ui->srcx, ui->srcy).flags & F_TILE_ASSOC) {
                sprintf(buf, "%sU%d,%d", sep, ui->srcx, ui->srcy);
                sep = ";";
            }
            if (sp->type == s_tile && !(sp->flags & F_DOT) && !(sp->flags & F_TILE_ASSOC)) {
                sprintf(buf + strlen(buf), "%sA%d,%d,%d,%d",
                        sep, ui->cur_x, ui->cur_y, ui->dotx, ui->doty);
            }
            return dupstr(buf);
        } else if (sp->flags & F_DOT) {
            ui->dragging = true;
            ui->dx = SCOORD(ui->cur_x);
            ui->dy = SCOORD(ui->cur_y);
            ui->dotx = ui->srcx = ui->cur_x;
            ui->doty = ui->srcy = ui->cur_y;
            return UI_UPDATE;
        } else if (sp->flags & F_TILE_ASSOC) {
            ui->dragging = true;
            ui->dx = SCOORD(ui->cur_x);
            ui->dy = SCOORD(ui->cur_y);
            ui->dotx = sp->dotx;
            ui->doty = sp->doty;
            ui->srcx = ui->cur_x;
            ui->srcy = ui->cur_y;
            return UI_UPDATE;
        } else if (sp->type == s_edge &&
                   edge_placement_legal(state, ui->cur_x, ui->cur_y)) {
            sprintf(buf, "E%d,%d", ui->cur_x, ui->cur_y);
            return dupstr(buf);
        }
    }

    return NULL;
}

static bool check_complete(const game_state *state, int *dsf, int *colours)
{
    int w = state->w, h = state->h;
    int x, y, i;
    bool ret;

    bool free_dsf;
    struct sqdata {
        int minx, miny, maxx, maxy;
        int cx, cy;
        bool valid;
        int colour;
    } *sqdata;

    if (!dsf) {
        dsf = snew_dsf(w*h);
        free_dsf = true;
    } else {
        dsf_init(dsf, w*h);
        free_dsf = false;
    }

    /*
     * During actual game play, completion checking is done on the
     * basis of the edges rather than the square associations. So
     * first we must go through the grid figuring out the connected
     * components into which the edges divide it.
     */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            if (y+1 < h && !(SPACE(state, 2*x+1, 2*y+2).flags & F_EDGE_SET))
                dsf_merge(dsf, y*w+x, (y+1)*w+x);
            if (x+1 < w && !(SPACE(state, 2*x+2, 2*y+1).flags & F_EDGE_SET))
                dsf_merge(dsf, y*w+x, y*w+(x+1));
        }

    /*
     * That gives us our connected components. Now, for each
     * component, decide whether it's _valid_. A valid component is
     * one which:
     *
     *  - is 180-degree rotationally symmetric
     *  - has a dot at its centre of symmetry
     *  - has no other dots anywhere within it (including on its
     *    boundary)
     *  - contains no internal edges (i.e. edges separating two
     *    squares which are both part of the component).
     */

    /*
     * First, go through the grid finding the bounding box of each
     * component.
     */
    sqdata = snewn(w*h, struct sqdata);
    for (i = 0; i < w*h; i++) {
        sqdata[i].minx = w+1;
        sqdata[i].miny = h+1;
        sqdata[i].maxx = sqdata[i].maxy = -1;
        sqdata[i].valid = false;
    }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            i = dsf_canonify(dsf, y*w+x);
            if (sqdata[i].minx > x)
                sqdata[i].minx = x;
            if (sqdata[i].maxx < x)
                sqdata[i].maxx = x;
            if (sqdata[i].miny > y)
                sqdata[i].miny = y;
            if (sqdata[i].maxy < y)
                sqdata[i].maxy = y;
            sqdata[i].valid = true;
        }

    /*
     * Now we're in a position to loop over each actual component
     * and figure out where its centre of symmetry has to be if
     * it's anywhere.
     */
    for (i = 0; i < w*h; i++)
        if (sqdata[i].valid) {
            int cx, cy;
            cx = sqdata[i].cx = sqdata[i].minx + sqdata[i].maxx + 1;
            cy = sqdata[i].cy = sqdata[i].miny + sqdata[i].maxy + 1;
            if (!(SPACE(state, sqdata[i].cx, sqdata[i].cy).flags & F_DOT))
                sqdata[i].valid = false;   /* no dot at centre of symmetry */
            if (dsf_canonify(dsf, (cy-1)/2*w+(cx-1)/2) != i ||
                dsf_canonify(dsf, (cy)/2*w+(cx-1)/2) != i ||
                dsf_canonify(dsf, (cy-1)/2*w+(cx)/2) != i ||
                dsf_canonify(dsf, (cy)/2*w+(cx)/2) != i)
                sqdata[i].valid = false;   /* dot at cx,cy isn't ours */
            if (SPACE(state, sqdata[i].cx, sqdata[i].cy).flags & F_DOT_BLACK)
                sqdata[i].colour = 2;
            else
                sqdata[i].colour = 1;
        }

    /*
     * Now we loop over the whole grid again, this time finding
     * extraneous dots (any dot which wholly or partially overlaps
     * a square and is not at the centre of symmetry of that
     * square's component disqualifies the component from validity)
     * and extraneous edges (any edge separating two squares
     * belonging to the same component also disqualifies that
     * component).
     */
    for (y = 1; y < state->sy-1; y++)
        for (x = 1; x < state->sx-1; x++) {
            space *sp = &SPACE(state, x, y);

            if (sp->flags & F_DOT) {
                /*
                 * There's a dot here. Use it to disqualify any
                 * component which deserves it.
                 */
                int cx, cy;
                for (cy = (y-1) >> 1; cy <= y >> 1; cy++)
                    for (cx = (x-1) >> 1; cx <= x >> 1; cx++) {
                        i = dsf_canonify(dsf, cy*w+cx);
                        if (x != sqdata[i].cx || y != sqdata[i].cy)
                            sqdata[i].valid = false;
                    }
            }

            if (sp->flags & F_EDGE_SET) {
                /*
                 * There's an edge here. Use it to disqualify a
                 * component if necessary.
                 */
                int cx1 = (x-1) >> 1, cx2 = x >> 1;
                int cy1 = (y-1) >> 1, cy2 = y >> 1;
                i = dsf_canonify(dsf, cy1*w+cx1);
                if (i == dsf_canonify(dsf, cy2*w+cx2))
                    sqdata[i].valid = false;
            }
        }

    /*
     * And finally we test rotational symmetry: for each square in
     * the grid, find which component it's in, test that that
     * component also has a square in the symmetric position, and
     * disqualify it if it doesn't.
     */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int x2, y2;

            i = dsf_canonify(dsf, y*w+x);

            x2 = sqdata[i].cx - 1 - x;
            y2 = sqdata[i].cy - 1 - y;
            if (i != dsf_canonify(dsf, y2*w+x2))
                sqdata[i].valid = false;
        }

    /*
     * That's it. We now have all the connected components marked
     * as valid or not valid. So now we return a `colours' array if
     * we were asked for one, and also we return an overall
     * true/false value depending on whether _every_ square in the
     * grid is part of a valid component.
     */
    ret = true;
    for (i = 0; i < w*h; i++) {
        int ci = dsf_canonify(dsf, i);
        bool thisok = sqdata[ci].valid;
        if (colours)
            colours[i] = thisok ? sqdata[ci].colour : 0;
        ret = ret && thisok;
    }

    sfree(sqdata);
    if (free_dsf)
	sfree(dsf);

    return ret;
}

static game_state *execute_move(const game_state *state, const char *move)
{
    int x, y, ax, ay, n, dx, dy;
    game_state *ret = dup_game(state);
    space *sp, *dot;
    bool currently_solving = false;

    while (*move) {
        char c = *move;
        if (c == 'E' || c == 'U' || c == 'M') {
            move++;
            if (sscanf(move, "%d,%d%n", &x, &y, &n) != 2 ||
                !INUI(ret, x, y))
                goto badmove;

            sp = &SPACE(ret, x, y);
            if (c == 'E') {
                if (sp->type != s_edge) goto badmove;
                sp->flags ^= F_EDGE_SET;
            } else if (c == 'U') {
                if (sp->type != s_tile || !(sp->flags & F_TILE_ASSOC))
                    goto badmove;
                /* The solver doesn't assume we'll mirror things */
                if (currently_solving)
                    remove_assoc(ret, sp);
                else
                    remove_assoc_with_opposite(ret, sp);
            } else if (c == 'M') {
                if (!(sp->flags & F_DOT)) goto badmove;
                sp->flags ^= F_DOT_HOLD;
            }
            move += n;
        } else if (c == 'A' || c == 'a') {
            move++;
            if (sscanf(move, "%d,%d,%d,%d%n", &x, &y, &ax, &ay, &n) != 4 ||
                x < 1 || y < 1 || x >= (ret->sx-1) || y >= (ret->sy-1) ||
                ax < 1 || ay < 1 || ax >= (ret->sx-1) || ay >= (ret->sy-1))
                goto badmove;

            dot = &GRID(ret, grid, ax, ay);
            if (!(dot->flags & F_DOT))goto badmove;
            if (dot->flags & F_DOT_HOLD) goto badmove;

            for (dx = -1; dx <= 1; dx++) {
                for (dy = -1; dy <= 1; dy++) {
                    sp = &GRID(ret, grid, x+dx, y+dy);
                    if (sp->type != s_tile) continue;
                    if (sp->flags & F_TILE_ASSOC) {
                        space *dot = &SPACE(ret, sp->dotx, sp->doty);
                        if (dot->flags & F_DOT_HOLD) continue;
                    }
                    /* The solver doesn't assume we'll mirror things */
                    if (currently_solving)
                        add_assoc(ret, sp, dot);
                    else
                        add_assoc_with_opposite(ret, sp, dot);
                }
            }
            move += n;
        } else if (c == 'S') {
            move++;
            ret->used_solve = true;
            currently_solving = true;
        } else
            goto badmove;

        if (*move == ';')
            move++;
        else if (*move)
            goto badmove;
    }
    if (check_complete(ret, NULL, NULL))
        ret->completed = true;
    return ret;

badmove:
    free_game(ret);
    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

/* Lines will be much smaller size than squares; say, 1/8 the size?
 *
 * Need a 'top-left corner of location XxY' to take this into account;
 * alternaticaly, that could give the middle of that location, and the
 * drawing code would just know the expected dimensions.
 *
 * We also need something to take a click and work out what it was
 * we were interested in. Clicking on vertices is required because
 * we may want to drag from them, for example.
 */

static void game_compute_size(const game_params *params, int sz,
                              int *x, int *y)
{
    struct { int tilesize, w, h; } ads, *ds = &ads;

    ds->tilesize = sz;
    ds->w = params->w;
    ds->h = params->h;

    *x = DRAW_WIDTH;
    *y = DRAW_HEIGHT;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int sz)
{
    ds->tilesize = sz;

    ds->bl = blitter_new(dr, TILE_SIZE, TILE_SIZE);
    ds->blmirror = blitter_new(dr, TILE_SIZE, TILE_SIZE);
    ds->cur_bl = blitter_new(dr, TILE_SIZE, TILE_SIZE);
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);
    int i;

    for (i = 0; i < 3; i++) {
        ret[COL_BACKGROUND * 3 + i] = 1.0F;

        /*
         * Currently, white dots and white-background squares are
         * both pure white.
         */
        ret[COL_WHITEDOT * 3 + i] = 1.0F;
        ret[COL_WHITEBG * 3 + i] = 0.75F;

        /*
         * But black-background squares are a dark grey, whereas
         * black dots are really black.
         */
        ret[COL_BLACKDOT * 3 + i] = 0.0F;
        ret[COL_BLACKBG * 3 + i] = 0.25F;

        /*
         * In unfilled squares, we draw a faint gridwork.
         */
        ret[COL_GRID * 3 + i] = 0.0F;

        /*
         * Edges and arrows are filled in in pure black.
         */
        ret[COL_EDGE * 3 + i] = 0.0F;
        ret[COL_ARROW * 3 + i] = 0.0F;

        ret[COL_CURSOR * 3 + i] = 0.75F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int i;

    ds->started = false;
    ds->w = state->w;
    ds->h = state->h;

    ds->grid = snewn(ds->w*ds->h, unsigned long);
    for (i = 0; i < ds->w*ds->h; i++)
        ds->grid[i] = 0xFFFFFFFFUL;
    ds->dx = snewn(ds->w*ds->h, int);
    ds->dy = snewn(ds->w*ds->h, int);

    ds->bl = NULL;
    ds->blmirror = NULL;
    ds->dragging = false;
    ds->dragx = ds->dragy = ds->oppx = ds->oppy = 0;

    ds->colour_scratch = snewn(ds->w * ds->h, int);

    ds->cur_bl = NULL;
    ds->cx = ds->cy = 0;
    ds->cur_visible = false;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    if (ds->cur_bl) blitter_free(dr, ds->cur_bl);
    sfree(ds->colour_scratch);
    if (ds->blmirror) blitter_free(dr, ds->blmirror);
    if (ds->bl) blitter_free(dr, ds->bl);
    sfree(ds->dx);
    sfree(ds->dy);
    sfree(ds->grid);
    sfree(ds);
}

#define DRAW_EDGE_L    0x0001
#define DRAW_EDGE_R    0x0002
#define DRAW_EDGE_U    0x0004
#define DRAW_EDGE_D    0x0008
#define DRAW_CORNER_UL 0x0010
#define DRAW_CORNER_UR 0x0020
#define DRAW_CORNER_DL 0x0040
#define DRAW_CORNER_DR 0x0080
#define DRAW_WHITE     0x0100
#define DRAW_BLACK     0x0200
#define DRAW_ARROW     0x0400
#define DRAW_CURSOR    0x0800
#define DOT_SHIFT_C    12
#define DOT_SHIFT_M    2
#define DOT_WHITE      1UL
#define DOT_BLACK      2UL

/*
 * Draw an arrow centred on (cx,cy), pointing in the direction
 * (ddx,ddy). (I.e. pointing at the point (cx+ddx, cy+ddy).
 */
static void draw_arrow(drawing *dr, game_drawstate *ds,
                       int cx, int cy, int ddx, int ddy, int col)
{
    int sqdist = ddx*ddx+ddy*ddy;
    if (sqdist == 0)
        return;                        /* avoid division by zero */
    float vlen = (float)sqrt(sqdist);
    float xdx = ddx/vlen, xdy = ddy/vlen;
    float ydx = -xdy, ydy = xdx;
    int e1x = cx + (int)(xdx*TILE_SIZE/3), e1y = cy + (int)(xdy*TILE_SIZE/3);
    int e2x = cx - (int)(xdx*TILE_SIZE/3), e2y = cy - (int)(xdy*TILE_SIZE/3);
    int adx = (int)((ydx-xdx)*TILE_SIZE/8), ady = (int)((ydy-xdy)*TILE_SIZE/8);
    int adx2 = (int)((-ydx-xdx)*TILE_SIZE/8), ady2 = (int)((-ydy-xdy)*TILE_SIZE/8);

    draw_line(dr, e1x, e1y, e2x, e2y, col);
    draw_line(dr, e1x, e1y, e1x+adx, e1y+ady, col);
    draw_line(dr, e1x, e1y, e1x+adx2, e1y+ady2, col);
}

static void draw_square(drawing *dr, game_drawstate *ds, int x, int y,
                        unsigned long flags, int ddx, int ddy)
{
    int lx = COORD(x), ly = COORD(y);
    int dx, dy;
    int gridcol;

    clip(dr, lx, ly, TILE_SIZE, TILE_SIZE);

    /*
     * Draw the tile background.
     */
    draw_rect(dr, lx, ly, TILE_SIZE, TILE_SIZE,
              (flags & DRAW_WHITE ? COL_WHITEBG :
               flags & DRAW_BLACK ? COL_BLACKBG : COL_BACKGROUND));

    /*
     * Draw the grid.
     */
    gridcol = (flags & DRAW_BLACK ? COL_BLACKDOT : COL_GRID);
    draw_rect(dr, lx-2, ly, 5, TILE_SIZE, gridcol);
    draw_rect(dr, lx, ly-2, TILE_SIZE, 5, gridcol);

    /*
     * Draw the arrow, if present, or the cursor, if here.
     */
    if (flags & DRAW_ARROW)
        draw_arrow(dr, ds, lx + TILE_SIZE/2, ly + TILE_SIZE/2, ddx, ddy,
                   (flags & DRAW_CURSOR) ? COL_CURSOR : COL_ARROW);
    else if (flags & DRAW_CURSOR)
        draw_rect_outline(dr,
                          lx + TILE_SIZE/2 - CURSOR_SIZE,
                          ly + TILE_SIZE/2 - CURSOR_SIZE,
                          2*CURSOR_SIZE+1, 2*CURSOR_SIZE+1,
                          COL_CURSOR);

    /*
     * Draw the edges.
     */
    if (flags & DRAW_EDGE_L)
        draw_rect(dr, lx, ly, EDGE_THICKNESS, TILE_SIZE, COL_EDGE);
    if (flags & DRAW_EDGE_R)
        draw_rect(dr, lx + TILE_SIZE - EDGE_THICKNESS + 1, ly,
                  EDGE_THICKNESS - 1, TILE_SIZE, COL_EDGE);
    if (flags & DRAW_EDGE_U)
        draw_rect(dr, lx, ly, TILE_SIZE, EDGE_THICKNESS, COL_EDGE);
    if (flags & DRAW_EDGE_D)
        draw_rect(dr, lx, ly + TILE_SIZE - EDGE_THICKNESS + 1,
                  TILE_SIZE, EDGE_THICKNESS - 1, COL_EDGE);
    if (flags & DRAW_CORNER_UL)
        draw_rect(dr, lx, ly, EDGE_THICKNESS, EDGE_THICKNESS, COL_EDGE);
    if (flags & DRAW_CORNER_UR)
        draw_rect(dr, lx + TILE_SIZE - EDGE_THICKNESS + 1, ly,
                  EDGE_THICKNESS - 1, EDGE_THICKNESS, COL_EDGE);
    if (flags & DRAW_CORNER_DL)
        draw_rect(dr, lx, ly + TILE_SIZE - EDGE_THICKNESS + 1,
                  EDGE_THICKNESS, EDGE_THICKNESS - 1, COL_EDGE);
    if (flags & DRAW_CORNER_DR)
        draw_rect(dr, lx + TILE_SIZE - EDGE_THICKNESS + 1,
                  ly + TILE_SIZE - EDGE_THICKNESS + 1,
                  EDGE_THICKNESS - 1, EDGE_THICKNESS - 1, COL_EDGE);

    /*
     * Draw the dots.
     */
    for (dy = 0; dy < 3; dy++)
        for (dx = 0; dx < 3; dx++) {
            int dotval = (flags >> (DOT_SHIFT_C + DOT_SHIFT_M*(dy*3+dx)));
            dotval &= (1 << DOT_SHIFT_M)-1;

            if (dotval) {
                draw_circle(dr, lx+dx*TILE_SIZE/2, ly+dy*TILE_SIZE/2,
                            DOT_SIZE, COL_BLACKDOT, COL_BLACKDOT);
                if (dotval == 1)
                draw_circle(dr, lx+dx*TILE_SIZE/2, ly+dy*TILE_SIZE/2,
                            DOT_SIZE-7, COL_WHITEDOT, COL_BLACKDOT);
            }
        }

    unclip(dr);
    draw_update(dr, lx, ly, TILE_SIZE, TILE_SIZE);
}

static void calculate_opposite_point(const game_ui *ui,
                                     const game_drawstate *ds, const int x,
                                     const int y, int *oppositex,
                                     int *oppositey)
{
    /* oppositex - dotx = dotx - x <=> oppositex = 2 * dotx - x */
    *oppositex = 2 * SCOORD(ui->dotx) - x;
    *oppositey = 2 * SCOORD(ui->doty) - y;
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int w = ds->w, h = ds->h;
    int x, y;

    if (ds->dragging) {
        blitter_load(dr, ds->blmirror, ds->oppx, ds->oppy);
        draw_update(dr, ds->oppx, ds->oppy, TILE_SIZE, TILE_SIZE);
        blitter_load(dr, ds->bl, ds->dragx, ds->dragy);
        draw_update(dr, ds->dragx, ds->dragy, TILE_SIZE, TILE_SIZE);
        ds->dragging = false;
    }
    if (ds->cur_visible) {
        blitter_load(dr, ds->cur_bl, ds->cx, ds->cy);
        draw_update(dr, ds->cx, ds->cy, CURSOR_SIZE*2+1, CURSOR_SIZE*2+1);
        ds->cur_visible = false;
    }

    if (!ds->started) {
        draw_rect(dr, BORDER - EDGE_THICKNESS + 1, BORDER - EDGE_THICKNESS + 1,
                  w*TILE_SIZE + EDGE_THICKNESS*2 - 1,
                  h*TILE_SIZE + EDGE_THICKNESS*2 - 1, COL_EDGE);
        draw_update(dr, 0, 0, DRAW_WIDTH, DRAW_HEIGHT);
        ds->started = true;
    }

    check_complete(state, NULL, ds->colour_scratch);

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            unsigned long flags = 0;
            int ddx = 0, ddy = 0;
            space *sp, *opp;
            int dx, dy;

            /*
             * Set up the flags for this square. Firstly, see if we
             * have edges.
             */
            if (SPACE(state, x*2, y*2+1).flags & F_EDGE_SET)
                flags |= DRAW_EDGE_L;
            if (SPACE(state, x*2+2, y*2+1).flags & F_EDGE_SET)
                flags |= DRAW_EDGE_R;
            if (SPACE(state, x*2+1, y*2).flags & F_EDGE_SET)
                flags |= DRAW_EDGE_U;
            if (SPACE(state, x*2+1, y*2+2).flags & F_EDGE_SET)
                flags |= DRAW_EDGE_D;

            /*
             * Also, mark corners of neighbouring edges.
             */
            if ((x > 0 && SPACE(state, x*2-1, y*2).flags & F_EDGE_SET) ||
                (y > 0 && SPACE(state, x*2, y*2-1).flags & F_EDGE_SET))
                flags |= DRAW_CORNER_UL;
            if ((x+1 < w && SPACE(state, x*2+3, y*2).flags & F_EDGE_SET) ||
                (y > 0 && SPACE(state, x*2+2, y*2-1).flags & F_EDGE_SET))
                flags |= DRAW_CORNER_UR;
            if ((x > 0 && SPACE(state, x*2-1, y*2+2).flags & F_EDGE_SET) ||
                (y+1 < h && SPACE(state, x*2, y*2+3).flags & F_EDGE_SET))
                flags |= DRAW_CORNER_DL;
            if ((x+1 < w && SPACE(state, x*2+3, y*2+2).flags & F_EDGE_SET) ||
                (y+1 < h && SPACE(state, x*2+2, y*2+3).flags & F_EDGE_SET))
                flags |= DRAW_CORNER_DR;

            /*
             * If this square is part of a valid region, paint it
             * that region's colour. Exception: if we're flashing,
             * everything goes briefly back to background colour.
             */
            sp = &SPACE(state, x*2+1, y*2+1);
            if (sp->flags & F_TILE_ASSOC) {
                opp = tile_opposite(state, sp);
            } else {
                opp = NULL;
            }
            if (ds->colour_scratch[y*w+x]) {
                flags |= (ds->colour_scratch[y*w+x] == 2 ?
                          DRAW_BLACK : DRAW_WHITE);
            }

            /*
             * If this square is associated with a dot but it isn't
             * part of a valid region, draw an arrow in it pointing
             * in the direction of that dot.
	     * 
	     * Exception: if this is the source point of an active
	     * drag, we don't draw the arrow.
             */
            if ((sp->flags & F_TILE_ASSOC) && !ds->colour_scratch[y*w+x]) {
		if (ui->dragging && ui->srcx == x*2+1 && ui->srcy == y*2+1) {
                    /* tile is the source, don't do it */
                } else if (ui->dragging && opp && ui->srcx == opp->x && ui->srcy == opp->y) {
                    /* opposite tile is the source, don't do it */
		} else if (sp->doty != y*2+1 || sp->dotx != x*2+1) {
                    flags |= DRAW_ARROW;
                    ddy = sp->doty - (y*2+1);
                    ddx = sp->dotx - (x*2+1);
                }
            }

            /*
             * Now go through the nine possible places we could
             * have dots.
             */
            for (dy = 0; dy < 3; dy++)
                for (dx = 0; dx < 3; dx++) {
                    sp = &SPACE(state, x*2+dx, y*2+dy);
                    if (sp->flags & F_DOT) {
                        unsigned long dotval = (sp->flags & F_DOT_BLACK ?
                                                DOT_BLACK : DOT_WHITE);
                        flags |= dotval << (DOT_SHIFT_C +
                                            DOT_SHIFT_M*(dy*3+dx));
                    }
                }

            /*
             * Now work out if we have to draw a cursor for this square;
             * cursors-on-lines are taken care of below.
             */
            if (ui->cur_visible &&
                ui->cur_x == x*2+1 && ui->cur_y == y*2+1 &&
                !(SPACE(state, x*2+1, y*2+1).flags & F_DOT))
                flags |= DRAW_CURSOR;

            /*
             * Now we have everything we're going to need. Draw the
             * square.
             */
            if (ds->grid[y*w+x] != flags ||
                ds->dx[y*w+x] != ddx ||
                ds->dy[y*w+x] != ddy) {
                draw_square(dr, ds, x, y, flags, ddx, ddy);
                ds->grid[y*w+x] = flags;
                ds->dx[y*w+x] = ddx;
                ds->dy[y*w+x] = ddy;
            }
        }

    /*
     * Draw a cursor. This secondary blitter is much less invasive than trying
     * to fix up all of the rest of the code with sufficient flags to be able to
     * display this sensibly.
     */
    if (ui->cur_visible) {
        space *sp = &SPACE(state, ui->cur_x, ui->cur_y);
        ds->cur_visible = true;
        ds->cx = SCOORD(ui->cur_x) - CURSOR_SIZE;
        ds->cy = SCOORD(ui->cur_y) - CURSOR_SIZE;
        blitter_save(dr, ds->cur_bl, ds->cx, ds->cy);
        if (sp->flags & F_DOT) {
            /* draw a red dot (over the top of whatever would be there already) */
            draw_circle(dr, SCOORD(ui->cur_x), SCOORD(ui->cur_y), DOT_SIZE,
                        COL_CURSOR, COL_BLACKDOT);
            draw_circle(dr, SCOORD(ui->cur_x), SCOORD(ui->cur_y), DOT_SIZE-1,
                        COL_CURSOR, COL_BLACKDOT);
        } else if (sp->type != s_tile) {
            /* draw an edge/vertex square; tile cursors are dealt with above. */
            int dx = (ui->cur_x % 2) ? CURSOR_SIZE : CURSOR_SIZE/3;
            int dy = (ui->cur_y % 2) ? CURSOR_SIZE : CURSOR_SIZE/3;
            int x1 = SCOORD(ui->cur_x)-dx, y1 = SCOORD(ui->cur_y)-dy;
            int xs = dx*2+1, ys = dy*2+1;

            draw_rect(dr, x1, y1, xs, ys, COL_CURSOR);
        }
        draw_update(dr, ds->cx, ds->cy, CURSOR_SIZE*2+1, CURSOR_SIZE*2+1);
    }

    if (ui->dragging) {
        int oppx, oppy;

        ds->dragging = true;
        ds->dragx = ui->dx - TILE_SIZE/2;
        ds->dragy = ui->dy - TILE_SIZE/2;
        calculate_opposite_point(ui, ds, ui->dx, ui->dy, &oppx, &oppy);
        ds->oppx = oppx - TILE_SIZE/2;
        ds->oppy = oppy - TILE_SIZE/2;

        blitter_save(dr, ds->bl, ds->dragx, ds->dragy);
        clip(dr, ds->dragx, ds->dragy, TILE_SIZE, TILE_SIZE);
        draw_arrow(dr, ds, ui->dx, ui->dy, SCOORD(ui->dotx) - ui->dx,
                   SCOORD(ui->doty) - ui->dy, COL_ARROW);
        unclip(dr);
        draw_update(dr, ds->dragx, ds->dragy, TILE_SIZE, TILE_SIZE);

        blitter_save(dr, ds->blmirror, ds->oppx, ds->oppy);
        clip(dr, ds->oppx, ds->oppy, TILE_SIZE, TILE_SIZE);
        draw_arrow(dr, ds, oppx, oppy, SCOORD(ui->dotx) - oppx,
                   SCOORD(ui->doty) - oppy, COL_ARROW);
        unclip(dr);
        draw_update(dr, ds->oppx, ds->oppy, TILE_SIZE, TILE_SIZE);
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

static bool game_timing_state(const game_state *state, game_ui *ui)
{
    return true;
}

#ifdef COMBINED
#define thegame galaxies
#endif

static const char rules[] = "You have a rectangular grid containing a number of dots. Your aim is to partition the rectangle into connected regions of squares, in such a way that every region is 180° rotationally symmetric, and contains exactly one dot which is located at its centre of symmetry.\n\n"
"To enter your solution, you draw lines along the grid edges to mark the boundaries of the regions. The puzzle is complete when the marked lines on the grid are precisely those that separate two squares belonging to different regions.\n\n\n"
"This puzzle was contributed by James Harvey.";

const struct game thegame = {
    "Galaxies", "games.galaxies", "galaxies", rules,
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
    true, solve_game,
    false, NULL, NULL,
    new_ui,
    free_ui,
    encode_ui,
    decode_ui,
    NULL, /* game_request_keys */
    game_changed_state,
    interpret_move,
    execute_move,
    PREFERRED_TILE_SIZE, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    NULL,
    NULL,
    game_status,
    false, false, NULL, NULL,
    false,                     /* wants_statusbar */
    false, game_timing_state,
    0, /* REQUIRE_RBUTTON,            flags */
};

