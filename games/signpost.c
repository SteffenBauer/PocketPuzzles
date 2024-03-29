/*
 * signpost.c: implementation of the janko game 'arrow path'
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

#define PREFERRED_TILE_SIZE 64
#define TILE_SIZE (ds->tilesize)
#define BORDER    (TILE_SIZE / 2)

#define COORD(x)  ( (x) * TILE_SIZE + BORDER )
#define FROMCOORD(x)  ( ((x) - BORDER + TILE_SIZE) / TILE_SIZE - 1 )

#define INGRID(s,x,y) ((x) >= 0 && (x) < (s)->w && (y) >= 0 && (y) < (s)->h)

enum {
    COL_WHITE,
    COL_BLACK,
    COL_VDARKGREY,
    COL_DARKGREY,
    COL_LIGHTGREY,
    COL_VLIGHTGREY,
    NCOLOURS
};

struct game_params {
    int w, h;
    bool force_corner_start;
};

enum { DIR_N = 0, DIR_NE, DIR_E, DIR_SE, DIR_S, DIR_SW, DIR_W, DIR_NW, DIR_MAX };

static const int dxs[DIR_MAX] = {  0,  1, 1, 1, 0, -1, -1, -1 };
static const int dys[DIR_MAX] = { -1, -1, 0, 1, 1,  1,  0, -1 };

#define DIR_OPPOSITE(d) ((d+4)%8)

struct game_state {
    int w, h, n;
    bool completed, used_solve, impossible;
    int *dirs;                  /* direction enums, size n */
    int *nums;                  /* numbers, size n */
    unsigned int *flags;        /* flags, size n */
    int *next, *prev;           /* links to other cell indexes, size n (-1 absent) */
    DSF *dsf;                   /* connects regions with a dsf. */
    int *numsi;                 /* for each number, which index is it in? (-1 absent) */
};

#define FLAG_IMMUTABLE  1
#define FLAG_ERROR      2

/* --- Generally useful functions --- */

#define ISREALNUM(state, num) ((num) > 0 && (num) <= (state)->n)

static int whichdir(int fromx, int fromy, int tox, int toy)
{
    int i, dx, dy;

    dx = tox - fromx;
    dy = toy - fromy;

    if (dx && dy && abs(dx) != abs(dy)) return -1;

    if (dx) dx = dx / abs(dx); /* limit to (-1, 0, 1) */
    if (dy) dy = dy / abs(dy); /* ditto */

    for (i = 0; i < DIR_MAX; i++) {
        if (dx == dxs[i] && dy == dys[i]) return i;
    }
    return -1;
}

static int whichdiri(game_state *state, int fromi, int toi)
{
    int w = state->w;
    return whichdir(fromi%w, fromi/w, toi%w, toi/w);
}

static bool ispointing(const game_state *state, int fromx, int fromy,
                       int tox, int toy)
{
    int w = state->w, dir = state->dirs[fromy*w+fromx];

    /* (by convention) squares do not point to themselves. */
    if (fromx == tox && fromy == toy) return false;

    /* the final number points to nothing. */
    if (state->nums[fromy*w + fromx] == state->n) return false;

    while (1) {
        if (!INGRID(state, fromx, fromy)) return false;
        if (fromx == tox && fromy == toy) return true;
        fromx += dxs[dir]; fromy += dys[dir];
    }
    return false; /* not reached */
}

static bool ispointingi(game_state *state, int fromi, int toi)
{
    int w = state->w;
    return ispointing(state, fromi%w, fromi/w, toi%w, toi/w);
}

/* Taking the number 'num', work out the gap between it and the next
 * available number up or down (depending on d). Return true if the
 * region at (x,y) will fit in that gap. */
static bool move_couldfit(
    const game_state *state, int num, int d, int x, int y)
{
    int n, gap, i = y*state->w+x, sz;

    assert(d != 0);
    /* The 'gap' is the number of missing numbers in the grid between
     * our number and the next one in the sequence (up or down), or
     * the end of the sequence (if we happen not to have 1/n present) */
    for (n = num + d, gap = 0;
         ISREALNUM(state, n) && state->numsi[n] == -1;
         n += d, gap++) ; /* empty loop */

    if (gap == 0) {
        /* no gap, so the only allowable move is that that directly
         * links the two numbers. */
        n = state->nums[i];
        return n != num+d;
    }
    if (state->prev[i] == -1 && state->next[i] == -1)
        return true; /* single unconnected square, always OK */

    sz = dsf_size(state->dsf, i);
    return sz <= gap;
}

static bool isvalidmove(const game_state *state, bool clever,
                        int fromx, int fromy, int tox, int toy)
{
    int w = state->w, from = fromy*w+fromx, to = toy*w+tox;
    int nfrom, nto;

    if (!INGRID(state, fromx, fromy) || !INGRID(state, tox, toy))
        return false;

    /* can only move where we point */
    if (!ispointing(state, fromx, fromy, tox, toy))
        return false;

    nfrom = state->nums[from]; nto = state->nums[to];

    /* can't move _from_ the preset final number, or _to_ the preset 1. */
    if (((nfrom == state->n) && (state->flags[from] & FLAG_IMMUTABLE)) ||
        ((nto   == 1)        && (state->flags[to]   & FLAG_IMMUTABLE)))
        return false;

    /* can't create a new connection between cells in the same region
     * as that would create a loop. */
    if (dsf_equivalent(state->dsf, from, to))
        return false;

    /* if both cells are actual numbers, can't drag if we're not
     * one digit apart. */
    if (ISREALNUM(state, nfrom) && ISREALNUM(state, nto)) {
        if (nfrom != nto-1)
            return false;
    } else if (clever && ISREALNUM(state, nfrom)) {
        if (!move_couldfit(state, nfrom, +1, tox, toy))
            return false;
    } else if (clever && ISREALNUM(state, nto)) {
        if (!move_couldfit(state, nto, -1, fromx, fromy))
            return false;
    }

    return true;
}

static void makelink(game_state *state, int from, int to)
{
    if (state->next[from] != -1)
        state->prev[state->next[from]] = -1;
    state->next[from] = to;

    if (state->prev[to] != -1)
        state->next[state->prev[to]] = -1;
    state->prev[to] = from;
}

static void strip_nums(game_state *state) {
    int i;
    for (i = 0; i < state->n; i++) {
        if (!(state->flags[i] & FLAG_IMMUTABLE))
            state->nums[i] = 0;
    }
    memset(state->next, -1, state->n*sizeof(int));
    memset(state->prev, -1, state->n*sizeof(int));
    memset(state->numsi, -1, (state->n+1)*sizeof(int));
    dsf_reinit(state->dsf);
}

