/*
 * unruly.c: Implementation for Binary Puzzles.
 * (C) 2012 Lennard Sprong
 * Created for Simon Tatham's Portable Puzzle Collection
 * See LICENCE for licence details
 *
 * Objective of the game: Fill the grid with zeros and ones, with the
 * following rules:
 * - There can't be a run of three or more equal numbers.
 * - Each row and column contains an equal amount of zeros and ones.
 *
 * This puzzle type is known under several names, including
 * Tohu-Wa-Vohu, One and Two and Binairo.
 *
 * Some variants include an extra constraint, stating that no two rows or two
 * columns may contain the same exact sequence of zeros and ones.
 * This rule is rarely used, so it is not enabled in the default presets
 * (but it can be selected via the Custom configurer).
 *
 * More information:
 * http://www.janko.at/Raetsel/Tohu-Wa-Vohu/index.htm
 */

/*
 * Possible future improvements:
 *
 * More solver cleverness
 *
 *  - a counting-based deduction in which you find groups of squares
 *    which must each contain at least one of a given colour, plus
 *    other squares which are already known to be that colour, and see
 *    if you have any squares left over when you've worked out where
 *    they all have to be. This is a generalisation of the current
 *    check_near_complete: where that only covers rows with three
 *    unfilled squares, this would handle more, such as
 *        0 . . 1 0 1 . . 0 .
 *    in which each of the two-square gaps must contain a 0, and there
 *    are three 0s placed, and that means the rightmost square can't
 *    be a 0.
 *
 *  - an 'Unreasonable' difficulty level, supporting recursion and
 *    backtracking.
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
    COL_EMPTY,
    /*
     * When editing this enum, maintain the invariants
     *   COL_n_HIGHLIGHT = COL_n + 1
     *   COL_n_LOWLIGHT = COL_n + 2
     */
    COL_0,
    COL_0_HIGHLIGHT,
    COL_0_LOWLIGHT,
    COL_1,
    COL_1_HIGHLIGHT,
    COL_1_LOWLIGHT,
    COL_ERROR,
    NCOLOURS
};

enum {
    PREF_CLICK_ACTIONS,
    PREF_SHOW_ERRORS,
    N_PREF_ITEMS
};

struct game_params {
    int w2, h2;        /* full grid width and height respectively */
    bool unique;       /* should row and column patterns be unique? */
    int diff;
};
#define DIFFLIST(A)                             \
    A(EASY,Easy, e)                             \
    A(NORMAL,Normal, n)                         \
    A(TRICKY,Tricky, t)                         \

#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const unruly_diffnames[] = { DIFFLIST(TITLE) };

static char const unruly_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

static const struct game_params unruly_presets[] = {
    { 6,  6, false, DIFF_EASY},
    { 6,  6, false, DIFF_NORMAL},
    { 6,  6, true, DIFF_TRICKY},
    { 8,  8, true, DIFF_TRICKY},
    {10, 10, true, DIFF_TRICKY},
    {12, 12, true, DIFF_TRICKY}
};

#define DEFAULT_PRESET 3

enum {
    EMPTY,
    N_ONE,
    N_ZERO,
    BOGUS
};

#define FE_HOR_ROW_LEFT   0x0001
#define FE_HOR_ROW_MID    0x0003
#define FE_HOR_ROW_RIGHT  0x0002

#define FE_VER_ROW_TOP    0x0004
#define FE_VER_ROW_MID    0x000C
#define FE_VER_ROW_BOTTOM 0x0008

#define FE_COUNT          0x0010

#define FE_ROW_MATCH      0x0020
#define FE_COL_MATCH      0x0040

#define FF_ONE            0x0080
#define FF_ZERO           0x0100

#define FF_IMMUTABLE      0x1000

typedef struct unruly_common {
    int refcount;
    bool *immutable;
} unruly_common;

struct game_state {
    int w2, h2;
    bool unique;
    char *grid;
    unruly_common *common;

    bool completed, cheated;
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    *ret = unruly_presets[DEFAULT_PRESET];        /* structure copy */

    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(unruly_presets))
        return false;

    ret = snew(game_params);
    *ret = unruly_presets[i];     /* structure copy */

    sprintf(buf, "%dx%d %s%s", ret->w2, ret->h2, 
        unruly_diffnames[ret->diff], ret->unique ? ", uniq" : "");

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
    *ret = *params;             /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
    char const *p = string;

    params->unique = false;

    params->w2 = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;
    if (*p == 'x') {
        p++;
        params->h2 = atoi(p);
        while (*p && isdigit((unsigned char)*p)) p++;
    } else {
        params->h2 = params->w2;
    }

    if (*p == 'u') {
        p++;
        params->unique = true;
    }

    if (*p == 'd') {
        int i;
        p++;
        params->diff = DIFFCOUNT + 1;   /* ...which is invalid */
        if (*p) {
            for (i = 0; i < DIFFCOUNT; i++) {
                if (*p == unruly_diffchars[i])
                    params->diff = i;
            }
            p++;
        }
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char buf[80];

    sprintf(buf, "%dx%d", params->w2, params->h2);
    if (params->unique)
        strcat(buf, "u");
    if (full)
        sprintf(buf + strlen(buf), "d%c", unruly_diffchars[params->diff]);

    return dupstr(buf);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];

    ret = snewn(5, config_item);

    ret[0].name = "Width";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->w2);
    ret[0].u.string.sval = dupstr(buf);

    ret[1].name = "Height";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->h2);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = "Unique rows and columns";
    ret[2].type = C_BOOLEAN;
    ret[2].u.boolean.bval = params->unique;

    ret[3].name = "Difficulty";
    ret[3].type = C_CHOICES;
    ret[3].u.choices.choicenames = DIFFCONFIG;
    ret[3].u.choices.selected = params->diff;

    ret[4].name = NULL;
    ret[4].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->w2 = atoi(cfg[0].u.string.sval);
    ret->h2 = atoi(cfg[1].u.string.sval);
    ret->unique = cfg[2].u.boolean.bval;
    ret->diff = cfg[3].u.choices.selected;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if ((params->w2 & 1) || (params->h2 & 1))
        return "Width and height must both be even";
    if (params->w2 < 6 || params->h2 < 6)
        return "Width and height must be at least 6";
    if (params->w2 > 16 || params->h2 > 16)
        return "Width and height must be at most 16";
    if (params->unique) {
        static const long A177790[] = {
            /*
             * The nth element of this array gives the number of
             * distinct possible Unruly rows of length 2n (that is,
             * containing exactly n 1s and n 0s and not containing
             * three consecutive elements the same) for as long as
             * those numbers fit in a 32-bit signed int.
             *
             * So in unique-rows mode, if the puzzle width is 2n, then
             * the height must be at most (this array)[n], and vice
             * versa.
             *
             * This is sequence A177790 in the Online Encyclopedia of
             * Integer Sequences: http://oeis.org/A177790
             */
            1L, 2L, 6L, 14L, 34L, 84L, 208L, 518L, 1296L, 3254L,
            8196L, 20700L, 52404L, 132942L, 337878L, 860142L,
            2192902L, 5598144L, 14308378L, 36610970L, 93770358L,
            240390602L, 616787116L, 1583765724L
        };
        if (params->w2 < 2*lenof(A177790) &&
            params->h2 > A177790[params->w2/2]) {
            return "Puzzle is too tall for unique-rows mode";
        }
        if (params->h2 < 2*lenof(A177790) &&
            params->w2 > A177790[params->h2/2]) {
            return "Puzzle is too long for unique-rows mode";
        }
        if (params->diff == DIFF_EASY)
            return "Unique rows aren't used for easy puzzles";
    }
    if (params->diff >= DIFFCOUNT)
        return "Unknown difficulty rating";

    return NULL;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    int w2 = params->w2, h2 = params->h2;
    int s = w2 * h2;

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

    if (pos < s+1)
        return "Description too short";
    if (pos > s+1)
        return "Description too long";

    return NULL;
}

