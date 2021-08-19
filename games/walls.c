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
    A(NORMAL,Normal,n) \
    A(TRICKY,Tricky,t) \
    A(HARD,Hard,h)
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

#define FLAG_NONE        (0x00)
#define FLAG_WALL        (0x01)
#define FLAG_PATH        (0x02)
#define FLAG_FIXED       (0x04)
#define FLAG_ERROR       (0x08)
#define FLAG_DRAG        (0x20)
#define FLAG_FLASH       (0x40)

#define FLAG_LEFT       (0x00010000)
#define FLAG_RIGHT      (0x00020000)
#define FLAG_UP         (0x00040000)
#define FLAG_DOWN       (0x00080000)

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
    COL_PATH_A,
    COL_PATH_B,
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
    unsigned char *cellstate;
    bool completed;
    bool used_solve;
};

#define DEFAULT_PRESET 1
static const struct game_params walls_presets[] = {
    {4, 5,  DIFF_EASY},
    {4, 5,  DIFF_NORMAL},
    {4, 5,  DIFF_TRICKY},
    {6, 7,  DIFF_NORMAL},
    {6, 7,  DIFF_TRICKY},
    {6, 7,  DIFF_HARD},
    {7, 8, DIFF_NORMAL},
    {7, 8, DIFF_TRICKY},
    {7, 8, DIFF_HARD}
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
    if (params->difficulty >= DIFF_HARD && (params->w < 4 || params->h < 4))
        return "Hard puzzles should be at least size 4x4";
    if (params->difficulty >= DIFF_HARD && (params->w > 8 || params->h > 9))
        return "Hard puzzles should be maximum size 8x9";
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

static int check_solution(game_state *state, bool full) {
    int x,y,i;
    int count_walls;
    int count_paths;
    unsigned char *edges[4];

    gridstate *grid;
    struct findloopstate *fls;
    struct neighbour_ctx ctx;

    int exit_count, exit1, exit2;

    int *dsf;
    int first_cell;
    bool mark_unconnected = false;

    int w = state->w;
    int h = state->h;
    int solved = SOLVED;

    /* Reset error flags */
    if (full) {
        for (i=0;i<w*(h+1);i++) state->edge_h[i] &= ~FLAG_ERROR;
        for (i=0;i<(w+1)*h;i++) state->edge_v[i] &= ~FLAG_ERROR;
        for (i=0;i<(w+2)*(h+2);i++) state->cellstate[i] = FLAG_NONE;
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
            if ((*edges[i] & FLAG_WALL) > 0x00) count_walls++;
            if ((*edges[i] & FLAG_PATH) > 0x00) count_paths++;
        }
        if (full) {
            if (count_paths < 2) solved = AMBIGUOUS;
            if (count_paths > 2 || count_walls > 2) {
                solved = INVALID;
                state->cellstate[(x+1)+(y+1)*(w+2)] |= FLAG_ERROR;
                for (i=0;i<4;i++) {
                    if ((*edges[i] & FLAG_PATH) > 0x00) {
                        *edges[i] |= FLAG_ERROR;
                    }
                }
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
        if ((*edges[0] & FLAG_PATH) > 0x00) grid->faces[i] |= U;
        if ((*edges[1] & FLAG_PATH) > 0x00) grid->faces[i] |= D;
        if ((*edges[2] & FLAG_PATH) > 0x00) grid->faces[i] |= L;
        if ((*edges[3] & FLAG_PATH) > 0x00) grid->faces[i] |= R;
    }
    fls = findloop_new_state(w*h);
    ctx.grid = grid;
    if (findloop_run(fls, w*h, neighbour, &ctx)) {
        for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            int u, v;
            u = y*w + x;
            for (v = neighbour(u, &ctx); v >= 0; v = neighbour(-1, &ctx)) {
                if (findloop_is_loop_edge(fls, u, v)) {
                    solved = INVALID;
                    state->cellstate[(x+1)+(y+1)*(w+2)] |= FLAG_ERROR;
                    edges[0] = state->edge_h + y*w + x;
                    edges[1] = edges[0] + w;
                    edges[2] = state->edge_v + y*(w+1) + x;
                    edges[3] = edges[2] + 1;
                    for (i=0;i<4;i++)
                        if ((*edges[i] & FLAG_PATH) > 0x00) {
                            *edges[i] |= FLAG_ERROR;
                        }
                    }
                }
            }
        }
    }

    findloop_free_state(fls);
    sfree(grid->faces);
    sfree(grid);

    /* Check for exactly two exits */
    exit_count = 0; exit1 = exit2 = -1;
    for (i=0;i<w;i++)
        if ((state->edge_h[i] & FLAG_PATH) > 0x00) {
            exit_count++;
            if (exit1 == -1) exit1 = i; else exit2 = i;
        }
    for (i=w*h;i<w*(h+1);i++)
        if ((state->edge_h[i] & FLAG_PATH) > 0x00) {
            exit_count++;
            if (exit1 == -1) exit1 = i-w; else exit2 = i-w;
        }
    for (i=0;i<(w+1)*h;i+=(w+1))
        if ((state->edge_v[i] & FLAG_PATH) > 0x00) {
            exit_count++;
            if (exit1 == -1) exit1 = w*(i/(w+1)); else exit2 = w*(i/(w+1));
        }
    for (i=w;i<(w+1)*h;i+=(w+1))
        if ((state->edge_v[i] & FLAG_PATH) > 0x00) {
            exit_count++;
            if (exit1 == -1) exit1 = (w-1)+w*(i/(w+1)); else exit2 = (w-1)+w*(i/(w+1));
        }
    if (exit_count < 2)
        solved = AMBIGUOUS;
    else if (exit_count > 2) {
        solved = INVALID;
        for (i=0;i<w;i++)
            if ((state->edge_h[i] & FLAG_PATH) > 0x00) {
                state->edge_h[i] |= FLAG_ERROR;
                x = 1+i%w; y = i/w;
                state->cellstate[x+y*(w+2)] |= FLAG_ERROR;
            }
        for (i=w*h;i<w*(h+1);i++)
            if ((state->edge_h[i] & FLAG_PATH) > 0x00) {
                state->edge_h[i] |= FLAG_ERROR;
                x = 1+i%w; y = 1+i/w;
                state->cellstate[x+y*(w+2)] |= FLAG_ERROR;
            }
        for (i=0;i<(w+1)*h;i+=(w+1))
            if ((state->edge_v[i] & FLAG_PATH) > 0x00) {
                state->edge_v[i] |= FLAG_ERROR;
                x = i%(w+1); y = 1+i/(w+1);
                state->cellstate[x+y*(w+2)] |= FLAG_ERROR;
            }
        for (i=w;i<(w+1)*h;i+=(w+1))
            if ((state->edge_v[i] & FLAG_PATH) > 0x00) {
                state->edge_v[i] |= FLAG_ERROR;
                x = 1+i%(w+1); y = 1+i/(w+1);
                state->cellstate[x+y*(w+2)] |= FLAG_ERROR;
            }
    }

    /* Check if all cells are connected */
    dsf = snewn(w*h,int);
    dsf_init(dsf, w*h);
    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        i = x+y*w;
        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;
        if ((*edges[0] & FLAG_PATH) > 0x00 && y>0)   dsf_merge(dsf, i, i-w);
        if ((*edges[1] & FLAG_PATH) > 0x00 && y<h-1) dsf_merge(dsf, i, i+w);
        if ((*edges[2] & FLAG_PATH) > 0x00 && x>0)   dsf_merge(dsf, i, i-1);
        if ((*edges[3] & FLAG_PATH) > 0x00 && x<w-1) dsf_merge(dsf, i, i+1);
    }
    first_cell = dsf_canonify(dsf, 0);
    if (exit_count == 2 && dsf_canonify(dsf, exit1) == dsf_canonify(dsf, exit2)) {
        mark_unconnected = true;
        first_cell = dsf_canonify(dsf, exit1);
    }
    for (i=0;i<w*h;i++) {
        if (dsf_canonify(dsf, i) != first_cell) {
            solved = INVALID;
            if (mark_unconnected) {
                x = i%w; y=i/w;
                state->cellstate[(x+1)+(y+1)*(w+2)] |= FLAG_ERROR;
            }
        }
    }
    sfree(dsf);

    return solved;
}