static bool check_nums(game_state *orig, game_state *copy, bool only_immutable)
{
    int i;
    bool ret = true;
    for (i = 0; i < copy->n; i++) {
        if (only_immutable && !(copy->flags[i] & FLAG_IMMUTABLE)) continue;
        if (copy->nums[i] != orig->nums[i]) {
            ret = false;
        }
    }
    return ret;
}

/* --- Game parameter/presets functions --- */

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);
    ret->w = 4;
    ret->h = 4;
    ret->force_corner_start = true;

    return ret;
}

static const struct game_params signpost_presets[] = {
  { 4, 4, 1 },
  { 4, 4, 0 },
  { 5, 5, 1 },
  { 5, 5, 0 },
  { 6, 6, 0 },
  { 7, 7, 0 },
  { 8, 8, 0 }
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(signpost_presets))
        return false;

    ret = default_params();
    *ret = signpost_presets[i];
    *params = ret;

    sprintf(buf, "%dx%d%s", ret->w, ret->h,
            ret->force_corner_start ? "" : ", free ends");
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
    *ret = *params;               /* structure copy */
    return ret;
}

static void decode_params(game_params *ret, char const *string)
{
    ret->w = ret->h = atoi(string);
    while (*string && isdigit((unsigned char)*string)) string++;
    if (*string == 'x') {
        string++;
        ret->h = atoi(string);
        while (*string && isdigit((unsigned char)*string)) string++;
    }
    ret->force_corner_start = false;
    if (*string == 'c') {
        string++;
        ret->force_corner_start = true;
    }

}

static char *encode_params(const game_params *params, bool full)
{
    char data[256];

    if (full)
        sprintf(data, "%dx%d%s", params->w, params->h,
                params->force_corner_start ? "c" : "");
    else
        sprintf(data, "%dx%d", params->w, params->h);

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

    ret[2].name = "Start and end in corners";
    ret[2].type = C_BOOLEAN;
    ret[2].u.boolean.bval = params->force_corner_start;

    ret[3].name = NULL;
    ret[3].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->force_corner_start = cfg[2].u.boolean.bval;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->w < 1) return "Width must be at least one";
    if (params->h < 1) return "Height must be at least one";
    if (params->w > 16) return "Width must be at most 16";
    if (params->h > 16) return "Height must be at most 16";
    if (full && params->w == 1 && params->h == 1)
    /* The UI doesn't let us move these from unsolved to solved,
     * so we disallow generating (but not playing) them. */
    return "Width and height cannot both be one";
    return NULL;
}

/* --- Game description string generation and unpicking --- */

static void blank_game_into(game_state *state)
{
    memset(state->dirs, 0, state->n*sizeof(int));
    memset(state->nums, 0, state->n*sizeof(int));
    memset(state->flags, 0, state->n*sizeof(unsigned int));
    memset(state->next, -1, state->n*sizeof(int));
    memset(state->prev, -1, state->n*sizeof(int));
    memset(state->numsi, -1, (state->n+1)*sizeof(int));
}

static game_state *blank_game(int w, int h)
{
    game_state *state = snew(game_state);

    memset(state, 0, sizeof(game_state));
    state->w = w;
    state->h = h;
    state->n = w*h;

    state->dirs  = snewn(state->n, int);
    state->nums  = snewn(state->n, int);
    state->flags = snewn(state->n, unsigned int);
    state->next  = snewn(state->n, int);
    state->prev  = snewn(state->n, int);
    state->dsf = dsf_new(state->n);
    state->numsi  = snewn(state->n+1, int);

    blank_game_into(state);

    return state;
}

static void dup_game_to(game_state *to, const game_state *from)
{
    to->completed = from->completed;
    to->used_solve = from->used_solve;
    to->impossible = from->impossible;

    memcpy(to->dirs, from->dirs, to->n*sizeof(int));
    memcpy(to->flags, from->flags, to->n*sizeof(unsigned int));
    memcpy(to->nums, from->nums, to->n*sizeof(int));

    memcpy(to->next, from->next, to->n*sizeof(int));
    memcpy(to->prev, from->prev, to->n*sizeof(int));

    dsf_copy(to->dsf, from->dsf);
    memcpy(to->numsi, from->numsi, (to->n+1)*sizeof(int));
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = blank_game(state->w, state->h);
    dup_game_to(ret, state);
    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->dirs);
    sfree(state->nums);
    sfree(state->flags);
    sfree(state->next);
    sfree(state->prev);
    dsf_free(state->dsf);
    sfree(state->numsi);
    sfree(state);
}

static void unpick_desc(const game_params *params, const char *desc,
                        game_state **sout, const char **mout)
{
    game_state *state = blank_game(params->w, params->h);
    const char *msg = NULL;
    char c;
    int num = 0, i = 0;

    while (*desc) {
        if (i >= state->n) {
            msg = "Game description longer than expected";
            goto done;
        }

        c = *desc;
        if (isdigit((unsigned char)c)) {
            num = (num*10) + (int)(c-'0');
            if (num > state->n) {
                msg = "Number too large";
                goto done;
            }
        } else if ((c-'a') >= 0 && (c-'a') < DIR_MAX) {
            state->nums[i] = num;
            state->flags[i] = num ? FLAG_IMMUTABLE : 0;
            num = 0;

            state->dirs[i] = c - 'a';
            i++;
        } else if (!*desc) {
            msg = "Game description shorter than expected";
            goto done;
        } else {
            msg = "Game description contains unexpected characters";
            goto done;
        }
        desc++;
    }
    if (i < state->n) {
        msg = "Game description shorter than expected";
        goto done;
    }

done:
    if (msg) { /* sth went wrong. */
        if (mout) *mout = msg;
        free_game(state);
    } else {
        if (mout) *mout = NULL;
        if (sout) *sout = state;
        else free_game(state);
    }
}

static char *generate_desc(game_state *state, bool issolve)
{
    char *ret, buf[80];
    int retlen, i, k;

    ret = NULL; retlen = 0;
    if (issolve) {
        ret = sresize(ret, 2, char);
        ret[0] = 'S'; ret[1] = '\0';
        retlen += 1;
    }
    for (i = 0; i < state->n; i++) {
        if (state->nums[i])
            k = sprintf(buf, "%d%c", state->nums[i], (int)(state->dirs[i]+'a'));
        else
            k = sprintf(buf, "%c", (int)(state->dirs[i]+'a'));
        ret = sresize(ret, retlen + k + 1, char);
        strcpy(ret + retlen, buf);
        retlen += k;
    }
    return ret;
}

/* --- Game generation --- */

/* Fills in preallocated arrays ai (indices) and ad (directions)
 * showing all non-numbered cells adjacent to index i, returns length */