static game_state *blank_state(int w2, int h2, bool unique, bool new_common)
{
    game_state *state = snew(game_state);
    int s = w2 * h2;

    state->w2 = w2;
    state->h2 = h2;
    state->unique = unique;
    state->grid = snewn(s, char);
    memset(state->grid, EMPTY, s);

    if (new_common) {
        state->common = snew(unruly_common);
        state->common->refcount = 1;
        state->common->immutable = snewn(s, bool);
        memset(state->common->immutable, 0, s*sizeof(bool));
    }

    state->completed = state->cheated = false;

    return state;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    int w2 = params->w2, h2 = params->h2;
    int s = w2 * h2;

    game_state *state = blank_state(w2, h2, params->unique, true);

    const char *p = desc;
    int pos = 0;

    while (*p) {
        if (*p >= 'a' && *p < 'z') {
            pos += (*p - 'a');
            if (pos < s) {
                state->grid[pos] = N_ZERO;
                state->common->immutable[pos] = true;
            }
            pos++;
        } else if (*p >= 'A' && *p < 'Z') {
            pos += (*p - 'A');
            if (pos < s) {
                state->grid[pos] = N_ONE;
                state->common->immutable[pos] = true;
            }
            pos++;
        } else if (*p == 'Z' || *p == 'z') {
            pos += 25;
        } else
            assert(!"Description contains invalid characters");

        ++p;
    }
    assert(pos == s+1);

    return state;
}

static game_state *dup_game(const game_state *state)
{
    int w2 = state->w2, h2 = state->h2;
    int s = w2 * h2;

    game_state *ret = blank_state(w2, h2, state->unique, false);

    memcpy(ret->grid, state->grid, s);
    ret->common = state->common;
    ret->common->refcount++;

    ret->completed = state->completed;
    ret->cheated = state->cheated;

    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->grid);
    if (--state->common->refcount == 0) {
        sfree(state->common->immutable);
        sfree(state->common);
    }

    sfree(state);
}

/* ****** *
 * Solver *
 * ****** */

struct unruly_scratch {
    int *ones_rows;
    int *ones_cols;
    int *zeros_rows;
    int *zeros_cols;
};

static void unruly_solver_update_remaining(const game_state *state,
                                           struct unruly_scratch *scratch)
{
    int w2 = state->w2, h2 = state->h2;
    int x, y;

    /* Reset all scratch data */
    memset(scratch->ones_rows, 0, h2 * sizeof(int));
    memset(scratch->ones_cols, 0, w2 * sizeof(int));
    memset(scratch->zeros_rows, 0, h2 * sizeof(int));
    memset(scratch->zeros_cols, 0, w2 * sizeof(int));

    for (x = 0; x < w2; x++)
        for (y = 0; y < h2; y++) {
            if (state->grid[y * w2 + x] == N_ONE) {
                scratch->ones_rows[y]++;
                scratch->ones_cols[x]++;
            } else if (state->grid[y * w2 + x] == N_ZERO) {
                scratch->zeros_rows[y]++;
                scratch->zeros_cols[x]++;
            }
        }
}

static struct unruly_scratch *unruly_new_scratch(const game_state *state)
{
    int w2 = state->w2, h2 = state->h2;

    struct unruly_scratch *ret = snew(struct unruly_scratch);

    ret->ones_rows = snewn(h2, int);
    ret->ones_cols = snewn(w2, int);
    ret->zeros_rows = snewn(h2, int);
    ret->zeros_cols = snewn(w2, int);

    unruly_solver_update_remaining(state, ret);

    return ret;
}

static void unruly_free_scratch(struct unruly_scratch *scratch)
{
    sfree(scratch->ones_rows);
    sfree(scratch->ones_cols);
    sfree(scratch->zeros_rows);
    sfree(scratch->zeros_cols);

    sfree(scratch);
}

