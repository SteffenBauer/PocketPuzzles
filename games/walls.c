/*

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

#define DIFFLIST(A) \
    A(EASY,Easy,e) \
    A(NORMAL,Normal,n)
#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const walls_diffnames[] = { DIFFLIST(TITLE) "(count)" };
static char const walls_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

#define BLANK (0x00)
#define L (0x01)
#define R (0x02)
#define U (0x04)
#define D (0x08)

#define EDGE_NONE       (0x00)
#define EDGE_WALL       (0x01)
#define EDGE_PATH       (0x02)
#define EDGE_FIXED      (0x04)
#define EDGE_ERROR      (0x08)
#define EDGE_DRAG       (0x10)

char DIRECTIONS[4] = {L, R, U, D};

enum {
    SOLVED,
    INVALID,
    AMBIGUOUS
};

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_FLOOR_A,
    COL_FLOOR_B,
    COL_FIXED,
    COL_WALL_A,
    COL_WALL_B,
    COL_PATH,
    COL_DRAGON,
    COL_DRAGOFF,
    COL_ERROR,
    NCOLOURS
};

struct game_params {
    int w, h;
    int difficulty;
};

struct game_state {
    int w, h;
    int diff;
    unsigned char *edge_h;
    unsigned char *edge_v;
    bool completed;
    bool used_solve;
};

#define DEFAULT_PRESET 1
static const struct game_params walls_presets[] = {
    {4, 4,  DIFF_EASY},
    {4, 4,  DIFF_NORMAL},
    {6, 6,  DIFF_NORMAL},
    {8, 8,  DIFF_NORMAL},
    {10, 10,  DIFF_NORMAL},
    {12, 12,  DIFF_NORMAL},
};

static game_params *default_params(void) {
    game_params *ret = snew(game_params);
    *ret = walls_presets[DEFAULT_PRESET];
    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params) {
    game_params *ret;
    char buf[64];

    if (i < 0 || i >= lenof(walls_presets)) return false;

    ret = default_params();
    *ret = walls_presets[i]; /* struct copy */
    *params = ret;

    sprintf(buf, "%dx%d %s",
            ret->w, ret->h,
            walls_diffnames[ret->difficulty]);
    *name = dupstr(buf);

    return true;
}

static void free_params(game_params *params) {
    sfree(params);
}

static game_params *dup_params(const game_params *params) {
    game_params *ret = snew(game_params);
    *ret = *params;       /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string) {
    params->w = params->h = atoi(string);
    while (*string && isdigit((unsigned char) *string)) ++string;
    if (*string == 'x') {
        string++;
        params->h = atoi(string);
        while (*string && isdigit((unsigned char)*string)) string++;
    }

    params->difficulty = DIFF_EASY;

    if (*string == 'd') {
        int i;
        string++;
        for (i = 0; i < DIFFCOUNT; i++)
            if (*string == walls_diffchars[i])
                params->difficulty = i;
        if (*string) string++;
    }
    return;
}

static char *encode_params(const game_params *params, bool full) {
    char buf[256];
    sprintf(buf, "%dx%d", params->w, params->h);
    if (full)
        sprintf(buf + strlen(buf), "d%c",
                walls_diffchars[params->difficulty]);
    return dupstr(buf);
}

static config_item *game_configure(const game_params *params) {
    config_item *ret;
    char buf[64];

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
    ret[2].u.choices.selected = params->difficulty;

    ret[3].name = NULL;
    ret[3].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg) {
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->difficulty = cfg[2].u.choices.selected;

    return ret;
}

static const char *validate_params(const game_params *params, bool full) {
    if (params->w < 3) return "Width must be at least three";
    if (params->h < 3) return "Height must be at least three";
    if (params->difficulty < 0 || params->difficulty >= DIFFCOUNT)
        return "Unknown difficulty level";
    return NULL;
}

typedef struct gridstate {
    int w, h;
    unsigned char *faces;
} gridstate;

struct neighbour_ctx {
    gridstate *grid;
    int i, n, neighbours[4];
};