/* This function has been somewhat optimised... */
static int cell_adj(game_state *state, int i, int *ai, int *ad)
{
    int n = 0, a, x, y, sx, sy, dx, dy, newi;
    int w = state->w, h = state->h;

    sx = i % w; sy = i / w;

    for (a = 0; a < DIR_MAX; a++) {
        x = sx; y = sy;
        dx = dxs[a]; dy = dys[a];
        while (1) {
            x += dx; y += dy;
            if (x < 0 || y < 0 || x >= w || y >= h) break;

            newi = y*w + x;
            if (state->nums[newi] == 0) {
                ai[n] = newi;
                ad[n] = a;
                n++;
            }
        }
    }
    return n;
}

static bool new_game_fill(game_state *state, random_state *rs,
                          int headi, int taili)
{
    int nfilled, an, j;
    bool ret = false;
    int *aidx, *adir;

    aidx = snewn(state->n, int);
    adir = snewn(state->n, int);

    memset(state->nums, 0, state->n*sizeof(int));

    state->nums[headi] = 1;
    state->nums[taili] = state->n;

    state->dirs[taili] = 0;
    nfilled = 2;
    assert(state->n > 1);

    while (nfilled < state->n) {
        /* Try and expand _from_ headi; keep going if there's only one
         * place to go to. */
        an = cell_adj(state, headi, aidx, adir);
        do {
            if (an == 0) goto done;
            j = random_upto(rs, an);
            state->dirs[headi] = adir[j];
            state->nums[aidx[j]] = state->nums[headi] + 1;
            nfilled++;
            headi = aidx[j];
            an = cell_adj(state, headi, aidx, adir);
        } while (an == 1);

    if (nfilled == state->n) break;

        /* Try and expand _to_ taili; keep going if there's only one
         * place to go to. */
        an = cell_adj(state, taili, aidx, adir);
        do {
            if (an == 0) goto done;
            j = random_upto(rs, an);
            state->dirs[aidx[j]] = DIR_OPPOSITE(adir[j]);
            state->nums[aidx[j]] = state->nums[taili] - 1;
            nfilled++;
            taili = aidx[j];
            an = cell_adj(state, taili, aidx, adir);
        } while (an == 1);
    }
    /* If we get here we have headi and taili set but unconnected
     * by direction: we need to set headi's direction so as to point
     * at taili. */
    state->dirs[headi] = whichdiri(state, headi, taili);

    /* it could happen that our last two weren't in line; if that's the
     * case, we have to start again. */
    if (state->dirs[headi] != -1) ret = true;

done:
    sfree(aidx);
    sfree(adir);
    return ret;
}

/* Better generator: with the 'generate, sprinkle numbers, solve,
 * repeat' algorithm we're _never_ generating anything greater than
 * 6x6, and spending all of our time in new_game_fill (and very little
 * in solve_state).
 *
 * So, new generator steps:
   * generate the grid, at random (same as now). Numbers 1 and N get
      immutable flag immediately.
   * squirrel that away for the solved state.
   *
   * (solve:) Try and solve it.
   * If we solved it, we're done:
     * generate the description from current immutable numbers,
     * free stuff that needs freeing,
     * return description + solved state.
   * If we didn't solve it:
     * count #tiles in state we've made deductions about.
     * while (1):
       * randomise a scratch array.
       * for each index in scratch (in turn):
         * if the cell isn't empty, continue (through scratch array)
         * set number + immutable in state.
         * try and solve state.
         * if we've solved it, we're done.
         * otherwise, count #tiles. If it's more than we had before:
           * good, break from this loop and re-randomise.
         * otherwise (number didn't help):
           * remove number and try next in scratch array.
       * if we've got to the end of the scratch array, no luck:
          free everything we need to, and go back to regenerate the grid.
   */

static int solve_state(game_state *state);

/* Expects a fully-numbered game_state on input, and makes sure
 * FLAG_IMMUTABLE is only set on those numbers we need to solve
 * (as for a real new-game); returns true if it managed
 * this (such that it could solve it), or false if not. */
static bool new_game_strip(game_state *state, random_state *rs)
{
    int *scratch, i, j;
    bool ret = true;
    game_state *copy = dup_game(state);

    strip_nums(copy);

    if (solve_state(copy) > 0) {
        free_game(copy);
        return true;
    }

    scratch = snewn(state->n, int);
    for (i = 0; i < state->n; i++) scratch[i] = i;
    shuffle(scratch, state->n, sizeof(int), rs);

    /* This is scungy. It might just be quick enough.
     * It goes through, adding set numbers in empty squares
     * until either we run out of empty squares (in the one
     * we're half-solving) or else we solve it properly.
     * NB that we run the entire solver each time, which
     * strips the grid beforehand; we will save time if we
     * avoid that. */
    for (i = 0; i < state->n; i++) {
        j = scratch[i];
        if (copy->nums[j] > 0 && copy->nums[j] <= state->n)
            continue; /* already solved to a real number here. */
        assert(state->nums[j] <= state->n);
        copy->nums[j] = state->nums[j];
        copy->flags[j] |= FLAG_IMMUTABLE;
        state->flags[j] |= FLAG_IMMUTABLE;
        strip_nums(copy);
        if (solve_state(copy) > 0) goto solved;
        check_nums(state, copy, true);
    }
    ret = false;
    goto done;

solved:
    /* Since we added basically at random, try now to remove numbers
     * and see if we can still solve it; if we can (still), really
     * remove the number. Make sure we don't remove the anchor numbers
     * 1 and N. */
    for (i = 0; i < state->n; i++) {
        j = scratch[i];
        if ((state->flags[j] & FLAG_IMMUTABLE) &&
            (state->nums[j] != 1 && state->nums[j] != state->n)) {
            state->flags[j] &= ~FLAG_IMMUTABLE;
            dup_game_to(copy, state);
            strip_nums(copy);
            if (solve_state(copy) > 0) {
                check_nums(state, copy, false);
            } else {
                assert(state->nums[j] <= state->n);
                copy->nums[j] = state->nums[j];
                state->flags[j] |= FLAG_IMMUTABLE;
            }
        }
    }

done:
    sfree(scratch);
    free_game(copy);
    return ret;
}

static char *new_game_desc(const game_params *params, random_state *rs,
               char **aux, bool interactive)
{
    game_state *state = blank_game(params->w, params->h);
    char *ret;
    int headi, taili;

    /* this shouldn't happen (validate_params), but let's play it safe */
    if (params->w == 1 && params->h == 1) return dupstr("1a");

generate:
    blank_game_into(state);

    /* keep trying until we fill successfully. */
    do {
        if (params->force_corner_start) {
            headi = 0;
            taili = state->n-1;
        } else {
            do {
                headi = random_upto(rs, state->n);
                taili = random_upto(rs, state->n);
            } while (headi == taili);
        }
    } while (!new_game_fill(state, rs, headi, taili));

    assert(state->nums[headi] <= state->n);
    assert(state->nums[taili] <= state->n);

    state->flags[headi] |= FLAG_IMMUTABLE;
    state->flags[taili] |= FLAG_IMMUTABLE;

    /* This will have filled in directions and _all_ numbers.
     * Store the game definition for this, as the solved-state. */
    if (!new_game_strip(state, rs)) {
        goto generate;
    }
    strip_nums(state);
    {
        game_state *tosolve = dup_game(state);
        assert(solve_state(tosolve) > 0);
        free_game(tosolve);
    }
    ret = generate_desc(state, false);
    free_game(state);
    return ret;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    const char *ret = NULL;

    unpick_desc(params, desc, NULL, &ret);
    return ret;
}

