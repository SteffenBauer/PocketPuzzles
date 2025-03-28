/*
 * creek.c
 */

/*
 * In this puzzle you have a grid of squares, each of which must
 * be filled either black or white; you also have clue numbers placed at
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
 * each point will be +1 for a black cell ('B'), -1 for a
 * white cell ('W'), and 0 for unknown.
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
    COL_LIGHT,
    COL_GRID,
    COL_INK,
    COL_ERROR,
    COL_CREEK_UNDEF,
    COL_CREEK_FILLED,
    COL_CREEK_EMPTY,
    NCOLOURS
};

enum {
    PREF_CLICK_ACTIONS,
    N_PREF_ITEMS
};

/*
 * Difficulty levels. I do some macro ickery here to ensure that my
 * enum and the various forms of my name list always match up.
 */
#define DIFFLIST(A) \
    A(EASY,Easy,e) \
    A(TRICKY,Tricky,t) \
    A(HARD,Hard,h)
#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const creek_diffnames[] = { DIFFLIST(TITLE) };
static char const creek_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

struct game_params {
    int w, h, diff;
};

typedef struct game_clues {
    int w, h;
    signed char *clues;
    DSF *tmpdsf;
    int refcount;
} game_clues;

#define ERR_VERTEX 1
#define ERR_SQUARE 2

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

    ret->w = 8;
    ret->h = 8;
    ret->diff = DIFF_TRICKY;
    return ret;
}