static int neighbour(int vertex, void *vctx) {
    struct neighbour_ctx *ctx = (struct neighbour_ctx *)vctx;
    if (vertex >= 0) {
        gridstate *grid = ctx->grid;
        int w = grid->w, x = vertex % w, y = vertex / w;
        ctx->i = ctx->n = 0;
        if (grid->faces[vertex] & R) {
            int nx = x + 1, ny = y;
            if (nx < grid->w)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
        if (grid->faces[vertex] & L) {
            int nx = x - 1, ny = y;
            if (nx >= 0)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
        if (grid->faces[vertex] & U) {
            int nx = x, ny = y - 1;
            if (ny >= 0)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
        if (grid->faces[vertex] & D) {
            int nx = x, ny = y + 1;
            if (ny < grid->h)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
    }
    if (ctx->i < ctx->n)
        return ctx->neighbours[ctx->i++];
    else
        return -1;
}

int check_solution(game_state *state, bool full) {
    int x,y,i;
    int count_walls;
    int count_paths;
    unsigned char *edges[4];

    gridstate *grid;
    struct findloopstate *fls;
    struct neighbour_ctx ctx;

    int exit_count;

    int *dsf;
    int first_cell;

    int w = state->w;
    int h = state->h;
    int solved = SOLVED;

    /* Reset error flags */
    if (full) {
        for (i=0;i<w*(h+1);i++) state->edge_h[i] &= ~EDGE_ERROR;
        for (i=0;i<(w+1)*h;i++) state->edge_v[i] &= ~EDGE_ERROR;
    }
    
    /* Check if every cell has exactly two paths. Mark error flag */
    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        count_walls = 0;
        count_paths = 0;
        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;
        for (i=0;i<4;i++) {
            if ((*edges[i] & EDGE_WALL) > 0x00) count_walls++;
            if ((*edges[i] & EDGE_PATH) > 0x00) count_paths++;
        }
        if (full) {
            if (count_paths < 2) solved = AMBIGUOUS;
            if (count_paths > 2) {
                solved = INVALID;
                for (i=0;i<4;i++)
                    if ((*edges[i] & EDGE_PATH) > 0x00) *edges[i] |= EDGE_ERROR;
            }
        }
        else if ((count_walls != 2 || count_paths != 2)) return AMBIGUOUS;
    }
    if (!full) return SOLVED;

    /* Find path loops, mark as error */
    grid = snew(gridstate);
    grid->faces = snewn(w*h, unsigned char);
    grid->w = w; grid->h = h;

    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        i = x+y*w;
        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;
        grid->faces[i] = BLANK;
        if ((*edges[0] & EDGE_PATH) > 0x00) grid->faces[i] |= U;
        if ((*edges[1] & EDGE_PATH) > 0x00) grid->faces[i] |= D;
        if ((*edges[2] & EDGE_PATH) > 0x00) grid->faces[i] |= L;
        if ((*edges[3] & EDGE_PATH) > 0x00) grid->faces[i] |= R;
    }
    fls = findloop_new_state(w*h);
    ctx.grid = grid;
    if (findloop_run(fls, w*h, neighbour, &ctx)) {
        for (x = 0; x < w; x++)
        for (y = 0; y < h; y++) {
            int u, v;
            u = y*w + x;
            for (v = neighbour(u, &ctx); v >= 0; v = neighbour(-1, &ctx))
                if (findloop_is_loop_edge(fls, u, v)) {
                    solved = INVALID;
                    edges[0] = state->edge_h + y*w + x;
                    edges[1] = edges[0] + w;
                    edges[2] = state->edge_v + y*(w+1) + x;
                    edges[3] = edges[2] + 1;
                    for (i=0;i<4;i++)
                        if ((*edges[i] & EDGE_PATH) > 0x00) *edges[i] |= EDGE_ERROR;
                }
        }
    }

    findloop_free_state(fls);
    sfree(grid->faces);
    sfree(grid);

    /* Check for exactly two exits */
    exit_count = 0;
    for (i=0;i<w;i++)            if ((state->edge_h[i] & EDGE_PATH) > 0x00) exit_count++;
    for (i=w*h;i<w*(h+1);i++)    if ((state->edge_h[i] & EDGE_PATH) > 0x00) exit_count++;
    for (i=0;i<(w+1)*h;i+=(w+1)) if ((state->edge_v[i] & EDGE_PATH) > 0x00) exit_count++;
    for (i=w;i<(w+1)*h;i+=(w+1)) if ((state->edge_v[i] & EDGE_PATH) > 0x00) exit_count++;
    if (exit_count < 2)
        solved = AMBIGUOUS;
    else if (exit_count > 2) {
        solved = INVALID;
        for (i=0;i<w;i++)
            if ((state->edge_h[i] & EDGE_PATH) > 0x00) state->edge_h[i] |= EDGE_ERROR;
        for (i=w*h;i<w*(h+1);i++)
            if ((state->edge_h[i] & EDGE_PATH) > 0x00) state->edge_h[i] |= EDGE_ERROR;
        for (i=0;i<(w+1)*h;i+=(w+1))
            if ((state->edge_v[i] & EDGE_PATH) > 0x00) state->edge_v[i] |= EDGE_ERROR;
        for (i=w;i<(w+1)*h;i+=(w+1))
            if ((state->edge_v[i] & EDGE_PATH) > 0x00) state->edge_v[i] |= EDGE_ERROR;
    }

    if (solved != SOLVED) return solved;

    /* Check if all cells are connected 
     * TODO Mark unconnected cells if exit_count >=2 && dsf[exit1] == dsf[exit2]
    */
    dsf = snewn(w*h,int);
    dsf_init(dsf, w*h);
    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        i = x+y*w;
        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;
        if ((*edges[0] & EDGE_PATH) > 0x00 && y>0) dsf_merge(dsf, i, i-w);
        if ((*edges[1] & EDGE_PATH) > 0x00 && y<h-1) dsf_merge(dsf, i, i+w);
        if ((*edges[2] & EDGE_PATH) > 0x00 && x>0) dsf_merge(dsf, i, i-1);
        if ((*edges[3] & EDGE_PATH) > 0x00 && x<w-1) dsf_merge(dsf, i, i+1);
    }
    first_cell = dsf_canonify(dsf, 0);
    for (i=0;i<w*h;i++) {
        if (dsf_canonify(dsf, i) != first_cell) {
            solved = INVALID;
            break;
        }
    }

    sfree(dsf);

    return solved;
}

struct solver_scratch {
    int *loopdsf;
    int difficulty;
};

bool solve_single_cells(game_state *state, struct solver_scratch *scratch) {
    int i, j, x, y;
    bool changed = false;
    int w = state->w;
    int h = state->h;

    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        unsigned char *edges[4];
        unsigned char available_mask = 0;
        unsigned char path_count = 0;
        unsigned char wall_count = 0;

        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;

        for (i = 0; i < 4; ++i) {
            if (*edges[i] == EDGE_NONE) available_mask |= (0x01 << i);
            else if ((*edges[i] & EDGE_PATH) == EDGE_PATH) path_count++;
            else if ((*edges[i] & EDGE_WALL) == EDGE_WALL) wall_count++;
        }
        if (wall_count == 2 && path_count < 2) {
            for (i = 0; i < 4; i++) {
                if (available_mask & (0x01 << i)){ 
                    *edges[i] |= EDGE_PATH;
                    if (scratch->difficulty >= DIFF_NORMAL) {
                        j = x+y*w;
                        if (i==0 && y>0)     dsf_merge(scratch->loopdsf, j, j-w);
                        if (i==1 && y<(h-1)) dsf_merge(scratch->loopdsf, j, j+w);
                        if (i==2 && x>0)     dsf_merge(scratch->loopdsf, j, j-1);
                        if (i==3 && x<(w-1)) dsf_merge(scratch->loopdsf, j, j+1);
                    }
                }
            }
            changed = true;
        }
        else if (path_count == 2 && wall_count < 2) {
            for (i = 0; i < 4; ++i) {
                if (available_mask & (0x01 << i)) {
                    *edges[i] |= EDGE_WALL;
                }
            }
            changed = true;
        }
    }
    return changed;
}