static int unruly_solver_check_threes(game_state *state, int *rowcount,
                                      int *colcount, bool horizontal,
                                      char check, char block)
{
    int w2 = state->w2, h2 = state->h2;

    int dx = horizontal ? 1 : 0, dy = 1 - dx;
    int sx = dx, sy = dy;
    int ex = w2 - dx, ey = h2 - dy;

    int x, y;
    int ret = 0;

    /* Check for any three squares which almost form three in a row */
    for (y = sy; y < ey; y++) {
        for (x = sx; x < ex; x++) {
            int i1 = (y-dy) * w2 + (x-dx);
            int i2 = y * w2 + x;
            int i3 = (y+dy) * w2 + (x+dx);

            if (state->grid[i1] == check && state->grid[i2] == check
                && state->grid[i3] == EMPTY) {
                ret++;
                state->grid[i3] = block;
                rowcount[i3 / w2]++;
                colcount[i3 % w2]++;
            }
            if (state->grid[i1] == check && state->grid[i2] == EMPTY
                && state->grid[i3] == check) {
                ret++;
                state->grid[i2] = block;
                rowcount[i2 / w2]++;
                colcount[i2 % w2]++;
            }
            if (state->grid[i1] == EMPTY && state->grid[i2] == check
                && state->grid[i3] == check) {
                ret++;
                state->grid[i1] = block;
                rowcount[i1 / w2]++;
                colcount[i1 % w2]++;
            }
        }
    }

    return ret;
}

static int unruly_solver_check_all_threes(game_state *state,
                                          struct unruly_scratch *scratch)
{
    int ret = 0;

    ret +=
        unruly_solver_check_threes(state, scratch->zeros_rows,
                                   scratch->zeros_cols, true, N_ONE, N_ZERO);
    ret +=
        unruly_solver_check_threes(state, scratch->ones_rows,
                                   scratch->ones_cols, true, N_ZERO, N_ONE);
    ret +=
        unruly_solver_check_threes(state, scratch->zeros_rows,
                                   scratch->zeros_cols, false, N_ONE,
                                   N_ZERO);
    ret +=
        unruly_solver_check_threes(state, scratch->ones_rows,
                                   scratch->ones_cols, false, N_ZERO, N_ONE);

    return ret;
}

static int unruly_solver_check_uniques(game_state *state, int *rowcount,
                                       bool horizontal, char check, char block,
                                       struct unruly_scratch *scratch)
{
    int w2 = state->w2, h2 = state->h2;

    int rmult = (horizontal ? w2 : 1);
    int cmult = (horizontal ? 1 : w2);
    int nr = (horizontal ? h2 : w2);
    int nc = (horizontal ? w2 : h2);
    int max = nc / 2;

    int r, r2, c;
    int ret = 0;

    /*
     * Find each row that has max entries of type 'check', and see if
     * all those entries match those in any row with max-1 entries. If
     * so, set the last non-matching entry of the latter row to ensure
     * that it's different.
     */
    for (r = 0; r < nr; r++) {
        if (rowcount[r] != max)
            continue;
        for (r2 = 0; r2 < nr; r2++) {
            int nmatch = 0, nonmatch = -1;
            if (rowcount[r2] != max-1)
                continue;
            for (c = 0; c < nc; c++) {
                if (state->grid[r*rmult + c*cmult] == check) {
                    if (state->grid[r2*rmult + c*cmult] == check)
                        nmatch++;
                    else
                        nonmatch = c;
                }
            }
            if (nmatch == max-1) {
                int i1 = r2 * rmult + nonmatch * cmult;
                assert(nonmatch != -1);
                if (state->grid[i1] == block)
                    continue;
                assert(state->grid[i1] == EMPTY);
                state->grid[i1] = block;
                if (block == N_ONE) {
                    scratch->ones_rows[i1 / w2]++;
                    scratch->ones_cols[i1 % w2]++;
                } else {
                    scratch->zeros_rows[i1 / w2]++;
                    scratch->zeros_cols[i1 % w2]++;
                }
                ret++;
            }
        }
    }
    return ret;
}

static int unruly_solver_check_all_uniques(game_state *state,
                                           struct unruly_scratch *scratch)
{
    int ret = 0;

    ret += unruly_solver_check_uniques(state, scratch->ones_rows,
                                       true, N_ONE, N_ZERO, scratch);
    ret += unruly_solver_check_uniques(state, scratch->zeros_rows,
                                       true, N_ZERO, N_ONE, scratch);
    ret += unruly_solver_check_uniques(state, scratch->ones_cols,
                                       false, N_ONE, N_ZERO, scratch);
    ret += unruly_solver_check_uniques(state, scratch->zeros_cols,
                                       false, N_ZERO, N_ONE, scratch);

    return ret;
}

static int unruly_solver_fill_row(game_state *state, int i, bool horizontal,
                                  int *rowcount, int *colcount, char fill)
{
    int ret = 0;
    int w2 = state->w2, h2 = state->h2;
    int j;

    /* Place a number in every empty square in a row/column */
    for (j = 0; j < (horizontal ? w2 : h2); j++) {
        int p = (horizontal ? i * w2 + j : j * w2 + i);

        if (state->grid[p] == EMPTY) {
            ret++;
            state->grid[p] = fill;
            rowcount[(horizontal ? i : j)]++;
            colcount[(horizontal ? j : i)]++;
        }
    }

    return ret;
}

static int unruly_solver_check_single_gap(game_state *state,
                                          int *complete, bool horizontal,
                                          int *rowcount, int *colcount,
                                          char fill)
{
    int w2 = state->w2, h2 = state->h2;
    int count = (horizontal ? h2 : w2); /* number of rows to check */
    int target = (horizontal ? w2 : h2) / 2; /* target number of 0s/1s */
    int *other = (horizontal ? rowcount : colcount);

    int ret = 0;

    int i;
    /* Check for completed rows/cols for one number, then fill in the rest */
    for (i = 0; i < count; i++) {
        if (complete[i] == target && other[i] == target - 1) {
            ret += unruly_solver_fill_row(state, i, horizontal, rowcount,
                                          colcount, fill);
        }
    }

    return ret;
}

static int unruly_solver_check_all_single_gap(game_state *state,
                                              struct unruly_scratch *scratch)
{
    int ret = 0;

    ret +=
        unruly_solver_check_single_gap(state, scratch->ones_rows, true,
                                       scratch->zeros_rows,
                                       scratch->zeros_cols, N_ZERO);
    ret +=
        unruly_solver_check_single_gap(state, scratch->ones_cols, false,
                                       scratch->zeros_rows,
                                       scratch->zeros_cols, N_ZERO);
    ret +=
        unruly_solver_check_single_gap(state, scratch->zeros_rows, true,
                                       scratch->ones_rows,
                                       scratch->ones_cols, N_ONE);
    ret +=
        unruly_solver_check_single_gap(state, scratch->zeros_cols, false,
                                       scratch->ones_rows,
                                       scratch->ones_cols, N_ONE);