static const struct game_params creek_presets[] = {
    { 6,  6, DIFF_EASY},
    { 6,  6, DIFF_TRICKY},
    { 8,  8, DIFF_TRICKY},
    { 8,  8, DIFF_HARD},
    {10, 10, DIFF_TRICKY},
    {10, 10, DIFF_HARD}
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char str[80];

    if (i < 0 || i >= lenof(creek_presets))
        return false;

    ret = snew(game_params);
    *ret = creek_presets[i];

    sprintf(str, "%dx%d, %s", ret->w, ret->h, creek_diffnames[ret->diff]);

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
    *ret = *params; /* structure copy */
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
    if (*string && *string == 'd') {
        int i;
        string++;
        for (i = 0; i < DIFFCOUNT; i++)
            if (*string == creek_diffchars[i])
                ret->diff = i;
        if (*string) string++;
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char data[256];

    sprintf(data, "%dx%d", params->w, params->h);
    if (full)
        sprintf(data + strlen(data), "d%c", creek_diffchars[params->diff]);

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
     * And puzzles with a dimension of 2 can't be made Hard, which
     * means the simplest thing is to forbid them altogether.
     */

    if (params->w < 3 || params->h < 3)
        return "Width and height must both be at least three";
    if (params->w > 12 || params->h > 12)
        return "Width and height must both be at most 12";
    if (params->diff == DIFF_HARD && (params->w > 10 || params->h > 10))
        return "Width and height for a hard puzzle should be at most 10";
    return NULL;
}

struct solver_scratch_creek {
    DSF *whitedsf;
    signed char *tmpsoln;
    const signed char *clues;
    int depth;
};

static struct solver_scratch_creek *new_scratch_creek(int w, int h) {
    struct solver_scratch_creek *ret = snew(struct solver_scratch_creek);
    ret->whitedsf = dsf_new(w*h);
    ret->tmpsoln = snewn(w*h, signed char);
    ret->depth = 0;
    return ret;
}

static struct solver_scratch_creek *dup_scratch_creek(int w, int h, const struct solver_scratch_creek *scc) {
    struct solver_scratch_creek *ret = snew(struct solver_scratch_creek);
    if (scc->whitedsf != NULL) {
        ret->whitedsf = dsf_new(w*h);
        dsf_copy(ret->whitedsf, scc->whitedsf);
    }     
    
    if (scc->tmpsoln != NULL) {
        ret->tmpsoln = snewn(w*h, signed char); 
        memcpy(ret->tmpsoln, scc->tmpsoln, w*h*sizeof(signed char));
    }
    ret->clues = scc->clues;
    ret->depth = scc->depth;
    return ret;
}

static void free_scratch_creek(struct solver_scratch_creek *scc) {
    dsf_free(scc->whitedsf);
    sfree(scc->tmpsoln);
    sfree(scc);
}

static int vertex_degree_creek(int w, int h, signed char *soln, int x, int y,
                         bool anti, int *sx, int *sy)
{
    int ret = 0;
    int neigh = 0;
    assert(x >= 0 && x <= w && y >= 0 && y <= h);
    if (x > 0 && y > 0) {
        neigh++;
        if( soln[(y-1)*w+(x-1)] == (anti ? -1 : 1) )  ret++;
    }
    if (x > 0 && y < h) {
        neigh++;
        if (soln[y*w+(x-1)] == (anti ? -1 : 1) )      ret++;
    }
    if (x < w && y > 0) {
        neigh++;
        if (soln[(y-1)*w+x] == (anti ? -1 : 1) )      ret++;
    }
    if (x < w && y < h) {
        neigh++;
        if (soln[y*w+x] == (anti ? -1 : 1) )          ret++;
    }

    return anti ? ret + (4-neigh) : ret;
}

static bool check_connectedness_creek(int w, int h, DSF *dsf,
                     signed char *soln, const signed char *clues, int level) {
    int x, y, i, first_white = -1;
    int W = w+1, H = h+1;
    
    dsf_reinit(dsf);
    
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
               if (soln[y*w+x] < 0 && first_white < 0) {
                   first_white = y*w+x;
                   break;
               }
        }
        if (first_white >= 0)
            break;
    }

    if (first_white >= 0) {
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                int g = y*w+x;
                if (soln[g] <= 0) { 
                    int gl,gr,gu,gd;
                    gl = y*w+(x-1);
                    gr = y*w+(x+1);
                    gu = (y-1)*w+x;
                    gd = (y+1)*w+x;

                    if (level == DIFF_EASY) {
                        if (x>0 && soln[gl] <= 0)
                            dsf_merge(dsf, g, gl);
                        if (x < w-1 && soln[gr] <= 0)
                            dsf_merge(dsf, g, gr);
                        if (y>0 && soln[gu] <= 0)
                            dsf_merge(dsf, g, gu);
                        if (y < h-1 && soln[gd] <= 0)
                            dsf_merge(dsf, g, gd);
                    }
                    else {
                        int cur,cdr,cul,cdl;
                        cul = y*W+x;
                        cdl = (y+1)*W+x;
                        cur = y*W+(x+1);
                        cdr = (y+1)*W+(x+1);

                        if (x>0 && soln[gl] <= 0 && 
                           (clues[cul] != 3 && clues[cdl] != 3) && 
                           (y != 0 || clues[cul] != 1) && (y != h-1 || clues[cdl] != 1))
                            dsf_merge(dsf, g, gl);
                        if (y>0 && soln[gu] <= 0 && 
                           (clues[cul] != 3 && clues[cur] != 3) &&
                           (x != 0 || clues[cul] != 1) && (x != w-1 || clues[cur] != 1))
                            dsf_merge(dsf, g, gu);
                        if (x < w-1 && soln[gr] <= 0 && 
                           (clues[cur] != 3 && clues[cdr] != 3) &&
                           (y != 0 || clues[cur] != 1) && (y != h-1 || clues[cdr] != 1))
                            dsf_merge(dsf, g, gr);
                        if (y < h-1 && soln[gd] <= 0 && 
                           (clues[cdl] != 3 && clues[cdr] != 3) &&
                           (x != 0 || clues[cdl] != 1) && (x != w-1 || clues[cdr] != 1))
                            dsf_merge(dsf, g, gd);
                    }
                }
            }
        }

        /* Every white grid cell must be somehow reachable 
         * from any other white space */
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                if (soln[y*w+x] < 0) {
                    if (!dsf_equivalent(dsf, first_white, y*w+x)) {
                        return false;
                    }
                }
            }
        }

        /* At least 4 minus clue number grid spaces around each clue 
         * must be somehow reachable from any other white space */
        if (level > DIFF_TRICKY) {
            for (y = 0; y < H; y++)
            for (x = 0; x < W; x++) {
                int c, nu, nw, nb, nneigh, co;
                int neighbours[4];

                if ((c = clues[y*W+x]) < 0)
                    continue;
            
                nneigh = 0;
                if (x > 0 && y > 0) neighbours[nneigh++] = (y-1)*w+(x-1);
                if (x > 0 && y < h) neighbours[nneigh++] = y*w+(x-1);
                if (x < w && y < h) neighbours[nneigh++] = y*w+x;
                if (x < w && y > 0) neighbours[nneigh++] = (y-1)*w+x;
                
                nu = nw = nb = 0;
                for (i = 0; i < nneigh; i++) {
                    if (soln[neighbours[i]] == 0)         nu++;
                    else if (soln[neighbours[i]] == -1) nw++;
                    else if (soln[neighbours[i]] == +1) nb++;
                }
                
                if (nw < (nneigh-c)) {
                    co = 0;
                    for (i=0;i<nneigh;i++) {
                        if (soln[neighbours[i]] <= 0)
                            if (dsf_equivalent(dsf, first_white, neighbours[i]))
                                co++;
                    }
                    if (co < nneigh-c)
                        return false;
                }
            }
        }
    }
    
    return true;
}