bool solve_loops(game_state *state, struct solver_scratch *scratch) {
    int i,x,y,p,idx;
    unsigned char *edges[4];
    unsigned char wall_count, free_count;
    int dsf_idx[4];
    int single_idx;
    bool changed = false;
    int w = state->w;
    int h = state->h;
    for (y=0;y<h;y++) {
        for (x=0;x<w;x++) {
            p = x+y*w;
            wall_count = free_count = 0;
            /* Loop ladder detection 
             * 
             * Conditions: 3 free edges
                           1 wall
                           2 free edges are connected
               -> 3rd edge must be a path
            */
            edges[0] = state->edge_h + y*w + x;
            edges[1] = edges[0] + w;
            edges[2] = state->edge_v + y*(w+1) + x;
            edges[3] = edges[2] + 1;
            for (i=0;i<4;i++) {
                if (*edges[i] == EDGE_NONE) {
                    free_count++;
                    idx = -1;
                    if (i==0 && y>0)     idx = dsf_canonify(scratch->loopdsf, p-w);
                    if (i==1 && y<(h-1)) idx = dsf_canonify(scratch->loopdsf, p+w);
                    if (i==2 && x>0)     idx = dsf_canonify(scratch->loopdsf, p-1);
                    if (i==3 && x<(w-1)) idx = dsf_canonify(scratch->loopdsf, p+1);
                    dsf_idx[i] = idx;
                }
                if ((*edges[i] & EDGE_WALL) == EDGE_WALL) {
                    wall_count++;
                    dsf_idx[i] = -2;
                }
            }
            if (free_count == 3 && wall_count == 1) {
                single_idx = -1;
                if (dsf_idx[0] == -2) {
                    if (dsf_idx[1] != dsf_idx[2] && dsf_idx[2] == dsf_idx[3]) single_idx = 1;
                    if (dsf_idx[2] != dsf_idx[1] && dsf_idx[1] == dsf_idx[3]) single_idx = 2;
                    if (dsf_idx[3] != dsf_idx[1] && dsf_idx[1] == dsf_idx[2]) single_idx = 3;
                }
                else if (dsf_idx[1] == -2) {
                    if (dsf_idx[0] != dsf_idx[2] && dsf_idx[2] == dsf_idx[3]) single_idx = 0;
                    if (dsf_idx[2] != dsf_idx[0] && dsf_idx[0] == dsf_idx[3]) single_idx = 2;
                    if (dsf_idx[3] != dsf_idx[0] && dsf_idx[0] == dsf_idx[2]) single_idx = 3;
                }
                else if (dsf_idx[2] == -2) {
                    if (dsf_idx[0] != dsf_idx[1] && dsf_idx[1] == dsf_idx[3]) single_idx = 0;
                    if (dsf_idx[1] != dsf_idx[0] && dsf_idx[0] == dsf_idx[3]) single_idx = 1;
                    if (dsf_idx[3] != dsf_idx[0] && dsf_idx[0] == dsf_idx[1]) single_idx = 3;
                }
                else if (dsf_idx[3] == -2) {
                    if (dsf_idx[0] != dsf_idx[1] && dsf_idx[1] == dsf_idx[2]) single_idx = 0;
                    if (dsf_idx[1] != dsf_idx[0] && dsf_idx[0] == dsf_idx[2]) single_idx = 1;
                    if (dsf_idx[2] != dsf_idx[0] && dsf_idx[0] == dsf_idx[1]) single_idx = 2;
                }
                if (single_idx != -1) {
                    *edges[single_idx] |= EDGE_PATH;
                    for (i=0;i<4;i++) {
                        if (dsf_idx[i] != -2) {
                            if (i==0 && y>0)     dsf_merge(scratch->loopdsf, p, p-w);
                            if (i==1 && y<(h-1)) dsf_merge(scratch->loopdsf, p, p+w);
                            if (i==2 && x>0)     dsf_merge(scratch->loopdsf, p, p-1);
                            if (i==3 && x<(w-1)) dsf_merge(scratch->loopdsf, p, p+1);
                        }
                    }
                    changed = true;
                    break;
                }
            }
        }
        if (changed) break;
    }

    return changed;
}

int walls_solve(game_state *state, int difficulty) {
    struct solver_scratch *scratch = snew(struct solver_scratch);
    scratch->loopdsf = snewn(state->w*state->h, int);
    dsf_init(scratch->loopdsf, state->w*state->h);
    scratch->difficulty = difficulty;

    while(true) {
        if (difficulty >= DIFF_EASY   && solve_single_cells(state, scratch)) continue;
        if (difficulty >= DIFF_NORMAL && solve_loops(state, scratch)) continue;
        break;
    }

    sfree(scratch->loopdsf);
    sfree(scratch);
    return check_solution(state, false);
}

/*
 * Path generator
 * 
 * Employing the algorithm described at:
 * http://clisby.net/projects/hamiltonian_path/
 */

void reverse_path(int i1, int i2, int *pathx, int *pathy) {
    int i;
    int ilim = (i2-i1+1)/2;
    int temp;
    for (i=0; i<ilim; i++) {
        temp = pathx[i1+i];
        pathx[i1+i] = pathx[i2-i];
        pathx[i2-i] = temp;

        temp = pathy[i1+i];
        pathy[i1+i] = pathy[i2-i];
        pathy[i2-i] = temp;
    }
}

int backbite_left(int step, int n, int *pathx, int *pathy, int w, int h) {
    int neighx, neighy;
    int i, inPath = false;
    switch(step) {
        case L: neighx = pathx[0]-1; neighy = pathy[0];   break;
        case R: neighx = pathx[0]+1; neighy = pathy[0];   break;
        case U: neighx = pathx[0];   neighy = pathy[0]-1; break;
        case D: neighx = pathx[0];   neighy = pathy[0]+1; break; 
        default: neighx = -1; neighy = -1; break;
    }
    if (neighx < 0 || neighx >= w || neighy < 0 || neighy >= h)
        return n;

    for (i=1;i<n;i+=2) {
        if (neighx == pathx[i] && neighy == pathy[i]) { inPath = true; break; }
    }
    if (inPath) {
        reverse_path(0, i-1, pathx, pathy);
    }
    else {
        reverse_path(0, n-1, pathx, pathy);
        pathx[n] = neighx;
        pathy[n] = neighy;
        n++;
    }

    return n;
}