/* --- Linked-list and numbers array --- */

/* Assuming numbers are always up-to-date, there are only four possibilities
 * for regions changing after a single valid move:
 *
 * 1) two differently-coloured regions being combined (the resulting colouring
 *     should be based on the larger of the two regions)
 * 2) a numbered region having a single number added to the start (the
 *     region's colour will remain, and the numbers will shift by 1)
 * 3) a numbered region having a single number added to the end (the
 *     region's colour and numbering remains as-is)
 * 4) two unnumbered squares being joined (will pick the smallest unused set
 *     of colours to use for the new region).
 *
 * There should never be any complications with regions containing 3 colours
 * being combined, since two of those colours should have been merged on a
 * previous move.
 *
 * Most of the complications are in ensuring we don't accidentally set two
 * regions with the same colour (e.g. if a region was split). If this happens
 * we always try and give the largest original portion the original colour.
 */

#define COLOUR(a) ((a) / (state->n+1))
#define START(c) ((c) * (state->n+1))

struct head_meta {
    int i;      /* position */
    int sz;     /* size of region */
    int start;  /* region start number preferred, or 0 if !preference */
    int preference; /* 0 if we have no preference (and should just pick one) */
    const char *why;
};

static void head_number(game_state *state, int i, struct head_meta *head)
{
    int off = 0, ss, j = i, c, n, sz;

    /* Insist we really were passed the head of a chain. */
    assert(state->prev[i] == -1 && state->next[i] != -1);

    head->i = i;
    head->sz = dsf_size(state->dsf, i);
    head->why = NULL;

    /* Search through this chain looking for real numbers, checking that
     * they match up (if there are more than one). */
    head->preference = 0;
    while (j != -1) {
        if (state->flags[j] & FLAG_IMMUTABLE) {
            ss = state->nums[j] - off;
            if (!head->preference) {
                head->start = ss;
                head->preference = 1;
                head->why = "contains cell with immutable number";
            } else if (head->start != ss) {
                state->impossible = true;
            }
        }
        off++;
        j = state->next[j];
        assert(j != i); /* we have created a loop, obviously wrong */
    }
    if (head->preference) goto done;

    if (state->nums[i] == 0 && state->nums[state->next[i]] > state->n) {
        /* (probably) empty cell onto the head of a coloured region:
         * make sure we start at a 0 offset. */
        head->start = START(COLOUR(state->nums[state->next[i]]));
        head->preference = 1;
        head->why = "adding blank cell to head of numbered region";
    } else if (state->nums[i] <= state->n) {
        /* if we're 0 we're probably just blank -- but even if we're a
         * (real) numbered region, we don't have an immutable number
         * in it (any more) otherwise it'd have been caught above, so
         * reassign the colour. */
        head->start = 0;
        head->preference = 0;
        head->why = "lowest available colour group";
    } else {
        c = COLOUR(state->nums[i]);
        n = 1;
        sz = dsf_size(state->dsf, i);
        j = i;
        while (state->next[j] != -1) {
            j = state->next[j];
            if (state->nums[j] == 0 && state->next[j] == -1) {
                head->start = START(c);
                head->preference = 1;
                head->why = "adding blank cell to end of numbered region";
                goto done;
            }
            if (COLOUR(state->nums[j]) == c)
                n++;
            else {
                int start_alternate = START(COLOUR(state->nums[j]));
                if (n < (sz - n)) {
                    head->start = start_alternate;
                    head->preference = 1;
                    head->why = "joining two coloured regions, swapping to larger colour";
                } else {
                    head->start = START(c);
                    head->preference = 1;
                    head->why = "joining two coloured regions, taking largest";
                }
                goto done;
            }
        }
        /* If we got here then we may have split a region into
         * two; make sure we don't assign a colour we've already used. */
        if (c == 0) {
            /* not convinced this shouldn't be an assertion failure here. */
            head->start = 0;
            head->preference = 0;
        } else {
            head->start = START(c);
            head->preference = 1;
        }
        head->why = "got to end of coloured region";
    }

done:
    assert(head->why != NULL);
    return;
}

static void connect_numbers(game_state *state)
{
    int i, di, dni;

    dsf_reinit(state->dsf);
    for (i = 0; i < state->n; i++) {
        if (state->next[i] != -1) {
            assert(state->prev[state->next[i]] == i);
            di = dsf_canonify(state->dsf, i);
            dni = dsf_canonify(state->dsf, state->next[i]);
            if (di == dni) {
                state->impossible = true;
            }
            dsf_merge(state->dsf, di, dni);
        }
    }
}

static int compare_heads(const void *a, const void *b)
{
    const struct head_meta *ha = (const struct head_meta *)a;
    const struct head_meta *hb = (const struct head_meta *)b;

    /* Heads with preferred colours first... */
    if (ha->preference && !hb->preference) return -1;
    if (hb->preference && !ha->preference) return 1;

    /* ...then heads with low colours first... */
    if (ha->start < hb->start) return -1;
    if (ha->start > hb->start) return 1;

    /* ... then large regions first... */
    if (ha->sz > hb->sz) return -1;
    if (ha->sz < hb->sz) return 1;

    /* ... then position. */
    if (ha->i > hb->i) return -1;
    if (ha->i < hb->i) return 1;

    return 0;
}

static int lowest_start(game_state *state, struct head_meta *heads, int nheads)
{
    int n, c;

    /* NB start at 1: colour 0 is real numbers */
    for (c = 1; c < state->n; c++) {
        for (n = 0; n < nheads; n++) {
            if (COLOUR(heads[n].start) == c)
                goto used;
        }
        return c;
used:
        ;
    }
    assert(!"No available colours!");
    return 0;
}