static bool check_completed_creek(int w, int h,
           const signed char *clues,
           signed char *soln,
           unsigned char *errors,
           DSF *dsf)
{
    int W = w+1, H = h+1;
    int x, y;
    bool err = false;
    memset(errors, 0, W*H);

    if (!check_connectedness_creek(w, h, dsf, soln, clues, DIFF_EASY)) {
        err = true;
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                if (soln[y*w+x] < 0) {
                    errors[y*W+x] |= ERR_SQUARE;
                }
            }
        }
    }

    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int c;

            if ((c = clues[y*W+x]) < 0)
                continue;

            if (vertex_degree_creek(w, h, soln, x, y,
                             false, NULL, NULL) > c ||
                vertex_degree_creek(w, h, soln, x, y,
                             true, NULL, NULL) > 4-c) {
                errors[y*W+x] |= ERR_VERTEX;
                err = true;
            }
        }


    if (err)
        return false;

    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
        if (soln[y*w+x] == 0) return false;

    return true;
}

static void initialize_solver_creek(int w, int h, const signed char *clues,
                              signed char *soln, struct solver_scratch_creek *scc,
                      int difficulty) {
    memset(soln, 0, w*h);
    scc->clues = clues;
    dsf_reinit(scc->whitedsf);
    return;
}
                      