struct solver_scratch {
    int *loopdsf;
    bool *done_islands;
    int island_counter;
    bool exits_found;
    int difficulty;
};

static bool solve_single_cells(game_state *state, struct solver_scratch *scratch) {
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
            if (*edges[i] == FLAG_NONE) available_mask |= (0x01 << i);
            else if ((*edges[i] & FLAG_PATH) == FLAG_PATH) path_count++;
            else if ((*edges[i] & FLAG_WALL) == FLAG_WALL) wall_count++;
        }
        if (wall_count == 2 && path_count < 2) {
            for (i = 0; i < 4; i++) {
                if (available_mask & (0x01 << i)){ 
                    *edges[i] |= FLAG_PATH;
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
                    *edges[i] |= FLAG_WALL;
                }
            }
            changed = true;
        }
    }
    return changed;
}

static bool solve_loop_ladders(game_state *state, struct solver_scratch *scratch) {
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
                if (*edges[i] == FLAG_NONE) {
                    free_count++;
                    idx = -1;
                    if (i==0 && y>0)     idx = dsf_canonify(scratch->loopdsf, p-w);
                    if (i==1 && y<(h-1)) idx = dsf_canonify(scratch->loopdsf, p+w);
                    if (i==2 && x>0)     idx = dsf_canonify(scratch->loopdsf, p-1);
                    if (i==3 && x<(w-1)) idx = dsf_canonify(scratch->loopdsf, p+1);
                    dsf_idx[i] = idx;
                }
                if ((*edges[i] & FLAG_WALL) == FLAG_WALL) {
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
                    *edges[single_idx] |= FLAG_PATH;
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

static bool check_partition(const game_state *state) {
    int i,x,y;
    int w = state->w;
    int h = state->h;
    unsigned char *edges[4];
    int *dsf;
    int first_cell;

    /* Check if all cells can be connected */
    dsf = snewn(w*h,int);
    dsf_init(dsf, w*h);
    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        i = x+y*w;
        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;
        if ((*edges[0] & FLAG_WALL) == 0x00 && y>0)   dsf_merge(dsf, i, i-w);
        if ((*edges[1] & FLAG_WALL) == 0x00 && y<h-1) dsf_merge(dsf, i, i+w);
        if ((*edges[2] & FLAG_WALL) == 0x00 && x>0)   dsf_merge(dsf, i, i-1);
        if ((*edges[3] & FLAG_WALL) == 0x00 && x<w-1) dsf_merge(dsf, i, i+1);
    }
    first_cell = dsf_canonify(dsf, 0);
    for (i=0;i<w*h;i++) {
        if (dsf_canonify(dsf, i) != first_cell) {
            sfree(dsf);
            return false;
        }
    }
    sfree(dsf);
    return true;
}

static bool solve_partitions(game_state *state, struct solver_scratch *scratch) {
    int i;
    int w = state->w;
    int h = state->h;
    for (i=0;i<w*(h+1);i++) {
        if (state->edge_h[i] == FLAG_NONE) {
            state->edge_h[i] = FLAG_WALL;
            if (!check_partition(state)) {
                state->edge_h[i] = FLAG_PATH;
                return true;
            }
            state->edge_h[i] = FLAG_NONE;
        }
    }
    for (i=0;i<(w+1)*h;i++) {
        if (state->edge_v[i] == FLAG_NONE) {
            state->edge_v[i] = FLAG_WALL;
            if (!check_partition(state)) {
                state->edge_v[i] = FLAG_PATH;
                return true;
            }
            state->edge_v[i] = FLAG_NONE;
        }
    }

    return false;
}

static inline bool parity(int x, int y) {
    return (x^y)&1;
}

static bool parity_check_block(game_state *state, struct solver_scratch *scratch,
    int bx, int by, int bw, int bh) {
    int i,j,n,x,y,f;
    int h = state->h;
    int w = state->w;
    unsigned char *edges[4];
    int *dsf;
    bool found;
    bool result = false;
    int processed_count = 0;
    int *processed_cells = snewn(w*h, int);
    int *group_cells = snewn(w*h, int);

    scratch->done_islands[scratch->island_counter] = true;

    /* Build a dsf over the relevant area */
    dsf = snewn(w*h,int);
    dsf_init(dsf, w*h);
    for (y=by;y<by+bh;y++)
    for (x=bx;x<bx+bw;x++) {
        i = x+y*w;
        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;
        if ((*edges[0] & FLAG_WALL) == 0x00 && y>by)        dsf_merge(dsf, i, i-w);
        if ((*edges[1] & FLAG_WALL) == 0x00 && y<(by+bh)-1) dsf_merge(dsf, i, i+w);
        if ((*edges[2] & FLAG_WALL) == 0x00 && x>bx)        dsf_merge(dsf, i, i-1);
        if ((*edges[3] & FLAG_WALL) == 0x00 && x<(bx+bw)-1) dsf_merge(dsf, i, i+1);
        for (j=0;j<4;j++)
            if (*edges[j] == FLAG_NONE)
                scratch->done_islands[scratch->island_counter] = false;
    }

    if (scratch->done_islands[scratch->island_counter]) {
        goto finish_parity;
    }

    /* Process each separate dsf group */
    for (i=0;i<w*h;i++) processed_cells[i] = -1;
    while(true) {
        int count_black, count_white;
        int avail_black, avail_white;
        int paths_black, paths_white;
        int walls_black, walls_white;
        
        int min_cells;
        int extra_white, extra_black;
        int min_white, min_black;
        int max_white, max_black;
        bool only_min_path_avail_white, only_min_path_avail_black;
        bool black_used_max_paths, white_used_max_paths;
        bool fill_paths_white, fill_paths_black;
        bool fill_walls_white, fill_walls_black;
        
        int group_count = 0;

        count_black = count_white = 0;
        avail_black = avail_white = 0;
        paths_black = paths_white = 0;
        walls_black = walls_white = 0;

        for (i=0;i<w*h;i++) {
            found = false;
            for (j=0;j<w*h;j++) {
                if (processed_cells[j] == i) {
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
        if (i==w*h) break;

        f = dsf_canonify(dsf, i);
        for (n = 0; n < w*h; n++) {
            if (dsf_canonify(dsf, n) == f) {
                processed_cells[processed_count++] = n;
                group_cells[group_count++] = n;
                x = n%state->w; y=n/state->w;
                parity(x,y) ? count_white++ : count_black++;
            }
        }
        if (group_count == 1) continue;

        for (n=0;n<group_count;n++) {
            x = group_cells[n]%w; y=group_cells[n]/w;
            edges[0] = state->edge_h + y*w + x;
            edges[1] = edges[0] + w;
            edges[2] = state->edge_v + y*(w+1) + x;
            edges[3] = edges[2] + 1;
            if (y == by || dsf_canonify(dsf, x+(y-1)*w) != f) {
                if ((*edges[0] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
                if ((*edges[0] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
                if ((*edges[0] & FLAG_WALL) == FLAG_WALL) parity(x,y) ? walls_white++ : walls_black++;
            }
            if (y == by+bh-1 || dsf_canonify(dsf, x+(y+1)*w) != f) {
                if ((*edges[1] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
                if ((*edges[1] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
                if ((*edges[1] & FLAG_WALL) == FLAG_WALL) parity(x,y) ? walls_white++ : walls_black++;
            }
            if (x == bx || dsf_canonify(dsf, (x-1)+y*w) != f) {
                if ((*edges[2] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
                if ((*edges[2] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
                if ((*edges[2] & FLAG_WALL) == FLAG_WALL) parity(x,y) ? walls_white++ : walls_black++;
            }
            if (x == bx+bw-1 || dsf_canonify(dsf, (x+1)+y*w) != f) {
                if ((*edges[3] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
                if ((*edges[3] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
                if ((*edges[3] & FLAG_WALL) == FLAG_WALL) parity(x,y) ? walls_white++ : walls_black++;
            }
        }

        min_cells = min(count_white, count_black);

        extra_white = count_white - min_cells;
        extra_black = count_black - min_cells;

        min_white = 1 + extra_white - extra_black;
        min_black = 1 + extra_black - extra_white;

        max_white = min(avail_white, avail_black+2*(extra_white-extra_black));
        max_black = min(avail_black, avail_white+2*(extra_black-extra_white));

        if (count_black + count_white == w*h) {
            min_white = max_white = (count_black == count_white) ? 1 :
                                    (count_black <  count_white) ? 2 : 0;
            min_black = max_black = (count_black == count_white) ? 1 :
                                    (count_black <  count_white) ? 0 : 2;
        }

        only_min_path_avail_white = (min_white == avail_white) && (paths_white < min_white);
        only_min_path_avail_black = (min_black == avail_black) && (paths_black < min_black);

        black_used_max_paths = (max_black == paths_black) && (avail_white == max_white) && (paths_white < max_white);
        white_used_max_paths = (max_white == paths_white) && (avail_black == max_black) && (paths_black < max_black);

        fill_paths_white = (only_min_path_avail_white || black_used_max_paths);
        fill_paths_black = (only_min_path_avail_black || white_used_max_paths);

        fill_walls_white = (paths_white == max_white) && (avail_white > max_white);
        fill_walls_black = (paths_black == max_black) && (avail_black > max_black);

        if (fill_paths_white || fill_paths_black || fill_walls_white || fill_walls_black) {
            for (n=0;n<group_count;n++) {
                x = group_cells[n]%w; y=group_cells[n]/w;
                edges[0] = state->edge_h + y*w + x;
                edges[1] = edges[0] + w;
                edges[2] = state->edge_v + y*(w+1) + x;
                edges[3] = edges[2] + 1;
                if ((fill_paths_white && parity(x,y)) ||
                    (fill_paths_black && !parity(x,y))) {
                    if ((*edges[0] == FLAG_NONE) && 
                        (y == by || dsf_canonify(dsf, x+(y-1)*w) != f)) {
                        *edges[0] = FLAG_PATH;
                    }
                    if ((*edges[1] == FLAG_NONE) && 
                        (y == by+bh-1 || dsf_canonify(dsf, x+(y+1)*w) != f)) {
                        *edges[1] = FLAG_PATH;
                    }
                    if ((*edges[2] == FLAG_NONE) && 
                        (x == bx || dsf_canonify(dsf, (x-1)+y*w) != f)) {
                        *edges[2] = FLAG_PATH;
                    }
                    if ((*edges[3] == FLAG_NONE) && 
                        (x == bx+bw-1 || dsf_canonify(dsf, (x+1)+y*w) != f)) {
                        *edges[3] = FLAG_PATH;
                    }
                }
                if ((fill_walls_white && parity(x,y)) ||
                    (fill_walls_black && !parity(x,y))) {
                    if ((*edges[0] == FLAG_NONE) && 
                        (y == by || dsf_canonify(dsf, x+(y-1)*w) != f)) {
                        *edges[0] = FLAG_WALL;
                    }
                    if ((*edges[1] == FLAG_NONE) && 
                        (y == by+bh-1 || dsf_canonify(dsf, x+(y+1)*w) != f)) {
                        *edges[1] = FLAG_WALL;
                    }
                    if ((*edges[2] == FLAG_NONE) && 
                        (x == bx || dsf_canonify(dsf, (x-1)+y*w) != f)) {
                        *edges[2] = FLAG_WALL;
                    }
                    if ((*edges[3] == FLAG_NONE) && 
                        (x == bx+bw-1 || dsf_canonify(dsf, (x+1)+y*w) != f)) {
                        *edges[3] = FLAG_WALL;
                    }
                }
            }
            result = true;
            goto finish_parity;
        }
    }
    
finish_parity:
    sfree(group_cells);
    sfree(processed_cells);
    sfree(dsf);
    return result;
}

static bool solve_parity(game_state *state, struct solver_scratch *scratch) {
    int w,h,x,y;
    scratch->island_counter = 0;
    for (h=state->h;h>=2;h--) 
    for (w=state->w;w>=2;w--) {
        for (y=0;y<=state->h-h;y++)
        for (x=0;x<=state->w-w;x++) {
            if (!scratch->done_islands[scratch->island_counter])
                if (parity_check_block(state, scratch, x, y, w, h)) return true;
            scratch->island_counter++;
        }
    }
    return false;
}

static bool solve_early_exits(game_state *state, struct solver_scratch *scratch) {
    int i;
    int w = state->w;
    int h = state->h;
    int exit_count;

    if (scratch->exits_found) return false;

    exit_count = 0;
    for (i=0;i<w;i++)
        if ((state->edge_h[i] & FLAG_PATH) > 0x00) exit_count++;
    for (i=w*h;i<w*(h+1);i++)
        if ((state->edge_h[i] & FLAG_PATH) > 0x00) exit_count++;
    for (i=0;i<(w+1)*h;i+=(w+1))
        if ((state->edge_v[i] & FLAG_PATH) > 0x00) exit_count++;
    for (i=w;i<(w+1)*h;i+=(w+1))
        if ((state->edge_v[i] & FLAG_PATH) > 0x00) exit_count++;

    if (exit_count == 2) {
        for (i=0;i<w;i++) {
            if (state->edge_h[i] == FLAG_NONE) state->edge_h[i] = FLAG_WALL;
        }
        for (i=w*h;i<w*(h+1);i++) {
            if (state->edge_h[i] == FLAG_NONE) state->edge_h[i] = FLAG_WALL;
        }
        for (i=0;i<(w+1)*h;i+=(w+1)) {
            if (state->edge_v[i] == FLAG_NONE) state->edge_v[i] = FLAG_WALL;
        }
        for (i=w;i<(w+1)*h;i+=(w+1)) {
            if (state->edge_v[i] == FLAG_NONE) state->edge_v[i] = FLAG_WALL;
        }
        scratch->exits_found = true;
        return true;
    }
    return false;
}

static bool check_for_loops(game_state *state) {
    int i,x,y;
    int w = state->w;
    int h = state->h;
    bool result = false;
    unsigned char *edges[4];

    gridstate *grid;
    struct findloopstate *fls;
    struct neighbour_ctx ctx;

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
        if ((*edges[0] & FLAG_PATH) > 0x00) grid->faces[i] |= U;
        if ((*edges[1] & FLAG_PATH) > 0x00) grid->faces[i] |= D;
        if ((*edges[2] & FLAG_PATH) > 0x00) grid->faces[i] |= L;
        if ((*edges[3] & FLAG_PATH) > 0x00) grid->faces[i] |= R;
    }
    fls = findloop_new_state(w*h);
    ctx.grid = grid;
    if (findloop_run(fls, w*h, neighbour, &ctx)) {
        for (x = 0; x < w; x++) {
            for (y = 0; y < h; y++) {
                int u, v;
                u = y*w + x;
                for (v = neighbour(u, &ctx); v >= 0; v = neighbour(-1, &ctx)) {
                    if (findloop_is_loop_edge(fls, u, v)) {
                        result = true;
                        goto finish_loopcheck;
                    }
                }
            }
        }
    }

finish_loopcheck:

    findloop_free_state(fls);
    sfree(grid->faces);
    sfree(grid);
    return result;
}

static bool solve_loops(game_state *state, struct solver_scratch *scratch) {
    int i;
    int w = state->w;
    int h = state->h;

    for (i=0;i<w*(h+1);i++) {
        if (state->edge_h[i] == FLAG_NONE) {
            state->edge_h[i] = FLAG_PATH;
            if (check_for_loops(state)) {
                state->edge_h[i] = FLAG_WALL;
                return true;
            }
            state->edge_h[i] = FLAG_NONE;
        }
    }
    for (i=0;i<(w+1)*h;i++) {
        if (state->edge_v[i] == FLAG_NONE) {
            state->edge_v[i] = FLAG_PATH;
            if (check_for_loops(state)) {
                state->edge_v[i] = FLAG_WALL;
                return true;
            }
            state->edge_v[i] = FLAG_NONE;
        }
    }

    return false;
}

static bool solve_exit_parity(game_state *state, struct solver_scratch *scratch) {
    int i,x,y;
    int w = state->w;
    int h = state->h;
    int avail_white, avail_black;
    int paths_white, paths_black;
    int needed_white, needed_black;
    bool only_min_path_avail_white, only_min_path_avail_black;
    bool black_used_max_paths, white_used_max_paths;
    bool fill_paths_white, fill_paths_black;
    bool fill_walls_white, fill_walls_black;

    avail_white = avail_black = 0;
    paths_white = paths_black = 0;

    for (i=0;i<w;i++) {
        x = i; y = 0;
        if ((state->edge_h[i] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
        if ((state->edge_h[i] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
    }
    for (i=w*h;i<w*(h+1);i++) {
        x = i-w*h; y = h-1;
        if ((state->edge_h[i] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
        if ((state->edge_h[i] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
    }
    for (i=0;i<(w+1)*h;i+=(w+1)) {
        x = 0; y = i/(w+1);
        if ((state->edge_v[i] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
        if ((state->edge_v[i] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
    }
    for (i=w;i<(w+1)*h;i+=(w+1)) {
        x = w-1; y = i/(w+1);
        if ((state->edge_v[i] & FLAG_WALL) == 0)         parity(x,y) ? avail_white++ : avail_black++;
        if ((state->edge_v[i] & FLAG_PATH) == FLAG_PATH) parity(x,y) ? paths_white++ : paths_black++;
    }

    needed_white = (w*h) % 2 == 0 ? 1 : 0;
    needed_black = (w*h) % 2 == 0 ? 1 : 2;

    only_min_path_avail_white = (needed_white == avail_white) && (paths_white < needed_white);
    only_min_path_avail_black = (needed_black == avail_black) && (paths_black < needed_black);

    black_used_max_paths = (needed_black == paths_black) && 
                           (avail_white == needed_white) && 
                           (paths_white < needed_white);
    white_used_max_paths = (needed_white == paths_white) && 
                           (avail_black == needed_black) &&
                           (paths_black < needed_black);

    fill_paths_white = (only_min_path_avail_white || black_used_max_paths);
    fill_paths_black = (only_min_path_avail_black || white_used_max_paths);

    fill_walls_white = (paths_white == needed_white) && (avail_white > needed_white);
    fill_walls_black = (paths_black == needed_black) && (avail_black > needed_black);

    if (fill_paths_white || fill_paths_black || fill_walls_white || fill_walls_black) {
        for (i=0;i<w;i++) {
            x = i; y = 0;
            if ( (state->edge_h[i] == FLAG_NONE) && 
               ( (fill_paths_white && parity(x,y)) ||
                 (fill_paths_black && !parity(x,y)) )) { 
                    state->edge_h[i] = FLAG_PATH;
            }
            if ( (state->edge_h[i] == FLAG_NONE) && 
               ( (fill_walls_white && parity(x,y)) ||
                 (fill_walls_black && !parity(x,y)) )) { 
                    state->edge_h[i] = FLAG_WALL;
            }
        }
        for (i=w*h;i<w*(h+1);i++) {
            x = i-w*h; y = h-1;
            if ( (state->edge_h[i] == FLAG_NONE) && 
               ( (fill_paths_white && parity(x,y)) ||
                 (fill_paths_black && !parity(x,y)) )) { 
                    state->edge_h[i] = FLAG_PATH;
            }
            if ( (state->edge_h[i] == FLAG_NONE) && 
               ( (fill_walls_white && parity(x,y)) ||
                 (fill_walls_black && !parity(x,y)) )) { 
                    state->edge_h[i] = FLAG_WALL;
            }
        }
        for (i=0;i<(w+1)*h;i+=(w+1)) {
            x = 0; y = i/(w+1);
            if ( (state->edge_v[i] == FLAG_NONE) && 
               ( (fill_paths_white && parity(x,y)) ||
                 (fill_paths_black && !parity(x,y)) )) { 
                    state->edge_v[i] = FLAG_PATH;
            }
            if ( (state->edge_v[i] == FLAG_NONE) && 
               ( (fill_walls_white && parity(x,y)) ||
                 (fill_walls_black && !parity(x,y)) )) { 
                    state->edge_v[i] = FLAG_WALL;
            }
        }
        for (i=w;i<(w+1)*h;i+=(w+1)) {
            x = w-1; y = (i-w)/(w+1);
            if ( (state->edge_v[i] == FLAG_NONE) && 
               ( (fill_paths_white && parity(x,y)) ||
                 (fill_paths_black && !parity(x,y)) )) { 
                    state->edge_v[i] = FLAG_PATH;
            }
            if ( (state->edge_v[i] == FLAG_NONE) && 
               ( (fill_walls_white && parity(x,y)) ||
                 (fill_walls_black && !parity(x,y)) )) { 
                    state->edge_v[i] = FLAG_WALL;
            }
        }
        return true;
    }

    return false;
}
static int walls_solve(game_state *state, int difficulty) {
    int i;
    int w = state->w;
    int h = state->h;
    int islands = (((w-1)*(w))*((h-1)*(h)))/4;
    struct solver_scratch *scratch = snew(struct solver_scratch);

    scratch->loopdsf = snewn(w*h, int);
    dsf_init(scratch->loopdsf, w*h);
    scratch->done_islands = snewn(islands, bool);
    scratch->exits_found = false;
    scratch->difficulty = difficulty;
    for (i=0;i<islands;i++) scratch->done_islands[i] = false;

    while(true) {
        if (difficulty >= DIFF_EASY   && solve_single_cells(state, scratch)) continue;
        if (difficulty >= DIFF_EASY   && solve_early_exits(state, scratch)) continue;
        if (check_solution(state, false) == SOLVED) break;
        if (difficulty >= DIFF_NORMAL && solve_loops(state, scratch)) continue;
        if (difficulty >= DIFF_NORMAL && solve_partitions(state, scratch)) continue;
        if (check_solution(state, false) == SOLVED) break;
        if (difficulty >= DIFF_TRICKY && solve_loop_ladders(state, scratch)) continue;
        if (difficulty >= DIFF_TRICKY && solve_exit_parity(state, scratch)) continue;
        if (check_solution(state, false) == SOLVED) break;
        if (difficulty >= DIFF_HARD   && solve_parity(state, scratch)) continue;
        break;
    }

    sfree(scratch->done_islands);
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

static void reverse_path(int i1, int i2, int *pathx, int *pathy) {
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

static int backbite_left(int step, int n, int *pathx, int *pathy, int w, int h) {
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

static int backbite_right(int step, int n, int *pathx, int *pathy, int w, int h) {
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

static int backbite(int n, int *pathx, int *pathy, int w, int h, random_state *rs) {
    return (random_upto(rs, 2) == 0) ?
        backbite_left( DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h) :
        backbite_right(DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h);
}

static void generate_hamiltonian_path(game_state *state, random_state *rs) {
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
            if      (pathx[n+1] - pathx[n] ==  1) state->edge_v[y*(w+1)+x+1] = FLAG_NONE;
            else if (pathx[n+1] - pathx[n] == -1) state->edge_v[y*(w+1)+x]   = FLAG_NONE;
            else if (pathy[n+1] - pathy[n] ==  1) state->edge_h[y*w+x+w]     = FLAG_NONE;
            else if (pathy[n+1] - pathy[n] == -1) state->edge_h[y*w+x]       = FLAG_NONE;
        }
        if (n == 0 || n == (w*h)-1) {
            if      (pathx[n] == 0)   state->edge_v[y*(w+1)+x]   = FLAG_NONE;
            else if (pathx[n] == w-1) state->edge_v[y*(w+1)+x+1] = FLAG_NONE;
            else if (pathy[n] == 0)   state->edge_h[y*w+x]       = FLAG_NONE;
            else if (pathy[n] == h-1) state->edge_h[y*w+x+w]     = FLAG_NONE;
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
    state->cellstate = snewn((state->w + 2)*(state->h + 2), unsigned char);

    memset(state->edge_h, FLAG_WALL | FLAG_FIXED, state->w*(state->h+1)*sizeof(unsigned char));
    memset(state->edge_v, FLAG_WALL | FLAG_FIXED, (state->w+1)*state->h*sizeof(unsigned char));
    memset(state->cellstate, FLAG_NONE, (state->w+2)*(state->h+2)*sizeof(unsigned char));

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
    ret->cellstate = snewn((state->w + 2)*(state->h + 2), unsigned char);

    memcpy(ret->edge_h, state->edge_h, state->w*(state->h + 1) * sizeof(unsigned char));
    memcpy(ret->edge_v, state->edge_v, (state->w + 1)*state->h * sizeof(unsigned char));
    memcpy(ret->cellstate, state->cellstate, (state->w+2)*(state->h+2)*sizeof(unsigned char));

    return ret;
}

static void free_state(game_state *state) {
    sfree(state->edge_v);
    sfree(state->edge_h);
    sfree(state->cellstate);
    sfree(state);
}

static void count_edges(unsigned char edge, char **e, int *erun, int *wrun) {
    if ((edge & FLAG_WALL) == 0 && *wrun > 0) {
        *e += sprintf(*e, "%d", *wrun);
        *wrun = *erun = 0;
    }
    else if ((edge & FLAG_WALL) && *erun > 0) {
        while (*erun >= 25) {
            *(*e)++ = 'z';
            *erun -= 25;
        }
        if (*erun == 0) *wrun = 0;
        else {
            *(*e)++ = ('a' + *erun - 1);
            *erun = 0; *wrun = -1;
        }
    }
    if(edge & FLAG_WALL) (*wrun)++;
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
    int difficulty = params->difficulty;
    int x,y;

    int i;
    int borderreduce;
    int bordernum = 0;
    int wallnum = 0;
    int ws = (w+1)*h + w*(h+1);
    int vo = w*(h+1);
    int *wallidx;
    int result;

    wallidx = snewn(ws, int);
    
    while (true) {
        borderreduce = difficulty == DIFF_EASY   ? random_upto(rs, 4) :
                       difficulty == DIFF_NORMAL ? random_upto(rs, 8) :
                                                   2*w+2*h;
        wallnum = bordernum = 0;
        new = new_state(params);
        generate_hamiltonian_path(new, rs);

        for (i=0;i<w*(h+1);i++)
            if ((new->edge_h[i] & FLAG_WALL) > 0x00)
                wallidx[wallnum++] = i;
        for (i=0;i<(w+1)*h;i++)
            if ((new->edge_v[i] & FLAG_WALL) > 0x00)
                wallidx[wallnum++] = i+vo;

        shuffle(wallidx, wallnum, sizeof(int), rs);

        for (i=0;i<wallnum;i++) {
            int wi = wallidx[i];

            /* Remove border walls up to 'borderreduce' */
            /* Avoid two open border edges on a corner cell */
            if (wi<vo) {
                int xh, yh;
                xh = wi%w; yh = wi/w;
                if ((xh == 0     && yh == 0 && ((new->edge_v[0]           & FLAG_WALL) == 0x00)) ||
                    (xh == (w-1) && yh == 0 && ((new->edge_v[w]           & FLAG_WALL) == 0x00)) ||
                    (xh == 0     && yh == h && ((new->edge_v[(w+1)*(h-1)] & FLAG_WALL) == 0x00)) ||
                    (xh == (w-1) && yh == h && ((new->edge_v[(w+1)*h-1]   & FLAG_WALL) == 0x00))) {
                    continue;
                }
                if ((yh == 0 || yh == h) && (bordernum >= borderreduce)) {
                    continue;
                }
            }
            else {
                int xv, yv;
                xv = (wi-vo)%(w+1); yv = (wi-vo)/(w+1);
                if ((xv == 0 && yv == 0     && ((new->edge_h[0]           & FLAG_WALL) == 0x00)) ||
                    (xv == w && yv == 0     && ((new->edge_h[w-1]         & FLAG_WALL) == 0x00)) ||
                    (xv == 0 && yv == (h-1) && ((new->edge_h[w*h]         & FLAG_WALL) == 0x00)) ||
                    (xv == w && yv == (h-1) && ((new->edge_h[w*(h+1)-1]   & FLAG_WALL) == 0x00))) {
                    continue;
                }
                if ((xv == 0 || xv == w) && (bordernum >= borderreduce)) {
                    continue;
                }
            }

            /* Temporarily remove wall, check if game is still solveable */
            tmp = dup_state(new);
            if (wi<vo) tmp->edge_h[wi]    = FLAG_NONE;
            else       tmp->edge_v[wi-vo] = FLAG_NONE;
            
            /* It is, remove this wall permanently */
            if (walls_solve(tmp, difficulty) == SOLVED) {
                if (wi<vo) new->edge_h[wi]    = FLAG_NONE;
                else       new->edge_v[wi-vo] = FLAG_NONE;
                if (wi<vo && (wi/w == 0 || wi/w == h)) bordernum++;
                else if ((wi-vo)%(w+1) == 0 || (wi-vo)%(w+1) == w) bordernum++;
            }
            free_state(tmp);
        }

        if (difficulty == DIFF_EASY) break;
        tmp = dup_state(new);
        result = walls_solve(tmp, difficulty-1);
        free_state(tmp);
        if (result == SOLVED) {
            free_state(new);
            if (difficulty == DIFF_HARD && (w>7 || h>8)) difficulty--;
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
    while (erun >= 25) {*e++ = 'z'; erun -= 25; }
    if(erun > 0) *e++ = ('a' + erun - 1);
    *e++ = ',';
    erun = wrun = 0;
    for (y=0;y<h;y++)
    for (x=0;x<=w;x++) 
        count_edges(new->edge_v[y*(w+1)+x], &e, &erun, &wrun);
    if(wrun > 0) e += sprintf(e, "%d", wrun);
    while (erun >= 25) {*e++ = 'z'; erun -= 25; }
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
                if (fh) state->edge_h[i] = FLAG_WALL | FLAG_FIXED;
                else    state->edge_v[i] = FLAG_WALL | FLAG_FIXED;
                i++;
            }
            while (*desc && isdigit((unsigned char)*desc)) desc++;
        }
        else if(*desc == 'z') {
            for (c=25;c>0;c--) {
                if (fh) state->edge_h[i] = FLAG_NONE;
                else    state->edge_v[i] = FLAG_NONE;
                i++;
            }
            desc++;
        }
        else if(*desc >= 'a' && *desc < 'z') {
            for (c = *desc - 'a' + 1; c > 0; c--) {
                if (fh) state->edge_h[i] = FLAG_NONE;
                else    state->edge_v[i] = FLAG_NONE;
                i++;
            }
            if (i < (fh ? w*(h+1) : (w+1)*h)) {
                if (fh) state->edge_h[i] = FLAG_WALL | FLAG_FIXED;
                else    state->edge_v[i] = FLAG_WALL | FLAG_FIXED;
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
    walls_solve(solve_state, DIFF_HARD);
    p += sprintf(p, "S");
    for (i = 0; i < w*(h+1); i++) {
        if (solve_state->edge_h[i] == FLAG_WALL)
            p += sprintf(p, ";W%d", i);
        if (solve_state->edge_h[i] == FLAG_NONE)
            p += sprintf(p, ";C%d", i);
        if (solve_state->edge_h[i] == FLAG_PATH)
            p += sprintf(p, ";P%d", i);
    }
    for (i = 0; i < (w+1)*h; i++) {
        if (solve_state->edge_v[i] == FLAG_WALL)
            p += sprintf(p, ";W%d", i+voff);
        if (solve_state->edge_v[i] == FLAG_NONE)
            p += sprintf(p, ";C%d", i+voff);
        if (solve_state->edge_v[i] == FLAG_PATH)
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

    bool show_grid;        /* true if checkerboard grid is shown */
};


static game_ui *new_ui(const game_state *state) {
    int w = state->w;
    int h = state->h;
    game_ui *ui = snew(game_ui);

    ui->dragcoords = snewn((8*w+5)*(8*h+5), int);
    ui->ndragcoords = -1;
    ui->dragdir = BLANK;

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
#define BORDER (5*TILESIZE/2)

#define COORD(x) ( (x) * 8 * TILESIZE + BORDER )
#define CENTERED_COORD(x) ( COORD(x) + 4*TILESIZE )
#define FROMCOORD(x) ( ((x) < BORDER) ? -1 : ( ((x) - BORDER) / (8 * TILESIZE) ) )

#define CELLCOORD(x) ( (x) * TILESIZE )
#define FROMCELLCOORD(x) ( (x) / TILESIZE )

struct game_drawstate {
    int tilesize;
    int w, h;
    bool tainted;
    unsigned long *cell;
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
                if ((state->edge_h[edge] & FLAG_FIXED) > 0x00) return NULL;
                if ((state->edge_h[edge] & FLAG_PATH) > 0x00 || 
                    (state->edge_h[edge] & FLAG_WALL) > 0x00) sprintf(buf,"C%d",edge);
                else sprintf(buf,"%c%d",(button == LEFT_BUTTON) ? 'P' : 'W', edge);
            }
            else {
                edge = direction == L ? fx+fy*(w+1) : (fx+1)+fy*(w+1);
                if ((state->edge_v[edge] & FLAG_FIXED) > 0x00) return NULL;
                if ((state->edge_v[edge] & FLAG_PATH) > 0x00 || 
                    (state->edge_v[edge] & FLAG_WALL) > 0x00) sprintf(buf,"C%d",edge+w*(h+1));
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
            newedge = (c=='W') ? FLAG_WALL :
                      (c=='P') ? FLAG_PATH :
                                 FLAG_NONE;
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

    *x = (8 * (params->w) + 5) * TILESIZE;
    *y = (8 * (params->h) + 5) * TILESIZE;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize) {
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours) {
    float *ret = snewn(3 * NCOLOURS, float);

    ret[COL_BACKGROUND * 3 + 0] = 1.0F;
    ret[COL_BACKGROUND * 3 + 1] = 1.0F;
    ret[COL_BACKGROUND * 3 + 2] = 1.0F;

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

    ret[COL_WALL_B * 3 + 0] = 1.0F;
    ret[COL_WALL_B * 3 + 1] = 1.0F;
    ret[COL_WALL_B * 3 + 2] = 1.0F;

    ret[COL_PATH_A * 3 + 0] = 0.75F;
    ret[COL_PATH_A * 3 + 1] = 0.75F;
    ret[COL_PATH_A * 3 + 2] = 0.75F;

    ret[COL_PATH_B * 3 + 0] = 0.0F;
    ret[COL_PATH_B * 3 + 1] = 0.0F;
    ret[COL_PATH_B * 3 + 2] = 0.0F;

    ret[COL_DRAGON * 3 + 0] = 0.25F;
    ret[COL_DRAGON * 3 + 1] = 0.25F;
    ret[COL_DRAGON * 3 + 2] = 0.25F;

    ret[COL_DRAGOFF * 3 + 0] = 0.5F;
    ret[COL_DRAGOFF * 3 + 1] = 0.5F;
    ret[COL_DRAGOFF * 3 + 2] = 0.5F;

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
    ds->cell   = snewn((8*w+5)*(8*h+5), unsigned long);

    for (i=0;i<(8*w+5)*(8*h+5); i++) ds->cell[i] = 0x00000000;

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
    int h = ds->h;
    int x = pos%(8*w+5);
    int y = pos/(8*w+5);
    int ox = x*ds->tilesize;
    int oy = y*ds->tilesize;
    int cx = x-2;
    int cy = y-2;
    int ts = ds->tilesize;
    int ts2 = (ds->tilesize+1)/2;
    int ts3 = (ds->tilesize+1)/3;
    int ts4 = (ds->tilesize+1)/4;

    clip(dr, ox, oy, ts, ts);

    if ((ds->cell[pos] & (FLAG_WALL | FLAG_FIXED)) == (FLAG_WALL|FLAG_FIXED)) {
        draw_rect(dr, ox, oy, ts, ts, COL_FIXED);
    }
    else if ((ds->cell[pos] & FLAG_PATH) == FLAG_PATH) {
        int cornercol = (ds->cell[pos] & FLAG_ERROR)>0 ? COL_ERROR : COL_PATH_B;
        int width = (ds->cell[pos] & FLAG_ERROR)>0 ? ts3:ts4;
        draw_rect(dr, ox, oy, ts, ts,
            (ds->cell[pos] & FLAG_DRAG) >0 ? COL_DRAGOFF :
            (ds->cell[pos] & FLAG_ERROR)>0 ? COL_ERROR :
                                             COL_PATH_A);
        if ((cx<0 || cx>(8*w) || (cy%8)==4) && (cx%8)!=4) {
            draw_rect(dr, ox, oy, ts, width, cornercol);
            draw_rect(dr, ox, oy+ts-width, ts, width, cornercol);
        }
        else if ((cy<0 || cy>(8*h) || (cx%8)==4) && (cy%8)!=4) {
            draw_rect(dr, ox, oy, width, ts, cornercol);
            draw_rect(dr, ox+ts-width, oy, width, ts, cornercol);
        }
        else if ((cx%8)==4 && (cy%8)==4) {
            if ((ds->cell[pos] & FLAG_LEFT) > 0) {
                draw_rect(dr, ox, oy,          width, width, cornercol);
                draw_rect(dr, ox, oy+ts-width, width, width, cornercol);
            }
            else {
                draw_rect(dr, ox, oy, width, ts, cornercol);
            }
            if ((ds->cell[pos] & FLAG_RIGHT) > 0) {
                draw_rect(dr, ox+ts-width, oy,          width, width, cornercol);
                draw_rect(dr, ox+ts-width, oy+ts-width, width, width, cornercol);
            }
            else {
                draw_rect(dr, ox+ts-width, oy, width, ts, cornercol);
            }
            if ((ds->cell[pos] & FLAG_UP) > 0) {
                draw_rect(dr, ox,          oy, width, width, cornercol);
                draw_rect(dr, ox+ts-width, oy, width, width, cornercol);
            }
            else {
                draw_rect(dr, ox, oy, ts, width, cornercol);
            }
            if ((ds->cell[pos] & FLAG_DOWN) > 0) {
                draw_rect(dr, ox,          oy+ts-width, width, width, cornercol);
                draw_rect(dr, ox+ts-width, oy+ts-width, width, width, cornercol);
            }
            else {
                draw_rect(dr, ox, oy+ts-width, ts, width, cornercol);
            }
        }

    }
    else {
        for (i=0;i<4;i++) {
            unsigned char bg = (ds->cell[pos] >> (8+(2*i))) & 0x03;
            if      (i==0) { dx = ox;     dy = oy; }
            else if (i==1) { dx = ox;     dy = oy+ts2; }
            else if (i==2) { dx = ox+ts2; dy = oy; }
            else           { dx = ox+ts2; dy = oy+ts2; }
            col = (ds->cell[pos] & FLAG_DRAG) >0 ? COL_DRAGON :
                  (bg==0x00) ? COL_BACKGROUND :
                  (bg==0x01) ? COL_FLOOR_A :
                               COL_FLOOR_B;
            draw_rect(dr, dx, dy, ts2, ts2, col);
        }
        if ((ds->cell[pos] & FLAG_WALL) == FLAG_WALL) {
            int c[8];
            c[0] = ox; c[1] = oy;
            c[2] = ox; c[3] = oy+ts2;
            c[4] = ox+ts2; c[5] = oy;
            draw_polygon(dr, c, 3, COL_WALL_A, COL_WALL_A);
            c[0] = ox; c[1] = oy+ts2; 
            c[2] = ox; c[3] = oy+ts;
            c[4] = ox+ts; c[5] = oy; 
            c[6] = ox+ts2; c[7] = oy;
            draw_polygon(dr, c, 4, COL_WALL_B, COL_WALL_B);
            c[0] = ox; c[1] = oy+ts; 
            c[2] = ox+ts2; c[3] = oy+ts;
            c[4] = ox+ts; c[5] = oy+ts2; 
            c[6] = ox+ts; c[7] = oy;
            draw_polygon(dr, c, 4, COL_WALL_A, COL_WALL_A);
            c[0] = ox+ts2; c[1] = oy+ts;
            c[2] = ox+ts; c[3] = oy+ts;
            c[4] = ox+ts; c[5] = oy+ts2;
            draw_polygon(dr, c, 3, COL_WALL_B, COL_WALL_B);
        }
        else if ((ds->cell[pos] & FLAG_ERROR) >0) {
            draw_rect(dr, ox, oy, ts, ts, COL_ERROR);
        }
    }
    draw_update(dr, ox, oy, ts, ts);
    unclip(dr);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime) {
    int i,j,w,h,x,y,cx,cy,csx,csy;
    unsigned long *newcell;
    unsigned char cellerror;

    w = state->w; h = state->h;
    newcell = snewn((8*w+5)*(8*h+5), unsigned long);
    for (i=0;i<((8*w)+5)*((8*h)+5); i++) newcell[i] = 0x00000000;

    if (ui->ndragcoords > 0)
        for (i=0;i<ui->ndragcoords;i++)
            newcell[ui->dragcoords[i]] |= FLAG_DRAG;
    
    for (i=0;i<((8*w)+5)*((8*h)+5); i++) {
        x = i%(8*w+5)-2; y = i/(8*w+5)-2;
        cx = x/8; cy = y/8;
        csx = (x<0) ? 0 : cx+1; csy = (y<0) ? 0 : cy+1;

        cellerror = state->cellstate[csx+csy*(w+2)];

        /* Corner border cells. Unused. */
        if ((x<0 && y<0) || (x<0 && y>8*h) || (x>8*w && y<0) || (x>8*w && y>8*h)) {}

        /* Left / Right border cells */
        else if (x<0 || x>8*w) {
            if (y%8==4 && x==-1)
                newcell[i] |= (state->edge_v[cy*(w+1)]   & (FLAG_PATH|cellerror));
            if (y%8==4 && x==8*w+1)
                newcell[i] |= (state->edge_v[w+cy*(w+1)] & (FLAG_PATH|cellerror));
        }

        /* Top / Bottom border cells */
        else if (y<0 || y>8*h) {
            if (x%8==4 && y==-1)
                newcell[i] |= (state->edge_h[cx]         & (FLAG_PATH|cellerror));
            if (x%8==4 && y==8*h+1)
                newcell[i] |= (state->edge_h[cx+h*w]     & (FLAG_PATH|cellerror));
        }

        /* 4-Corner cells */
        else if ((x%8 == 0) && (y%8 == 0)) {
            if (ui->show_grid) {
                if (cx > 0 && cy > 0) newcell[i] |= ((cx-1)^(cy-1))&1 ? 0x0100:0x0200;
                if (cx > 0 && cy < h) newcell[i] |= ((cx-1)^ cy)   &1 ? 0x0400:0x0800;
                if (cx < w && cy > 0) newcell[i] |= ( cx   ^(cy-1))&1 ? 0x1000:0x2000;
                if (cx < w && cy < h) newcell[i] |= ( cx   ^ cy)   &1 ? 0x4000:0x8000;
            }
            if (cx > 0) newcell[i] |= state->edge_h[(cx-1)+cy*w]     & (FLAG_WALL | FLAG_FIXED);
            if (cx < w) newcell[i] |= state->edge_h[cx+cy*w]         & (FLAG_WALL | FLAG_FIXED);
            if (cy > 0) newcell[i] |= state->edge_v[cx+(cy-1)*(w+1)] & (FLAG_WALL | FLAG_FIXED);
            if (cy < h) newcell[i] |= state->edge_v[cx+cy*(w+1)]     & (FLAG_WALL | FLAG_FIXED);
        }

        /* Horizontal edge cells */
        else if (y%8 == 0) {
            if (ui->show_grid) {
                if (cy > 0) newcell[i] |= (cx^(cy-1))&1 ? 0x1100:0x2200;
                if (cy < h) newcell[i] |= (cx^ cy)   &1 ? 0x4400:0x8800;
            }
            newcell[i] |= state->edge_h[cx+cy*w] & (FLAG_WALL|FLAG_FIXED);
            if (x%8==4) {
                newcell[i] |= (state->edge_h[cx+cy*w] & (FLAG_PATH|FLAG_ERROR));
                if ((newcell[i] & FLAG_PATH)>0) newcell[i] |= cellerror;
            }
        }

        /* Vertical edge cells */
        else if (x%8 == 0) {
            if (ui->show_grid) {
                if (cx > 0) newcell[i] |= ((cx-1)^cy)&1 ? 0x0500:0x0a00;
                if (cx < w) newcell[i] |= ( cx   ^cy)&1 ? 0x5000:0xa000;
            }
            newcell[i] |= state->edge_v[cx+cy*(w+1)] & (FLAG_WALL|FLAG_FIXED);
            if (y%8==4) {
                newcell[i] |= (state->edge_v[cx+cy*(w+1)] & (FLAG_PATH|FLAG_ERROR));
                if ((newcell[i] & FLAG_PATH)>0) newcell[i] |= cellerror;
            }
        }
        
        /* Path cells */
        else {
            if (ui->show_grid) 
                for (j=0;j<4;j++) newcell[i] |= ((cx^cy)&1 ? 0x01:0x02) << (8+2*j);
            if (x%8==4 && y%8<=4)
                newcell[i] |= (state->edge_h[cx+cy*w]         & (FLAG_PATH|cellerror));
            if (x%8==4 && y%8>=4)
                newcell[i] |= (state->edge_h[cx+(cy+1)*w]     & (FLAG_PATH|cellerror));
            if (y%8==4 && x%8<=4)
                newcell[i] |= (state->edge_v[cx+cy*(w+1)]     & (FLAG_PATH|cellerror));
            if (y%8==4 && x%8>=4)
                newcell[i] |= (state->edge_v[(cx+1)+cy*(w+1)] & (FLAG_PATH|cellerror));
            if (x%8==4 && y%8==4) {
                if ((state->edge_h[cx+cy*w]         & FLAG_PATH) > 0) newcell[i] |= FLAG_UP;
                if ((state->edge_h[cx+(cy+1)*w]     & FLAG_PATH) > 0) newcell[i] |= FLAG_DOWN;
                if ((state->edge_v[cx+cy*(w+1)]     & FLAG_PATH) > 0) newcell[i] |= FLAG_LEFT;
                if ((state->edge_v[(cx+1)+cy*(w+1)] & FLAG_PATH) > 0) newcell[i] |= FLAG_RIGHT;
                if ((state->cellstate[csx+csy*(w+2)] & FLAG_ERROR)>0) newcell[i] |= FLAG_ERROR;
            }
            if ((x%8==4 || y%8==4) && (newcell[i] & FLAG_PATH)>0) newcell[i] |= cellerror;
        }
    }
    for (i=0;i<((8*w)+5)*((8*h)+5); i++) {
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