static void update_numbers(game_state *state)
{
    int i, j, n, nnum, nheads;
    struct head_meta *heads = snewn(state->n, struct head_meta);

    for (n = 0; n < state->n; n++)
        state->numsi[n] = -1;

    for (i = 0; i < state->n; i++) {
        if (state->flags[i] & FLAG_IMMUTABLE) {
            assert(state->nums[i] > 0);
            assert(state->nums[i] <= state->n);
            state->numsi[state->nums[i]] = i;
        }
        else if (state->prev[i] == -1 && state->next[i] == -1)
            state->nums[i] = 0;
    }
    connect_numbers(state);

    /* Construct an array of the heads of all current regions, together
     * with their preferred colours. */
    nheads = 0;
    for (i = 0; i < state->n; i++) {
        /* Look for a cell that is the start of a chain
         * (has a next but no prev). */
        if (state->prev[i] != -1 || state->next[i] == -1) continue;

        head_number(state, i, &heads[nheads++]);
    }

    /* Sort that array:
     * - heads with preferred colours first, then
     * - heads with low colours first, then
     * - large regions first
     */
    qsort(heads, nheads, sizeof(struct head_meta), compare_heads);

    /* Remove duplicate-coloured regions. */
    for (n = nheads-1; n >= 0; n--) { /* order is important! */
        if ((n != 0) && (heads[n].start == heads[n-1].start)) {
            /* We have a duplicate-coloured region: since we're
             * sorted in size order and this is not the first
             * of its colour it's not the largest: recolour it. */
            heads[n].start = START(lowest_start(state, heads, nheads));
            heads[n].preference = -1; /* '-1' means 'was duplicate' */
        }
        else if (!heads[n].preference) {
            assert(heads[n].start == 0);
            heads[n].start = START(lowest_start(state, heads, nheads));
        }
    }

    for (n = 0; n < nheads; n++) {
        nnum = heads[n].start;
        j = heads[n].i;
        while (j != -1) {
            if (!(state->flags[j] & FLAG_IMMUTABLE)) {
                if (nnum > 0 && nnum <= state->n)
                    state->numsi[nnum] = j;
                state->nums[j] = nnum;
            }
            nnum++;
            j = state->next[j];
            assert(j != heads[n].i); /* loop?! */
        }
    }
    sfree(heads);
}

static bool check_completion(game_state *state, bool mark_errors)
{
    int n, j, k;
    bool error = false, complete;

    /* NB This only marks errors that are possible to perpetrate with
     * the current UI in interpret_move. Things like forming loops in
     * linked sections and having numbers not add up should be forbidden
     * by the code elsewhere, so we don't bother marking those (because
     * it would add lots of tricky drawing code for very little gain). */
    if (mark_errors) {
        for (j = 0; j < state->n; j++)
            state->flags[j] &= ~FLAG_ERROR;
    }

    /* Search for repeated numbers. */
    for (j = 0; j < state->n; j++) {
        if (state->nums[j] > 0 && state->nums[j] <= state->n) {
            for (k = j+1; k < state->n; k++) {
                if (state->nums[k] == state->nums[j]) {
                    if (mark_errors) {
                        state->flags[j] |= FLAG_ERROR;
                        state->flags[k] |= FLAG_ERROR;
                    }
                    error = true;
                }
            }
        }
    }

    /* Search and mark numbers n not pointing to n+1; if any numbers
     * are missing we know we've not completed. */
    complete = true;
    for (n = 1; n < state->n; n++) {
        if (state->numsi[n] == -1 || state->numsi[n+1] == -1)
            complete = false;
        else if (!ispointingi(state, state->numsi[n], state->numsi[n+1])) {
            if (mark_errors) {
                state->flags[state->numsi[n]] |= FLAG_ERROR;
                state->flags[state->numsi[n+1]] |= FLAG_ERROR;
            }
            error = true;
        } else {
            /* make sure the link is explicitly made here; for instance, this
             * is nice if the user drags from 2 out (making 3) and a 4 is also
             * visible; this ensures that the link from 3 to 4 is also made. */
            if (mark_errors)
                makelink(state, state->numsi[n], state->numsi[n+1]);
        }
    }

    /* Search and mark numbers less than 0, or 0 with links. */
    for (n = 1; n < state->n; n++) {
        if ((state->nums[n] < 0) ||
            (state->nums[n] == 0 &&
             (state->next[n] != -1 || state->prev[n] != -1))) {
            error = true;
            if (mark_errors)
                state->flags[n] |= FLAG_ERROR;
        }
    }

    if (error) return false;
    return complete;
}
static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    game_state *state = NULL;

    unpick_desc(params, desc, &state, NULL);
    if (!state) assert(!"new_game failed to unpick");

    update_numbers(state);
    check_completion(state, true); /* update any auto-links */

    return state;
}

/* --- Solver --- */

/* If a tile has a single tile it can link _to_, or there's only a single
 * location that can link to a given tile, fill that link in. */
static int solve_single(game_state *state, game_state *copy, int *from)
{
    int i, j, sx, sy, x, y, d, poss, w=state->w, nlinks = 0;

    /* The from array is a list of 'which square can link _to_ us';
     * we start off with from as '-1' (meaning 'not found'); if we find
     * something that can link to us it is set to that index, and then if
     * we find another we set it to -2. */

    memset(from, -1, state->n*sizeof(int));

    /* poss is 'can I link to anything' with the same meanings. */

    for (i = 0; i < state->n; i++) {
        if (state->next[i] != -1) continue;
        if (state->nums[i] == state->n) continue; /* no next from last no. */

        d = state->dirs[i];
        poss = -1;
        sx = x = i%w; sy = y = i/w;
        while (1) {
            x += dxs[d]; y += dys[d];
            if (!INGRID(state, x, y)) break;
            if (!isvalidmove(state, true, sx, sy, x, y)) continue;

            /* can't link to somewhere with a back-link we would have to
             * break (the solver just doesn't work like this). */
            j = y*w+x;
            if (state->prev[j] != -1) continue;

            if (state->nums[i] > 0 && state->nums[j] > 0 &&
                state->nums[i] <= state->n && state->nums[j] <= state->n &&
                state->nums[j] == state->nums[i]+1) {
                poss = j;
                from[j] = i;
                break;
            }

            /* if there's been a valid move already, we have to move on;
             * we can't make any deductions here. */
            poss = (poss == -1) ? j : -2;

            /* Modify the from array as described above (which is enumerating
             * what points to 'j' in a similar way). */
            from[j] = (from[j] == -1) ? i : -2;
        }
        if (poss == -2) {
            ;
        } else if (poss == -1) {
            copy->impossible = true;
            return -1;
        } else {
            makelink(copy, i, poss);
            nlinks++;
        }
    }

    for (i = 0; i < state->n; i++) {
        if (state->prev[i] != -1) continue;
        if (state->nums[i] == 1) continue; /* no prev from 1st no. */

        x = i%w; y = i/w;
        if (from[i] == -1) {
            copy->impossible = true;
            return -1;
        } else if (from[i] == -2) {
            ;
        } else {
            makelink(copy, from[i], i);
            nlinks++;
        }
    }

    return nlinks;
}

