/*
 * slant.c: Puzzle from nikoli.co.jp involving drawing a diagonal
 * line through each square of a grid.
 */

/*
 * In this puzzle you have a grid of squares, each of which must
 * contain a diagonal line; you also have clue numbers placed at
 * _points_ of that grid, which means there's a (w+1) x (h+1) array
 * of possible clue positions.
 * 
 * I'm therefore going to adopt a rigid convention throughout this
 * source file of using w and h for the dimensions of the grid of
 * squares, and W and H for the dimensions of the grid of points.
 * Thus, W == w+1 and H == h+1 always.
 * 
 * Clue arrays will be W*H `signed char's, and the clue at each
 * point will be a number from 0 to 4, or -1 if there's no clue.
 * 
 * Solution arrays will be W*H `signed char's, and the number at
 * each point will be +1 for a forward slash (/), -1 for a
 * backslash (\), and 0 for unknown.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_INK,
    COL_SLANT1,
    COL_SLANT2,
    COL_ERROR,
    COL_FILLEDSQUARE,
    COL_GROUNDED,
    NCOLOURS
};

enum {
    PREF_CLICK_ACTIONS,
    PREF_SHADE_FILLED,
    PREF_SHOW_ERRORS,
    PREF_FADE_GROUNDED,
    N_PREF_ITEMS
};

/*
 * Difficulty levels. I do some macro ickery here to ensure that my
 * enum and the various forms of my name list always match up.
 */
#define DIFFLIST(A) \
    A(EASY,Easy,e) \
    A(TRICKY,Tricky,t)
#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const slant_diffnames[] = { DIFFLIST(TITLE) };
static char const slant_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

struct game_params {
    int w, h, diff;
};

typedef struct game_clues {
    int w, h;
    signed char *clues;
    int *tmpdsf;
    int refcount;
} game_clues;

#define ERR_VERTEX 1
#define ERR_SQUARE 2
#define BORDER_EDGE 4 /* kind of an abuse: not an error */

struct game_state {
    struct game_params p;
    game_clues *clues;
    signed char *soln;
    unsigned char *errors;
    bool completed;
    bool used_solve;           /* used to suppress completion flash */
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->w = ret->h = 8;
    ret->diff = DIFF_EASY;

    return ret;
}

static const struct game_params slant_presets[] = {
    {5, 5, DIFF_EASY},
    {5, 5, DIFF_TRICKY},
    {8, 8, DIFF_EASY},
    {8, 8, DIFF_TRICKY},
    {10, 10, DIFF_EASY},
    {10, 10, DIFF_TRICKY},
    {12, 12, DIFF_EASY},
    {12, 12, DIFF_TRICKY},
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char str[80];

    if (i < 0 || i >= lenof(slant_presets))
        return false;

    ret = snew(game_params);
    *ret = slant_presets[i];

    sprintf(str, "%dx%d %s", ret->w, ret->h, slant_diffnames[ret->diff]);

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
    *ret = *params;               /* structure copy */
    return ret;
}