int backbite_right(int step, int n, int *pathx, int *pathy, int w, int h) {
    int neighx, neighy;
    int i, inPath = false;
    switch(step) {
        case L: neighx = pathx[n-1]-1; neighy = pathy[n-1];   break;
        case R: neighx = pathx[n-1]+1; neighy = pathy[n-1];   break;
        case U: neighx = pathx[n-1];   neighy = pathy[n-1]-1; break;
        case D: neighx = pathx[n-1];   neighy = pathy[n-1]+1; break; 
        default: neighx = -1; neighy = -1; break;
    }
    if (neighx < 0 || neighx >= w || neighy < 0 || neighy >= h)
        return n;

    for (i=n-2;i>=0;i-=2) {
        if (neighx == pathx[i] && neighy == pathy[i]) { inPath = true; break; }
    }
    if (inPath) {
        reverse_path(i+1, n-1, pathx, pathy);
    }
    else {
        pathx[n] = neighx;
        pathy[n] = neighy;
        n++;
    }

    return n;
}

int backbite(int n, int *pathx, int *pathy, int w, int h, random_state *rs) {
    return (random_upto(rs, 2) == 0) ?
        backbite_left( DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h) :
        backbite_right(DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h);
}

void generate_hamiltonian_path(game_state *state, random_state *rs) {
    int w = state->w;
    int h = state->h;
    int *pathx = snewn(w*h, int);
    int *pathy = snewn(w*h, int);
    int n = 1;
    int pos, x, y;
    pathx[0] = random_upto(rs, w);
    pathy[0] = random_upto(rs, h);

    while (n < w*h) {
        n = backbite(n, pathx, pathy, w, h, rs);
    }

    while (!(pathx[0] == 0 || pathx[0] == w-1) && 
           !(pathy[0] == 0 || pathy[0] == h-1)) {
        backbite_left(DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h);
    }

    while (!(pathx[n-1] == 0 || pathx[n-1] == w-1) && 
           !(pathy[n-1] == 0 || pathy[n-1] == h-1)) {
        backbite_right(DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h);
    }

    for (n=0;n<w*h;n++) {
        pos = pathx[n] + pathy[n]*w;
        x = pos % w;
        y = pos / w;
        if (n < (w*h-1)) {
            if      (pathx[n+1] - pathx[n] ==  1) state->edge_v[y*(w+1)+x+1] = EDGE_NONE;
            else if (pathx[n+1] - pathx[n] == -1) state->edge_v[y*(w+1)+x]   = EDGE_NONE;
            else if (pathy[n+1] - pathy[n] ==  1) state->edge_h[y*w+x+w]     = EDGE_NONE;
            else if (pathy[n+1] - pathy[n] == -1) state->edge_h[y*w+x]       = EDGE_NONE;
        }
        if (n == 0 || n == (w*h)-1) {
            if      (pathx[n] == 0)   state->edge_v[y*(w+1)+x]   = EDGE_NONE;
            else if (pathx[n] == w-1) state->edge_v[y*(w+1)+x+1] = EDGE_NONE;
            else if (pathy[n] == 0)   state->edge_h[y*w+x]       = EDGE_NONE;
            else if (pathy[n] == h-1) state->edge_h[y*w+x+w]     = EDGE_NONE;
        }
    }

    sfree(pathx);
    sfree(pathy);

    return;
}

static game_state *new_state(const game_params *params) {
    game_state *state = snew(game_state);

    state->w = params->w;
    state->h = params->h;
    state->diff = params->difficulty;
    state->completed = false;
    state->used_solve = false;

    state->edge_h = snewn(state->w*(state->h + 1), unsigned char);
    state->edge_v = snewn((state->w + 1)*state->h, unsigned char);

    memset(state->edge_h, EDGE_WALL | EDGE_FIXED, state->w*(state->h+1)*sizeof(unsigned char));
    memset(state->edge_v, EDGE_WALL | EDGE_FIXED, (state->w+1)*state->h*sizeof(unsigned char));

    return state;
}

static game_state *dup_state(const game_state *state) {
    game_state *ret = snew(game_state);
    ret->w = state->w;
    ret->h = state->h;
    ret->diff = state->diff;
    ret->completed = state->completed;
    ret->used_solve = state->used_solve;

    ret->edge_h = snewn(state->w*(state->h + 1), unsigned char);
    ret->edge_v = snewn((state->w + 1)*state->h, unsigned char);

    memcpy(ret->edge_h, state->edge_h, state->w*(state->h + 1) * sizeof(unsigned char));
    memcpy(ret->edge_v, state->edge_v, (state->w + 1)*state->h * sizeof(unsigned char));

    return ret;
}

static void free_state(game_state *state) {
    sfree(state->edge_v);
    sfree(state->edge_h);
    sfree(state);
}