static int creek_solve(int w, int h, const signed char *clues,
               signed char *soln, struct solver_scratch_creek *scc,
               int difficulty)
{
    int W = w+1, H = h+1;
    int x, y, i, j;
    bool done_something;
    int firstwhite;
    
    if (scc->depth >= 2 && difficulty <= DIFF_TRICKY) return 3;
    
    do {
        done_something = false;

     /* Any clue point with the number of remaining filled boxes equal
      * to zero or to the number of remaining unfilled
      * boxes can be filled in completely. */
        for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int c, nu, nw, nb, nneigh;
            int neighbours[4];
            if ((c = clues[y*W+x]) < 0)
                continue;

            /* We have a clue point. Start by listing its
             * neighbouring squares, in order around the point */
            nneigh = 0;
            if (x > 0 && y > 0) neighbours[nneigh++] = (y-1)*w+(x-1);
            if (x > 0 && y < h) neighbours[nneigh++] = y*w+(x-1);
            if (x < w && y < h) neighbours[nneigh++] = y*w+x;
            if (x < w && y > 0) neighbours[nneigh++] = (y-1)*w+x;

            nu = nw = nb = 0;
            for (i = 0; i < nneigh; i++) {
                j = neighbours[i];
                if (soln[j] == 0) nu++;
                else if (soln[j] == -1)            nw++;
                else if (soln[j] == +1)            nb++;
            }

            if (nw == (nneigh-c) && nb < c) {
                for (i=0;i<nneigh;i++) {
                    j = neighbours[i];
                    if (soln[j] == 0)
                        soln[j] = 1;
                }
                done_something = true;
            }
            else if (nw < (nneigh-c) && nb == c) {
                for (i=0;i<nneigh;i++) {
                    j = neighbours[i];
                    if (soln[j] == 0)
                        soln[j] = -1;
                }
                done_something = true;
            }
            else if (nb > c || nw > (nneigh-c)) {
                return 0;           /* impossible */
            }

        }
        if (done_something) continue;

        /* Fill single white fields */
        if (scc->depth == 0 || difficulty == DIFF_HARD) 
        for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int nneigh, nj;
            if (soln[y*w+x] != -1) continue;
            
            nneigh = 0;
            if (x > 0   && soln[y*w+(x-1)] == 0) { nneigh++; nj = y*w+(x-1); }
            if (x < w-1 && soln[y*w+(x+1)] == 0) { nneigh++; nj = y*w+(x+1); }
            if (y > 0   && soln[(y-1)*w+x] == 0) { nneigh++; nj = (y-1)*w+x; }
            if (y < h-1 && soln[(y+1)*w+x] == 0) { nneigh++; nj = (y+1)*w+x; }
            
            if (nneigh == 1) {
                memcpy (scc->tmpsoln, soln, w*h*sizeof(signed char));
                scc->tmpsoln[nj] = 1;
                if (!check_connectedness_creek(w, h, scc->whitedsf, scc->tmpsoln, clues, difficulty)) {
                    soln[nj] = -1;
                    done_something = true;
                }
                if (done_something) break;
            }
            if (done_something) break;
        }        
        if (done_something) continue;
        
        if (difficulty >= DIFF_TRICKY && scc->depth == 0) {
            for (y = 0; y < H; y++) 
            for (x = 0; x < W; x++) {
                int c, nu, nw, nb, nneigh, no;
                int neighbours[4];
                if ((c = clues[y*W+x]) < 0)
                    continue;

                /* We have a clue point. Start by listing its
                 * neighbouring squares, in order around the point */
                nneigh = 0;
                if (x > 0 && y > 0) neighbours[nneigh++] = (y-1)*w+(x-1);
                if (x > 0 && y < h) neighbours[nneigh++] = y*w+(x-1);
                if (x < w && y < h) neighbours[nneigh++] = y*w+x;
                if (x < w && y > 0) neighbours[nneigh++] = (y-1)*w+x;
                    
                nu = nw = nb = 0;
                for (i = 0; i < nneigh; i++) {
                    j = neighbours[i];
                    if (soln[j] == 0)       nu++;
                    else if (soln[j] == -1) nw++;
                    else if (soln[j] == +1) nb++;
                }
                no = -1;
                if (c == 1) no = nu;
                if (c == 3) no = nu;
                if (c == 2 && nu == 2) no = 2;
                if (c == 2 && nu == 3) no = 3;
                if (c == 2 && nu == 4) no = 6;
                
                if (c == 3 && no > 0) {
                    for (i = 0;i < nneigh; i++) {
                        int ret;
                        struct solver_scratch_creek *tscc = dup_scratch_creek(w,h,scc);
                        memcpy(scc->tmpsoln, soln, w*h*sizeof(signed char));
                        tscc->depth = 1;
                        j = neighbours[i];
                        if (soln[j] == 0) {
                            scc->tmpsoln[j] = -1;
                            ret = creek_solve(w,h,clues,scc->tmpsoln, tscc, difficulty);
                            if (ret == 0) {
                                done_something = true;
                                soln[j] = 1;
                            }
                        }
                        free_scratch_creek(tscc);
                        if (done_something) break;
                    }
                }

                if ((c == 1 || c == 2) && no > 0 && difficulty == DIFF_HARD) {
                    for (i = 0; i < nneigh; i++) {
                        int ret;
                        struct solver_scratch_creek *tscc = dup_scratch_creek(w,h,scc);
                        memcpy(scc->tmpsoln, soln, w*h*sizeof(signed char));
                        tscc->depth = 1;
                        j = neighbours[i];
                        if (soln[j] == 0) {
                            scc->tmpsoln[j] = 1;
                            ret = creek_solve(w,h,clues,scc->tmpsoln, tscc, difficulty);
                            if (ret == 0) {
                                done_something = true;
                                soln[j] = -1;
                            }
                        }
                        free_scratch_creek(tscc);
                        if (done_something) break;

                        if (c == 2) {
                            tscc = dup_scratch_creek(w,h,scc);
                            memcpy(scc->tmpsoln, soln, w*h*sizeof(signed char));
                            tscc->depth = 1;
                            j = neighbours[i];
                            if (soln[j] == 0) {
                                scc->tmpsoln[j] = -1;
                                ret = creek_solve(w,h,clues,scc->tmpsoln, tscc, difficulty);
                                if (ret == 0) {
                                    done_something = true;
                                    soln[j] = 1;
                                }
                            }
                            free_scratch_creek(tscc);
                            if (done_something) break;
                        }
                    }

                    if (c == 2 && no == 6) {
                        int ret;
                        int r1,r2;
                        int cb[4];
                        struct solver_scratch_creek *tscc;
                        for (i=0;i<4;i++) cb[i] = 0;
                            
                        for (r1=0;r1<3;r1++)
                        for (r2=r1+1;r2<4;r2++) {
                            tscc = dup_scratch_creek(w,h,scc);
                            memcpy(scc->tmpsoln, soln, w*h*sizeof(signed char));
                            tscc->depth = 1;
                            scc->tmpsoln[neighbours[r1]] = 1;
                            scc->tmpsoln[neighbours[r2]] = 1;
                            ret = creek_solve(w,h,clues,scc->tmpsoln, tscc, difficulty);
                            free_scratch_creek(tscc);
                            if (ret == 0) {
                                cb[r1]++;
                                cb[r2]++;
                            }
                        }

                        for (i=0;i<4;i++) {
                            if (cb[i] == 3) {
                                done_something = true;
                                j = neighbours[i];
                                soln[j] = -1;
                            }
                        }
                    }
                }
                if (done_something) break;
            }
        }
        if (done_something) continue;

        /* Fill in isolated grey areas */
        firstwhite = -1;
        for (i=0;i<w*h;i++)
            if (soln[i] == -1) {
                firstwhite = i;
                break;
            }
        
        if (firstwhite >= 0) {
            dsf_reinit(scc->whitedsf);
            for (i=0;i<w*h;i++) {
                if (soln[i] == 1) continue;
                x = i % w; y = i / w;
                if (x > 0   && soln[y*w+(x-1)] != 1) dsf_merge(scc->whitedsf, i, y*w+(x-1));
                if (x < w-1 && soln[y*w+(x+1)] != 1) dsf_merge(scc->whitedsf, i, y*w+(x+1));
                if (y > 0   && soln[(y-1)*w+x] != 1) dsf_merge(scc->whitedsf, i, (y-1)*w+x);
                if (y < h-1 && soln[(y+1)*w+x] != 1) dsf_merge(scc->whitedsf, i, (y+1)*w+x);                
            }
            
            for (i=0;i<w*h;i++) {
                if (soln[i] == 0) {
                    if (!dsf_equivalent(scc->whitedsf, i, firstwhite)) {
                        soln[i] = 1;
                        done_something = true; 
                    }
                }
            }
        }
        if (done_something) continue;

    } while (done_something);

       /* Check if grid is connected */
       if (!check_connectedness_creek(w, h, scc->whitedsf, soln, clues, DIFF_EASY)) {
           return 0;
       }

    /* Solver can make no more progress. See if the grid is full. */
    for (i = 0; i < w*h; i++)
        if (soln[i] == 0) {
            return 2;               /* failed to converge */
        }

    return 1;                   /* success */
}