/* Returns 1 if we managed to solve it, 0 otherwise. */
static int solve_state(game_state *state)
{
    game_state *copy = dup_game(state);
    int *scratch = snewn(state->n, int), ret;

    while (1) {
        update_numbers(state);

        if (solve_single(state, copy, scratch)) {
            dup_game_to(state, copy);
            if (state->impossible) break; else continue;
        }
        break;
    }
    free_game(copy);
    sfree(scratch);

    update_numbers(state);
    ret = state->impossible ? -1 : check_completion(state, false);
    return ret;
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
    game_state *tosolve;
    char *ret = NULL;
    int result;

    tosolve = dup_game(currstate);
    result = solve_state(tosolve);
    if (result > 0)
        ret = generate_desc(tosolve, true);
    free_game(tosolve);
    if (ret) return ret;

    tosolve = dup_game(state);
    result = solve_state(tosolve);
    if (result < 0)
        *error = "Puzzle is impossible.";
    else if (result == 0)
        *error = "Unable to solve puzzle.";
    else
        ret = generate_desc(tosolve, true);

    free_game(tosolve);

    return ret;
}

/* --- UI and move routines. --- */


struct game_ui {
    bool dragging, drag_is_from, show_highlight, do_highlight;
    int sx, sy;         /* grid coords of start cell */
    int dx, dy;         /* pixel coords of drag posn */
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    /* NB: if this is ever changed to as to require more than a structure
     * copy to clone, there's code that needs fixing in game_redraw too. */

    ui->dragging = false;
    ui->drag_is_from = false;
    ui->show_highlight = false;
    ui->do_highlight = true;

    ui->sx = ui->sy = ui->dx = ui->dy = 0;

    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static config_item *get_prefs(game_ui *ui)
{
    config_item *ret;

    ret = snewn(2, config_item);

    ret[0].name = "Long-click highlights in-pointing arrows";
    ret[0].kw = "do-highlight";
    ret[0].type = C_BOOLEAN;
    ret[0].u.boolean.bval = ui->do_highlight;

    ret[1].name = NULL;
    ret[1].type = C_END;

    return ret;
}

static void set_prefs(game_ui *ui, const config_item *cfg)
{
    ui->do_highlight = cfg[0].u.boolean.bval;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
    if (!oldstate->completed && newstate->completed) {
        ui->dragging = false;
    }
}

struct game_drawstate {
    int tilesize;
    bool started, solved;
    int w, h, n;
    int *nums, *dirp;
    unsigned int *f;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int mx, int my, int button, bool swapped)
{
    int x = FROMCOORD(mx), y = FROMCOORD(my), w = state->w;
    char buf[80];

    if (IS_MOUSE_DOWN(button)) {
        if (!INGRID(state, x, y)) return NULL;
        ui->dragging = true;
        ui->drag_is_from = true;
        ui->show_highlight = (button == RIGHT_BUTTON) && (ui->do_highlight);
        ui->sx = x;
        ui->sy = y;
        ui->dx = mx;
        ui->dy = my;
        return MOVE_UI_UPDATE;
    } else if (IS_MOUSE_RELEASE(button) && ui->dragging) {
        ui->dragging = false;
        ui->show_highlight = false;
        if (ui->sx == x && ui->sy == y) return MOVE_UI_UPDATE; /* single click */

        if (!INGRID(state, x, y)) {
            int si = ui->sy*w+ui->sx;
            if (state->prev[si] == -1 && state->next[si] == -1)
                return MOVE_UI_UPDATE;
            sprintf(buf, "%c%d,%d",
                    (int)(ui->drag_is_from ? 'C' : 'X'), ui->sx, ui->sy);
            return dupstr(buf);
        }

        if (ui->drag_is_from) {
            if (!isvalidmove(state, false, ui->sx, ui->sy, x, y))
                return MOVE_UI_UPDATE;
            sprintf(buf, "L%d,%d-%d,%d", ui->sx, ui->sy, x, y);
        } else {
            if (!isvalidmove(state, false, x, y, ui->sx, ui->sy))
                return MOVE_UI_UPDATE;
            sprintf(buf, "L%d,%d-%d,%d", x, y, ui->sx, ui->sy);
        }
        return dupstr(buf);
    }

    return MOVE_UNUSED;
}

static void unlink_cell(game_state *state, int si)
{
    if (state->prev[si] != -1) {
        state->next[state->prev[si]] = -1;
        state->prev[si] = -1;
    }
    if (state->next[si] != -1) {
        state->prev[state->next[si]] = -1;
        state->next[si] = -1;
    }
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move)
{
    game_state *ret = NULL;
    int sx, sy, ex, ey, si, ei, w = state->w;
    char c;

    if (move[0] == 'S') {
        game_params p;
        game_state *tmp;
        const char *valid;
        int i;

        p.w = state->w; p.h = state->h;
        valid = validate_desc(&p, move+1);
        if (valid) {
            return NULL;
        }
        ret = dup_game(state);
        tmp = new_game(NULL, &p, move+1);
        for (i = 0; i < state->n; i++) {
            ret->prev[i] = tmp->prev[i];
            ret->next[i] = tmp->next[i];
        }
        free_game(tmp);
        ret->used_solve = true;
    } else if (sscanf(move, "L%d,%d-%d,%d", &sx, &sy, &ex, &ey) == 4) {
        if (!isvalidmove(state, false, sx, sy, ex, ey)) return NULL;

        ret = dup_game(state);
        ret->used_solve = false;
        si = sy*w+sx; ei = ey*w+ex;
        makelink(ret, si, ei);
    } else if (sscanf(move, "%c%d,%d", &c, &sx, &sy) == 3) {
        int sset;

        if (c != 'C' && c != 'X') return NULL;
        if (!INGRID(state, sx, sy)) return NULL;
        si = sy*w+sx;
        if (state->prev[si] == -1 && state->next[si] == -1)
            return NULL;

        ret = dup_game(state);
        ret->used_solve = false;
        sset = state->nums[si] / (state->n+1);
        if (c == 'C' || (c == 'X' && sset == 0)) {
            /* Unlink the single cell we dragged from the board. */
            unlink_cell(ret, si);
        } else {
            int i, set;
            for (i = 0; i < state->n; i++) {
                /* Unlink all cells in the same set as the one we dragged
                 * from the board. */

                if (state->nums[i] == 0) continue;
                set = state->nums[i] / (state->n+1);
                if (set != sset) continue;

                unlink_cell(ret, i);
            }
        }
    } else if (strcmp(move, "H") == 0) {
        ret = dup_game(state);
        solve_state(ret);
    }
    if (ret) {
        update_numbers(ret);
        ret->completed = check_completion(ret, true);
    }

    return ret;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize, order; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = TILE_SIZE * params->w + 2 * BORDER;
    *y = TILE_SIZE * params->h + 2 * BORDER;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
    assert(TILE_SIZE > 0);
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);
    int i;