static void decode_params(game_params *ret, char const *string)
{
    ret->w = ret->h = atoi(string);
    ret->diff = DIFF_EASY;
    while (*string && isdigit((unsigned char)*string)) string++;
    if (*string == 'x') {
        string++;
        ret->h = atoi(string);
    while (*string && isdigit((unsigned char)*string)) string++;
    }
    if (*string == 'd') {
    int i;
    string++;
    for (i = 0; i < DIFFCOUNT; i++)
        if (*string == slant_diffchars[i])
        ret->diff = i;
    if (*string) string++;
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char data[256];

    sprintf(data, "%dx%d", params->w, params->h);
    if (full)
    sprintf(data + strlen(data), "d%c", slant_diffchars[params->diff]);

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
    /*
     * (At least at the time of writing this comment) The grid
     * generator is actually capable of handling even zero grid
     * dimensions without crashing. Puzzles with a zero-area grid
     * are a bit boring, though, because they're already solved :-)
     * And puzzles with a dimension of 1 can't be made tricky, which
     * means the simplest thing is to forbid them altogether.
     */

    if (params->w < 2 || params->h < 2)
        return "Width and height must both be at least two";
    if (params->w > 16 || params->h > 16)
        return "Width or height must not be bigger than 16";

    return NULL;
}

/*
 * Scratch space for solver.
 */
struct solver_scratch {
    /*
     * Disjoint set forest which tracks the connected sets of
     * points.
     */
    DSF *connected;

    /*
     * Counts the number of possible exits from each connected set
     * of points. (That is, the number of possible _simultaneous_
     * exits: an unconnected point labelled 2 has an exit count of
     * 2 even if all four possible edges are still under
     * consideration.)
     */
    int *exits;

    /*
     * Tracks whether each connected set of points includes a
     * border point.
     */
    bool *border;

    /*
     * Another disjoint set forest. This one tracks _squares_ which
     * are known to slant in the same direction.
     */
    DSF *equiv;

    /*
     * Stores slash values which we know for an equivalence class.
     * When we fill in a square, we set slashval[canonify(x)] to
     * the same value as soln[x], so that we can then spot other
     * squares equivalent to it and fill them in immediately via
     * their known equivalence.
     */
    signed char *slashval;

    /*
     * Stores possible v-shapes. This array is w by h in size, but
     * not every bit of every entry is meaningful. The bits mean:
     * 
     *  - bit 0 for a square means that that square and the one to
     *    its right might form a v-shape between them
     *  - bit 1 for a square means that that square and the one to
     *    its right might form a ^-shape between them
     *  - bit 2 for a square means that that square and the one
     *    below it might form a >-shape between them
     *  - bit 3 for a square means that that square and the one
     *    below it might form a <-shape between them
     * 
     * Any starting 1 or 3 clue rules out four bits in this array
     * immediately; a 2 clue propagates any ruled-out bit past it
     * (if the two squares on one side of a 2 cannot be a v-shape,
     * then neither can the two on the other side be the same
     * v-shape); we can rule out further bits during play using
     * partially filled 2 clues; whenever a pair of squares is
     * known not to be _either_ kind of v-shape, we can mark them
     * as equivalent.
     */
    unsigned char *vbitmap;

    /*
     * Useful to have this information automatically passed to
     * solver subroutines. (This pointer is not dynamically
     * allocated by new_scratch and free_scratch.)
     */
    const signed char *clues;
};

static struct solver_scratch *new_scratch(int w, int h)
{
    int W = w+1, H = h+1;
    struct solver_scratch *ret = snew(struct solver_scratch);
    ret->connected = dsf_new(W*H);
    ret->exits = snewn(W*H, int);
    ret->border = snewn(W*H, bool);
    ret->equiv = dsf_new(w*h);
    ret->slashval = snewn(w*h, signed char);
    ret->vbitmap = snewn(w*h, unsigned char);
    return ret;
}

static void free_scratch(struct solver_scratch *sc)
{
    sfree(sc->vbitmap);
    sfree(sc->slashval);
    dsf_free(sc->equiv);
    sfree(sc->border);
    sfree(sc->exits);
    dsf_free(sc->connected);
    sfree(sc);
}

/*
 * Wrapper on dsf_merge() which updates the `exits' and `border'
 * arrays.
 */
static void merge_vertices(DSF *connected,
               struct solver_scratch *sc, int i, int j)
{
    int exits = -1;
    bool border = false;    /* initialise to placate optimiser */

    if (sc) {
    i = dsf_canonify(connected, i);
    j = dsf_canonify(connected, j);

    /*
     * We have used one possible exit from each of the two
     * classes. Thus, the viable exit count of the new class is
     * the sum of the old exit counts minus two.
     */
    exits = sc->exits[i] + sc->exits[j] - 2;

    border = sc->border[i] || sc->border[j];
    }

    dsf_merge(connected, i, j);

    if (sc) {
    i = dsf_canonify(connected, i);
    sc->exits[i] = exits;
    sc->border[i] = border;
    }
}

/*
 * Called when we have just blocked one way out of a particular
 * point. If that point is a non-clue point (thus has a variable
 * number of exits), we have therefore decreased its potential exit
 * count, so we must decrement the exit count for the group as a
 * whole.
 */
static void decr_exits(struct solver_scratch *sc, int i)
{
    if (sc->clues[i] < 0) {
    i = dsf_canonify(sc->connected, i);
    sc->exits[i]--;
    }
}

static void fill_square(int w, int h, int x, int y, int v,
            signed char *soln,
            DSF *connected, struct solver_scratch *sc)
{
    int W = w+1 /*, H = h+1 */;

    assert(x >= 0 && x < w && y >= 0 && y < h);

    if (soln[y*w+x] != 0) {
    return;                   /* do nothing */
    }

#ifdef SOLVER_DIAGNOSTICS
    if (verbose)
    printf("  placing %c in %d,%d\n", v == -1 ? '\\' : '/', x, y);
#endif

    soln[y*w+x] = v;

    if (sc) {
    int c = dsf_canonify(sc->equiv, y*w+x);
    sc->slashval[c] = v;
    }

    if (v < 0) {
    merge_vertices(connected, sc, y*W+x, (y+1)*W+(x+1));
    if (sc) {
        decr_exits(sc, y*W+(x+1));
        decr_exits(sc, (y+1)*W+x);
    }
    } else {
    merge_vertices(connected, sc, y*W+(x+1), (y+1)*W+x);
    if (sc) {
        decr_exits(sc, y*W+x);
        decr_exits(sc, (y+1)*W+(x+1));
    }
    }
}

static bool vbitmap_clear(int w, int h, struct solver_scratch *sc,
                          int x, int y, int vbits, const char *reason, ...)
{
    bool done_something = false;
    int vbit;

    for (vbit = 1; vbit <= 8; vbit <<= 1)
        if (vbits & sc->vbitmap[y*w+x] & vbit) {
            done_something = true;
#ifdef SOLVER_DIAGNOSTICS
            if (verbose) {
                va_list ap;

                printf("ruling out %c shape at (%d,%d)-(%d,%d) (",
                       "!v^!>!!!<"[vbit], x, y,
                       x+((vbit&0x3)!=0), y+((vbit&0xC)!=0));

                va_start(ap, reason);
                vprintf(reason, ap);
                va_end(ap);

                printf(")\n");
            }
#endif
            sc->vbitmap[y*w+x] &= ~vbit;
        }

    return done_something;
}

/*
 * Solver. Returns 0 for impossibility, 1 for success, 2 for
 * ambiguity or failure to converge.
 */
static int slant_solve(int w, int h, const signed char *clues,
               signed char *soln, struct solver_scratch *sc,
               int difficulty)
{
    int W = w+1, H = h+1;
    int x, y, i, j;
    bool done_something;

    /*
     * Clear the output.
     */
    memset(soln, 0, w*h);

    sc->clues = clues;

    /*
     * Establish a disjoint set forest for tracking connectedness
     * between grid points.
     */
    dsf_reinit(sc->connected);

    /*
     * Establish a disjoint set forest for tracking which squares
     * are known to slant in the same direction.
     */
    dsf_reinit(sc->equiv);

    /*
     * Clear the slashval array.
     */
    memset(sc->slashval, 0, w*h);

    /*
     * Set up the vbitmap array. Initially all types of v are possible.
     */
    memset(sc->vbitmap, 0xF, w*h);

    /*
     * Initialise the `exits' and `border' arrays. These are used
     * to do second-order loop avoidance: the dual of the no loops
     * constraint is that every point must be somehow connected to
     * the border of the grid (otherwise there would be a solid
     * loop around it which prevented this).
     * 
     * I define a `dead end' to be a connected group of points
     * which contains no border point, and which can form at most
     * one new connection outside itself. Then I forbid placing an
     * edge so that it connects together two dead-end groups, since
     * this would yield a non-border-connected isolated subgraph
     * with no further scope to extend it.
     */
    for (y = 0; y < H; y++)
    for (x = 0; x < W; x++) {
        if (y == 0 || y == H-1 || x == 0 || x == W-1)
        sc->border[y*W+x] = true;
        else
        sc->border[y*W+x] = false;

        if (clues[y*W+x] < 0)
        sc->exits[y*W+x] = 4;
        else
        sc->exits[y*W+x] = clues[y*W+x];
    }

    /*
     * Repeatedly try to deduce something until we can't.
     */
    do {
    done_something = false;

    /*
     * Any clue point with the number of remaining lines equal
     * to zero or to the number of remaining undecided
     * neighbouring squares can be filled in completely.
     */
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
        struct {
            int pos, slash;
        } neighbours[4];
        int nneighbours;
        int nu, nl, c, s, eq, eq2, last, meq, mj1, mj2;

        if ((c = clues[y*W+x]) < 0)
            continue;

        /*
         * We have a clue point. Start by listing its
         * neighbouring squares, in order around the point,
         * together with the type of slash that would be
         * required in that square to connect to the point.
         */
        nneighbours = 0;
        if (x > 0 && y > 0) {
            neighbours[nneighbours].pos = (y-1)*w+(x-1);
            neighbours[nneighbours].slash = -1;
            nneighbours++;
        }
        if (x > 0 && y < h) {
            neighbours[nneighbours].pos = y*w+(x-1);
            neighbours[nneighbours].slash = +1;
            nneighbours++;
        }
        if (x < w && y < h) {
            neighbours[nneighbours].pos = y*w+x;
            neighbours[nneighbours].slash = -1;
            nneighbours++;
        }
        if (x < w && y > 0) {
            neighbours[nneighbours].pos = (y-1)*w+x;
            neighbours[nneighbours].slash = +1;
            nneighbours++;
        }

        /*
         * Count up the number of undecided neighbours, and
         * also the number of lines already present.
         *
         * If we're not on DIFF_EASY, then in this loop we
         * also track whether we've seen two adjacent empty
         * squares belonging to the same equivalence class
         * (meaning they have the same type of slash). If
         * so, we count them jointly as one line.
         */
        nu = 0;
        nl = c;
        last = neighbours[nneighbours-1].pos;
        if (soln[last] == 0)
            eq = dsf_canonify(sc->equiv, last);
        else
            eq = -1;
        meq = mj1 = mj2 = -1;
        for (i = 0; i < nneighbours; i++) {
            j = neighbours[i].pos;
            s = neighbours[i].slash;
            if (soln[j] == 0) {
            nu++;           /* undecided */
            if (meq < 0 && difficulty > DIFF_EASY) {
                eq2 = dsf_canonify(sc->equiv, j);
                if (eq == eq2 && last != j) {
                /*
                 * We've found an equivalent pair.
                 * Mark it. This also inhibits any
                 * further equivalence tracking
                 * around this square, since we can
                 * only handle one pair (and in
                 * particular we want to avoid
                 * being misled by two overlapping
                 * equivalence pairs).
                 */
                meq = eq;
                mj1 = last;
                mj2 = j;
                nl--;   /* count one line */
                nu -= 2;   /* and lose two undecideds */
                } else
                eq = eq2;
            }
            } else {
            eq = -1;
            if (soln[j] == s)
                nl--;      /* here's a line */
            }
            last = j;
        }

        /*
         * Check the counts.
         */
        if (nl < 0 || nl > nu) {
            /*
             * No consistent value for this at all!
             */
#ifdef SOLVER_DIAGNOSTICS
            if (verbose)
            printf("need %d / %d lines around clue point at %d,%d!\n",
                   nl, nu, x, y);
#endif
            return 0;           /* impossible */
        }

        if (nu > 0 && (nl == 0 || nl == nu)) {
#ifdef SOLVER_DIAGNOSTICS
            if (verbose) {
            if (meq >= 0)
                printf("partially (since %d,%d == %d,%d) ",
                   mj1%w, mj1/w, mj2%w, mj2/w);
            printf("%s around clue point at %d,%d\n",
                   nl ? "filling" : "emptying", x, y);
            }
#endif
            for (i = 0; i < nneighbours; i++) {
            j = neighbours[i].pos;
            s = neighbours[i].slash;
            if (soln[j] == 0 && j != mj1 && j != mj2)
                fill_square(w, h, j%w, j/w, (nl ? s : -s), soln,
                    sc->connected, sc);
            }

            done_something = true;
        } else if (nu == 2 && nl == 1 && difficulty > DIFF_EASY) {
            /*
             * If we have precisely two undecided squares
             * and precisely one line to place between
             * them, _and_ those squares are adjacent, then
             * we can mark them as equivalent to one
             * another.
             * 
             * This even applies if meq >= 0: if we have a
             * 2 clue point and two of its neighbours are
             * already marked equivalent, we can indeed
             * mark the other two as equivalent.
             * 
             * We don't bother with this on DIFF_EASY,
             * since we wouldn't have used the results
             * anyway.
             */
            last = -1;
            for (i = 0; i < nneighbours; i++) {
            j = neighbours[i].pos;
            if (soln[j] == 0 && j != mj1 && j != mj2) {
                if (last < 0)
                last = i;
                else if (last == i-1 || (last == 0 && i == 3))
                break; /* found a pair */
            }
            }
            if (i < nneighbours) {
            int sv1, sv2;

            assert(last >= 0);
            /*
             * neighbours[last] and neighbours[i] are
             * the pair. Mark them equivalent.
             */
#ifdef SOLVER_DIAGNOSTICS
            if (verbose) {
                if (meq >= 0)
                printf("since %d,%d == %d,%d, ",
                       mj1%w, mj1/w, mj2%w, mj2/w);
            }
#endif
            mj1 = neighbours[last].pos;
            mj2 = neighbours[i].pos;
#ifdef SOLVER_DIAGNOSTICS
            if (verbose)
                printf("clue point at %d,%d implies %d,%d == %d,"
                   "%d\n", x, y, mj1%w, mj1/w, mj2%w, mj2/w);
#endif
            mj1 = dsf_canonify(sc->equiv, mj1);
            sv1 = sc->slashval[mj1];
            mj2 = dsf_canonify(sc->equiv, mj2);
            sv2 = sc->slashval[mj2];
            if (sv1 != 0 && sv2 != 0 && sv1 != sv2) {
#ifdef SOLVER_DIAGNOSTICS
                if (verbose)
                printf("merged two equivalence classes with"
                       " different slash values!\n");
#endif
                return 0;
            }
            sv1 = sv1 ? sv1 : sv2;
            dsf_merge(sc->equiv, mj1, mj2);
            mj1 = dsf_canonify(sc->equiv, mj1);
            sc->slashval[mj1] = sv1;
            }
        }
        }

    if (done_something)
        continue;

    /*
     * Failing that, we now apply the second condition, which
     * is that no square may be filled in such a way as to form
     * a loop. Also in this loop (since it's over squares
     * rather than points), we check slashval to see if we've
     * already filled in another square in the same equivalence
     * class.
     * 
     * The slashval check is disabled on DIFF_EASY, as is dead
     * end avoidance. Only _immediate_ loop avoidance remains.
     */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
        bool fs, bs;
                int v, c1, c2;
#ifdef SOLVER_DIAGNOSTICS
        const char *reason = "<internal error>";
#endif

        if (soln[y*w+x])
            continue;           /* got this one already */

        fs = false;
        bs = false;

        if (difficulty > DIFF_EASY)
            v = sc->slashval[dsf_canonify(sc->equiv, y*w+x)];
        else
            v = 0;

        /*
         * Try to rule out connectivity between (x,y) and
         * (x+1,y+1); if successful, we will deduce that we
         * must have a forward slash.
         */
        c1 = dsf_canonify(sc->connected, y*W+x);
        c2 = dsf_canonify(sc->connected, (y+1)*W+(x+1));
        if (c1 == c2) {
            fs = true;
#ifdef SOLVER_DIAGNOSTICS
            reason = "simple loop avoidance";
#endif
        }
        if (difficulty > DIFF_EASY &&
            !sc->border[c1] && !sc->border[c2] &&
            sc->exits[c1] <= 1 && sc->exits[c2] <= 1) {
            fs = true;
#ifdef SOLVER_DIAGNOSTICS
            reason = "dead end avoidance";
#endif
        }
        if (v == +1) {
            fs = true;
#ifdef SOLVER_DIAGNOSTICS
            reason = "equivalence to an already filled square";
#endif
        }

        /*
         * Now do the same between (x+1,y) and (x,y+1), to
         * see if we are required to have a backslash.
         */
        c1 = dsf_canonify(sc->connected, y*W+(x+1));
        c2 = dsf_canonify(sc->connected, (y+1)*W+x);
        if (c1 == c2) {
            bs = true;
#ifdef SOLVER_DIAGNOSTICS
            reason = "simple loop avoidance";
#endif
        }
        if (difficulty > DIFF_EASY &&
            !sc->border[c1] && !sc->border[c2] &&
            sc->exits[c1] <= 1 && sc->exits[c2] <= 1) {
            bs = true;
#ifdef SOLVER_DIAGNOSTICS
            reason = "dead end avoidance";
#endif
        }
        if (v == -1) {
            bs = true;
#ifdef SOLVER_DIAGNOSTICS
            reason = "equivalence to an already filled square";
#endif
        }

        if (fs && bs) {
            /*
             * No consistent value for this at all!
             */
#ifdef SOLVER_DIAGNOSTICS
            if (verbose)
            printf("%d,%d has no consistent slash!\n", x, y);
#endif
            return 0;          /* impossible */
        }

        if (fs) {
#ifdef SOLVER_DIAGNOSTICS
            if (verbose)
            printf("employing %s\n", reason);
#endif
            fill_square(w, h, x, y, +1, soln, sc->connected, sc);
            done_something = true;
        } else if (bs) {
#ifdef SOLVER_DIAGNOSTICS
            if (verbose)
            printf("employing %s\n", reason);
#endif
            fill_square(w, h, x, y, -1, soln, sc->connected, sc);
            done_something = true;
        }
        }

        if (done_something)
            continue;

        /*
         * Now see what we can do with the vbitmap array. All
         * vbitmap deductions are disabled at Easy level.
         */
        if (difficulty <= DIFF_EASY)
            continue;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
                int s, c;

                /*
                 * Any line already placed in a square must rule
                 * out any type of v which contradicts it.
                 */
                if ((s = soln[y*w+x]) != 0) {
                    if (x > 0)
                        done_something |=
                        vbitmap_clear(w, h, sc, x-1, y, (s < 0 ? 0x1 : 0x2),
                                      "contradicts known edge at (%d,%d)",x,y);
                    if (x+1 < w)
                        done_something |=
                        vbitmap_clear(w, h, sc, x, y, (s < 0 ? 0x2 : 0x1),
                                      "contradicts known edge at (%d,%d)",x,y);
                    if (y > 0)
                        done_something |=
                        vbitmap_clear(w, h, sc, x, y-1, (s < 0 ? 0x4 : 0x8),
                                      "contradicts known edge at (%d,%d)",x,y);
                    if (y+1 < h)
                        done_something |=
                        vbitmap_clear(w, h, sc, x, y, (s < 0 ? 0x8 : 0x4),
                                      "contradicts known edge at (%d,%d)",x,y);
                }

                /*
                 * If both types of v are ruled out for a pair of
                 * adjacent squares, mark them as equivalent.
                 */
                if (x+1 < w && !(sc->vbitmap[y*w+x] & 0x3)) {
                    int n1 = y*w+x, n2 = y*w+(x+1);
                    if (dsf_canonify(sc->equiv, n1) !=
                        dsf_canonify(sc->equiv, n2)) {
                        dsf_merge(sc->equiv, n1, n2);
                        done_something = true;
#ifdef SOLVER_DIAGNOSTICS
                        if (verbose)
                            printf("(%d,%d) and (%d,%d) must be equivalent"
                                   " because both v-shapes are ruled out\n",
                                   x, y, x+1, y);
#endif
                    }
                }
                if (y+1 < h && !(sc->vbitmap[y*w+x] & 0xC)) {
                    int n1 = y*w+x, n2 = (y+1)*w+x;
                    if (dsf_canonify(sc->equiv, n1) !=
                        dsf_canonify(sc->equiv, n2)) {
                        dsf_merge(sc->equiv, n1, n2);
                        done_something = true;
#ifdef SOLVER_DIAGNOSTICS
                        if (verbose)
                            printf("(%d,%d) and (%d,%d) must be equivalent"
                                   " because both v-shapes are ruled out\n",
                                   x, y, x, y+1);
#endif
                    }
                }

                /*
                 * The remaining work in this loop only works
                 * around non-edge clue points.
                 */
                if (y == 0 || x == 0)
                    continue;
        if ((c = clues[y*W+x]) < 0)
            continue;

                /*
                 * x,y marks a clue point not on the grid edge. See
                 * if this clue point allows us to rule out any v
                 * shapes.
                 */

                if (c == 1) {
                    /*
                     * A 1 clue can never have any v shape pointing
                     * at it.
                     */
                    done_something |=
                        vbitmap_clear(w, h, sc, x-1, y-1, 0x5,
                                      "points at 1 clue at (%d,%d)", x, y);
                    done_something |=
                        vbitmap_clear(w, h, sc, x-1, y, 0x2,
                                      "points at 1 clue at (%d,%d)", x, y);
                    done_something |=
                        vbitmap_clear(w, h, sc, x, y-1, 0x8,
                                      "points at 1 clue at (%d,%d)", x, y);
                } else if (c == 3) {
                    /*
                     * A 3 clue can never have any v shape pointing
                     * away from it.
                     */
                    done_something |=
                        vbitmap_clear(w, h, sc, x-1, y-1, 0xA,
                                      "points away from 3 clue at (%d,%d)", x, y);
                    done_something |=
                        vbitmap_clear(w, h, sc, x-1, y, 0x1,
                                      "points away from 3 clue at (%d,%d)", x, y);
                    done_something |=
                        vbitmap_clear(w, h, sc, x, y-1, 0x4,
                                      "points away from 3 clue at (%d,%d)", x, y);
                } else if (c == 2) {
                    /*
                     * If a 2 clue has any kind of v ruled out on
                     * one side of it, the same v is ruled out on
                     * the other side.
                     */
                    done_something |=
                        vbitmap_clear(w, h, sc, x-1, y-1,
                                      (sc->vbitmap[(y  )*w+(x-1)] & 0x3) ^ 0x3,
                                      "propagated by 2 clue at (%d,%d)", x, y);
                    done_something |=
                        vbitmap_clear(w, h, sc, x-1, y-1,
                                      (sc->vbitmap[(y-1)*w+(x  )] & 0xC) ^ 0xC,
                                      "propagated by 2 clue at (%d,%d)", x, y);
                    done_something |=
                        vbitmap_clear(w, h, sc, x-1, y,
                                      (sc->vbitmap[(y-1)*w+(x-1)] & 0x3) ^ 0x3,
                                      "propagated by 2 clue at (%d,%d)", x, y);
                    done_something |=
                        vbitmap_clear(w, h, sc, x, y-1,
                                      (sc->vbitmap[(y-1)*w+(x-1)] & 0xC) ^ 0xC,
                                      "propagated by 2 clue at (%d,%d)", x, y);
                }

#undef CLEARBITS

            }

    } while (done_something);

    /*
     * Solver can make no more progress. See if the grid is full.
     */
    for (i = 0; i < w*h; i++)
    if (!soln[i])
        return 2;               /* failed to converge */
    return 1;                   /* success */
}