static void count_edges(unsigned char edge, char **e, int *erun, int *wrun) {
    if ((edge & EDGE_WALL) == 0 && *wrun > 0) {
        *e += sprintf(*e, "%d", *wrun);
        *wrun = *erun = 0;
    }
    else if ((edge & EDGE_WALL) && *erun > 0) {
        while (*erun >= 26) {
            *(*e)++ = 'z';
            *erun -= 26;
        }
        if (*erun == 0) *wrun = 0;
        else {
            *(*e)++ = ('a' + *erun - 1);
            *erun = 0; *wrun = -1;
        }
    }
    if(edge & EDGE_WALL) (*wrun)++;
    else                 (*erun)++;
}

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive) {
    game_state *new;
    game_state *tmp;
    
    char *desc, *e;
    int erun, wrun;
    int w = params->w;
    int h = params->h;
    int x,y;

    int i;
    int borderreduce;
    int bordernum = 0;
    int wallnum = 0;
    int ws = (w+1)*h + w*(h+1);
    int vo = w*(h+1);
    int *wallidx;
    int result;

    borderreduce = 2*w+2*h;
    wallidx = snewn(ws, int);
    
    while (true) {
        wallnum = bordernum = 0;
        new = new_state(params);
        generate_hamiltonian_path(new, rs);
        /* print_grid(new); */

        for (i=0;i<w*(h+1);i++)
            if ((new->edge_h[i] & EDGE_WALL) > 0x00)
                wallidx[wallnum++] = i;
        for (i=0;i<(w+1)*h;i++)
            if ((new->edge_v[i] & EDGE_WALL) > 0x00)
                wallidx[wallnum++] = i+vo;

        shuffle(wallidx, wallnum, sizeof(int), rs);

        for (i=0;i<wallnum;i++) {
            int wi = wallidx[i];

            /* Remove border walls up to 'borderreduce' */
            /* Avoid two open border edges on a corner cell */
            if (wi<vo) {
                int xh, yh;
                xh = wi%w; yh = wi/w;
                if ((xh == 0     && yh == 0 && ((new->edge_v[0]           & EDGE_WALL) == 0x00)) ||
                    (xh == (w-1) && yh == 0 && ((new->edge_v[w]           & EDGE_WALL) == 0x00)) ||
                    (xh == 0     && yh == h && ((new->edge_v[w*h]         & EDGE_WALL) == 0x00)) ||
                    (xh == (w-1) && yh == h && ((new->edge_v[(w+1)*h-1]   & EDGE_WALL) == 0x00)) ||
                    ((yh == 0 || yh == h) && (bordernum >= borderreduce)))
                    continue;
            }
            else {
                int xv, yv;
                xv = (wi-vo)%(w+1); yv = (wi-vo)/(w+1);
                if ((xv == 0 && yv == 0     && ((new->edge_h[0]           & EDGE_WALL) == 0x00)) ||
                    (xv == w && yv == 0     && ((new->edge_h[w-1]         & EDGE_WALL) == 0x00)) ||
                    (xv == 0 && yv == (h-1) && ((new->edge_h[w*h]         & EDGE_WALL) == 0x00)) ||
                    (xv == w && yv == (h-1) && ((new->edge_h[w*(h+1)-1]   & EDGE_WALL) == 0x00)) ||
                    ((xv == 0 || xv == w) && (bordernum >= borderreduce))) {
                    continue;
                }
            }

            tmp = dup_state(new);
            /* Remove interior wall */
            if (wi<vo) tmp->edge_h[wi]    = EDGE_NONE;
            else       tmp->edge_v[wi-vo] = EDGE_NONE;
            
            if (walls_solve(tmp, params->difficulty) == SOLVED) {
                if (wi<vo) new->edge_h[wi]    = EDGE_NONE;
                else       new->edge_v[wi-vo] = EDGE_NONE;
                if (wi<vo && (wi/w == 0 || wi/w == h)) bordernum++;
                else if ((wi-vo)%(w+1) == 0 || (wi-vo)%(w+1) == w) bordernum++;
            }
            free_state(tmp);
        }

        if (params->difficulty == DIFF_EASY) break;
        tmp = dup_state(new);
        result = walls_solve(tmp, params->difficulty-1);
        free_state(tmp);
        if (result == SOLVED) {
            free_state(new);
            continue;
        }
        break;
    }

    /* Encode walls */
    desc = snewn((w+1)*h + w*(h+1) + (w*h) + 1, char);
    e = desc;
    erun = wrun = 0;
    for (y=0;y<=h;y++)
    for (x=0;x<w;x++)
        count_edges(new->edge_h[y*w+x], &e, &erun, &wrun);
    if(wrun > 0) e += sprintf(e, "%d", wrun);
    if(erun > 0) *e++ = ('a' + erun - 1);
    *e++ = ',';
    erun = wrun = 0;
    for (y=0;y<h;y++)
    for (x=0;x<=w;x++) 
        count_edges(new->edge_v[y*(w+1)+x], &e, &erun, &wrun);
    if(wrun > 0) e += sprintf(e, "%d", wrun);
    if(erun > 0) *e++ = ('a' + erun - 1);
    *e++ = '\0';

    free_state(new);
    sfree(wallidx);

    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc) {
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc) {
    int i, c;
    int w = params->w;
    int h = params->h;
    bool fh;
    game_state *state = new_state(params);

    i = 0; fh = true;
    while (*desc) {
        if(isdigit((unsigned char)*desc)) {
            for (c = atoi(desc); c > 0; c--) {
                if (fh) state->edge_h[i] = EDGE_WALL | EDGE_FIXED;
                else    state->edge_v[i] = EDGE_WALL | EDGE_FIXED;
                i++;
            }
            while (*desc && isdigit((unsigned char)*desc)) desc++;
        }
        else if(*desc >= 'a' && *desc <= 'z') {
            for (c = *desc - 'a' + 1; c > 0; c--) {
                if (fh) state->edge_h[i] = EDGE_NONE;
                else    state->edge_v[i] = EDGE_NONE;
                i++;
            }
            if (*desc < 'z' && i < (fh ? w*(h+1) : (w+1)*h)) {
                if (fh) state->edge_h[i] = EDGE_WALL | EDGE_FIXED;
                else    state->edge_v[i] = EDGE_WALL | EDGE_FIXED;
                i++;
            }
            desc++;
        }
        else if (*desc == ',') {
            fh = false;
            i = 0;
            desc++;
        }
    }
    return state;
}

static game_state *dup_game(const game_state *state) {
    return dup_state(state);
}