    for (i = 0; i < 3; i++) {
        ret[COL_BLACK      * 3 + i] = 0.0F;
        ret[COL_VDARKGREY  * 3 + i] = 0.2F;
        ret[COL_DARKGREY   * 3 + i] = 0.4F;
        ret[COL_LIGHTGREY  * 3 + i] = 0.6F;
        ret[COL_VLIGHTGREY * 3 + i] = 0.8F;
        ret[COL_WHITE      * 3 + i] = 1.0F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int i;

    ds->tilesize = 0;
    ds->started = false;
    ds->solved = false;
    ds->w = state->w;
    ds->h = state->h;
    ds->n = state->n;

    ds->nums = snewn(state->n, int);
    ds->dirp = snewn(state->n, int);
    ds->f = snewn(state->n, unsigned int);
    for (i = 0; i < state->n; i++) {
        ds->nums[i] = 0;
        ds->dirp[i] = -1;
        ds->f[i] = 0;
    }

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->nums);
    sfree(ds->dirp);
    sfree(ds->f);

    sfree(ds);
}

/* cx, cy are top-left corner. sz is the 'radius' of the arrow.
 * ang is in radians, clockwise from 0 == straight up. */
static void draw_arrow(drawing *dr, int cx, int cy, int sz, double ang,
                       int cfill, int cout)
{
    int coords[8];
    int xdx, ydx, xdy, ydy, xdx3, xdy3;
    double s = sin(ang), c = cos(ang);

    xdx3 = (int)(sz * (c/3 + 1) + 0.5) - sz;
    xdy3 = (int)(sz * (s/3 + 1) + 0.5) - sz;
    xdx = (int)(sz * (c + 1) + 0.5) - sz;
    xdy = (int)(sz * (s + 1) + 0.5) - sz;
    ydx = -xdy;
    ydy = xdx;

    coords[2*0 + 0] = cx - ydx;
    coords[2*0 + 1] = cy - ydy;
    coords[2*1 + 0] = cx + xdx;
    coords[2*1 + 1] = cy + xdy;
    coords[2*2 + 0] = cx - xdx;
    coords[2*2 + 1] = cy - xdy;
    draw_polygon(dr, coords, 3, cfill, cout);

    coords[2*0 + 0] = cx + xdx3;
    coords[2*0 + 1] = cy + xdy3;
    coords[2*1 + 0] = cx + xdx3 + ydx;
    coords[2*1 + 1] = cy + xdy3 + ydy;
    coords[2*2 + 0] = cx - xdx3 + ydx;
    coords[2*2 + 1] = cy - xdy3 + ydy;
    coords[2*3 + 0] = cx - xdx3;
    coords[2*3 + 1] = cy - xdy3;
    draw_polygon(dr, coords, 4, cfill, cout);
}

static void draw_arrow_dir(drawing *dr, int cx, int cy, int sz, int dir,
                           int cfill, int cout)
{
    double ang = 2.0 * PI * (double)dir / 8.0 ;
    draw_arrow(dr, cx, cy, sz, ang, cfill, cout);
}

/* cx, cy are centre coordinates.. */
static void draw_star(drawing *dr, int cx, int cy, int rad, int npoints,
                      int cfill, int cout)
{
    int *coords, n;
    double a, r;

    assert(npoints > 0);

    coords = snewn(npoints * 2, int);

    for (n = 0; n < npoints; n++) {
        a = 2.0 * PI * ((double)n / ((double)npoints));
        r = (double)rad/2.0;
        coords[2*n+0] = cx + (int)( r * sin(a));
        coords[2*n+1] = cy + (int)(-r * cos(a));
    }
    draw_polygon(dr, coords, npoints, cfill, cout);

    for (n = 0; n < npoints; n++) {
        a = 2.0 * PI * ((double)n / ((double)npoints));
        r = (double)rad;
        coords[0] = cx + (int)( r * sin(a));
        coords[1] = cy + (int)(-r * cos(a));

        a = 2.0 * PI * ((double)(n-1) / ((double)npoints));
        r = (double)rad/2.0;
        coords[2] = cx + (int)( r * sin(a));
        coords[3] = cy + (int)(-r * cos(a));

        a = 2.0 * PI * ((double)(n+1) / ((double)npoints));
        r = (double)rad/2.0;
        coords[4] = cx + (int)( r * sin(a));
        coords[5] = cy + (int)(-r * cos(a));
        draw_polygon(dr, coords, 3, cfill, cout);
    }

    sfree(coords);
}

#define ARROW_HALFSZ (7 * TILE_SIZE / 32)

#define F_DRAG_SRC      0x001   /* Tile is source of a drag. */
#define F_ERROR         0x002   /* Tile marked in error. */
#define F_IMMUTABLE     0x004   /* Tile (number) is immutable. */
#define F_ARROW_POINT   0x008   /* Tile points to other tile */
#define F_ARROW_INPOINT 0x010   /* Other tile points in here. */
#define F_HIGHLIGHT     0x020   /* Tile is highlighted */