    return ret;
}

static int unruly_solver_check_complete_nums(game_state *state,
                                             int *complete, bool horizontal,
                                             int *rowcount, int *colcount,
                                             char fill)
{
    int w2 = state->w2, h2 = state->h2;
    int count = (horizontal ? h2 : w2); /* number of rows to check */
    int target = (horizontal ? w2 : h2) / 2; /* target number of 0s/1s */
    int *other = (horizontal ? rowcount : colcount);

    int ret = 0;

    int i;
    /* Check for completed rows/cols for one number, then fill in the rest */
    for (i = 0; i < count; i++) {
        if (complete[i] == target && other[i] < target) {
            ret += unruly_solver_fill_row(state, i, horizontal, rowcount,
                                          colcount, fill);
        }
    }

    return ret;
}

static int unruly_solver_check_all_complete_nums(game_state *state,
                                                 struct unruly_scratch *scratch)
{
    int ret = 0;

    ret +=
        unruly_solver_check_complete_nums(state, scratch->ones_rows, true,
                                          scratch->zeros_rows,
                                          scratch->zeros_cols, N_ZERO);
    ret +=
        unruly_solver_check_complete_nums(state, scratch->ones_cols, false,
                                          scratch->zeros_rows,
                                          scratch->zeros_cols, N_ZERO);
    ret +=
        unruly_solver_check_complete_nums(state, scratch->zeros_rows, true,
                                          scratch->ones_rows,
                                          scratch->ones_cols, N_ONE);
    ret +=
        unruly_solver_check_complete_nums(state, scratch->zeros_cols, false,
                                          scratch->ones_rows,
                                          scratch->ones_cols, N_ONE);

    return ret;
}

static int unruly_solver_check_near_complete(game_state *state,
                                             int *complete, bool horizontal,
                                             int *rowcount, int *colcount,
                                             char fill)
{
    int w2 = state->w2, h2 = state->h2;
    int w = w2/2, h = h2/2;

    int dx = horizontal ? 1 : 0, dy = 1 - dx;

    int sx = dx, sy = dy;
    int ex = w2 - dx, ey = h2 - dy;

    int x, y;
    int ret = 0;

    /*
     * This function checks for a row with one Y remaining, then looks
     * for positions that could cause the remaining squares in the row
     * to make 3 X's in a row. Example:
     *
     * Consider the following row:
     * 1 1 0 . . .
     * If the last 1 was placed in the last square, the remaining
     * squares would be 0:
     * 1 1 0 0 0 1
     * This violates the 3 in a row rule. We now know that the last 1
     * shouldn't be in the last cell.
     * 1 1 0 . . 0
     */

    /* Check for any two blank and one filled square */
    for (y = sy; y < ey; y++) {
        /* One type must have 1 remaining, the other at least 2 */
        if (horizontal && (complete[y] < w - 1 || rowcount[y] > w - 2))
            continue;

        for (x = sx; x < ex; x++) {
            int i, i1, i2, i3;
            if (!horizontal
                && (complete[x] < h - 1 || colcount[x] > h - 2))
                continue;

            i = (horizontal ? y : x);
            i1 = (y-dy) * w2 + (x-dx);
            i2 = y * w2 + x;
            i3 = (y+dy) * w2 + (x+dx);

            if (state->grid[i1] == fill && state->grid[i2] == EMPTY
                && state->grid[i3] == EMPTY) {
                /*
                 * Temporarily fill the empty spaces with something else.
                 * This avoids raising the counts for the row and column
                 */
                state->grid[i2] = BOGUS;
                state->grid[i3] = BOGUS;

                ret +=
                    unruly_solver_fill_row(state, i, horizontal, rowcount,
                                         colcount, fill);

                state->grid[i2] = EMPTY;
                state->grid[i3] = EMPTY;
            }

            else if (state->grid[i1] == EMPTY && state->grid[i2] == fill
                     && state->grid[i3] == EMPTY) {
                state->grid[i1] = BOGUS;
                state->grid[i3] = BOGUS;

                ret +=
                    unruly_solver_fill_row(state, i, horizontal, rowcount,
                                         colcount, fill);

                state->grid[i1] = EMPTY;
                state->grid[i3] = EMPTY;
            }

            else if (state->grid[i1] == EMPTY && state->grid[i2] == EMPTY
                     && state->grid[i3] == fill) {
                state->grid[i1] = BOGUS;
                state->grid[i2] = BOGUS;

                ret +=
                    unruly_solver_fill_row(state, i, horizontal, rowcount,
                                         colcount, fill);

                state->grid[i1] = EMPTY;
                state->grid[i2] = EMPTY;
            }

            else if (state->grid[i1] == EMPTY && state->grid[i2] == EMPTY
                     && state->grid[i3] == EMPTY) {
                state->grid[i1] = BOGUS;
                state->grid[i2] = BOGUS;
                state->grid[i3] = BOGUS;

                ret +=
                    unruly_solver_fill_row(state, i, horizontal, rowcount,
                                         colcount, fill);

                state->grid[i1] = EMPTY;
                state->grid[i2] = EMPTY;
                state->grid[i3] = EMPTY;
            }
        }
    }

    return ret;
}

static int unruly_solver_check_all_near_complete(game_state *state,
                                                 struct unruly_scratch *scratch)
{
    int ret = 0;

    ret +=
        unruly_solver_check_near_complete(state, scratch->ones_rows, true,
                                        scratch->zeros_rows,
                                        scratch->zeros_cols, N_ZERO);
    ret +=
        unruly_solver_check_near_complete(state, scratch->ones_cols, false,
                                        scratch->zeros_rows,
                                        scratch->zeros_cols, N_ZERO);
    ret +=
        unruly_solver_check_near_complete(state, scratch->zeros_rows, true,
                                        scratch->ones_rows,
                                        scratch->ones_cols, N_ONE);
    ret +=
        unruly_solver_check_near_complete(state, scratch->zeros_cols, false,
                                        scratch->ones_rows,
                                        scratch->ones_cols, N_ONE);

    return ret;
}