static void free_game(game_state *state) {
    free_state(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error) {
    int i;
    int w = state->w;
    int h = state->h;
    char *move = snewn(w*h*40, char), *p = move;
    int voff = w*(h+1);
    
    game_state *solve_state = dup_game(state);
    walls_solve(solve_state, DIFF_NORMAL);
    p += sprintf(p, "S");
    for (i = 0; i < w*(h+1); i++) {
        if (solve_state->edge_h[i] == EDGE_WALL)
            p += sprintf(p, ";W%d", i);
        if (solve_state->edge_h[i] == EDGE_NONE)
            p += sprintf(p, ";C%d", i);
        if (solve_state->edge_h[i] == EDGE_PATH)
            p += sprintf(p, ";P%d", i);
    }
    for (i = 0; i < (w+1)*h; i++) {
        if (solve_state->edge_v[i] == EDGE_WALL)
            p += sprintf(p, ";W%d", i+voff);
        if (solve_state->edge_v[i] == EDGE_NONE)
            p += sprintf(p, ";C%d", i+voff);
        if (solve_state->edge_v[i] == EDGE_PATH)
            p += sprintf(p, ";P%d", i+voff);
    }
    *p++ = '\0';
    move = sresize(move, p - move, char);
    free_game(solve_state);

    return move;
}

struct game_ui {
    int *dragcoords;       /* list of (y*w+x) coords in drag so far */
    int ndragcoords;       /* number of entries in dragcoords. */
    unsigned char dragdir; /* Current direction of drag */

    int curx, cury;        /* grid position of keyboard cursor */
    bool cursor_active;    /* true if cursor is shown */

    bool show_grid;        /* true if checkerboard grid is shown */
};


static game_ui *new_ui(const game_state *state) {
    int w = state->w;
    int h = state->h;
    game_ui *ui = snew(game_ui);

    ui->dragcoords = snewn((8*w+7)*(8*h+7), int);
    ui->ndragcoords = -1;
    ui->dragdir = BLANK;
    ui->cursor_active = false;
    ui->show_grid = true;
    return ui;
}

static void free_ui(game_ui *ui) {
    sfree(ui->dragcoords);
    sfree(ui);
}

static char *encode_ui(const game_ui *ui) {
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding) {
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate) {
}

#define PREFERRED_TILE_SIZE (8)
#define TILESIZE (ds->tilesize)
#define BORDER (9*TILESIZE/2)

#define COORD(x) ( (x) * 8 * TILESIZE + BORDER )
#define CENTERED_COORD(x) ( COORD(x) + 4*TILESIZE )
#define FROMCOORD(x) ( ((x) < BORDER) ? -1 : ( ((x) - BORDER) / (8 * TILESIZE) ) )

#define CELLCOORD(x) ( (x) * TILESIZE )
#define FROMCELLCOORD(x) ( (x) / TILESIZE )

struct game_drawstate {
    int tilesize;
    int w, h;
    bool tainted;
    unsigned short *cell;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button, bool swapped) {

    char buf[80];
    int w = state->w;
    int h = state->h;
    int fx = FROMCOORD(x);
    int fy = FROMCOORD(y); 
    int cx = CENTERED_COORD(fx);
    int cy = CENTERED_COORD(fy);

    if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        int direction, edge;
        ui->cursor_active = false;
        if (ui->ndragcoords > 0 ) {

            ui->ndragcoords = -1;
            return UI_UPDATE;
        }
        else {
            ui->ndragcoords = -1;
            if ((fx<0 && fy<0) || (fx>=w && fy<0) || (fx<0 && fy>=h) || (fx>=w && fy>=h)) return NULL;
            if      (fx<0 && x > cx)  direction = R;
            else if (fx>=w && x < cx) direction = L;
            else if (fy<0 && y > cy)  direction = D;
            else if (fy>=h && y < cy) direction = U;
            else if (fx<0 || fx>=w || fy<0 || fy >=h) return NULL;
            else {
                if (abs(x-cx) < abs(y-cy)) direction = (y < cy) ? U : D;
                else                       direction = (x < cx) ? L : R;
            }
            if (direction == U || direction == D) {
                edge = direction == U ? fx+fy*w : fx+(fy+1)*w;
                if ((state->edge_h[edge] & EDGE_FIXED) > 0x00) return NULL;
                if ((state->edge_h[edge] & EDGE_PATH) > 0x00 || 
                    (state->edge_h[edge] & EDGE_WALL) > 0x00) sprintf(buf,"C%d",edge);
                else sprintf(buf,"%c%d",(button == LEFT_BUTTON) ? 'P' : 'W', edge);
            }
            else {
                edge = direction == L ? fx+fy*(w+1) : (fx+1)+fy*(w+1);
                if ((state->edge_v[edge] & EDGE_FIXED) > 0x00) return NULL;
                if ((state->edge_v[edge] & EDGE_PATH) > 0x00 || 
                    (state->edge_v[edge] & EDGE_WALL) > 0x00) sprintf(buf,"C%d",edge+w*(h+1));
                else sprintf(buf,"%c%d",(button == LEFT_BUTTON) ? 'P' : 'W', edge+w*(h+1));
            }
            return dupstr(buf);
        }
    }

    if (button == 'G' || button == 'g') {
        ui->show_grid = !ui->show_grid;
        return UI_UPDATE;
    }
    return NULL;
}