/*
 * Filled-grid generator.
 */
static void slant_generate(int w, int h, signed char *soln, random_state *rs)
{
    int W = w+1, H = h+1;
    int x, y, i;
    DSF *connected;
    int *indices;

    /*
     * Clear the output.
     */
    memset(soln, 0, w*h);

    /*
     * Establish a disjoint set forest for tracking connectedness
     * between grid points.
     */
    connected = dsf_new(W*H);

    /*
     * Prepare a list of the squares in the grid, and fill them in
     * in a random order.
     */
    indices = snewn(w*h, int);
    for (i = 0; i < w*h; i++)
    indices[i] = i;
    shuffle(indices, w*h, sizeof(*indices), rs);

    /*
     * Fill in each one in turn.
     */
    for (i = 0; i < w*h; i++) {
    bool fs, bs;
        int v;

    y = indices[i] / w;
    x = indices[i] % w;

    fs = (dsf_canonify(connected, y*W+x) ==
          dsf_canonify(connected, (y+1)*W+(x+1)));
    bs = (dsf_canonify(connected, (y+1)*W+x) ==
          dsf_canonify(connected, y*W+(x+1)));

    /*
     * It isn't possible to get into a situation where we
     * aren't allowed to place _either_ type of slash in a
     * square. Thus, filled-grid generation never has to
     * backtrack.
     * 
     * Proof (thanks to Gareth Taylor):
     * 
     * If it were possible, it would have to be because there
     * was an existing path (not using this square) between the
     * top-left and bottom-right corners of this square, and
     * another between the other two. These two paths would
     * have to cross at some point.
     * 
     * Obviously they can't cross in the middle of a square, so
     * they must cross by sharing a point in common. But this
     * isn't possible either: if you chessboard-colour all the
     * points on the grid, you find that any continuous
     * diagonal path is entirely composed of points of the same
     * colour. And one of our two hypothetical paths is between
     * two black points, and the other is between two white
     * points - therefore they can have no point in common. []
     */
    assert(!(fs && bs));

    v = fs ? +1 : bs ? -1 : 2 * random_upto(rs, 2) - 1;
    fill_square(w, h, x, y, v, soln, connected, NULL);
    }

    sfree(indices);
    dsf_free(connected);
}