static int unruly_validate_rows(const game_state *state, bool horizontal,
                                char check, int *errors)
{
    int w2 = state->w2, h2 = state->h2;

    int dx = horizontal ? 1 : 0, dy = 1 - dx;

    int sx = dx, sy = dy;
    int ex = w2 - dx, ey = h2 - dy;

    int x, y;
    int ret = 0;

    int err1 = (horizontal ? FE_HOR_ROW_LEFT : FE_VER_ROW_TOP);
    int err2 = (horizontal ? FE_HOR_ROW_MID : FE_VER_ROW_MID);
    int err3 = (horizontal ? FE_HOR_ROW_RIGHT : FE_VER_ROW_BOTTOM);

    /* Check for any three in a row, and mark errors accordingly (if
     * required) */
    for (y = sy; y < ey; y++) {
        for (x = sx; x < ex; x++) {
            int i1 = (y-dy) * w2 + (x-dx);
            int i2 = y * w2 + x;
            int i3 = (y+dy) * w2 + (x+dx);

            if (state->grid[i1] == check && state->grid[i2] == check
                && state->grid[i3] == check) {
                ret++;
                if (errors) {
                    errors[i1] |= err1;
                    errors[i2] |= err2;
                    errors[i3] |= err3;
                }
            }
        }
    }

    return ret;
}

static int unruly_validate_unique(const game_state *state, bool horizontal,
                                  int *errors)
{
    int w2 = state->w2, h2 = state->h2;

    int rmult = (horizontal ? w2 : 1);
    int cmult = (horizontal ? 1 : w2);
    int nr = (horizontal ? h2 : w2);
    int nc = (horizontal ? w2 : h2);
    int err = (horizontal ? FE_ROW_MATCH : FE_COL_MATCH);

    int r, r2, c;
    int ret = 0;

    /* Check for any two full rows matching exactly, and mark errors
     * accordingly (if required) */
    for (r = 0; r < nr; r++) {
        int nfull = 0;
        for (c = 0; c < nc; c++)
            if (state->grid[r*rmult + c*cmult] != EMPTY)
                nfull++;
        if (nfull != nc)
            continue;
        for (r2 = r+1; r2 < nr; r2++) {
            bool match = true;
            for (c = 0; c < nc; c++)
                if (state->grid[r*rmult + c*cmult] !=
                    state->grid[r2*rmult + c*cmult])
                    match = false;
            if (match) {
                if (errors) {
                    for (c = 0; c < nc; c++) {
                        errors[r*rmult + c*cmult] |= err;
                        errors[r2*rmult + c*cmult] |= err;
                    }
                }
                ret++;
            }
        }
    }

    return ret;
}

static int unruly_validate_all_rows(const game_state *state, int *errors)
{
    int errcount = 0;

    errcount += unruly_validate_rows(state, true, N_ONE, errors);
    errcount += unruly_validate_rows(state, false, N_ONE, errors);
    errcount += unruly_validate_rows(state, true, N_ZERO, errors);
    errcount += unruly_validate_rows(state, false, N_ZERO, errors);

    if (state->unique) {
        errcount += unruly_validate_unique(state, true, errors);
        errcount += unruly_validate_unique(state, false, errors);
    }

    if (errcount)
        return -1;
    return 0;
}

static int unruly_validate_counts(const game_state *state,
                                  struct unruly_scratch *scratch, bool *errors)
{
    int w2 = state->w2, h2 = state->h2;
    int w = w2/2, h = h2/2;
    bool below = false;
    bool above = false;
    int i;

    /* See if all rows/columns are satisfied. If one is exceeded,
     * mark it as an error (if required)
     */

    bool hasscratch = true;
    if (!scratch) {
        scratch = unruly_new_scratch(state);
        hasscratch = false;
    }

    for (i = 0; i < w2; i++) {
        if (scratch->ones_cols[i] < h)
            below = true;
        if (scratch->zeros_cols[i] < h)
            below = true;

        if (scratch->ones_cols[i] > h) {
            above = true;
            if (errors)
                errors[2*h2 + i] = true;
        } else if (errors)
            errors[2*h2 + i] = false;

        if (scratch->zeros_cols[i] > h) {
            above = true;
            if (errors)
                errors[2*h2 + w2 + i] = true;
        } else if (errors)
            errors[2*h2 + w2 + i] = false;
    }
    for (i = 0; i < h2; i++) {
        if (scratch->ones_rows[i] < w)
            below = true;
        if (scratch->zeros_rows[i] < w)
            below = true;

        if (scratch->ones_rows[i] > w) {
            above = true;
            if (errors)
                errors[i] = true;
        } else if (errors)
            errors[i] = false;

        if (scratch->zeros_rows[i] > w) {
            above = true;
            if (errors)
                errors[h2 + i] = true;
        } else if (errors)
            errors[h2 + i] = false;
    }

    if (!hasscratch)
        unruly_free_scratch(scratch);

    return (above ? -1 : below ? 1 : 0);
}

static int unruly_solve_game(game_state *state,
                             struct unruly_scratch *scratch, int diff)
{
    int done, maxdiff = -1;

    while (true) {
        done = 0;

        /* Check for impending 3's */
        done += unruly_solver_check_all_threes(state, scratch);

        /* Keep using the simpler techniques while they produce results */
        if (done) {
            if (maxdiff < DIFF_EASY)
                maxdiff = DIFF_EASY;
            continue;
        }

        /* Check for rows with only one unfilled square */
        done += unruly_solver_check_all_single_gap(state, scratch);

        if (done) {
            if (maxdiff < DIFF_EASY)
                maxdiff = DIFF_EASY;
            continue;
        }

        /* Normal techniques */
        if (diff < DIFF_NORMAL)
            break;

        /* Check for completed rows */
        done += unruly_solver_check_all_complete_nums(state, scratch);

        if (done) {
            if (maxdiff < DIFF_NORMAL)
                maxdiff = DIFF_NORMAL;
            continue;
        }

        /* Check for impending failures of row/column uniqueness, if
         * it's enabled in this game mode */
        if (state->unique) {
            done += unruly_solver_check_all_uniques(state, scratch);

            if (done) {
                if (maxdiff < DIFF_NORMAL)
                    maxdiff = DIFF_NORMAL;
                continue;
            }
        }

        /* Tricky techniques */
        if (diff < DIFF_TRICKY)
            break;

        /* Check for nearly completed rows */
        done += unruly_solver_check_all_near_complete(state, scratch);

        if (done) {
            if (maxdiff < DIFF_TRICKY)
                maxdiff = DIFF_TRICKY;
            continue;
        }

        break;
    }
    return maxdiff;
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
    game_state *solved = dup_game(state);
    struct unruly_scratch *scratch = unruly_new_scratch(solved);
    char *ret = NULL;
    int result;