static game_state *execute_move(const game_state *state, const char *move) {
    char c;
    unsigned char newedge;
    int edge, n;
    int w = state->w;
    int h = state->h;
    game_state *ret = dup_game(state);

    while (*move) {
        c = *move;
        if (c == 'S') {
            ret->used_solve = true;
            move++;
        } 
        else if (c == 'W' || c == 'P' || c == 'C') {
            move++;
            if (sscanf(move, "%d%n", &edge, &n) < 1) {
                free_game(ret);
                return NULL;
            }
            newedge = (c=='W') ? EDGE_WALL :
                      (c=='P') ? EDGE_PATH :
                                 EDGE_NONE;
            if (edge < w*(h+1)) ret->edge_h[edge] = newedge;
            else ret->edge_v[edge-w*(h+1)] = newedge;
            move += n;
        }
        if (*move == ';')
            move++;
        else if (*move) {
            free_game(ret);
            return NULL;
        }
    }

    if (check_solution(ret, true) == SOLVED) ret->completed = true;

    return ret;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              int *x, int *y) {
   /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = (8 * (params->w) + 7) * TILESIZE;
    *y = (8 * (params->h) + 7) * TILESIZE;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize) {
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours) {
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

    ret[COL_GRID * 3 + 0] = 0.0F;
    ret[COL_GRID * 3 + 1] = 0.0F;
    ret[COL_GRID * 3 + 2] = 0.0F;

    ret[COL_FLOOR_A * 3 + 0] = 1.0F;
    ret[COL_FLOOR_A * 3 + 1] = 1.0F;
    ret[COL_FLOOR_A * 3 + 2] = 1.0F;

    ret[COL_FLOOR_B * 3 + 0] = 0.75F;
    ret[COL_FLOOR_B * 3 + 1] = 0.75F;
    ret[COL_FLOOR_B * 3 + 2] = 0.75F;

    ret[COL_FIXED * 3 + 0] = 0.0F;
    ret[COL_FIXED * 3 + 1] = 0.0F;
    ret[COL_FIXED * 3 + 2] = 0.0F;

    ret[COL_WALL_A * 3 + 0] = 0.0F;
    ret[COL_WALL_A * 3 + 1] = 0.0F;
    ret[COL_WALL_A * 3 + 2] = 0.0F;

    ret[COL_WALL_B * 3 + 0] = 0.75F;
    ret[COL_WALL_B * 3 + 1] = 0.75F;
    ret[COL_WALL_B * 3 + 2] = 0.75F;

    ret[COL_PATH * 3 + 0] = 0.0F;
    ret[COL_PATH * 3 + 1] = 0.0F;
    ret[COL_PATH * 3 + 2] = 0.0F;

    ret[COL_DRAGON * 3 + 0] = 0.0F;
    ret[COL_DRAGON * 3 + 1] = 0.0F;
    ret[COL_DRAGON * 3 + 2] = 1.0F;

    ret[COL_DRAGOFF * 3 + 0] = 0.5F;
    ret[COL_DRAGOFF * 3 + 1] = 0.5F;
    ret[COL_DRAGOFF * 3 + 2] = 1.0F;

    ret[COL_ERROR * 3 + 0] = 0.5F;
    ret[COL_ERROR * 3 + 1] = 0.5F;
    ret[COL_ERROR * 3 + 2] = 0.5F;

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state) {
    int i;
    int w = state->w;
    int h = state->h;

    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->w = w;
    ds->h = h;
    ds->tainted = true;
    ds->cell   = snewn((8*w+7)*(8*h+7), unsigned short);

    for (i=0;i<(8*w+7)*(8*h+7); i++) ds->cell[i] = 0x0000;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds) {
    sfree(ds->cell);
    sfree(ds);
}