static char *new_game_desc(const game_params *params, random_state *rs,
               char **aux, bool interactive)
{
    int w = params->w, h = params->h, W = w+1, H = h+1;
    signed char *soln, *tmpsoln, *clues;
    int *clueindices;
    struct solver_scratch *sc;
    int x, y, v, i, j;
    char *desc;

    soln = snewn(w*h, signed char);
    tmpsoln = snewn(w*h, signed char);
    clues = snewn(W*H, signed char);
    clueindices = snewn(W*H, int);
    sc = new_scratch(w, h);

    do {
    /*
     * Create the filled grid.
     */
    slant_generate(w, h, soln, rs);

    /*
     * Fill in the complete set of clues.
     */
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
        v = 0;

        if (x > 0 && y > 0 && soln[(y-1)*w+(x-1)] == -1) v++;
        if (x > 0 && y < h && soln[y*w+(x-1)] == +1) v++;
        if (x < w && y > 0 && soln[(y-1)*w+x] == +1) v++;
        if (x < w && y < h && soln[y*w+x] == -1) v++;

        clues[y*W+x] = v;
        }

    /*
     * With all clue points filled in, all puzzles are easy: we can
     * simply process the clue points in lexicographic order, and
     * at each clue point we will always have at most one square
     * undecided, which we can then fill in uniquely.
     */
    assert(slant_solve(w, h, clues, tmpsoln, sc, DIFF_EASY) == 1);

    /*
     * Remove as many clues as possible while retaining solubility.
     *
     * In DIFF_TRICKY mode, we prioritise the removal of obvious
     * starting points (4s, 0s, border 2s and corner 1s), on
     * the grounds that having as few of these as possible
     * seems like a good thing. In particular, we can often get
     * away without _any_ completely obvious starting points,
     * which is even better.
     */
    for (i = 0; i < W*H; i++)
        clueindices[i] = i;
    shuffle(clueindices, W*H, sizeof(*clueindices), rs);
    for (j = 0; j < 2; j++) {
        for (i = 0; i < W*H; i++) {
        int pass;
                bool yb, xb;

        y = clueindices[i] / W;
        x = clueindices[i] % W;
        v = clues[y*W+x];

        /*
         * Identify which pass we should process this point
         * in. If it's an obvious start point, _or_ we're
         * in DIFF_EASY, then it goes in pass 0; otherwise
         * pass 1.
         */
        xb = (x == 0 || x == W-1);
        yb = (y == 0 || y == H-1);
        if (params->diff == DIFF_EASY || v == 4 || v == 0 ||
            (v == 2 && (xb||yb)) || (v == 1 && xb && yb))
            pass = 0;
        else
            pass = 1;

        if (pass == j) {
            clues[y*W+x] = -1;
            if (slant_solve(w, h, clues, tmpsoln, sc,
                    params->diff) != 1)
            clues[y*W+x] = v;           /* put it back */
        }
        }
    }

    /*
     * And finally, verify that the grid is of _at least_ the
     * requested difficulty, by running the solver one level
     * down and verifying that it can't manage it.
     */
    } while (params->diff > 0 &&
         slant_solve(w, h, clues, tmpsoln, sc, params->diff - 1) <= 1);

    /*
     * Now we have the clue set as it will be presented to the
     * user. Encode it in a game desc.
     */
    {
    char *p;
    int run, i;

    desc = snewn(W*H+1, char);
    p = desc;
    run = 0;
    for (i = 0; i <= W*H; i++) {
        int n = (i < W*H ? clues[i] : -2);

        if (n == -1)
        run++;
        else {
        if (run) {
            while (run > 0) {
            int c = 'a' - 1 + run;
            if (run > 26)
                c = 'z';
            *p++ = c;
            run -= c - ('a' - 1);
            }
        }
        if (n >= 0)
            *p++ = '0' + n;
        run = 0;
        }
    }
    assert(p - desc <= W*H);
    *p++ = '\0';
    desc = sresize(desc, p - desc, char);
    }

    /*
     * Encode the solution as an aux_info.
     */
    {
    char *auxbuf;
    *aux = auxbuf = snewn(w*h+1, char);
    for (i = 0; i < w*h; i++)
        auxbuf[i] = soln[i] < 0 ? '\\' : '/';
    auxbuf[w*h] = '\0';
    }

    free_scratch(sc);
    sfree(clueindices);
    sfree(clues);
    sfree(tmpsoln);
    sfree(soln);

    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    int w = params->w, h = params->h, W = w+1, H = h+1;
    int area = W*H;
    int squares = 0;

    while (*desc) {
        int n = *desc++;
        if (n >= 'a' && n <= 'z') {
            squares += n - 'a' + 1;
        } else if (n >= '0' && n <= '4') {
            squares++;
        } else
            return "Invalid character in game description";
    }

    if (squares < area)
        return "Not enough data to fill grid";

    if (squares > area)
        return "Too much data to fit in grid";

    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    int w = params->w, h = params->h, W = w+1, H = h+1;
    game_state *state = snew(game_state);
    int squares = 0;

    state->p = *params;
    state->soln = snewn(w*h, signed char);
    memset(state->soln, 0, w*h);
    state->completed = state->used_solve = false;
    state->errors = snewn(W*H, unsigned char);
    memset(state->errors, 0, W*H);

    state->clues = snew(game_clues);
    state->clues->w = w;
    state->clues->h = h;
    state->clues->clues = snewn(W*H, signed char);
    state->clues->refcount = 1;
    state->clues->tmpdsf = snewn(W*H*2+W+H, int);
    memset(state->clues->clues, -1, W*H);
    while (*desc) {
        int n = *desc++;
        if (n >= 'a' && n <= 'z') {
            squares += n - 'a' + 1;
        } else if (n >= '0' && n <= '4') {
            state->clues->clues[squares++] = n - '0';
        }
    }

    return state;
}