    unruly_solve_game(solved, scratch, DIFFCOUNT);

    result = unruly_validate_counts(solved, scratch, NULL);
    if (unruly_validate_all_rows(solved, NULL) == -1)
        result = -1;

    if (result == 0) {
        int w2 = solved->w2, h2 = solved->h2;
        int s = w2 * h2;
        char *p;
        int i;

        ret = snewn(s + 2, char);
        p = ret;
        *p++ = 'S';

        for (i = 0; i < s; i++)
            *p++ = (solved->grid[i] == N_ONE ? '1' : '0');

        *p++ = '\0';
    } else if (result == 1)
        *error = "No solution found.";
    else if (result == -1)
        *error = "Puzzle is invalid.";

    free_game(solved);
    unruly_free_scratch(scratch);
    return ret;
}

/* ********* *
 * Generator *
 * ********* */

static bool unruly_fill_game(game_state *state, struct unruly_scratch *scratch,
                             random_state *rs)
{

    int w2 = state->w2, h2 = state->h2;
    int s = w2 * h2;
    int i, j;
    int *spaces;

    /* Generate random array of spaces */
    spaces = snewn(s, int);
    for (i = 0; i < s; i++)
        spaces[i] = i;
    shuffle(spaces, s, sizeof(*spaces), rs);

    /*
     * Construct a valid filled grid by repeatedly picking an unfilled
     * space and fill it, then calling the solver to fill in any
     * spaces forced by the change.
     */
    for (j = 0; j < s; j++) {
        i = spaces[j];

        if (state->grid[i] != EMPTY)
            continue;

        if (random_upto(rs, 2)) {
            state->grid[i] = N_ONE;
            scratch->ones_rows[i / w2]++;
            scratch->ones_cols[i % w2]++;
        } else {
            state->grid[i] = N_ZERO;
            scratch->zeros_rows[i / w2]++;
            scratch->zeros_cols[i % w2]++;
        }

        unruly_solve_game(state, scratch, DIFFCOUNT);
    }
    sfree(spaces);

    if (unruly_validate_all_rows(state, NULL) != 0
        || unruly_validate_counts(state, scratch, NULL) != 0)
        return false;

    return true;
}

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive)
{
    int w2 = params->w2, h2 = params->h2;
    int s = w2 * h2;
    int *spaces;
    int i, j, run;
    char *ret, *p;

    game_state *state;
    struct unruly_scratch *scratch;

    while (1) {
        while (true) {
            state = blank_state(w2, h2, params->unique, true);
            scratch = unruly_new_scratch(state);
            if (unruly_fill_game(state, scratch, rs))
                break;
            free_game(state);
            unruly_free_scratch(scratch);
        }

        unruly_free_scratch(scratch);

        /* Generate random array of spaces */
        spaces = snewn(s, int);
        for (i = 0; i < s; i++)
            spaces[i] = i;
        shuffle(spaces, s, sizeof(*spaces), rs);

        /*
         * Winnow the clues by starting from our filled grid, repeatedly
         * picking a filled space and emptying it, as long as the solver
         * reports that the puzzle can still be solved after doing so.
         */
        for (j = 0; j < s; j++) {
            char c;
            game_state *solver;

            i = spaces[j];

            c = state->grid[i];
            state->grid[i] = EMPTY;

            solver = dup_game(state);
            scratch = unruly_new_scratch(state);

            unruly_solve_game(solver, scratch, params->diff);

            if (unruly_validate_counts(solver, scratch, NULL) != 0)
                state->grid[i] = c;

            free_game(solver);
            unruly_free_scratch(scratch);
        }
        sfree(spaces);

        /*
         * See if the game has accidentally come out too easy.
         */
        if (params->diff > 0) {
            bool ok;
            game_state *solver;

            solver = dup_game(state);
            scratch = unruly_new_scratch(state);

            unruly_solve_game(solver, scratch, params->diff - 1);

            ok = unruly_validate_counts(solver, scratch, NULL) > 0;

            free_game(solver);
            unruly_free_scratch(scratch);

            if (ok)
                break;
        } else {
            /*
             * Puzzles of the easiest difficulty can't be too easy.
             */
            break;
        }
    }

    /* Encode description */
    ret = snewn(s + 1, char);
    p = ret;
    run = 0;
    for (i = 0; i < s+1; i++) {
        if (i == s || state->grid[i] == N_ZERO) {
            while (run > 24) {
                *p++ = 'z';
                run -= 25;
            }
            *p++ = 'a' + run;
            run = 0;
        } else if (state->grid[i] == N_ONE) {
            while (run > 24) {
                *p++ = 'Z';
                run -= 25;
            }
            *p++ = 'A' + run;
            run = 0;
        } else {
            run++;
        }
    }
    *p = '\0';

    free_game(state);

    return ret;
}

/* ************** *
 * User Interface *
 * ************** */

struct game_ui {
    int click_mode;
    bool show_errors;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ret = snew(game_ui);

    ret->click_mode = 0;
    ret->show_errors = true;