/*
 * Filled-grid generator.
 */
static void creek_generate(int w, int h, signed char *soln, random_state *rs)
{
    int i;
    int *whites, nw;
    DSF *connected;
    int ridx;

    memset(soln, -1, w*h);
    connected = dsf_new(w*h);
    whites = snewn(w*h, int);

    while(true) {
        nw = 0;
        for (i=0;i<w*h;i++)
            if (soln[i] == -1)
                whites[nw++] = i;

        if (nw < (w*h)/3) break;
        for (i=0;i<(w+h)/2;i++) {
            ridx = random_upto(rs, nw);
            soln[whites[ridx]] = 1;
            if (!check_connectedness_creek(w, h, connected, soln, NULL, DIFF_EASY))
                soln[whites[ridx]] = -1;
            else
                break;
        }
        if (i == (w+h)/2) break;
    }

    sfree(whites);
    dsf_free(connected);
}

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive)
{
    int w = params->w, h = params->h, W = w+1, H = h+1;
    signed char *soln, *tmpsoln, *clues;
    int *clueindices;
    struct solver_scratch_creek *scc;
    int x, y, v, i, j;
    char *desc;
    char cont;

    soln = snewn(w*h, signed char);
    tmpsoln = snewn(w*h, signed char);
    clues = snewn(W*H, signed char);
    clueindices = snewn(W*H, int);
    scc = new_scratch_creek(w, h);

    do {
        /*
         * Create the filled grid.
         */
        creek_generate(w, h, soln, rs);

        /*
         * Fill in the complete set of clues.
         */
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                v = 0;
                if (x > 0 && y > 0 && soln[(y-1)*w+(x-1)] == +1) v++;
                if (x > 0 && y < h && soln[y*w+(x-1)] == +1) v++;
                if (x < w && y > 0 && soln[(y-1)*w+x] == +1) v++;
                if (x < w && y < h && soln[y*w+x] == +1) v++;

                clues[y*W+x] = v;
            }
        }
        /*
         * With all clue points filled in, all puzzles are easy: we can
         * simply process the clue points in lexicographic order, and
         * at each clue point we will always have at most one square
         * undecided, which we can then fill in uniquely.
         */
        initialize_solver_creek(w, h, clues, tmpsoln, scc, DIFF_EASY);
        assert(creek_solve(w, h, clues, tmpsoln, scc, DIFF_EASY) == 1);

        /*
         * Remove as many clues as possible while retaining solubility.
         *
         * In DIFF_TRICKY and higher mode, we prioritise the removal of obvious
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
                if (v == -1) continue;

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
                    initialize_solver_creek(w, h, clues, tmpsoln, scc, params->diff);
                    if (creek_solve(w, h, clues, tmpsoln, scc, params->diff) != 1)
                        clues[y*W+x] = v;           /* put it back */
                }
            }
        }
        /*
         * And finally, verify that the grid is of _at least_ the
         * requested difficulty, by running the solver one level
         * down and verifying that it can't manage it.
         */
        if (params->diff > 0) {
            initialize_solver_creek(w, h, clues, tmpsoln, scc, params->diff - 1);
            cont = creek_solve(w, h, clues, tmpsoln, scc, params->diff - 1) <= 1;
        }
        else
            cont = false;

    } while (cont);
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

    free_scratch_creek(scc);
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
    state->clues->tmpdsf = dsf_new(W*H*2+W+H);
    memset(state->clues->clues, -1, W*H);
    while (*desc) {
        int n = *desc++;
        if (n >= 'a' && n <= 'z') {
            squares += n - 'a' + 1;
        } else if (n >= '0' && n <= '4') {
            state->clues->clues[squares++] = n - '0';
        } else
            assert(!"can't get here");
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
        dsf_free(state->clues->tmpdsf);
        sfree(state->clues);
    }
    sfree(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
    int w = state->p.w, h = state->p.h;
    signed char *soln;
    int ret;
    char *move, buf[80];
    int movelen, movesize;
    int x, y;

    struct solver_scratch_creek *scc = new_scratch_creek(w, h);
    soln = snewn(w*h, signed char);
    initialize_solver_creek(w, h, state->clues->clues, soln, scc, DIFF_HARD);
    ret = creek_solve(w, h, state->clues->clues, soln, scc, DIFF_HARD);
    free_scratch_creek(scc);
    if (ret != 1) {
        sfree(soln);
        if (ret == 0)
            *error = "This puzzle is not self-consistent";
        else
            *error = "Unable to find a unique solution for this puzzle";
        return NULL;
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
            int v = soln[y*w+x];
            if (state->soln[y*w+x] != v && soln[y*w+x] != 0) {
                int len = sprintf(buf, ";%c%d,%d", (v < 0 ? 'W' : 'B'), x, y);
                if (movelen + len >= movesize) {
                    movesize = movelen + len + 256;
                    move = sresize(move, movesize, char);
                }
                strcpy(move + movelen, buf);
                movelen += len;
            }
        }

    sfree(soln);

    return move;
}