static game_state *dup_game(const game_state *state)
{
    int w = state->p.w, h = state->p.h, W = w+1, H = h+1;
    game_state *ret = snew(game_state);

    ret->p = state->p;
    ret->clues = state->clues;
    ret->clues->refcount++;
    ret->completed = state->completed;
    ret->used_solve = state->used_solve;

    ret->soln = snewn(w*h, signed char);
    memcpy(ret->soln, state->soln, w*h);

    ret->errors = snewn(W*H, unsigned char);
    memcpy(ret->errors, state->errors, W*H);

    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->errors);
    sfree(state->soln);
    assert(state->clues);
    if (--state->clues->refcount <= 0) {
        sfree(state->clues->clues);
        sfree(state->clues->tmpdsf);
        sfree(state->clues);
    }
    sfree(state);
}

/*
 * Utility function to return the current degree of a vertex. If
 * `anti' is set, it returns the number of filled-in edges
 * surrounding the point which _don't_ connect to it; thus 4 minus
 * its anti-degree is the maximum degree it could have if all the
 * empty spaces around it were filled in.
 * 
 * (Yes, _4_ minus its anti-degree even if it's a border vertex.)
 * 
 * If ret > 0, *sx and *sy are set to the coordinates of one of the
 * squares that contributed to it.
 */
static int vertex_degree(int w, int h, signed char *soln, int x, int y,
                         bool anti, int *sx, int *sy)
{
    int ret = 0;

    assert(x >= 0 && x <= w && y >= 0 && y <= h);
    if (x > 0 && y > 0 && soln[(y-1)*w+(x-1)] - anti < 0) {
        if (sx) *sx = x-1;
        if (sy) *sy = y-1;
        ret++;
    }
    if (x > 0 && y < h && soln[y*w+(x-1)] + anti > 0) {
        if (sx) *sx = x-1;
        if (sy) *sy = y;
        ret++;
    }
    if (x < w && y > 0 && soln[(y-1)*w+x] + anti > 0) {
        if (sx) *sx = x;
        if (sy) *sy = y-1;
        ret++;
    }
    if (x < w && y < h && soln[y*w+x] - anti < 0) {
        if (sx) *sx = x;
        if (sy) *sy = y;
        ret++;
    }

    return anti ? 4 - ret : ret;
}