    return ret;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static config_item *get_prefs(game_ui *ui)
{
    config_item *ret;

    ret = snewn(N_PREF_ITEMS+1, config_item);

    ret[PREF_CLICK_ACTIONS].name = "Short/Long click actions";
    ret[PREF_CLICK_ACTIONS].kw = "short-long";
    ret[PREF_CLICK_ACTIONS].type = C_CHOICES;
    ret[PREF_CLICK_ACTIONS].u.choices.choicenames = ":Black/White:White/Black";
    ret[PREF_CLICK_ACTIONS].u.choices.choicekws = ":black:white";
    ret[PREF_CLICK_ACTIONS].u.choices.selected = ui->click_mode;

    ret[PREF_SHOW_ERRORS].name = "Show errors";
    ret[PREF_SHOW_ERRORS].kw = "show-errors";
    ret[PREF_SHOW_ERRORS].type = C_BOOLEAN;
    ret[PREF_SHOW_ERRORS].u.boolean.bval = ui->show_errors;

    ret[N_PREF_ITEMS].name = NULL;
    ret[N_PREF_ITEMS].type = C_END;

    return ret;
}

static void set_prefs(game_ui *ui, const config_item *cfg)
{
    ui->click_mode = cfg[PREF_CLICK_ACTIONS].u.choices.selected;
    ui->show_errors = cfg[PREF_SHOW_ERRORS].u.boolean.bval;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

struct game_drawstate {
    int tilesize;
    int w2, h2;
    bool started;

    int *gridfs;
    bool *rowfs;

    int *grid;
};

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);

    int w2 = state->w2, h2 = state->h2;
    int s = w2 * h2;
    int i;

    ds->tilesize = 0;
    ds->w2 = w2;
    ds->h2 = h2;
    ds->started = false;

    ds->gridfs = snewn(s, int);
    ds->rowfs = snewn(2 * (w2 + h2), bool);

    ds->grid = snewn(s, int);
    for (i = 0; i < s; i++)
        ds->grid[i] = -1;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->gridfs);
    sfree(ds->rowfs);
    sfree(ds->grid);
    sfree(ds);
}