static void tile_redraw(drawing *dr, game_drawstate *ds, int tx, int ty,
                        int dir, int dirp, int num, unsigned int f)
{
    int cb = TILE_SIZE / 16, textsz;
    char buf[20];
    int arrowcol, sarrowcol, setcol, textcol;
    int acx, acy, asz;
    int set = num / (ds->n+1);
    bool isset = (set != 0);
    bool empty = (num == 0 && !(f & F_ARROW_POINT) && !(f & F_ARROW_INPOINT));
    bool isseq = ((f & F_ARROW_POINT) || (f & F_ARROW_INPOINT));
    bool midseq = ((f & F_ARROW_POINT) && (f & F_ARROW_INPOINT));
    bool startseq = ((f & F_ARROW_POINT) && !(f & F_ARROW_INPOINT));
    /* bool endseq = (!(f & F_ARROW_POINT) && (f & F_ARROW_INPOINT)); */

    /* Calculate colours. */

    setcol = (f & F_ERROR) ? COL_BLACK :
             isset         ? COL_LIGHTGREY :
             isseq         ? COL_VLIGHTGREY :
                             COL_WHITE;

    arrowcol = (f & F_ERROR)                   ? COL_VLIGHTGREY :
               (isset && (startseq || midseq)) ? COL_VLIGHTGREY :
               (startseq || midseq)            ? COL_DARKGREY :
                                                 COL_BLACK;

    textcol = (f & F_ERROR)       ? COL_VLIGHTGREY :
              (isset && (midseq)) ? COL_VLIGHTGREY :
              (midseq)            ? COL_DARKGREY :
                                    COL_BLACK;

    sarrowcol = (f & F_ERROR)   ? COL_VLIGHTGREY :
                                  COL_BLACK;

    if ((f & F_DRAG_SRC) || (f & F_HIGHLIGHT)) {
        arrowcol = (f & F_ERROR) ? COL_DARKGREY :
                    isset        ? COL_DARKGREY :
                    isseq        ? COL_LIGHTGREY :
                                   COL_LIGHTGREY;
    }

    /* Clear tile background */
    draw_rect(dr, tx, ty, TILE_SIZE, TILE_SIZE, setcol);

    /* Draw large (outwards-pointing) arrow. */
    asz = ARROW_HALFSZ;         /* 'radius' of arrow/star. */
    acx = tx+TILE_SIZE/2+asz;   /* centre x */
    acy = ty+TILE_SIZE/2+asz;   /* centre y */

    if (num == ds->n && (f & F_IMMUTABLE))
        draw_star(dr, acx, acy, asz, 5, arrowcol, arrowcol);
    else
        draw_arrow_dir(dr, acx, acy, asz, dir, arrowcol, arrowcol);

    /* Draw dot if this tile requires a predecessor and doesn't have one. */

    acx = tx+TILE_SIZE/2-asz;
    acy = ty+TILE_SIZE/2+asz;

    if (!(f & F_ARROW_INPOINT) && num != 1) {
        draw_circle(dr, acx, acy, asz / 4, sarrowcol, sarrowcol);
    }

    /* Draw small triangle indicator if this tile is immutable */
    if (f & F_IMMUTABLE) {
        int coords[6];
        coords[0] = tx+TILE_SIZE;     coords[1] = ty+7*TILE_SIZE/8;
        coords[2] = tx+TILE_SIZE;     coords[3] = ty+TILE_SIZE;
        coords[4] = tx+7*TILE_SIZE/8; coords[5] = ty+TILE_SIZE;
        draw_polygon(dr, coords, 3, textcol, textcol);
    }

    /* Draw text (number or set). */

    if (!empty) {
        int set = (num <= 0) ? 0 : num / (ds->n+1);

        char *p = buf;
        if (set == 0 || num <= 0) {
            sprintf(buf, "%d", num);
        } else {
            int n = num % (ds->n+1);
            p += sizeof(buf) - 1;

            if (n != 0) {
                sprintf(buf, "+%d", n);  /* Just to get the length... */
                p -= strlen(buf);
                sprintf(p, "+%d", n);
            } else {
                *p = '\0';
            }
            do {
                set--;
                p--;
                *p = (char)((set % 26)+'a');
                set /= 26;
            } while (set);
        }
        textsz = min(2*asz, (TILE_SIZE - 2 * cb) / (int)strlen(p));
        draw_text(dr, tx + cb, ty + TILE_SIZE/4, FONT_VARIABLE, textsz,
                  ALIGN_VCENTRE | ALIGN_HLEFT, textcol, p);
    }

    draw_rect_outline(dr, tx, ty, TILE_SIZE, TILE_SIZE, COL_VDARKGREY);
    draw_update(dr, tx, ty, TILE_SIZE, TILE_SIZE);
}
static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int x, y, xd, yd, i, w = ds->w, dirp;
    bool force = false;
    unsigned int f;
    game_state *postdrop = NULL;
    char buf[48];

    /* Draw status bar */
    sprintf(buf, "%s",
            state->used_solve ? "Auto-solved." :
            state->completed  ? "COMPLETED!" : "");
    status_bar(dr, buf);
    
    xd = FROMCOORD(ui->dx); yd = FROMCOORD(ui->dy);

    /* If an in-progress drag would make a valid move if finished, we
     * reflect that move in the board display. We let interpret_move do
     * most of the heavy lifting for us: we have to copy the game_ui so
     * as not to stomp on the real UI's drag state. */
    if (ui->dragging) {
        game_ui uicopy = *ui;
        char *movestr = interpret_move(state, &uicopy, ds, ui->dx, ui->dy, LEFT_RELEASE, false);

        if (movestr != NULL && strcmp(movestr, "") != 0) {
            postdrop = execute_move(state, NULL, movestr);
            sfree(movestr);

            state = postdrop;
        }
    }

    if (!ds->started) {
        int aw = TILE_SIZE * state->w;
        int ah = TILE_SIZE * state->h;
        draw_rect_outline(dr, BORDER - 1, BORDER - 1, aw + 2, ah + 2, COL_VDARKGREY);
        draw_update(dr, 0, 0, aw + 2 * BORDER, ah + 2 * BORDER);
    }
    for (x = 0; x < state->w; x++) {
        for (y = 0; y < state->h; y++) {
            i = y*w + x;
            f = 0;
            dirp = -1;

            if (ui->dragging && ui->show_highlight) {
                if (x == ui->sx && y == ui->sy)
                    f |= F_DRAG_SRC;
                else if (ui->drag_is_from && 
                         !(x == xd && y == yd) && 
                         ispointing(state, x, y, ui->sx, ui->sy))
                        f |= F_HIGHLIGHT; 
            }

            if (state->impossible ||
                state->nums[i] < 0 || state->flags[i] & FLAG_ERROR)
                f |= F_ERROR;
            if (state->flags[i] & FLAG_IMMUTABLE)
                f |= F_IMMUTABLE;

            if (state->next[i] != -1)
                f |= F_ARROW_POINT;

            if (state->prev[i] != -1) {
                /* Currently the direction here is from our square _back_
                 * to its previous. We could change this to give the opposite
                 * sense to the direction. */
                f |= F_ARROW_INPOINT;
                dirp = whichdir(x, y, state->prev[i]%w, state->prev[i]/w);
            }

            if (state->nums[i] != ds->nums[i] ||
                f != ds->f[i] || dirp != ds->dirp[i] ||
                force || !ds->started) {
                tile_redraw(dr, ds,
                            BORDER + x * TILE_SIZE,
                            BORDER + y * TILE_SIZE,
                            state->dirs[i], dirp, state->nums[i], f);
                ds->nums[i] = state->nums[i];
                ds->f[i] = f;
                ds->dirp[i] = dirp;
            }
        }
    }
    if (postdrop) free_game(postdrop);
    if (!ds->started) ds->started = true;
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
#define thegame signpost
#endif

static const char rules[] = "You have a grid of squares; each square (except the last one) contains an arrow, and some squares also contain numbers. Your job is to connect the squares to form a continuous list of numbers starting at 1 and linked in the direction of the arrows - so the arrow inside the square with the number 1 will point to the square containing the number 2, which will point to the square containing the number 3, etc. Each square can be any distance away from the previous one, as long as it is somewhere in the direction of the arrow.\n\n"
"- If you connect together two squares where one of them has a number in it, the appropriate number will appear in the other square.\n"
"- If you connect two non-numbered squares, they will be assigned temporary labels ('a' / 'a+1' etc.), that will be replaced with numbers once you connect the sequence with a numbered square.\n\n\n"
"This puzzle was contributed by James Harvey.";

const struct game thegame = {
    "Signpost", "games.signpost", "signpost", rules,
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
    NULL,  /* game_get_cursor_location */
    game_status,
    false, false, NULL, NULL,  /* print_size, print */
    true,                      /* wants_statusbar */
    false, NULL,               /* timing_state */
    REQUIRE_RBUTTON,           /* flags */
};