struct slant_neighbour_ctx {
    const game_state *state;
    int i, n, neighbours[4];
};
static int slant_neighbour(int vertex, void *vctx)
{
    struct slant_neighbour_ctx *ctx = (struct slant_neighbour_ctx *)vctx;

    if (vertex >= 0) {
        int w = ctx->state->p.w, h = ctx->state->p.h, W = w+1;
        int x = vertex % W, y = vertex / W;
        ctx->n = ctx->i = 0;
        if (x < w && y < h && ctx->state->soln[y*w+x] < 0)
            ctx->neighbours[ctx->n++] = (y+1)*W+(x+1);
        if (x > 0 && y > 0 && ctx->state->soln[(y-1)*w+(x-1)] < 0)
            ctx->neighbours[ctx->n++] = (y-1)*W+(x-1);
        if (x > 0 && y < h && ctx->state->soln[y*w+(x-1)] > 0)
            ctx->neighbours[ctx->n++] = (y+1)*W+(x-1);
        if (x < w && y > 0 && ctx->state->soln[(y-1)*w+x] > 0)
            ctx->neighbours[ctx->n++] = (y-1)*W+(x+1);
    }

    if (ctx->i < ctx->n)
        return ctx->neighbours[ctx->i++];
    else
        return -1;
}

static bool check_completion(game_state *state)
{
    int w = state->p.w, h = state->p.h, W = w+1, H = h+1;
    int x, y;
    bool err = false;

    memset(state->errors, 0, W*H);

    /*
     * Detect and grounded-highlight edge-connected components in the grid.
     */
    {
        DSF *connected = dsf_new(W*H);
        unsigned root_NW;
        int slash;
        int x, y;

        for (x = 0; x <= w; x++) {
            dsf_merge(connected, x, 0);
            dsf_merge(connected, h*W+x, 0);
        }
        for (y = 0; y <= h; y++) {
            dsf_merge(connected, y*W, 0);
            dsf_merge(connected, y*W+w, 0);
        }

        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                switch (state->soln[y*w+x]) {
                  case -1:
                    dsf_merge(connected, y*W+x, (y+1)*W+(x+1));
                    break;
                  case +1:
                    dsf_merge(connected, y*W+(x+1), (y+1)*W+x);
                    break;
                  default:
                    continue;
                }
            }
        }

        root_NW = dsf_canonify(connected, 0);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                if ((slash = state->soln[y*w+x]) && dsf_canonify(
                        connected, y*W + x + (slash == 1 ? 1 : 0)) == root_NW)
                    state->errors[y*w+x] |= BORDER_EDGE;
        dsf_free(connected);
    }

    /*
     * Detect and error-highlight loops in the grid.
     */
    {
        struct findloopstate *fls = findloop_new_state(W*H);
        struct slant_neighbour_ctx ctx;
        ctx.state = state;

        if (findloop_run(fls, W*H, slant_neighbour, &ctx))
            err = true;
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                int u, v;
                if (state->soln[y*w+x] == 0) {
                    continue;
                } else if (state->soln[y*w+x] > 0) {
                    u = y*W+(x+1);
                    v = (y+1)*W+x;
                } else {
                    u = (y+1)*W+(x+1);
                    v = y*W+x;
                }
                if (findloop_is_loop_edge(fls, u, v))
                    state->errors[y*W+x] |= ERR_SQUARE;
        }
        }

        findloop_free_state(fls);
    }

    /*
     * Now go through and check the degree of each clue vertex, and
     * mark it with ERR_VERTEX if it cannot be fulfilled.
     */
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int c;

        if ((c = state->clues->clues[y*W+x]) < 0)
        continue;

            /*
             * Check to see if there are too many connections to
             * this vertex _or_ too many non-connections. Either is
             * grounds for marking the vertex as erroneous.
             */
            if (vertex_degree(w, h, state->soln, x, y,
                              false, NULL, NULL) > c ||
                vertex_degree(w, h, state->soln, x, y,
                              true, NULL, NULL) > 4-c) {
                state->errors[y*W+x] |= ERR_VERTEX;
                err = true;
            }
        }

    /*
     * Now our actual victory condition is that (a) none of the
     * above code marked anything as erroneous, and (b) every
     * square has an edge in it.
     */

    if (err)
        return false;

    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
        if (state->soln[y*w+x] == 0)
        return false;

    return true;
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
    int w = state->p.w, h = state->p.h;
    signed char *soln;
    int bs, ret;
    bool free_soln = false;
    char *move, buf[80];
    int movelen, movesize;
    int x, y;

    if (aux) {
    /*
     * If we already have the solution, save ourselves some
     * time.
     */
    soln = (signed char *)aux;
    bs = (signed char)'\\';
    free_soln = false;
    } else {
    struct solver_scratch *sc = new_scratch(w, h);
    soln = snewn(w*h, signed char);
    bs = -1;
    ret = slant_solve(w, h, state->clues->clues, soln, sc, DIFF_TRICKY);
    free_scratch(sc);
    if (ret != 1) {
        sfree(soln);
        if (ret == 0)
        *error = "This puzzle is not self-consistent";
        else
        *error = "Unable to find a unique solution for this puzzle";
            return NULL;
    }
    free_soln = true;
    }

    /*
     * Construct a move string which turns the current state into
     * the solved state.
     */
    movesize = 256;
    move = snewn(movesize, char);
    movelen = 0;
    move[movelen++] = 'S';
    move[movelen] = '\0';
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        int v = (soln[y*w+x] == bs ? -1 : +1);
        if (state->soln[y*w+x] != v) {
        int len = sprintf(buf, ";%c%d,%d", (int)(v < 0 ? '\\' : '/'), x, y);
        if (movelen + len >= movesize) {
            movesize = movelen + len + 256;
            move = sresize(move, movesize, char);
        }
        strcpy(move + movelen, buf);
        movelen += len;
        }
    }

    if (free_soln)
    sfree(soln);

    return move;
}

struct game_ui {
     bool swap_buttons;
     bool show_filled;
     bool show_errors;
     bool fade_grounded;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);
    ui->swap_buttons = false;
    ui->show_filled = false;
    ui->show_errors = true;
    ui->fade_grounded = false;
    return ui;
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
    ret[PREF_CLICK_ACTIONS].u.choices.choicenames = ":Short \\, Long /:Short /, Long \\";
    ret[PREF_CLICK_ACTIONS].u.choices.choicekws = ":\\:/";
    ret[PREF_CLICK_ACTIONS].u.choices.selected = ui->swap_buttons;

    ret[PREF_SHADE_FILLED].name = "Shade filled cells";
    ret[PREF_SHADE_FILLED].kw = "show-filled";
    ret[PREF_SHADE_FILLED].type = C_BOOLEAN;
    ret[PREF_SHADE_FILLED].u.boolean.bval = ui->show_filled;

    ret[PREF_SHOW_ERRORS].name = "Show errors";
    ret[PREF_SHOW_ERRORS].kw = "show-errors";
    ret[PREF_SHOW_ERRORS].type = C_BOOLEAN;
    ret[PREF_SHOW_ERRORS].u.boolean.bval = ui->show_errors;

    ret[PREF_FADE_GROUNDED].name = "Fade grounded components";
    ret[PREF_FADE_GROUNDED].kw = "fade-grounded";
    ret[PREF_FADE_GROUNDED].type = C_BOOLEAN;
    ret[PREF_FADE_GROUNDED].u.boolean.bval = ui->fade_grounded;

    ret[N_PREF_ITEMS].name = NULL;
    ret[N_PREF_ITEMS].type = C_END;

    return ret;
}