#define COORD(x)     ( (x) * ds->tilesize + ds->tilesize/2 )
#define FROMCOORD(x) ( ((x)-(ds->tilesize/2)) / ds->tilesize )

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int ox, int oy, int button, bool swapped)
{
    int gx = FROMCOORD(ox);
    int gy = FROMCOORD(oy);

    int w2 = state->w2, h2 = state->h2;

    button = STRIP_BUTTON_MODIFIERS(button);

    if (ox < (ds->tilesize / 2) || gx >= w2 ||
        oy < (ds->tilesize / 2) || gy >= h2)
            return MOVE_NO_EFFECT;

    /* Place one */
    if (button == LEFT_BUTTON || button == RIGHT_BUTTON ) {
        char buf[80];
        char c, i;

        if (state->common->immutable[gy * w2 + gx])
            return MOVE_NO_EFFECT;

        c = '-';
        i = state->grid[gy * w2 + gx];

        if (button == '0' || button == '2')
            c = '0';
        else if (button == '1')
            c = '1';

        /* Cycle through options */
        else if ((button == RIGHT_BUTTON && ui->click_mode == 0) ||
                 (button == LEFT_BUTTON && ui->click_mode == 1))
            c = (i == EMPTY ? '0' : i == N_ZERO ? '1' : '-');
        else if ((button == LEFT_BUTTON && ui->click_mode == 0) ||
                (button == RIGHT_BUTTON && ui->click_mode == 1))
            c = (i == EMPTY ? '1' : i == N_ONE ? '0' : '-');

        if (state->grid[gy * w2 + gx] ==
            (c == '0' ? N_ZERO : c == '1' ? N_ONE : EMPTY))
            return MOVE_NO_EFFECT;               /* don't put no-ops on the undo chain */

        sprintf(buf, "P%c,%d,%d", c, gx, gy);

        return dupstr(buf);
    }
    return MOVE_UNUSED;
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move)
{
    int w2 = state->w2, h2 = state->h2;
    int s = w2 * h2;
    int x, y, i;
    char c;
    game_state *ret;

    if (move[0] == 'S') {
        const char *p;

        ret = dup_game(state);
        p = move + 1;

        for (i = 0; i < s; i++) {

            if (!*p || !(*p == '1' || *p == '0')) {
                free_game(ret);
                return NULL;
            }

            ret->grid[i] = (*p == '1' ? N_ONE : N_ZERO);
            p++;
        }

        ret->completed = ret->cheated = true;
        return ret;
    } else if (move[0] == 'P'
               && sscanf(move + 1, "%c,%d,%d", &c, &x, &y) == 3 && x >= 0
               && x < w2 && y >= 0 && y < h2 && (c == '-' || c == '0'
                                                 || c == '1')) {
        ret = dup_game(state);
        i = y * w2 + x;

        if (state->common->immutable[i]) {
            free_game(ret);
            return NULL;
        }
        ret->grid[i] = (c == '1' ? N_ONE : c == '0' ? N_ZERO : EMPTY);
        ret->completed = ((unruly_validate_counts(ret, NULL, NULL) == 0) && 
                          (unruly_validate_all_rows(ret, NULL) == 0));
        ret->cheated = false;
        return ret;
    }

    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    *x = tilesize * (params->w2 + 1);
    *y = tilesize * (params->h2 + 1);
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
        ret[COL_BACKGROUND  * 3 + i] = 1.0F;
        ret[COL_1           * 3 + i] = 0.2F;
        ret[COL_1_HIGHLIGHT * 3 + i] = 0.4F;
        ret[COL_1_LOWLIGHT  * 3 + i] = 0.0F;
        ret[COL_0           * 3 + i] = 0.8F;
        ret[COL_0_HIGHLIGHT * 3 + i] = 1.0F;
        ret[COL_0_LOWLIGHT  * 3 + i] = 0.6F;
        ret[COL_EMPTY       * 3 + i] = 0.5F;
        ret[COL_GRID        * 3 + i] = 0.3F;
        ret[COL_ERROR       * 3 + i] = 0.5F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static void unruly_draw_err_rectangle(drawing *dr, int x, int y, int w, int h,
                                      int tilesize)
{
    double thick = tilesize / 10;
    double margin = tilesize / 20;

    draw_rect(dr, x+margin, y+margin, w-2*margin, thick, COL_ERROR);
    draw_rect(dr, x+margin, y+margin, thick, h-2*margin, COL_ERROR);
    draw_rect(dr, x+margin, y+h-margin-thick, w-2*margin, thick, COL_ERROR);
    draw_rect(dr, x+w-margin-thick, y+margin, thick, h-2*margin, COL_ERROR);
}

static void unruly_draw_tile(drawing *dr, const game_ui *ui, int x, int y, int tilesize, int tile)
{
    clip(dr, x, y, tilesize, tilesize);

    /* Draw the grid edge first, so the tile can overwrite it */
    draw_rect(dr, x, y, tilesize, tilesize, COL_GRID);

    /* Background of the tile */
    {
        int val = (tile & FF_ZERO ? 0 : tile & FF_ONE ? 2 : 1);
        val = (val == 0 ? COL_0 : val == 2 ? COL_1 : COL_EMPTY);

        draw_rect(dr, x, y, tilesize-1, tilesize-1, val);

        if ((val == COL_0 || val == COL_1) && (tile & FF_IMMUTABLE)) {
            int coords[12];
            int offset = tilesize/6;
            int width = tilesize/16;
            coords[0]  = x + offset;                     coords[1]  = y + offset;
            coords[2]  = x + tilesize - offset;          coords[3]  = y + offset;
            coords[4]  = x + tilesize - offset  - width; coords[5]  = y + offset + width;
            coords[6]  = x + offset + width;             coords[7]  = y + offset + width;
            coords[8]  = x + offset + width;             coords[9]  = y + tilesize - offset - width;
            coords[10] = x + offset;                     coords[11] = y + tilesize - offset;
            draw_polygon(dr, coords, 6, val+2, val+2);
            coords[0]  = x + tilesize - offset;          coords[1]  = y + tilesize - offset;
            coords[2]  = x + tilesize - offset;          coords[3]  = y + offset;
            coords[4]  = x + tilesize - offset - width;  coords[5]  = y + offset + width;
            coords[6]  = x + tilesize - offset - width;  coords[7]  = y + tilesize - offset - width;
            coords[8]  = x + offset + width;             coords[9]  = y + tilesize - offset - width;
            coords[10] = x + offset;                     coords[11] = y + tilesize - offset;
            draw_polygon(dr, coords, 6, val+1, val+1);
        }
    }

    if (ui->show_errors) {
        /* 3-in-a-row errors */
        if (tile & (FE_HOR_ROW_LEFT | FE_HOR_ROW_RIGHT)) {
            int left = x, right = x + tilesize - 1;
            if ((tile & FE_HOR_ROW_LEFT))
                right += tilesize/2;
            if ((tile & FE_HOR_ROW_RIGHT))
                left -= tilesize/2;
            unruly_draw_err_rectangle(dr, left, y, right-left, tilesize-1, tilesize);
        }
        if (tile & (FE_VER_ROW_TOP | FE_VER_ROW_BOTTOM)) {
            int top = y, bottom = y + tilesize - 1;
            if ((tile & FE_VER_ROW_TOP))
                bottom += tilesize/2;
            if ((tile & FE_VER_ROW_BOTTOM))
                top -= tilesize/2;
            unruly_draw_err_rectangle(dr, x, top, tilesize-1, bottom-top, tilesize);
        }

        /* Count errors */
        if (tile & FE_COUNT) {
            draw_text(dr, x + tilesize/2, y + tilesize/2, FONT_VARIABLE,
                      tilesize/2, ALIGN_HCENTRE | ALIGN_VCENTRE, COL_ERROR, "!");
        }

        /* Row-match errors */
        if (tile & FE_ROW_MATCH) {
            draw_rect(dr, x, y+tilesize/2-tilesize/12,
                      tilesize, 2*(tilesize/12), COL_ERROR);
        }
        if (tile & FE_COL_MATCH) {
            draw_rect(dr, x+tilesize/2-tilesize/12, y,
                      2*(tilesize/12), tilesize, COL_ERROR);
        }
    }
    unclip(dr);
    draw_update(dr, x, y, tilesize, tilesize);
}

#define TILE_SIZE (ds->tilesize)
#define DEFAULT_TILE_SIZE 32

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int w2 = state->w2, h2 = state->h2;
    int s = w2 * h2;
    int x, y, i;

    char buf[48];
    /* Draw status bar */
    sprintf(buf, "%s",
            state->cheated   ? "Auto-solved." :
            state->completed ? "COMPLETED!" : "");
    status_bar(dr, buf);

    if (!ds->started) {
        /* Outer edge of grid */
        draw_rect(dr, COORD(0)-TILE_SIZE/10, COORD(0)-TILE_SIZE/10,
                  TILE_SIZE*w2 + 2*(TILE_SIZE/10) - 1,
                  TILE_SIZE*h2 + 2*(TILE_SIZE/10) - 1, COL_GRID);

        draw_update(dr, 0, 0, TILE_SIZE * (w2+1), TILE_SIZE * (h2+1));
        ds->started = true;
    }

    for (i = 0; i < s; i++)
        ds->gridfs[i] = 0;
    unruly_validate_all_rows(state, ds->gridfs);
    for (i = 0; i < 2 * (h2 + w2); i++)
        ds->rowfs[i] = false;
    unruly_validate_counts(state, NULL, ds->rowfs);

    for (y = 0; y < h2; y++) {
        for (x = 0; x < w2; x++) {
            int tile;

            i = y * w2 + x;

            tile = ds->gridfs[i];

            if (state->grid[i] == N_ONE) {
                tile |= FF_ONE;
                if (ds->rowfs[y] || ds->rowfs[2*h2 + x])
                    tile |= FE_COUNT;
            } else if (state->grid[i] == N_ZERO) {
                tile |= FF_ZERO;
                if (ds->rowfs[h2 + y] || ds->rowfs[2*h2 + w2 + x])
                    tile |= FE_COUNT;
            }

            if (state->common->immutable[i])
                tile |= FF_IMMUTABLE;


            if (ds->grid[i] != tile) {
                ds->grid[i] = tile;
                unruly_draw_tile(dr, ui, COORD(x), COORD(y), TILE_SIZE, tile);
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
#define thegame unruly
#endif

static const char rules[] = "You are given a grid of squares, which you must colour either black or white. Some squares are provided as clues; the rest are left for you to fill in, such that:\n\n"
"- Each row and column must contain the same number of black and white squares.\n"
"- No row or column may contain three consecutive squares of the same colour.\n"
"- Optional: No two rows may have the same arrangement of colors (same for columns)\n\n\n"
"This puzzle was contributed by Lennard Sprong.";

const struct game thegame = {
    "Unruly", "games.unruly", "unruly", rules,
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