typedef enum {CLEAR, BLACK, WHITE} DRAGMODE;

struct game_ui {
    bool is_drag;
    bool *seln;
    int dx, dy;
    DRAGMODE mode;
    int click_mode;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);
    ui->seln = state ? snewn(state->p.w * state->p.h, bool) : NULL;
    ui->is_drag = false;
    ui->dx = ui->dy = -1;
    ui->click_mode = 0;
    return ui;
}

static void free_ui(game_ui *ui)
{
    if (ui->seln) sfree(ui->seln);
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
#define ERR_TL    0x0001
#define ERR_TR    0x0002
#define ERR_BL    0x0004
#define ERR_BR    0x0008

#define CR_BLACK  0x0010
#define CR_WHITE  0x0020
#define CR_GREY   0x0040
#define CR_ERR    0x0080

struct game_drawstate {
    int tilesize;
    long *grid;
    long *todraw;
};

static DRAGMODE get_dragmode(int button, int cell, const game_ui *ui, bool swapped) {
    if (button == LEFT_DRAG) button = LEFT_BUTTON;
    if (button == RIGHT_DRAG) button = RIGHT_BUTTON;
    if (button == LEFT_BUTTON && cell == 0) return (ui->click_mode == 0) ? BLACK : WHITE;
    if (button == LEFT_BUTTON && cell == 1) return (ui->click_mode == 0) ? WHITE : CLEAR;
    if (button == LEFT_BUTTON && cell == -1) return (ui->click_mode == 0) ? CLEAR : BLACK;
    if (button == RIGHT_BUTTON && cell == 0) return (ui->click_mode == 0) ? WHITE : BLACK;
    if (button == RIGHT_BUTTON && cell == 1) return (ui->click_mode == 0) ? CLEAR : WHITE;
    if (button == RIGHT_BUTTON && cell == -1) return (ui->click_mode == 0) ? BLACK : CLEAR;
    return CLEAR;
}

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button, bool swapped)
{
    int w = state->p.w, h = state->p.h;

    x = FROMCOORD(x);
    y = FROMCOORD(y);

    if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        if (x < 0 || y < 0 || x >= w || y >= h) {
            ui->is_drag = false;
            ui->dx = ui->dy = -1;
            memset(ui->seln, false, (unsigned int)(w*h));
            return MOVE_UI_UPDATE;
        }
        ui->mode = get_dragmode(button, state->soln[y*w+x], ui, swapped);
        ui->is_drag = true;
        ui->dx = x; ui->dy = y;
        memset(ui->seln, false, w*h);
        ui->seln[y*w+x] = true;
        return MOVE_UI_UPDATE;
    } else if ((button == LEFT_DRAG || button == RIGHT_DRAG) &&
               (x >= 0 && y >= 0 && x < w && y < h)) {
        if (!ui->is_drag) {
            ui->is_drag = true;
            ui->dx = x; ui->dy = y;
            memset(ui->seln, false, w*h);
            ui->seln[y*w+x] = true;
            ui->mode = get_dragmode(button, state->soln[y*w+x], ui, swapped);
            return MOVE_UI_UPDATE;
        }
        if (x != ui->dx || y != ui->dy) {
            if (!ui->seln[y*w+x]) {
                if ((ui->mode == BLACK && state->soln[y*w+x] != 1) ||
                    (ui->mode == WHITE && state->soln[y*w+x] != -1) ||
                    (ui->mode == CLEAR && state->soln[y*w+x] != 0)) {
                     ui->seln[y*w+x] = true;
                     ui->dx = x; ui->dy = y;
                     return MOVE_UI_UPDATE;
                }
            } else if (ui->seln[ui->dy*w+ui->dx]){
                if ((ui->mode == BLACK && state->soln[ui->dy*w+ui->dx] != 1) ||
                    (ui->mode == WHITE && state->soln[ui->dy*w+ui->dx] != -1) ||
                    (ui->mode == CLEAR && state->soln[ui->dy*w+ui->dx] != 0)) {
                     ui->seln[ui->dy*w+ui->dx] = false;
                     ui->dx = x; ui->dy = y;
                     return MOVE_UI_UPDATE;
                }
            }
            return NULL;
        }
    } else if (button == LEFT_RELEASE || button == RIGHT_RELEASE ||
               x < 0 || y < 0 || x >= w || y >= h) {
        int i, tmplen, buflen, bufsize;
        char *buf;
        const char *sep;
        char tmpbuf[80];
        buflen = 0;
        bufsize = 256;
        buf = snewn(bufsize, char);
        sep = "";

        for (i=0;i<w*h;i++) {
            if (ui->seln[i]) {
                tmplen = sprintf(tmpbuf, 
                    "%s%c%d,%d", sep,
                         (ui->mode == CLEAR ? 'C' :
                          ui->mode == BLACK ? 'B' : 'W'),
                          i%w, i/w);
                    sep = ";";

                    if (buflen + tmplen >= bufsize) {
                        bufsize = buflen + tmplen + 256;
                        buf = sresize(buf, bufsize, char);
                    }

                    strcpy(buf+buflen, tmpbuf);
                    buflen += tmplen;
            }

        }
        ui->is_drag = false;
        ui->dx = ui->dy = -1;
        memset(ui->seln, false, (unsigned int)(w*h));

        if (buflen == 0) {
            sfree(buf);
            return MOVE_UI_UPDATE;          /* drag was terminated */
        } else {
            buf[buflen] = '\0';
            return buf;
        }
    }
    return MOVE_UNUSED;
}