static void set_prefs(game_ui *ui, const config_item *cfg)
{
    ui->swap_buttons = cfg[PREF_CLICK_ACTIONS].u.choices.selected;
    ui->show_filled = cfg[PREF_SHADE_FILLED].u.boolean.bval;
    ui->show_errors = cfg[PREF_SHOW_ERRORS].u.boolean.bval;
    ui->fade_grounded = cfg[PREF_FADE_GROUNDED].u.boolean.bval;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
}

#define PREFERRED_TILESIZE 32
#define TILESIZE (ds->tilesize)
#define BORDER ((TILESIZE+1) / 2)
#define CLUE_RADIUS (TILESIZE / 3)
#define CLUE_TEXTSIZE (TILESIZE / 2)
#define COORD(x)  ( (x) * TILESIZE + BORDER )
#define FROMCOORD(x)  ( ((x) - BORDER + TILESIZE) / TILESIZE - 1 )

/*
 * Bit fields in the `grid' and `todraw' elements of the drawstate.
 */
#define BACKSLASH 0x00000001L
#define FORWSLASH 0x00000002L
#define L_T       0x00000004L
#define ERR_L_T   0x00000008L
#define L_B       0x00000010L
#define ERR_L_B   0x00000020L
#define T_L       0x00000040L
#define ERR_T_L   0x00000080L
#define T_R       0x00000100L
#define ERR_T_R   0x00000200L
#define C_TL      0x00000400L
#define ERR_C_TL  0x00000800L
#define ERRSLASH  0x00002000L
#define ERR_TL    0x00004000L
#define ERR_TR    0x00008000L
#define ERR_BL    0x00010000L
#define ERR_BR    0x00020000L
#define GROUNDED  0x00040000L

struct game_drawstate {
    int tilesize;
    long *grid;
    long *todraw;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button, bool swapped)
{
    int w = state->p.w, h = state->p.h;
    int v;
    char buf[80];
    enum { CLOCKWISE, ANTICLOCKWISE, NONE } action = NONE;

    if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        action = ((button == LEFT_BUTTON && !ui->swap_buttons) ||
                  (button == RIGHT_BUTTON && ui->swap_buttons)) ? CLOCKWISE : ANTICLOCKWISE;

        x = FROMCOORD(x);
        y = FROMCOORD(y);
        if (x < 0 || y < 0 || x >= w || y >= h)
            return NULL;
    }

    if (action != NONE) {
        if (action == CLOCKWISE) {
            /*
             * Left-clicking cycles blank -> \ -> / -> blank.
             */
            v = state->soln[y*w+x] - 1;
            if (v == -2)
                v = +1;
        } else {
            /*
             * Right-clicking cycles blank -> / -> \ -> blank.
             */
            v = state->soln[y*w+x] + 1;
            if (v == +2)
                v = -1;
        }

        sprintf(buf, "%c%d,%d", (int)(v==-1 ? '\\' : v==+1 ? '/' : 'C'), x, y);
        return dupstr(buf);
    }

    return MOVE_UNUSED;
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move)
{
    int w = state->p.w, h = state->p.h;
    char c;
    int x, y, n;
    game_state *ret = dup_game(state);
    ret->used_solve = false;

    while (*move) {
        c = *move;
    if (c == 'S') {
        ret->used_solve = true;
        move++;
    } else if (c == '\\' || c == '/' || c == 'C') {
            move++;
            if (sscanf(move, "%d,%d%n", &x, &y, &n) != 2 ||
                x < 0 || y < 0 || x >= w || y >= h) {
                free_game(ret);
                return NULL;
            }
            ret->soln[y*w+x] = (c == '\\' ? -1 : c == '/' ? +1 : 0);
            move += n;
        } else {
            free_game(ret);
            return NULL;
        }
        if (*move == ';')
            move++;
        else if (*move) {
            free_game(ret);
            return NULL;
        }
    }

    ret->completed = check_completion(ret);

    return ret;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    /* fool the macros */
    struct dummy { int tilesize; } dummy, *ds = &dummy;
    dummy.tilesize = tilesize;

    *x = 2 * BORDER + params->w * TILESIZE + 1;
    *y = 2 * BORDER + params->h * TILESIZE + 1;
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
        ret[COL_BACKGROUND   * 3 + i] = 1.0F;
        ret[COL_FILLEDSQUARE * 3 + i] = 0.9F;
        ret[COL_GRID         * 3 + i] = 0.25F;
        ret[COL_INK          * 3 + i] = 0.0F;
        ret[COL_SLANT1       * 3 + i] = 0.0F;
        ret[COL_SLANT2       * 3 + i] = 0.0F;
        ret[COL_ERROR        * 3 + i] = 0.5F;
        ret[COL_GROUNDED     * 3 + i] = 0.75F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    int w = state->p.w, h = state->p.h;
    int i;
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->grid = snewn((w+2)*(h+2), long);
    ds->todraw = snewn((w+2)*(h+2), long);
    for (i = 0; i < (w+2)*(h+2); i++)
    ds->grid[i] = ds->todraw[i] = -1;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->todraw);
    sfree(ds->grid);
    sfree(ds);
}

static void draw_clue(drawing *dr, game_drawstate *ds, const game_ui *ui,
              int x, int y, long v, bool err, int bg, int colour)
{
    char p[2];
    int ccol = colour >= 0 ? colour : (ui->show_errors && err) ? COL_ERROR : COL_BACKGROUND;
    int tcol = colour >= 0 ? colour : (ui->show_errors && err) ? COL_BACKGROUND : COL_INK;

    if (v < 0)
    return;

    p[0] = (char)v + '0';
    p[1] = '\0';
    draw_circle(dr, COORD(x), COORD(y), CLUE_RADIUS, ccol, COL_INK);
    draw_text(dr, COORD(x), COORD(y), FONT_VARIABLE,
          CLUE_TEXTSIZE, ALIGN_VCENTRE|ALIGN_HCENTRE, tcol, p);
}

static void draw_slash(drawing *dr, game_drawstate *ds, const game_ui *ui,
                       int x, int y, long v, int colour) {
    int i, w, h, x1, x2, y1, y2;
    int STEP=15;
    float ws, hs;

    if (v & BACKSLASH) {
        x1 = COORD(x)+1;
        y1 = COORD(y)+1;
        x2 = COORD(x+1);
        y2 = COORD(y+1);
    }
    else {
        x1 = COORD(x+1);
        y1 = COORD(y)+1;
        x2 = COORD(x)+1;
        y2 = COORD(y+1);
    }
    if (ui->show_errors && (v & ERRSLASH)) {
        w = x2-x1;
        h = y2-y1;
        ws = ((float)w/(float)STEP);
        hs = ((float)h/(float)STEP);
        for (i=0;i<STEP;i+=2) {
            draw_thick_line(dr, 5.0, x1+ i   *ws, y1+ i   * hs, x1+(i+1)*ws, y1+(i+1)*hs, colour);
            draw_thick_line(dr, 5.0, x1+(i+1)*ws, y1+(i+1)* hs, x1+(i+2)*ws, y1+(i+2)*hs, COL_BACKGROUND);
        }
    }
    else {
        draw_thick_line(dr, 5.0, x1, y1, x2, y2, v & GROUNDED ? COL_GROUNDED : colour);
    }
}