static void draw_cell(drawing *dr, game_drawstate *ds, int pos) {
    int i;
    int dx, dy, col;
    int w = ds->w;
    int x = pos%(8*w+7);
    int y = pos/(8*w+7);
    int ox = x*ds->tilesize;
    int oy = y*ds->tilesize;
    int ts2 = (ds->tilesize+1)/2;

    clip(dr, ox, oy, ds->tilesize, ds->tilesize);

    if ((ds->cell[pos] & (EDGE_WALL | EDGE_FIXED)) == (EDGE_WALL|EDGE_FIXED)) {
        draw_rect(dr, ox, oy, ds->tilesize, ds->tilesize, COL_FIXED);
    }
    else if ((ds->cell[pos] & EDGE_PATH) == EDGE_PATH) {
        draw_rect(dr, ox, oy, ds->tilesize, ds->tilesize,
            (ds->cell[pos] & EDGE_DRAG) >0 ? COL_DRAGOFF :
            (ds->cell[pos] & EDGE_ERROR)>0 ? COL_ERROR :
                                             COL_PATH);
    }
    else {
        for (i=0;i<4;i++) {
            unsigned char bg = (ds->cell[pos] >> (8+(2*i))) & 0x03;
            if      (i==0) { dx = ox;     dy = oy; }
            else if (i==1) { dx = ox;     dy = oy+ts2; }
            else if (i==2) { dx = ox+ts2; dy = oy; }
            else           { dx = ox+ts2; dy = oy+ts2; }
            col = (ds->cell[pos] & EDGE_DRAG) >0 ? COL_DRAGON :
                  (bg==0x00) ? COL_BACKGROUND :
                  (bg==0x01) ? COL_FLOOR_A :
                               COL_FLOOR_B;
            draw_rect(dr, dx, dy, ts2, ts2, col);
        }
        if ((ds->cell[pos] & EDGE_WALL) == EDGE_WALL) {
            int c[8];
            c[0] = ox; c[1] = oy;
            c[2] = ox; c[3] = oy+ts2;
            c[4] = ox+ts2; c[5] = oy;
            draw_polygon(dr, c, 3, COL_WALL_A, COL_WALL_A);
            c[0] = ox; c[1] = oy+ts2; 
            c[2] = ox; c[3] = oy+ds->tilesize;
            c[4] = ox+ds->tilesize; c[5] = oy; 
            c[6] = ox+ts2; c[7] = oy;
            draw_polygon(dr, c, 4, COL_WALL_B, COL_WALL_B);
            c[0] = ox; c[1] = oy+ds->tilesize; 
            c[2] = ox+ts2; c[3] = oy+ds->tilesize;
            c[4] = ox+ds->tilesize; c[5] = oy+ts2; 
            c[6] = ox+ds->tilesize; c[7] = oy;
            draw_polygon(dr, c, 4, COL_WALL_A, COL_WALL_A);
            c[0] = ox+ts2; c[1] = oy+ds->tilesize;
            c[2] = ox+ds->tilesize; c[3] = oy+ds->tilesize;
            c[4] = ox+ds->tilesize; c[5] = oy+ts2;
            draw_polygon(dr, c, 3, COL_WALL_B, COL_WALL_B);
        }
    }
    draw_update(dr, ox, oy, ds->tilesize, ds->tilesize);
    unclip(dr);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime) {
    int i,j,w,h,x,y,cx,cy;
    unsigned short *newcell;

    w = state->w; h = state->h;
    newcell = snewn((8*w+7)*(8*h+7), unsigned short);
    for (i=0;i<((8*w)+7)*((8*h)+7); i++) newcell[i] = 0x0000;

    if (ui->ndragcoords > 0)
        for (i=0;i<ui->ndragcoords;i++)
            newcell[ui->dragcoords[i]] |= EDGE_DRAG;
    
    for (i=0;i<((8*w)+7)*((8*h)+7); i++) {
        x = i%(8*w+7)-4; y = i/(8*w+7)-4;
        cx = x/8; cy = y/8;

        /* Corner border cells. Unused. */
        if ((x<0 && y<0) || (x<0 && y>8*h) || (x>8*w && y<0) || (x>8*w && y>8*h)) {}

        /* Left / Right border cells */
        else if (x<0 || x>8*w) {
            if (y%8==4 && x==-1)
                newcell[i] |= (state->edge_v[cy*(w+1)]   & (EDGE_PATH|EDGE_ERROR));
            if (y%8==4 && x==8*w+1)
                newcell[i] |= (state->edge_v[w+cy*(w+1)] & (EDGE_PATH|EDGE_ERROR));
        }

        /* Top / Bottom border cells */
        else if (y<0 || y>8*h) {
            if (x%8==4 && y==-1)
                newcell[i] |= (state->edge_h[cx]         & (EDGE_PATH|EDGE_ERROR));
            if (x%8==4 && y==8*h+1)
                newcell[i] |= (state->edge_h[cx+h*w]     & (EDGE_PATH|EDGE_ERROR));
        }

        /* 4-Corner cells */
        else if ((x%8 == 0) && (y%8 == 0)) {
            if (ui->show_grid) {
                if (cx > 0 && cy > 0) newcell[i] |= ((cx-1)^(cy-1))&1 ? 0x0100:0x0200;
                if (cx > 0 && cy < h) newcell[i] |= ((cx-1)^ cy)   &1 ? 0x0400:0x0800;
                if (cx < w && cy > 0) newcell[i] |= ( cx   ^(cy-1))&1 ? 0x1000:0x2000;
                if (cx < w && cy < h) newcell[i] |= ( cx   ^ cy)   &1 ? 0x4000:0x8000;
            }
            if (cx > 0) newcell[i] |= state->edge_h[(cx-1)+cy*w]     & (EDGE_WALL | EDGE_FIXED);
            if (cx < w) newcell[i] |= state->edge_h[cx+cy*w]         & (EDGE_WALL | EDGE_FIXED);
            if (cy > 0) newcell[i] |= state->edge_v[cx+(cy-1)*(w+1)] & (EDGE_WALL | EDGE_FIXED);
            if (cy < h) newcell[i] |= state->edge_v[cx+cy*(w+1)]     & (EDGE_WALL | EDGE_FIXED);
        }

        /* Horizontal edge cells */
        else if (y%8 == 0) {
            if (ui->show_grid) {
                if (cy > 0) newcell[i] |= (cx^(cy-1))&1 ? 0x1100:0x2200;
                if (cy < h) newcell[i] |= (cx^ cy)   &1 ? 0x4400:0x8800;
            }
            newcell[i] |= state->edge_h[cx+cy*w] & (EDGE_WALL|EDGE_FIXED);
            if (x%8==4)
                newcell[i] |= (state->edge_h[cx+cy*w] & (EDGE_PATH|EDGE_ERROR));
        }

        /* Vertical edge cells */
        else if (x%8 == 0) {
            if (ui->show_grid) {
                if (cx > 0) newcell[i] |= ((cx-1)^cy)&1 ? 0x0500:0x0a00;
                if (cx < w) newcell[i] |= ( cx   ^cy)&1 ? 0x5000:0xa000;
            }
            newcell[i] |= state->edge_v[cx+cy*(w+1)] & (EDGE_WALL|EDGE_FIXED);
            if (y%8==4)
                newcell[i] |= (state->edge_v[cx+cy*(w+1)] & (EDGE_PATH|EDGE_ERROR));
        }
        
        /* Path cells */
        else {
            if (ui->show_grid) 
                for (j=0;j<4;j++) newcell[i] |= ((cx^cy)&1 ? 0x01:0x02) << (8+2*j);
            if (x%8==4 && y%8<=4)
                newcell[i] |= (state->edge_h[cx+cy*w]         & (EDGE_PATH|EDGE_ERROR));
            if (x%8==4 && y%8>=4)
                newcell[i] |= (state->edge_h[cx+(cy+1)*w]     & (EDGE_PATH|EDGE_ERROR));
            if (y%8==4 && x%8<=4)
                newcell[i] |= (state->edge_v[cx+cy*(w+1)]     & (EDGE_PATH|EDGE_ERROR));
            if (y%8==4 && x%8>=4)
                newcell[i] |= (state->edge_v[(cx+1)+cy*(w+1)] & (EDGE_PATH|EDGE_ERROR));
        }
    }
    for (i=0;i<((8*w)+7)*((8*h)+7); i++) {
        if (newcell[i] != ds->cell[i] || ds->tainted) {
            ds->cell[i] = newcell[i];
            draw_cell(dr, ds, i);
        }
    }
    ds->tainted = false;
    sfree(newcell);
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui) {
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui) {
    return 0.0F;
}

static int game_status(const game_state *state) {
    return state->completed ? +1 : 0;
}

static bool game_timing_state(const game_state *state, game_ui *ui) {
    return true;
}

#ifdef COMBINED
#define thegame walls
#endif

static const char rules[] = "Draw a single continuous line through all cells in the grid, such that:\n\n"
"- The line enters the grid from one open border, and leaves the grid through another open border.\n"
"- All grid cells are visited once.\n"
"- The line may not cross itself or form loops.\n"
"- The line cannot go through a wall.\n\n\n"
"This puzzle was contributed by Steffen Bauer.";

const struct game thegame = {
    "Walls", "games.walls", "walls", rules,
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
    false,                 /* wants_statusbar */
    false, game_timing_state,
    REQUIRE_RBUTTON,       /* flags */
};