static game_state *execute_move(const game_state *state, const game_ui *ui, const char *move)
{
    int w = state->p.w, h = state->p.h;
    char c;
    int x, y, n;
    game_state *ret = dup_game(state);
    ret->used_solve = ret->completed = false;
    while (*move) {
        c = *move;
        if (c == 'S') {
            ret->used_solve = true;
            move++;
        } else if (c == 'B' || c == 'W' || c == 'C') {
            move++;
            if (sscanf(move, "%d,%d%n", &x, &y, &n) != 2 ||
                x < 0 || y < 0 || x >= w || y >= h) {
                free_game(ret);
                return NULL;
            }
            ret->soln[y*w+x] = (c == 'B' ? 1 : c == 'W' ? -1 : 0);
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

    ret->completed = check_completed_creek(w, h, ret->clues->clues, ret->soln, ret->errors, ret->clues->tmpdsf);
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
        ret[COL_LIGHT        * 3 + i] = 0.9F;
        ret[COL_GRID         * 3 + i] = 0.25F;
        ret[COL_INK          * 3 + i] = 0.0F;
        ret[COL_ERROR        * 3 + i] = 0.5F;
        ret[COL_CREEK_UNDEF  * 3 + i] = 0.75F;
        ret[COL_CREEK_FILLED * 3 + i] = 0.0F;
        ret[COL_CREEK_EMPTY  * 3 + i] = 1.0F;
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

static void draw_clue(drawing *dr, game_drawstate *ds,
                      int x, int y, long v, bool err, int bg, int colour)
{
    char p[2];
    int ccol = colour >= 0 ? colour : err ? COL_ERROR : COL_BACKGROUND;
    int tcol = colour >= 0 ? colour : err ? COL_BACKGROUND : COL_INK;

    if (v < 0)
        return;

    p[0] = (char)v + '0';
    p[1] = '\0';
    draw_circle(dr, COORD(x), COORD(y), CLUE_RADIUS,
                ccol, COL_INK);
    draw_text(dr, COORD(x), COORD(y), FONT_VARIABLE,
              CLUE_TEXTSIZE, ALIGN_VCENTRE|ALIGN_HCENTRE, tcol, p);
}

static void draw_tile_creek(drawing *dr, game_drawstate *ds, game_clues *clues,
              int x, int y, long v)
{
    int w = clues->w, h = clues->h, W = w+1 /*, H = h+1 */;
    int cx, cy, tsx, tsy;
    cx = x<0 ? COORD(x+1) - BORDER: COORD(x);
    cy = y<0 ? COORD(y+1) - BORDER: COORD(y);
    tsx = x<0 || x==w ? BORDER : TILESIZE;
    tsy = y<0 || y==h ? BORDER : TILESIZE;

    clip(dr, cx, cy, tsx, tsy);

    draw_rect(dr, cx, cy, tsx, tsy,
          (v & CR_ERR)   ? COL_CREEK_EMPTY :
          (v & CR_BLACK) ? COL_CREEK_FILLED :
          (v & CR_WHITE) ? COL_CREEK_EMPTY :
          (x<0 || x==w || y<0 || y==h) ? COL_BACKGROUND : COL_CREEK_UNDEF);

    if (v & CR_ERR)
        draw_circle(dr, COORD(x) + (ds->tilesize)/2, COORD(y) + (ds->tilesize)/2, (9+ ds->tilesize) / 20, COL_ERROR, COL_GRID);

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
      * And finally the clues at the corners.
      */
     if (x >= 0 && y >= 0)
         draw_clue(dr, ds, x, y, clues->clues[y*W+x], v & ERR_TL, -1, -1);
     if (x < w && y >= 0)
         draw_clue(dr, ds, x+1, y, clues->clues[y*W+(x+1)], v & ERR_TR, -1, -1);
     if (x >= 0 && y < h)
         draw_clue(dr, ds, x, y+1, clues->clues[(y+1)*W+x], v & ERR_BL, -1, -1);
     if (x < w && y < h)
         draw_clue(dr, ds, x+1, y+1, clues->clues[(y+1)*W+(x+1)], v & ERR_BR,
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

    sprintf(buf, "%s",
            state->used_solve ? "Auto-solved." :
            state->completed  ? "COMPLETED!" : "");
    status_bar(dr, buf);

    for (y = -1; y <= h; y++) {
        for (x = -1; x <= w; x++) {
            ds->todraw[(y+1)*(w+2)+(x+1)] = 0;
        }
    }

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            bool err = state->errors[y*W+x] & ERR_SQUARE;

            if (err) {
                ds->todraw[(y+1)*(w+2)+(x+1)] |= CR_ERR;
            }
            else if ((state->soln[y*w+x] != 0) &&
               ui->is_drag && ui->mode == CLEAR && ui->seln[y*w+x]) {
            }
            else if (ui->is_drag && ui->mode == WHITE && ui->seln[y*w+x]) {
                ds->todraw[(y+1)*(w+2)+(x+1)] |= CR_WHITE;
            }
            else if (ui->is_drag && ui->mode == BLACK && ui->seln[y*w+x]) {
                ds->todraw[(y+1)*(w+2)+(x+1)] |= CR_BLACK;
            }
            else if (state->soln[y*w+x] < 0) {
                ds->todraw[(y+1)*(w+2)+(x+1)] |= CR_WHITE;
            }
            else if (state->soln[y*w+x] > 0) {
                ds->todraw[(y+1)*(w+2)+(x+1)] |= CR_BLACK;
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

        /*
         * Now go through and draw the grid squares.
         */
    for (y = -1; y <= h; y++) {
        for (x = -1; x <= w; x++) {
             if (ds->todraw[(y+1)*(w+2)+(x+1)] != ds->grid[(y+1)*(w+2)+(x+1)]) {
                draw_tile_creek(dr, ds, state->clues, x, y,
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
#define thegame creek
#endif

static const char rules[] = "You have a grid of squares, and some circles with clues.\n\n"
"Color each square either black or white, so that:\n\n"
"- All white squares must form a connected area.\n"
"- The circled clues indicate the number of black squares around it.\n\n\n"
"This puzzle was contributed by Steffen Bauer.";

const struct game thegame = {
    "Creek", "games.creek", "creek", rules,
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