static void draw_tile(drawing *dr, game_drawstate *ds, const game_ui *ui, game_clues *clues,
              int x, int y, long v)
{
    int w = clues->w, h = clues->h, W = w+1 /*, H = h+1 */;
    int chesscolour = (x ^ y) & 1;
    int bgcol = COL_BACKGROUND;
    int fscol = chesscolour ? COL_SLANT2 : COL_SLANT1;
    int bscol = chesscolour ? COL_SLANT1 : COL_SLANT2;
    int cx, cy, tsx, tsy;
    cx = x<0 ? COORD(x+1) - BORDER : COORD(x);
    cy = y<0 ? COORD(y+1) - BORDER : COORD(y);
    tsx = x<0 || x==w ? BORDER : TILESIZE;
    tsy = y<0 || y==h ? BORDER : TILESIZE;

    if (ui->show_filled && !(ui->show_errors && (v & ERRSLASH)) && (v & (BACKSLASH | FORWSLASH)))
        bgcol = COL_FILLEDSQUARE;

    clip(dr, cx, cy, tsx, tsy);
    draw_rect(dr, cx, cy, tsx, tsy, bgcol);

    /*
     * Draw the grid lines.
     */
    if (x >= 0 && x < w && y >= 0)
        draw_rect(dr, COORD(x), COORD(y), TILESIZE+1, 1, COL_GRID);
    if (x >= 0 && x < w && y < h)
        draw_rect(dr, COORD(x), COORD(y+1), TILESIZE+1, 1, COL_GRID);
    if (y >= 0 && y < h && x >= 0)
        draw_rect(dr, COORD(x), COORD(y), 1, TILESIZE+1, COL_GRID);
    if (y >= 0 && y < h && x < w)
        draw_rect(dr, COORD(x+1), COORD(y), 1, TILESIZE+1, COL_GRID);
    if (x == -1 && y == -1)
        draw_rect(dr, COORD(x+1), COORD(y+1), 1, 1, COL_GRID);
    if (x == -1 && y == h)
        draw_rect(dr, COORD(x+1), COORD(y), 1, 1, COL_GRID);
    if (x == w && y == -1)
        draw_rect(dr, COORD(x), COORD(y+1), 1, 1, COL_GRID);
    if (x == w && y == h)
        draw_rect(dr, COORD(x), COORD(y), 1, 1, COL_GRID);

    /*
     * Draw the slash.
     */
    if ((v & BACKSLASH) || (v & FORWSLASH))
        draw_slash(dr, ds, ui, x, y, v, (v & BACKSLASH) ? bscol : fscol);

    /*
     * And finally the clues at the corners.
     */
    if (x >= 0 && y >= 0)
        draw_clue(dr, ds, ui, x, y, clues->clues[y*W+x], v & ERR_TL, -1, -1);
    if (x < w && y >= 0)
        draw_clue(dr, ds, ui, x+1, y, clues->clues[y*W+(x+1)], v & ERR_TR, -1, -1);
    if (x >= 0 && y < h)
        draw_clue(dr, ds, ui, x, y+1, clues->clues[(y+1)*W+x], v & ERR_BL, -1, -1);
    if (x < w && y < h)
        draw_clue(dr, ds, ui, x+1, y+1, clues->clues[(y+1)*W+(x+1)], v & ERR_BR,
          -1, -1);

    unclip(dr);
    draw_update(dr, cx, cy, tsx, tsy);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int w = state->p.w, h = state->p.h, W = w+1, H = h+1;
    int x, y;

    char buf[48];
    /* Draw status bar */
    sprintf(buf, "%s",
            state->used_solve ? "Auto-solved." :
            state->completed  ? "COMPLETED!" : "");
    status_bar(dr, buf);

    /*
     * Loop over the grid and work out where all the slashes are.
     * We need to do this because a slash in one square affects the
     * drawing of the next one along.
     */
    for (y = -1; y <= h; y++)
    for (x = -1; x <= w; x++) {
            ds->todraw[(y+1)*(w+2)+(x+1)] = 0;
        }

    for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
        bool err = state->errors[y*W+x] & ERR_SQUARE;

        if (state->soln[y*w+x] < 0) {
            ds->todraw[(y+1)*(w+2)+(x+1)] |= BACKSLASH;
            ds->todraw[(y+2)*(w+2)+(x+1)] |= T_R;
            ds->todraw[(y+1)*(w+2)+(x+2)] |= L_B;
            ds->todraw[(y+2)*(w+2)+(x+2)] |= C_TL;
            if (err) {
                ds->todraw[(y+1)*(w+2)+(x+1)] |= ERRSLASH | ERR_T_L | ERR_L_T | ERR_C_TL;
                ds->todraw[(y+2)*(w+2)+(x+1)] |= ERR_T_R;
                ds->todraw[(y+1)*(w+2)+(x+2)] |= ERR_L_B;
                ds->todraw[(y+2)*(w+2)+(x+2)] |= ERR_C_TL;
            }
        } else if (state->soln[y*w+x] > 0) {
            ds->todraw[(y+1)*(w+2)+(x+1)] |= FORWSLASH;
            ds->todraw[(y+1)*(w+2)+(x+2)] |= L_T | C_TL;
            ds->todraw[(y+2)*(w+2)+(x+1)] |= T_L | C_TL;
            if (err) {
                ds->todraw[(y+1)*(w+2)+(x+1)] |= ERRSLASH | ERR_L_B | ERR_T_R;
                ds->todraw[(y+1)*(w+2)+(x+2)] |= ERR_L_T | ERR_C_TL;
                ds->todraw[(y+2)*(w+2)+(x+1)] |= ERR_T_L | ERR_C_TL;
            }
        }
    }
    }

    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            if (state->errors[y*W+x] & ERR_VERTEX) {
                ds->todraw[y*(w+2)+x] |= ERR_BR;
                ds->todraw[y*(w+2)+(x+1)] |= ERR_BL;
                ds->todraw[(y+1)*(w+2)+x] |= ERR_TR;
                ds->todraw[(y+1)*(w+2)+(x+1)] |= ERR_TL;
            }
    if (ui->fade_grounded)
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                if (state->errors[y*w+x] & BORDER_EDGE)
                    // dunno why but it works this way
                    ds->todraw[(y+1)*(W+1)+(x+1)] |= GROUNDED;

    /*
     * Now go through and draw the grid squares.
     */
    for (y = -1; y <= h; y++) {
    for (x = -1; x <= w; x++) {
        if (ds->todraw[(y+1)*(w+2)+(x+1)] != ds->grid[(y+1)*(w+2)+(x+1)]) {
            draw_tile(dr, ds, ui, state->clues, x, y,
                      ds->todraw[(y+1)*(w+2)+(x+1)]);
            ds->grid[(y+1)*(w+2)+(x+1)] = ds->todraw[(y+1)*(w+2)+(x+1)];
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
#define thegame slant
#endif

static const char rules[] = "You have a grid of squares, and some circles with clues. Your aim is to draw a diagonal line through each square, and choose which way each line slants so that the following conditions are met\n\n"
"- The diagonal lines never form a loop.\n"
"- Any point with a circled number has precisely that many lines meeting at it.\n\n\n"
"This puzzle was implemented by Simon Tatham.";

const struct game thegame = {
    "Slant", "games.slant", "slant", rules,
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
    PREFERRED_TILESIZE, game_compute_size, game_set_size,
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

