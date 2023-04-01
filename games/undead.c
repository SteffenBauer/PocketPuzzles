/*
 * undead: Implementation of Haunted Mirror Mazes
 *
 * (C) 2012-2015 Steffen Bauer & Simon Tatham
 * Created for Simon Tatham's Portable Puzzle Collection
 * See LICENCE for licence details
 *
 * http://www.janko.at/Raetsel/Spukschloss/index.htm
 *
 * Puzzle definition is the total number of each monster type, the
 * grid definition, and the list of sightings (clockwise, starting
 * from top left corner)
 *
 * Example: (Janko puzzle No. 1,
 * http://www.janko.at/Raetsel/Spukschloss/001.a.htm )
 *
 *   Ghosts: 0 Vampires: 2 Zombies: 6
 *
 *     2 1 1 1
 *   1 \ \ . / 2
 *   0 \ . / . 2
 *   0 / . / . 2
 *   3 . . . \ 2
 *     3 3 2 2
 *
 *  would be encoded into:
 *    4x4:0,2,6,LLaRLaRaRaRdL,2,1,1,1,2,2,2,2,2,2,3,3,3,0,0,1
 *
 *  Additionally, the game description can contain monsters fixed at a
 *  certain grid position. The internal generator does not use this feature,
 *  but this is needed to enter puzzles like Janko No. 14, which is encoded as:
 *    8x5:12,12,0,LaRbLaRaLaRLbRaVaVaGRaRaRaLbLaRbRLb,0,2,0,2,2,1,2,1,3,1,0,1,8,4,3,0,0,2,3,2,7,2,1,6,2,1
 *
 *  Furthermore, this version allows to have puzzles showing not all possible clues
 *  (choose the option 'strip clues' to generate one, similiar to the Magnets puzzle)
 *  An 'invisible' clue is encoded as a '-' in the puzzle specification.
 *  Example:
 *
 *   Ghosts: 2 Vampires: 2 Zombies: 3
 *
 *       2    
 *   4 . . \  
 *     . . . 2
 *   2 . / . 1
 *     2      
 *
 *  would be encoded into:
 *    3x3:2,2,3,bLdRa,-,2,-,-,2,1,-,-,2,2,-,4
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
    COL_TEXT,
    COL_ERROR,
    COL_ERROR_TEXT,
    COL_HIGHLIGHT,
    COL_GHOST,
    COL_ZOMBIE,
    COL_VAMPIRE,
    COL_DONE,
    NCOLOURS
};

#define DIFFLIST(A)                             \
    A(EASY,Easy,e)                              \
    A(NORMAL,Normal,n)                          \
    A(TRICKY,Tricky,t)                          \
    A(HARD,Hard,h)
#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const undead_diffnames[] = { DIFFLIST(TITLE) "(count)" };
static char const undead_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

struct game_params {
    int w;      /* Grid width */
    int h;      /* Grid height */
    int diff;   /* Puzzle difficulty */
    bool stripclues; /* Show only necessary clues */
};

static const struct game_params undead_presets[] = {
    {  4,  4, DIFF_EASY,   false },
    {  4,  4, DIFF_NORMAL, true },
    {  4,  4, DIFF_TRICKY, true },
    {  4,  4, DIFF_HARD,   true },
    {  5,  5, DIFF_NORMAL, true },
    {  5,  5, DIFF_TRICKY, true },
    {  5,  5, DIFF_HARD,   true },
    {  7,  7, DIFF_NORMAL, true },
    {  7,  7, DIFF_TRICKY, true }
};

#define DEFAULT_PRESET 2

static game_params *default_params(void) {
    game_params *ret = snew(game_params);

    *ret = undead_presets[DEFAULT_PRESET];
    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params) {
    game_params *ret;
    char buf[64];

    if (i < 0 || i >= lenof(undead_presets)) return false;

    ret = default_params();
    *ret = undead_presets[i]; /* struct copy */
    *params = ret;

    sprintf(buf, "%dx%d %s%s",
            undead_presets[i].w, undead_presets[i].h,
            undead_diffnames[undead_presets[i].diff],
            undead_presets[i].stripclues ? ", strip clues" : "");
    *name = dupstr(buf);

    return true;
}

static void free_params(game_params *params) {
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;            /* structure copy */
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

    params->diff = DIFF_NORMAL;
    if (*string == 'd') {
        int i;
        string++;
        for (i = 0; i < DIFFCOUNT; i++)
            if (*string == undead_diffchars[i])
                params->diff = i;
        if (*string) string++;
    }

    params->stripclues = false;
    if (*string == 'S') {
        string++;
        params->stripclues = true;
    }

    return;
}

static char *encode_params(const game_params *params, bool full)
{
    char buf[256];
    sprintf(buf, "%dx%d", params->w, params->h);
    if (full)
        sprintf(buf + strlen(buf), "d%c%s", undead_diffchars[params->diff],
        params->stripclues ? "S" : "");
    return dupstr(buf);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[64];

    ret = snewn(5, config_item);

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

    ret[3].name = "Strip clues";
    ret[3].type = C_BOOLEAN;
    ret[3].u.boolean.bval = params->stripclues;

    ret[4].name = NULL;
    ret[4].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->diff = cfg[2].u.choices.selected;
    ret->stripclues = cfg[3].u.boolean.bval;
    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if ((params->w * params->h ) > 54)  return "Grid is too big";
    if (params->w < 3)                  return "Width must be at least 3";
    if (params->h < 3)                  return "Height must be at least 3";
    if (params->diff >= DIFFCOUNT)      return "Unknown difficulty rating";
    return NULL;
}

/* --------------------------------------------------------------- */
/* Game state allocation, deallocation. */

struct path {
    int length;
    int *p;
    int grid_start;
    int grid_end;
    int num_monsters;
    int *mapping;
    int sightings_start;
    int sightings_end;
    int mirror_first;
    int mirror_last;
    int *xy;
};

struct game_common {
    int refcount;
    struct game_params params;
    int wh;
    int num_ghosts,num_vampires,num_zombies,num_total;
    int num_paths;
    struct path *paths;
    int *grid;
    int *xinfo;
    bool *fixed;
    bool contains_loop;
};

struct game_state {
    struct game_common *common;
    int *guess;
    int monster_counts[3];
    unsigned char *pencils;
    bool *cell_errors;
    bool *hint_errors;
    bool *hints_done;
    bool count_errors[3];
    bool solved;
    bool cheated;
};

static game_state *new_state(const game_params *params) {
    int i;
    game_state *state = snew(game_state);
    state->common = snew(struct game_common);

    state->common->refcount = 1;
    state->common->params.w = params->w;
    state->common->params.h = params->h;
    state->common->params.diff = params->diff;
    state->common->params.stripclues = params->stripclues;

    state->common->wh = (state->common->params.w +2) * (state->common->params.h +2);

    state->common->num_ghosts = 0;
    state->common->num_vampires = 0;
    state->common->num_zombies = 0;
    state->common->num_total = 0;

    state->common->grid = snewn(state->common->wh, int);
    state->common->xinfo = snewn(state->common->wh, int);
    state->common->fixed = NULL;
    state->common->contains_loop = false;

    state->common->num_paths =
        state->common->params.w + state->common->params.h;
    state->common->paths = snewn(state->common->num_paths, struct path);

    for (i=0;i<state->common->num_paths;i++) {
        state->common->paths[i].length = 0;
        state->common->paths[i].grid_start = -1;
        state->common->paths[i].grid_end = -1;
        state->common->paths[i].num_monsters = 0;
        state->common->paths[i].sightings_start = 0;
        state->common->paths[i].sightings_end = 0;
        state->common->paths[i].mirror_first = -1;
        state->common->paths[i].mirror_last = -1;
        state->common->paths[i].p = snewn(state->common->wh,int);
        state->common->paths[i].xy = snewn(state->common->wh,int);
        state->common->paths[i].mapping = snewn(state->common->wh,int);
    }

    state->guess = NULL;
    state->pencils = NULL;

    state->cell_errors = snewn(state->common->wh, bool);
    for (i=0;i<state->common->wh;i++)
        state->cell_errors[i] = false;
    state->hint_errors = snewn(2*state->common->num_paths, bool);
    for (i=0;i<2*state->common->num_paths;i++)
        state->hint_errors[i] = false;
    state->hints_done = snewn(2 * state->common->num_paths, bool);
    memset(state->hints_done, 0,
           2 * state->common->num_paths * sizeof(bool));
    for (i=0;i<3;i++) {
        state->count_errors[i] = false;
        state->monster_counts[i] = 0;
    }

    state->solved = false;
    state->cheated = false;

    return state;
}

static game_state *dup_game(const game_state *state)
{
    int i;
    game_state *ret = snew(game_state);

    ret->common = state->common;
    ret->common->refcount++;

    if (state->guess != NULL) {
        ret->guess = snewn(ret->common->num_total,int);
        memcpy(ret->guess, state->guess, ret->common->num_total*sizeof(int));
    }
    else ret->guess = NULL;

    if (state->pencils != NULL) {
        ret->pencils = snewn(ret->common->num_total,unsigned char);
        memcpy(ret->pencils, state->pencils,
               ret->common->num_total*sizeof(unsigned char));
    }
    else ret->pencils = NULL;

    if (state->cell_errors != NULL) {
        ret->cell_errors = snewn(ret->common->wh,bool);
        memcpy(ret->cell_errors, state->cell_errors,
               ret->common->wh*sizeof(bool));
    }
    else ret->cell_errors = NULL;

    if (state->hint_errors != NULL) {
        ret->hint_errors = snewn(2*ret->common->num_paths,bool);
        memcpy(ret->hint_errors, state->hint_errors,
               2*ret->common->num_paths*sizeof(bool));
    }
    else ret->hint_errors = NULL;

    if (state->hints_done != NULL) {
        ret->hints_done = snewn(2 * state->common->num_paths, bool);
        memcpy(ret->hints_done, state->hints_done,
               2 * state->common->num_paths * sizeof(bool));
    }
    else ret->hints_done = NULL;

    for (i=0;i<3;i++) {
        ret->count_errors[i] = state->count_errors[i];
        ret->monster_counts[i] = state->monster_counts[i];
    }

    ret->solved = state->solved;
    ret->cheated = state->cheated;

    return ret;
}

static void free_game(game_state *state) {
    int i;

    state->common->refcount--;
    if (state->common->refcount == 0) {
        for (i=0;i<state->common->num_paths;i++) {
            sfree(state->common->paths[i].mapping);
            sfree(state->common->paths[i].xy);
            sfree(state->common->paths[i].p);
        }
        sfree(state->common->paths);
        sfree(state->common->xinfo);
        sfree(state->common->grid);
        if (state->common->fixed != NULL) sfree(state->common->fixed);
        sfree(state->common);
    }
    if (state->hints_done != NULL) sfree(state->hints_done);
    if (state->hint_errors != NULL) sfree(state->hint_errors);
    if (state->cell_errors != NULL) sfree(state->cell_errors);
    if (state->pencils != NULL) sfree(state->pencils);
    if (state->guess != NULL) sfree(state->guess);
    sfree(state);

    return;
}

/* --------------------------------------------------------------- */
/* Puzzle generator */

/* cell states */
enum {
    CELL_EMPTY,
    CELL_MIRROR_L,
    CELL_MIRROR_R,
    CELL_GHOST,
    CELL_VAMPIRE,
    CELL_ZOMBIE,
    CELL_UNDEF
};

/* grid walk directions */
enum {
    DIRECTION_NONE,
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_LEFT,
    DIRECTION_DOWN
};

int start_guesses[8] = {0, 1, 2, 1, 4, 1, 2, 1};

static int range2grid(int rangeno, int width, int height, int *x, int *y) {

    if (rangeno < 0) {
        *x = 0; *y = 0; return DIRECTION_NONE;
    }
    if (rangeno < width) {
        *x = rangeno+1; *y = 0; return DIRECTION_DOWN;
    }
    rangeno = rangeno - width;
    if (rangeno < height) {
        *x = width+1; *y = rangeno+1; return DIRECTION_LEFT;
    }
    rangeno = rangeno - height;
    if (rangeno < width) {
        *x = width-rangeno; *y = height+1; return DIRECTION_UP;
    }
    rangeno = rangeno - width;
    if (rangeno < height) {
        *x = 0; *y = height-rangeno; return DIRECTION_RIGHT;
    }
    *x = 0; *y = 0;
    return DIRECTION_NONE;
}

static int grid2range(int x, int y, int w, int h) {
    if (x>0 && x<w+1 && y>0 && y<h+1)           return -1;
    if (x<0 || x>w+1 || y<0 || y>h+1)           return -1;
    if ((x == 0 || x==w+1) && (y==0 || y==h+1)) return -1;
    if (y==0)                                   return x-1;
    if (x==(w+1))                               return y-1+w;
    if (y==(h+1))                               return 2*w + h - x;
    return 2*(w+h) - y;
}

static void make_paths(game_state *state) {
    int i;
    int count = 0;

    for (i=0;i<2*(state->common->params.w + state->common->params.h);i++) {
        int x,y,dir;
        int j,k,num_monsters;
        bool found;
        int c,p; 
        int mirror_first, mirror_last;
        found = false;
        /* Check whether inverse path is already in list */
        for (j=0;j<count;j++) {
            if (i == state->common->paths[j].grid_end) {
                found = true;
                break;
            }
        }
        if (found) continue;

        /* We found a new path through the mirror maze */
        state->common->paths[count].grid_start = i;
        dir = range2grid(i, state->common->params.w,
                         state->common->params.h,&x,&y);
        state->common->paths[count].sightings_start =
            state->common->grid[x+y*(state->common->params.w +2)];
        mirror_first = mirror_last = -1;
        while (true) {
            int c,r;

            if      (dir == DIRECTION_DOWN)     y++;
            else if (dir == DIRECTION_LEFT)     x--;
            else if (dir == DIRECTION_UP)       y--;
            else if (dir == DIRECTION_RIGHT)    x++;

            r = grid2range(x, y, state->common->params.w,
                           state->common->params.h);
            if (r != -1) {
                state->common->paths[count].grid_end = r;
                state->common->paths[count].sightings_end =
                    state->common->grid[x+y*(state->common->params.w +2)];
                state->common->paths[count].mirror_first = mirror_first;
                state->common->paths[count].mirror_last = mirror_last;
                break;
            }

            c = state->common->grid[x+y*(state->common->params.w+2)];
            state->common->paths[count].xy[state->common->paths[count].length] =
                x+y*(state->common->params.w+2);
            if (c == CELL_MIRROR_L) {
                state->common->paths[count].p[state->common->paths[count].length] = -1;
                if (mirror_first == -1)
                    mirror_first = mirror_last =
                        state->common->paths[count].length;
                else mirror_last = state->common->paths[count].length;
                if (dir == DIRECTION_DOWN)          dir = DIRECTION_RIGHT;
                else if (dir == DIRECTION_LEFT)     dir = DIRECTION_UP;
                else if (dir == DIRECTION_UP)       dir = DIRECTION_LEFT;
                else if (dir == DIRECTION_RIGHT)    dir = DIRECTION_DOWN;
            }
            else if (c == CELL_MIRROR_R) {
                state->common->paths[count].p[state->common->paths[count].length] = -1;
                if (mirror_first == -1)
                    mirror_first = mirror_last =
                        state->common->paths[count].length;
                else mirror_last = state->common->paths[count].length;
                if (dir == DIRECTION_DOWN)          dir = DIRECTION_LEFT;
                else if (dir == DIRECTION_LEFT)     dir = DIRECTION_DOWN;
                else if (dir == DIRECTION_UP)       dir = DIRECTION_RIGHT;
                else if (dir == DIRECTION_RIGHT)    dir = DIRECTION_UP;
            }
            else {
                state->common->paths[count].p[state->common->paths[count].length] =
                    state->common->xinfo[x+y*(state->common->params.w+2)];
            }
            state->common->paths[count].length++;
        }
        /* Count unique monster entries in each path */
        state->common->paths[count].num_monsters = 0;
        for (j=0;j<state->common->num_total;j++) {
            num_monsters = 0;
            for (k=0;k<state->common->paths[count].length;k++)
                if (state->common->paths[count].p[k] == j)
                    num_monsters++;
            if (num_monsters > 0)
                state->common->paths[count].num_monsters++;
        }

        /* Generate mapping vector */
        c = 0;
        for (p=0;p<state->common->paths[count].length;p++) {
            int m;
            m = state->common->paths[count].p[p];
            if (m == -1) continue;
            found = false;
            for (j=0; j<c; j++)
                if (state->common->paths[count].mapping[j] == m) {
                    found = true;
                    state->common->contains_loop = true;
                }
            if (!found) state->common->paths[count].mapping[c++] = m;
        }
        count++;
    }
    return;
}

struct guess {
    int length;
    int *guess;
    int *possible;
};

static bool next_list(struct guess *g, int pos) {

    if (pos == 0) {
        if ((g->guess[pos] == 1 && g->possible[pos] == 1) ||
            (g->guess[pos] == 2 && (g->possible[pos] == 3 ||
                                    g->possible[pos] == 2)) ||
            g->guess[pos] == 4)
            return false;
        if (g->guess[pos] == 1 && (g->possible[pos] == 3 ||
                                   g->possible[pos] == 7)) {
            g->guess[pos] = 2; return true;
        }
        if (g->guess[pos] == 1 && g->possible[pos] == 5) {
            g->guess[pos] = 4; return true;
        }
        if (g->guess[pos] == 2 && (g->possible[pos] == 6 || g->possible[pos] == 7)) {
            g->guess[pos] = 4; return true;
        }
    }

    if (g->guess[pos] == 1) {
        if (g->possible[pos] == 1) {
            return next_list(g,pos-1);
        }
        if (g->possible[pos] == 3 || g->possible[pos] == 7) {
            g->guess[pos] = 2; return true;
        }
        if (g->possible[pos] == 5) {
            g->guess[pos] = 4; return true;
        }
    }

    if (g->guess[pos] == 2) {
        if (g->possible[pos] == 2) {
            return next_list(g,pos-1);
        }
        if (g->possible[pos] == 3) {
            g->guess[pos] = 1; return next_list(g,pos-1);
        }
        if (g->possible[pos] == 6 || g->possible[pos] == 7) {
            g->guess[pos] = 4; return true;
        }
    }

    if (g->guess[pos] == 4) {
        if (g->possible[pos] == 5 || g->possible[pos] == 7) {
            g->guess[pos] = 1; return next_list(g,pos-1);
        }
        if (g->possible[pos] == 6) {
            g->guess[pos] = 2; return next_list(g,pos-1);
        }
        if (g->possible[pos] == 4) {
            return next_list(g,pos-1);
        }
    }
    return false;
}

static void get_unique(game_state *state, int counter, random_state *rs) {

    int p,i,c,pathlimit,count_uniques;
    struct guess path_guess;
    int *view_count;

    struct entry {
        struct entry *link;
        int *guess;
        int start_view;
        int end_view;
    };

    struct {
        struct entry *head;
        struct entry *node;
    } views, single_views, test_views;

    struct entry test_entry;

    path_guess.length = state->common->paths[counter].num_monsters;
    path_guess.guess = snewn(path_guess.length,int);
    path_guess.possible = snewn(path_guess.length,int);
    for (i=0;i<path_guess.length;i++)
        path_guess.guess[i] = path_guess.possible[i] = 0;

    for (p=0;p<path_guess.length;p++) {
        path_guess.possible[p] =
            state->guess[state->common->paths[counter].mapping[p]];
        path_guess.guess[p] = start_guesses[path_guess.possible[p]];
    }

    views.head = NULL;
    views.node = NULL;

    pathlimit = state->common->paths[counter].length + 1;
    view_count = snewn(pathlimit*pathlimit, int);
    for (i = 0; i < pathlimit*pathlimit; i++)
        view_count[i] = 0;

    do {
        bool mirror;
        int start_view, end_view;

        mirror = false;
        start_view = 0;
        for (p=0;p<state->common->paths[counter].length;p++) {
            if (state->common->paths[counter].p[p] == -1) mirror = true;
            else {
                for (i=0;i<path_guess.length;i++) {
                    if (state->common->paths[counter].p[p] ==
                        state->common->paths[counter].mapping[i]) {
                        if (path_guess.guess[i] == 1 && mirror)
                            start_view++;
                        if (path_guess.guess[i] == 2 && !mirror)
                            start_view++;
                        if (path_guess.guess[i] == 4)
                            start_view++;
                        break;
                    }
                }
            }
        }
        mirror = false;
        end_view = 0;
        for (p=state->common->paths[counter].length-1;p>=0;p--) {
            if (state->common->paths[counter].p[p] == -1) mirror = true;
            else {
                for (i=0;i<path_guess.length;i++) {
                    if (state->common->paths[counter].p[p] ==
                        state->common->paths[counter].mapping[i]) {
                        if (path_guess.guess[i] == 1 && mirror)
                            end_view++;
                        if (path_guess.guess[i] == 2 && !mirror)
                            end_view++;
                        if (path_guess.guess[i] == 4)
                            end_view++;
                        break;
                    }
                }
            }
        }

        assert(start_view >= 0 && start_view < pathlimit);
        assert(end_view >= 0 && end_view < pathlimit);
        i = start_view * pathlimit + end_view;
        view_count[i]++;
        if (view_count[i] == 1) {
            views.node = snewn(1,struct entry);
            views.node->link = views.head;
            views.node->guess = snewn(path_guess.length,int);
            views.head = views.node;
            views.node->start_view = start_view;
            views.node->end_view = end_view;
            memcpy(views.node->guess, path_guess.guess,
                   path_guess.length*sizeof(int));
        }
    } while (next_list(&path_guess, path_guess.length-1));

    /*  extract single entries from view list */

    test_views.head = views.head;
    test_views.node = views.node;

    test_entry.guess = snewn(path_guess.length,int);

    single_views.head = NULL;
    single_views.node = NULL;

    count_uniques = 0;
    while (test_views.head != NULL) {
        test_views.node = test_views.head;
        test_views.head = test_views.head->link;
        i = test_views.node->start_view * pathlimit + test_views.node->end_view;
        if (view_count[i] == 1) {
            single_views.node = snewn(1,struct entry);
            single_views.node->link = single_views.head;
            single_views.node->guess = snewn(path_guess.length,int);
            single_views.head = single_views.node;
            single_views.node->start_view = test_views.node->start_view;
            single_views.node->end_view = test_views.node->end_view;
            memcpy(single_views.node->guess, test_views.node->guess,
                   path_guess.length*sizeof(int));
            count_uniques++;
        }
    }

    sfree(view_count);

    if (count_uniques > 0) {
        test_entry.start_view = 0;
        test_entry.end_view = 0;
        /* Choose one unique guess per random */
        /* While we are busy with looping through single_views, we
         * conveniently free the linked list single_view */
        c = random_upto(rs,count_uniques);
        while(single_views.head != NULL) {
            single_views.node = single_views.head;
            single_views.head = single_views.head->link;
            if (c-- == 0) {
                memcpy(test_entry.guess, single_views.node->guess,
                       path_guess.length*sizeof(int));
                test_entry.start_view = single_views.node->start_view;
                test_entry.end_view = single_views.node->end_view;
            }
            sfree(single_views.node->guess);
            sfree(single_views.node);
        }

        /* Modify state_guess according to path_guess.mapping */
        for (i=0;i<path_guess.length;i++)
            state->guess[state->common->paths[counter].mapping[i]] =
                test_entry.guess[i];
    }

    sfree(test_entry.guess);

    while (views.head != NULL) {
        views.node = views.head;
        views.head = views.head->link;
        sfree(views.node->guess);
        sfree(views.node);
    }

    sfree(path_guess.possible);
    sfree(path_guess.guess);

    return;
}

static int count_monsters(game_state *state,
                          int *cGhost, int *cVampire, int *cZombie) {
    int cNone;
    int i;

    *cGhost = *cVampire = *cZombie = cNone = 0;

    for (i=0;i<state->common->num_total;i++) {
        if (state->guess[i] == 1) (*cGhost)++;
        else if (state->guess[i] == 2) (*cVampire)++;
        else if (state->guess[i] == 4) (*cZombie)++;
        else cNone++;
    }

    return cNone;
}

static bool check_numbers(game_state *state, int *guess) {
    bool valid;
    int i;
    int count_ghosts, count_vampires, count_zombies;

    count_ghosts = count_vampires = count_zombies = 0;
    for (i=0;i<state->common->num_total;i++) {
        if (guess[i] == 1) count_ghosts++;
        if (guess[i] == 2) count_vampires++;
        if (guess[i] == 4) count_zombies++;
    }

    valid = true;

    if (count_ghosts   > state->common->num_ghosts)   valid = false;
    if (count_vampires > state->common->num_vampires) valid = false;
    if (count_zombies > state->common->num_zombies)   valid = false;

    return valid;
}

static bool check_solution(int *g, struct path path) {
    int i;
    bool mirror;
    int count;

    count = 0;
    mirror = false;
    for (i=0;i<path.length;i++) {
        if (path.p[i] == -1) mirror = true;
        else {
            if (g[path.p[i]] == 1 && mirror) count++;
            else if (g[path.p[i]] == 2 && !mirror) count++;
            else if (g[path.p[i]] == 4) count++;
        }
    }
    if (path.sightings_start >= 0 && count != path.sightings_start) return false;

    count = 0;
    mirror = false;
    for (i=path.length-1;i>=0;i--) {
        if (path.p[i] == -1) mirror = true;
        else {
            if (g[path.p[i]] == 1 && mirror) count++;
            else if (g[path.p[i]] == 2 && !mirror) count++;
            else if (g[path.p[i]] == 4) count++;
        }
    }
    if (path.sightings_end >= 0 && count != path.sightings_end) return false;

    return true;
}

static bool solve_iterative(game_state *state, int *current_guess, int *path_counts) {
    bool solved;
    int p,i,j,count;

    int *guess;
    int *possible;

    struct guess loop;

    solved = true;
    loop.length = state->common->num_total;
    guess = snewn(state->common->num_total,int);
    possible = snewn(state->common->num_total,int);

    for (i=0;i<state->common->num_total;i++) {
        guess[i] = current_guess[i];
        possible[i] = 0;
    }

    for (p=0;p<state->common->num_paths;p++) {
        if (state->common->paths[p].num_monsters > 0) {
            loop.length = state->common->paths[p].num_monsters;
            loop.guess = snewn(state->common->paths[p].num_monsters,int);
            loop.possible = snewn(state->common->paths[p].num_monsters,int);

            for (i=0;i<state->common->paths[p].num_monsters;i++) {
                loop.guess[i] = 
                    start_guesses[current_guess[state->common->paths[p].mapping[i]]];
                loop.possible[i] = current_guess[state->common->paths[p].mapping[i]];
                possible[state->common->paths[p].mapping[i]] = 0;
            }

            while(true) {
                for (i=0;i<state->common->num_total;i++) {
                    guess[i] = current_guess[i];
                }
                count = 0;
                for (i=0;i<state->common->paths[p].num_monsters;i++)
                    guess[state->common->paths[p].mapping[i]] =
                        loop.guess[count++];
                if (check_numbers(state,guess) &&
                    check_solution(guess,state->common->paths[p])) {
                    bool mirror_first = false;
                    bool mirror_last = false;
                    int elem,ix;
                    int count_before[3] = {0,0,0};
                    int count_middle[3] = {0,0,0};
                    int count_after[3] = {0,0,0};

                    for (j=0;j<state->common->paths[p].num_monsters;j++)
                        possible[state->common->paths[p].mapping[j]] |=
                            loop.guess[j];

                    for (j=0;j<state->common->paths[p].length;j++) {
                        if (state->common->paths[p].p[j] == -1) {
                            if (!mirror_first)
                                mirror_first = true;
                            else if (j >= state->common->paths[p].mirror_last)
                                mirror_last = true;
                            continue;
                        }
                        elem = guess[state->common->paths[p].p[j]];
                        ix = (elem == 1) ? 0 : (elem == 2) ? 1 : 2;
                        if (!mirror_first)
                            count_before[ix] += 1;
                        else if (mirror_first && !mirror_last)
                            count_middle[ix] += 1;
                        else
                            count_after[ix] += 1;
                    }

                    for (j=0;j<3;j++) {
                        if (path_counts[0+0+6*j+18*p] > count_before[j])
                            path_counts[0+0+6*j+18*p] = count_before[j];
                        if (path_counts[0+2+6*j+18*p] > count_middle[j])
                            path_counts[0+2+6*j+18*p] = count_middle[j];
                        if (path_counts[0+4+6*j+18*p] > count_after[j])
                            path_counts[0+4+6*j+18*p] = count_after[j];

                        if (path_counts[1+0+6*j+18*p] < count_before[j])
                            path_counts[1+0+6*j+18*p] = count_before[j];
                        if (path_counts[1+2+6*j+18*p] < count_middle[j])
                            path_counts[1+2+6*j+18*p] = count_middle[j];
                        if (path_counts[1+4+6*j+18*p] < count_after[j])
                            path_counts[1+4+6*j+18*p] = count_after[j];
                    }
                }
                if (!next_list(&loop,loop.length-1)) break;
            }
            for (i=0;i<state->common->paths[p].num_monsters;i++)
                current_guess[state->common->paths[p].mapping[i]] &=
                    possible[state->common->paths[p].mapping[i]];
            sfree(loop.possible);
            sfree(loop.guess);
        }
    }

    for (i=0;i<state->common->num_total;i++) {
        if (current_guess[i] == 3 || current_guess[i] == 5 ||
            current_guess[i] == 6 || current_guess[i] == 7) {
            solved = false; break;
        }
    }

    sfree(possible);
    sfree(guess);

    return solved;
}

static bool solve_combinations(game_state *state, int *current_guess,
                       int *path_counts) {

    const int check_var[3][3] = {{3,5,7}, {3,6,7}, {5,6,7}};
    const int check_fixed[3] = {1,2,4};

    int t,i,j,p;
    int *var_guess;
    int *monsters_fixed;
    int *monsters_variable;
    int *entries;
    int *var_entries;
    int num_fixed, num_unclear, num_check, num_var;
    bool solved = true;
    bool valid;

    var_guess = snewn(state->common->num_total,int);
    monsters_fixed = snewn(state->common->num_total,int);
    monsters_variable = snewn(state->common->num_total,int);
    for (i=0;i<state->common->num_total;i++) var_guess[i] = 0;

    for (t=0;t<3;t++) {
        num_fixed = num_unclear = 0;
        for (i=0;i<state->common->num_total;i++) {
            if (current_guess[i] == check_fixed[t])
                monsters_fixed[num_fixed++] = i;
            else for (j=0;j<3;j++)
                if (current_guess[i] == check_var[t][j])
                    monsters_variable[num_unclear++] = i;
        }

        /* All monsters of a certain type are already found. Nothing to do. */
        if (num_unclear == 0) continue;

        /* Check if all remaining ambiguous types of a monster type
         * satisfy only one possible combination */
        switch(t) {
            case 0: num_check = state->common->num_ghosts   - num_fixed; break;
            case 1: num_check = state->common->num_vampires - num_fixed; break;
            case 2: num_check = state->common->num_zombies  - num_fixed; break;
        }

        /* Special case: All monsters of a type are found, but there are
         * still ambiguous fields. (This can happen in grids with an
         * internal loop) Remove all corresponding ambiguities */
        if (num_check == 0) {
            for (i=0;i<state->common->num_total;i++)
                for (j=0;j<3;j++)
                    if (current_guess[i] == check_var[t][j])
                        current_guess[i] -= check_fixed[t];
            continue;
        }

        /* All remaining ambiguous fields happen to be exactly the number
         * of missing monsters of a type. Set them to that type. */
        if (num_check == num_unclear) {
            for (i=0;i<state->common->num_total;i++) {
                for (j=0;j<3;j++)
                    if (current_guess[i] == check_var[t][j])
                        current_guess[i] = var_guess[i] = check_fixed[t];
            }
            continue;
        }

        /* Check all possible combinations for a certain monster type.
         * Keep only those consistent with the max/min info in paths */
        entries = snewn(num_check, int);
        for (i=0;i<num_check;i++) entries[i] = i;
        num_var = num_fixed + num_check;
        var_entries = snewn(num_var,int);
        while (true) {
            for (i=0;i<num_fixed;i++)
                var_entries[i] = monsters_fixed[i];
            for (i=0;i<num_check;i++)
                var_entries[num_fixed+i] = monsters_variable[entries[i]];
            valid = true;
            for (p=0;p<state->common->num_paths;p++) {
                int count[3] = {0,0,0};
                bool mirror_first = false;
                bool mirror_last = false;

                if (state->common->paths[p].length == 0) continue;
                for (i=0;i<state->common->paths[p].length;i++) {
                    if (state->common->paths[p].p[i] == -1) {
                        if (!mirror_first) mirror_first = true;
                        else if (i >= state->common->paths[p].mirror_last)
                                mirror_last = true;
                        continue;
                    }
                    for (j=0;j<num_var;j++)
                        if (state->common->paths[p].p[i] == var_entries[j]) {
                            if (!mirror_first) count[0] += 1;
                            else if (mirror_first && !mirror_last)
                                     count[1] += 1;
                            else count[2] += 1;
                            break;
                        }
                }
                if (path_counts[0+0+6*t+18*p] > count[0]) valid = false;
                if (path_counts[0+2+6*t+18*p] > count[1]) valid = false;
                if (path_counts[0+4+6*t+18*p] > count[2]) valid = false;

                if (path_counts[1+0+6*t+18*p] < count[0]) valid = false;
                if (path_counts[1+2+6*t+18*p] < count[1]) valid = false;
                if (path_counts[1+4+6*t+18*p] < count[2]) valid = false;

                if (!valid) break;
            }
            if (valid) for (i=num_fixed;i<num_var;i++)
                var_guess[var_entries[i]] |= check_fixed[t];

            if (entries[num_check-1] < num_unclear - 1)
                entries[num_check-1] += 1;
            else {
                j = num_check-1;
                while (j >= 0 && entries[j] == num_unclear - num_check + j)
                    j -= 1;
                if (j < 0) break;
                entries[j] += 1;
                for (i=j+1;i<num_check;i++)
                    entries[i] = entries[i-1]+1;
            }
        }
        sfree(var_entries);
        sfree(entries);
    }

    for (i=0;i<state->common->num_total;i++) {
        if (var_guess[i] > 0)
            current_guess[i] = var_guess[i];
    }

    sfree(monsters_variable);
    sfree(monsters_fixed);
    sfree(var_guess);

    for (i=0;i<state->common->num_total;i++) {
        if (current_guess[i] == 3 || current_guess[i] == 5 ||
            current_guess[i] == 6 || current_guess[i] == 7) {
            solved = false; break;
        }
    }

    return solved;
}

static bool solve_bruteforce(game_state *state, int *current_guess) {
    bool solved, correct;
    int number_solutions;
    int p,i;

    struct guess loop;

    loop.guess = snewn(state->common->num_total,int);
    loop.possible = snewn(state->common->num_total,int);

    for (i=0;i<state->common->num_total;i++) {
        loop.possible[i] = current_guess[i];
        loop.guess[i] = start_guesses[current_guess[i]];
    }

    solved = false;
    number_solutions = 0;

    while (true) {

        correct = true;
        if (!check_numbers(state,loop.guess)) correct = false;
        else
            for (p=0;p<state->common->num_paths;p++)
                if (!check_solution(loop.guess,state->common->paths[p])) {
                    correct = false; break;
                }
        if (correct) {
            number_solutions++;
            solved = true;
            if(number_solutions > 1) {
                solved = false;
                break;
            }
            for (i=0;i<state->common->num_total;i++)
                current_guess[i] = loop.guess[i];
        }
        if (!next_list(&loop,state->common->num_total -1)) {
            break;
        }
    }

    sfree(loop.possible);
    sfree(loop.guess);

    return solved;
}

struct solution {
    bool solved_iterative;
    bool solved_combinative;
    bool solved_bruteforce;
    int iterative_depth;
    int combinative_depth;
    int num_ambiguous;
    bool contains_inconsistency;
    int *puzzle_solution;
    int *path_counts;
};

/* Solver:
 *
 *  We fill all empty grid cells with initial guesses (G,V,Z).
 *  The different solvers then try to reduce grid cell guesses until
 *  all grid cells contain a unique monster, or the guesses cannot
 *  get reduced anymore.
 *
 * 1) The Iterative solver removes all impossible entries for each path
 *    through the mirror maze.
 *
 *    Example:
 *              1 . 1
 *            . \ x / .
 *              . . .
 *
 *    The iterative solver reduces here the initial guess (G,V,Z)
 *    to (G,Z) for the grid cell x.
 *
 *    The iterative solver also counts all min/max nums of monster types
 *    in the areas start / middle / end of each mirror path through the maze
 *    (needed for the combinative solver)
 *
 * 2) The Combinative solver removes all globally impossible monster placings.
 *
 *    Example: G:3 V:2 Z:2
 *
 *                . . 2
 *              . V / w .
 *              1 G x y 1
 *              . G \ z .
 *                . . 2
 *
 *    Two ghosts are already found, one ghost is left to be placed.
 *
 *    Both the second row and the third column need an additional ghost.
 *    It is only possible to do so overall by placing a ghost into grid cell y.
 *
 *    The combinative solver would reduce here the guess (G,V,Z)
 *    to (V,Z) for the grid cells w,x and z, and would place a ghost into y.
 *
 * 3) The Brute-Force solver tries all possible remaining possibilities.
 *
 * We first apply the iterative solver repeatedly until it cannot reduce
 * puzzle_solution[] further.
 * Afterwards the combinative solver tries to reduce puzzle_solution[].
 * We continue to do so until both the iterative and the combinative solver
 * cannot reduce any grid cell guesses further.
 * If the puzzle is still not solved, the brute-force solver tries to find
 * a unique solution.
 *
*/

static bool solve(game_state *state, struct solution *sol) {
    int *old_guess;
    bool no_change;
    int p;

    /* Initialize guess structures */
    old_guess = snewn(state->common->num_total,int);
    for (p=0;p<state->common->num_total;p++) {
        if (state->common->fixed[p]) {
            sol->puzzle_solution[p] = state->guess[p];
            old_guess[p] = state->guess[p];
        }
        else {
            sol->puzzle_solution[p] = 7;
            old_guess[p] = 7;
        }
    }

   sol->path_counts = snewn(state->common->num_paths*18,int);
   for (p=0;p<state->common->num_paths;p++) {
        int monster,position;
        for (monster=0;monster<3;monster++)
        for (position=0;position<3;position++) {
            sol->path_counts[0+position*2+monster*2*3+p*2*3*3] =
                (state->common->paths[p].num_monsters > 0) ? 1000 : 0;
            sol->path_counts[1+position*2+monster*2*3+p*2*3*3] = 0;
        }
    }

    sol->iterative_depth = 0;
    sol->combinative_depth = 0;
    sol->num_ambiguous = 0;
    sol->solved_iterative = false;
    sol->solved_combinative = false;
    sol->solved_bruteforce = false;
    sol->contains_inconsistency = false;

    /* Solver starts here */
    while (true) {
        while (true) {
            no_change = true;
            sol->solved_iterative =
                solve_iterative(state,sol->puzzle_solution,sol->path_counts);
            for (p=0;p<state->common->num_total;p++) {
                if (sol->puzzle_solution[p] != old_guess[p]) no_change = false;
                    old_guess[p] = sol->puzzle_solution[p];
                    if (sol->puzzle_solution[p] == 0)
                        sol->contains_inconsistency = true;
                }
                if (!no_change) sol->iterative_depth++;
                if (sol->solved_iterative || no_change) break;
            }
            if (sol->solved_iterative) break;

            if (state->common->params.diff == DIFF_TRICKY ||
                state->common->params.diff == DIFF_HARD) {
                no_change = true;
                if (!sol->solved_iterative && !sol->contains_inconsistency) {
                    sol->solved_combinative =
                        solve_combinations(state, sol->puzzle_solution,
                                           sol->path_counts);
                }
                for (p=0;p<state->common->num_total;p++) {
                    if (sol->puzzle_solution[p] != old_guess[p])
                        no_change = false;
                    old_guess[p] = sol->puzzle_solution[p];
                    if (sol->puzzle_solution[p] == 0)
                        sol->contains_inconsistency = true;
                }
                if (!no_change) sol->combinative_depth++;
                if (sol->solved_combinative) break;

                no_change = true;
                sol->solved_iterative =
                    solve_iterative(state, sol->puzzle_solution,
                                    sol->path_counts);
                for (p=0;p<state->common->num_total;p++) {
                    if (sol->puzzle_solution[p] != old_guess[p])
                        no_change = false;
                    old_guess[p] = sol->puzzle_solution[p];
                    if (sol->puzzle_solution[p] == 0)
                        sol->contains_inconsistency = true;
                }
            }
            if (no_change) break;
            if (sol->solved_iterative || sol->solved_combinative) break;
            if (!no_change) sol->iterative_depth += 1;
        }
        if ((sol->solved_iterative || sol->solved_combinative) &&
             sol->combinative_depth > 0)
             sol->solved_combinative = true;

        /* If necessary, try to solve the puzzle with the brute-force solver */
        if (state->common->params.diff == DIFF_HARD) {
            sol->solved_bruteforce = false;
            if (!sol->solved_iterative && !sol->solved_combinative &&
                !sol->contains_inconsistency) {
                for (p=0;p<state->common->num_total;p++)
                    if (sol->puzzle_solution[p] != 1 &&
                        sol->puzzle_solution[p] != 2 &&
                        sol->puzzle_solution[p] != 4)
                        sol->num_ambiguous++;
                if (sol->num_ambiguous <= 12)
                    sol->solved_bruteforce =
                        solve_bruteforce(state,sol->puzzle_solution);
                else
                    sol->solved_bruteforce = false;
            }
        }

        sfree(sol->path_counts);
        sfree(old_guess);
        if (sol->contains_inconsistency) return false;
        return (sol->solved_iterative || sol->solved_combinative || sol->solved_bruteforce);
}

static int determine_difficulty(game_state *new, struct solution sol) {
    if ( sol.solved_iterative &&
        !sol.solved_combinative &&
        !sol.solved_bruteforce &&
         sol.iterative_depth < 2 &&
        !new->common->contains_loop &&
        !sol.contains_inconsistency) {
            return DIFF_EASY;
    }
    if ( sol.solved_iterative &&
        !sol.solved_combinative &&
        !sol.solved_bruteforce &&
         sol.iterative_depth > 1 &&
        !new->common->contains_loop &&
        !sol.contains_inconsistency) {
            return DIFF_NORMAL;
    }
    if ( sol.solved_combinative &&
        !sol.solved_bruteforce &&
        !sol.contains_inconsistency) {
            return DIFF_TRICKY;
    }
    if ( sol.solved_bruteforce &&
        !sol.contains_inconsistency) {
            return DIFF_HARD;
    }
    return 1000;
}

static void remove_clues(game_state *new, struct solution *sol, random_state *rs) {
    int p, x, y;
    int *clues;

    /* Remove all '0' hints from paths with no monsters */
    for (p=0;p<new->common->num_paths;p++) {
        if (new->common->paths[p].num_monsters  == 0) {
            range2grid(new->common->paths[p].grid_start,new->common->params.w,new->common->params.h,&x,&y);
            new->common->grid[x+y*(new->common->params.w+2)] = -1;

            range2grid(new->common->paths[p].grid_end,new->common->params.w,new->common->params.h,&x,&y);
            new->common->grid[x+y*(new->common->params.w+2)] = -1;

            new->common->paths[p].sightings_start = -1;
            new->common->paths[p].sightings_end = -1;
        }
    }

    /* Remove hints until puzzle becomes impossible or too difficult */
    sol->puzzle_solution = snewn(new->common->num_total,int);
    clues = snewn(2*new->common->num_paths, int);
    for (p=0;p<2*new->common->num_paths;p++)
        clues[p] = p;

    shuffle(clues, 2*new->common->num_paths, sizeof(int), rs);
    for (p=0;p<2*new->common->num_paths;p++) {
        int save_clue;
        int c = clues[p];
        if (c % 2 == 0) {
            save_clue = new->common->paths[c/2].sightings_start;
            new->common->paths[c/2].sightings_start = -1;
            range2grid(new->common->paths[c/2].grid_start,new->common->params.w,new->common->params.h,&x,&y);
            new->common->grid[x+y*(new->common->params.w+2)] = -1;
        }
        else {
            save_clue = new->common->paths[c/2].sightings_end;
            new->common->paths[c/2].sightings_end = -1;
            range2grid(new->common->paths[c/2].grid_end,new->common->params.w,new->common->params.h,&x,&y);
            new->common->grid[x+y*(new->common->params.w+2)] = -1;
        }

        solve(new, sol);

        if (determine_difficulty(new, *sol) > new->common->params.diff) {

            if (c % 2 == 0) {
                new->common->paths[c/2].sightings_start = save_clue;
                range2grid(new->common->paths[c/2].grid_start,new->common->params.w,new->common->params.h,&x,&y);
                new->common->grid[x+y*(new->common->params.w+2)] = save_clue;
            }
            else {
                new->common->paths[c/2].sightings_end = save_clue;
                range2grid(new->common->paths[c/2].grid_end,new->common->params.w,new->common->params.h,&x,&y);
                new->common->grid[x+y*(new->common->params.w+2)] = save_clue;
            }

        }

    }

    sfree(clues);
    solve(new, sol);

    sfree(sol->puzzle_solution);
    return;
}

static int path_cmp(const void *a, const void *b) {
    const struct path *pa = (const struct path *)a;
    const struct path *pb = (const struct path *)b;
    return pa->num_monsters - pb->num_monsters;
}

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive) {
    int i,count,c,w,h,r,p,g;
    game_state *new;

    /* Variables for puzzle generation algorithm */
    int filling;
    int max_length;
    int count_ghosts, count_vampires, count_zombies;
    bool abort;
    float ratio;

    /* Variables structure for solver algorithm */
    struct solution sol;

    /* Variables for game description generation */
    int x,y;
    char *e;
    char *desc;

    i = 0;
    while (true) {
        new = new_state(params);
        abort = false;

        /* Fill grid with random mirrors and (later to be populated)
         * empty monster cells */
        count = 0;
        for (h=1;h<new->common->params.h+1;h++)
            for (w=1;w<new->common->params.w+1;w++) {
                c = random_upto(rs,5);
                if (c >= 2) {
                    new->common->grid[w+h*(new->common->params.w+2)] = CELL_EMPTY;
                    new->common->xinfo[w+h*(new->common->params.w+2)] = count++;
                }
                else if (c == 0) {
                    new->common->grid[w+h*(new->common->params.w+2)] =
                        CELL_MIRROR_L;
                    new->common->xinfo[w+h*(new->common->params.w+2)] = -1;
                }
                else {
                    new->common->grid[w+h*(new->common->params.w+2)] =
                        CELL_MIRROR_R;
                    new->common->xinfo[w+h*(new->common->params.w+2)] = -1;
                }
            }
        new->common->num_total = count; /* Total number of monsters in maze */

        /* Puzzle is boring if it has too few monster cells. Discard
         * grid, make new grid */
        if (new->common->num_total <= 4) {
            free_game(new);
            continue;
        }

        /* Monsters / Mirrors ratio should be balanced */
        ratio = (float)new->common->num_total /
            (float)(new->common->params.w * new->common->params.h);
        if (ratio < 0.48 || ratio > 0.78) {
            free_game(new);
            continue;
        }

        /* Assign clue identifiers */
        for (r=0;r<2*(new->common->params.w+new->common->params.h);r++) {
            int x,y,gridno;
            gridno = range2grid(r,new->common->params.w,new->common->params.h,
                                &x,&y);
            new->common->grid[x+y*(new->common->params.w +2)] = gridno;
            new->common->xinfo[x+y*(new->common->params.w +2)] = 0;
        }
        /* The four corners don't matter at all for the game. Set them
         * all to zero, just to have a nice data structure */
        new->common->grid[0] = 0;
        new->common->xinfo[0] = 0;
        new->common->grid[new->common->params.w+1] = 0;
        new->common->xinfo[new->common->params.w+1] = 0;
        new->common->grid[new->common->params.w+1 + (new->common->params.h+1)*(new->common->params.w+2)] = 0;
        new->common->xinfo[new->common->params.w+1 + (new->common->params.h+1)*(new->common->params.w+2)] = 0;
        new->common->grid[(new->common->params.h+1)*(new->common->params.w+2)] = 0;
        new->common->xinfo[(new->common->params.h+1)*(new->common->params.w+2)] = 0;

        /* Initialize solution vector */
        new->guess = snewn(new->common->num_total,int);
        for (g=0;g<new->common->num_total;g++) new->guess[g] = 7;

        /* Initialize fixed flag from common. Not needed for the
         * puzzle generator; initialize it for having clean code */
        new->common->fixed = snewn(new->common->num_total, bool);
        for (g=0;g<new->common->num_total;g++)
            new->common->fixed[g] = false;

        /* paths generation */
        make_paths(new);

        /* Easy/Normal games should not contain a loop (a grid position is seen twice) */
        if (new->common->params.diff <= DIFF_NORMAL &&
            new->common->contains_loop) {
            free_game(new);
            continue;
        }

        /* Grid is invalid if max. path length > threshold. Discard
         * grid, make new one */
        switch (new->common->params.diff) {
          case DIFF_EASY:     max_length = min(new->common->params.w,new->common->params.h) + 2; break;
          case DIFF_NORMAL:   max_length = (max(new->common->params.w,new->common->params.h) * 3) / 2; break;
          case DIFF_TRICKY:   max_length = 10; break;
          case DIFF_HARD:     max_length = 10; break;
          default:            max_length = 10; break;
        }

        for (p=0;p<new->common->num_paths;p++) {
            if (new->common->paths[p].num_monsters > max_length) {
                abort = true;
            }
        }
        if (abort) {
            free_game(new);
            continue;
        }

        qsort(new->common->paths, new->common->num_paths,
              sizeof(struct path), path_cmp);

        /* Grid monster initialization */

        /* For easy puzzles, we try to fill nearly the whole grid
         * with unique solution paths (up to 2)
         * We fill half the empty cells of normal puzzles
         * with unique solutions.
         * Overall, we allow max. 16 ambiguous grid cells
         * (hard puzzles might need too long to generate otherwise) */

        switch (new->common->params.diff) {
          case DIFF_EASY:   filling = 2; break;
          case DIFF_NORMAL: filling = new->common->num_total / 2 < 16 ? new->common->num_total : 16; break;
          case DIFF_TRICKY: filling = 16; break;
          case DIFF_HARD:   filling = 16; break;
          default:          filling = 16; break;
        }

        count = 0;
        while ( (count_monsters(new, &count_ghosts, &count_vampires,
                                &count_zombies)) > filling) {
            if ((count) >= new->common->num_paths) break;
            if (new->common->paths[count].num_monsters == 0) {
                count++;
                continue;
            }
            get_unique(new,count,rs);
            count++;
        }

        /* Fill any remaining ambiguous entries with random monsters */
        for(g=0;g<new->common->num_total;g++) {
            if (new->guess[g] == 7) {
                r = random_upto(rs,3);
                new->guess[g] = (r == 0) ? 1 : ( (r == 1) ? 2 : 4 );
            }
        }

        /*  Determine all hints */
        count_monsters(new, &new->common->num_ghosts,
                       &new->common->num_vampires, &new->common->num_zombies);

        /* Puzzle is trivial if it has only one type of monster. Discard. */
        if ((new->common->num_ghosts == 0 && new->common->num_vampires == 0) ||
            (new->common->num_ghosts == 0 && new->common->num_zombies == 0) ||
            (new->common->num_vampires == 0 && new->common->num_zombies == 0)) {
                free_game(new);
                continue;
        }

        /* Discard puzzle if difficulty Tricky or Hard, and it has only 1 or 0
         * members of any monster type */
        if ((new->common->params.diff == DIFF_TRICKY ||
             new->common->params.diff == DIFF_HARD) &&
            (new->common->num_ghosts <= 1 ||
             new->common->num_vampires <= 1 ||
             new->common->num_zombies <= 1) ) {
                free_game(new);
                continue;
        }

        for (w=1;w<new->common->params.w+1;w++)
            for (h=1;h<new->common->params.h+1;h++) {
                c = new->common->xinfo[w+h*(new->common->params.w+2)];
                if (c >= 0) {
                    if (new->guess[c] == 1) new->common->grid[w+h*(new->common->params.w+2)] = CELL_GHOST;
                    if (new->guess[c] == 2) new->common->grid[w+h*(new->common->params.w+2)] = CELL_VAMPIRE;
                    if (new->guess[c] == 4) new->common->grid[w+h*(new->common->params.w+2)] = CELL_ZOMBIE;
                }
            }

        /* Prepare path information needed by the solver (containing all hints) */
        for (p=0;p<new->common->num_paths;p++) {
            bool mirror;
            int x,y;

            new->common->paths[p].sightings_start = 0;
            new->common->paths[p].sightings_end = 0;

            mirror = false;
            for (g=0;g<new->common->paths[p].length;g++) {

                if (new->common->paths[p].p[g] == -1) mirror = true;
                else {
                    if      (new->guess[new->common->paths[p].p[g]] == 1 && mirror)  (new->common->paths[p].sightings_start)++;
                    else if (new->guess[new->common->paths[p].p[g]] == 2 && !mirror) (new->common->paths[p].sightings_start)++;
                    else if (new->guess[new->common->paths[p].p[g]] == 4)            (new->common->paths[p].sightings_start)++;
                }
            }

            mirror = false;
            for (g=new->common->paths[p].length-1;g>=0;g--) {
                if (new->common->paths[p].p[g] == -1) mirror = true;
                else {
                    if      (new->guess[new->common->paths[p].p[g]] == 1 && mirror)  (new->common->paths[p].sightings_end)++;
                    else if (new->guess[new->common->paths[p].p[g]] == 2 && !mirror) (new->common->paths[p].sightings_end)++;
                    else if (new->guess[new->common->paths[p].p[g]] == 4)            (new->common->paths[p].sightings_end)++;
                }
            }

            range2grid(new->common->paths[p].grid_start,
                       new->common->params.w,new->common->params.h,&x,&y);
            new->common->grid[x+y*(new->common->params.w +2)] =
                new->common->paths[p].sightings_start;
            range2grid(new->common->paths[p].grid_end,
                       new->common->params.w,new->common->params.h,&x,&y);
            new->common->grid[x+y*(new->common->params.w +2)] =
                new->common->paths[p].sightings_end;
        }

        /* Try to solve the puzzle */
        sol.puzzle_solution = snewn(new->common->num_total,int);
        solve(new, &sol);
        sfree(sol.puzzle_solution);

        if (new->common->params.diff != determine_difficulty(new, sol)) {
            free_game(new);
            i++;
            continue;
        }

        if (new->common->params.stripclues) remove_clues(new, &sol, rs);

        /*  Determine puzzle difficulty level */
        if (new->common->params.diff == determine_difficulty(new, sol))
            break;

        /* If puzzle is not solvable or does not satisfy the desired
         * difficulty level, free memory and start from scratch */
        free_game(new);
        i++;
    }

    /* We have a valid puzzle! */

    desc = snewn(10 + new->common->wh +
                 6*(new->common->params.w + new->common->params.h), char);
    e = desc;

    /* Encode monster counts */
    e += sprintf(e, "%d,", new->common->num_ghosts);
    e += sprintf(e, "%d,", new->common->num_vampires);
    e += sprintf(e, "%d,", new->common->num_zombies);

    /* Encode grid */
    count = 0;
    for (y=1;y<new->common->params.h+1;y++)
        for (x=1;x<new->common->params.w+1;x++) {
            c = new->common->grid[x+y*(new->common->params.w+2)];
            if (count > 25) {
                *e++ = 'z';
                count -= 26;
            }
            if (c != CELL_MIRROR_L && c != CELL_MIRROR_R) {
                count++;
            }
            else if (c == CELL_MIRROR_L) {
                if (count > 0) *e++ = (count-1 + 'a');
                *e++ = 'L';
                count = 0;
            }
            else {
                if (count > 0) *e++ = (count-1 + 'a');
                *e++ = 'R';
                count = 0;
            }
        }
    if (count > 0) *e++ = (count-1 + 'a');

    /* Encode hints */
    for (p=0;p<2*(new->common->params.w + new->common->params.h);p++) {
        int hint;
        range2grid(p,new->common->params.w,new->common->params.h,&x,&y);
        hint = new->common->grid[x+y*(new->common->params.w+2)];
        if (hint >= 0) e += sprintf(e, ",%d", hint);
        else           e += sprintf(e, ",-");
    }

    *e++ = '\0';
    desc = sresize(desc, e - desc, char);

    free_game(new);

    return desc;
}

static void num2grid(int num, int width, int height, int *x, int *y) {
    *x = 1+(num%width);
    *y = 1+(num/width);
    return;
}

static key_label *game_request_keys(const game_params *params, int *nkeys)
{
    key_label *keys = snewn(5, key_label);
    *nkeys = 5;

    keys[0].button = 'G';  keys[0].label = "Ghost";
    keys[1].button = 'V';  keys[1].label = "Vampire";
    keys[2].button = 'Z';  keys[2].label = "Zombie";
    keys[3].button = '\b'; keys[3].label = NULL;
    keys[4].button = '+';  keys[4].label = NULL;

    return keys;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    int i;
    int n;
    int count;

    game_state *state = new_state(params);

    state->common->num_ghosts = atoi(desc);
    while (*desc && isdigit((unsigned char)*desc)) desc++;
    desc++;
    state->common->num_vampires = atoi(desc);
    while (*desc && isdigit((unsigned char)*desc)) desc++;
    desc++;
    state->common->num_zombies = atoi(desc);
    while (*desc && isdigit((unsigned char)*desc)) desc++;
    desc++;

    state->common->num_total = state->common->num_ghosts + state->common->num_vampires + state->common->num_zombies;

    state->guess = snewn(state->common->num_total,int);
    state->pencils = snewn(state->common->num_total,unsigned char);
    state->common->fixed = snewn(state->common->num_total, bool);
    for (i=0;i<state->common->num_total;i++) {
        state->guess[i] = 7;
        state->pencils[i] = 0;
        state->common->fixed[i] = false;
    }
    for (i=0;i<state->common->wh;i++)
        state->cell_errors[i] = false;
    for (i=0;i<2*state->common->num_paths;i++)
        state->hint_errors[i] = false;
    for (i=0;i<3;i++)
        state->count_errors[i] = false;

    count = 0;
    n = 0;
    while (*desc != ',') {
        int c;
        int x,y;

        if (*desc == 'L') {
            num2grid(n,state->common->params.w,state->common->params.h,&x,&y);
            state->common->grid[x+y*(state->common->params.w +2)] = CELL_MIRROR_L;
            state->common->xinfo[x+y*(state->common->params.w+2)] = -1;
            n++;
        }
        else if (*desc == 'R') {
            num2grid(n,state->common->params.w,state->common->params.h,&x,&y);
            state->common->grid[x+y*(state->common->params.w +2)] = CELL_MIRROR_R;
            state->common->xinfo[x+y*(state->common->params.w+2)] = -1;
            n++;
        }
        else if (*desc == 'G') {
            num2grid(n,state->common->params.w,state->common->params.h,&x,&y);
            state->common->grid[x+y*(state->common->params.w +2)] = CELL_GHOST;
            state->common->xinfo[x+y*(state->common->params.w+2)] = count;
            state->guess[count] = 1;
            state->common->fixed[count++] = true;
            n++;
        }
        else if (*desc == 'V') {
            num2grid(n,state->common->params.w,state->common->params.h,&x,&y);
            state->common->grid[x+y*(state->common->params.w +2)] = CELL_VAMPIRE;
            state->common->xinfo[x+y*(state->common->params.w+2)] = count;
            state->guess[count] = 2;
            state->common->fixed[count++] = true;
            n++;
        }
        else if (*desc == 'Z') {
            num2grid(n,state->common->params.w,state->common->params.h,&x,&y);
            state->common->grid[x+y*(state->common->params.w +2)] = CELL_ZOMBIE;
            state->common->xinfo[x+y*(state->common->params.w+2)] = count;
            state->guess[count] = 4;
            state->common->fixed[count++] = true;
            n++;
        }
        else {
            c = *desc - ('a' -1);
            while (c-- > 0) {
                num2grid(n,state->common->params.w,state->common->params.h,&x,&y);
                state->common->grid[x+y*(state->common->params.w +2)] = CELL_EMPTY;
                state->common->xinfo[x+y*(state->common->params.w+2)] = count;
                state->guess[count] = 7;
                state->common->fixed[count++] = false;
                n++;
            }
        }
        desc++;
    }
    desc++;

    for (i=0;i<2*(state->common->params.w + state->common->params.h);i++) {
        int x,y;
        int sights;

        if (*desc == '-') sights = -1;
        else              sights = atoi(desc);

        while (*desc && (isdigit((unsigned char)*desc) || *desc == '-')) desc++;
        desc++;

        range2grid(i,state->common->params.w,state->common->params.h,&x,&y);
        state->common->grid[x+y*(state->common->params.w +2)] = sights;
        state->common->xinfo[x+y*(state->common->params.w +2)] = -2;
    }

    state->common->grid[0] = 0;
    state->common->xinfo[0] = -2;
    state->common->grid[state->common->params.w+1] = 0;
    state->common->xinfo[state->common->params.w+1] = -2;
    state->common->grid[state->common->params.w+1 + (state->common->params.h+1)*(state->common->params.w+2)] = 0;
    state->common->xinfo[state->common->params.w+1 + (state->common->params.h+1)*(state->common->params.w+2)] = -2;
    state->common->grid[(state->common->params.h+1)*(state->common->params.w+2)] = 0;
    state->common->xinfo[(state->common->params.h+1)*(state->common->params.w+2)] = -2;

    make_paths(state);
    qsort(state->common->paths, state->common->num_paths, sizeof(struct path), path_cmp);

    return state;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    int i;
    int w = params->w, h = params->h;
    int wh = w*h;
    int area;
    int monsters;
    int monster_count;
    const char *desc_s = desc;

    for (i=0;i<3;i++) {
        if (!*desc) return "Faulty game description";
        while (*desc && isdigit((unsigned char)*desc)) { desc++; }
        if (*desc != ',') return "Invalid character in number list";
        desc++;
    }
    desc = desc_s;

    area = monsters = monster_count = 0;
    for (i=0;i<3;i++) {
        monster_count += atoi(desc);
        while (*desc && isdigit((unsigned char)*desc)) desc++;
        desc++;
    }
    while (*desc && *desc != ',') {
        if (*desc >= 'a' && *desc <= 'z') {
            area += *desc - 'a' +1; monsters += *desc - 'a' +1;
        } else if (*desc == 'G' || *desc == 'V' || *desc == 'Z') {
            area++; monsters++;
        } else if (*desc == 'L' || *desc == 'R') {
            area++;
        } else
            return "Invalid character in grid specification";
        desc++;
    }
    if (area < wh) return "Not enough data to fill grid";
    else if (area > wh) return "Too much data to fill grid";
    if (monsters != monster_count)
        return "Monster numbers do not match grid spaces";

    for (i = 0; i < 2*(w+h); i++) {
        if (!*desc) return "Not enough numbers given after grid specification";
        else if (*desc != ',') return "Invalid character in number list";
        desc++;
        while (*desc && (isdigit((unsigned char)*desc) || *desc == '-')) { desc++; }
    }

    if (*desc) return "Unexpected additional data at end of game description";

    return NULL;
}

static char *solve_game(const game_state *state_start, const game_state *currstate,
                        const char *aux, const char **error)
{
    struct solution sol;
    int i;
    char *move, *c;

    game_state *solve_state = dup_game(currstate);
    solve_state->common->params.diff = DIFF_HARD;

    sol.puzzle_solution = snewn(solve_state->common->num_total,int);
    solve(solve_state, &sol);

    if (sol.contains_inconsistency) {
        *error = "Puzzle is inconsistent";
        sfree(sol.puzzle_solution);
        free_game(solve_state);
        return NULL;
    }

    move = snewn(solve_state->common->num_total * 4 +2, char);
    c = move;
    *c++='S';
    for (i = 0; i < solve_state->common->num_total; i++) {
        if (sol.puzzle_solution[i] == 1) c += sprintf(c, ";G%d", i);
        if (sol.puzzle_solution[i] == 2) c += sprintf(c, ";V%d", i);
        if (sol.puzzle_solution[i] == 4) c += sprintf(c, ";Z%d", i);
    }
    *c++ = '\0';
    move = sresize(move, c - move, char);

    sfree(sol.puzzle_solution);
    free_game(solve_state);
    return move;
}

struct game_ui {
    int hx, hy;                         /* as for solo.c, highlight pos */
    bool hshow, hpencil, hcursor;       /* show state, type, and ?cursor. */
    char hhint;
    bool hdrag;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);
    ui->hx = ui->hy = 0;
    ui->hpencil = false;
    ui->hshow = false;
    ui->hcursor = false;
    ui->hhint = ' ';
    ui->hdrag = false;
    return ui;
}

static void free_ui(game_ui *ui) {
    sfree(ui);
    return;
}

static char *encode_ui(const game_ui *ui)
{
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding)
{
    return;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
    /* See solo.c; if we were pencil-mode highlighting and
     * somehow a square has just been properly filled, cancel
     * pencil mode. */
    if (ui->hshow && ui->hpencil && !ui->hcursor) {
        int g = newstate->guess[newstate->common->xinfo[ui->hx + ui->hy*(newstate->common->params.w+2)]];
        if (g == 1 || g == 2 || g == 4)
            ui->hshow = false;
    }
}

static bool is_key_highlighted(const game_ui *ui, char c) {
    return (c == ui->hhint);
}

struct game_drawstate {
    int tilesize;
    bool started, solved;
    int w, h;

    int *monsters;
    unsigned char *pencils;

    bool count_errors[3];
    int monster_counts[3];
    bool *cell_errors;
    bool *hint_errors;
    bool *hints_done;

    int hx, hy;
    bool hshow, hpencil; /* as for game_ui. */
};

static bool is_clue(const game_state *state, int x, int y)
{
    int h = state->common->params.h, w = state->common->params.w;

    if (state->common->grid[x+y*(state->common->params.w+2)] == -1)
        return false;

    if (((x == 0 || x == w + 1) && y > 0 && y <= h) ||
        ((y == 0 || y == h + 1) && x > 0 && x <= w))
        return true;

    return false;
}

static int clue_index(const game_state *state, int x, int y)
{
    int h = state->common->params.h, w = state->common->params.w;

    if (y == 0)
        return x - 1;
    else if (x == w + 1)
        return w + y - 1;
    else if (y == h + 1)
        return 2 * w + h - x;
    else if (x == 0)
        return 2 * (w + h) - y;

    return -1;
}

#define TILESIZE (ds->tilesize)
#define BORDER (TILESIZE/4)

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button, bool swapped)
{
    int gx,gy;
    int g,xi;
    char buf[80]; 

    gx = ((x-BORDER-1) / TILESIZE );
    gy = ((y-BORDER-2) / TILESIZE ) - 1;

    if (button == '+') {
        return dupstr("M");
    }

    if (ui->hshow && !ui->hpencil) {
        xi = state->common->xinfo[ui->hx + ui->hy*(state->common->params.w+2)];
        if (xi >= 0 && !state->common->fixed[xi]) {
            if (button == 'G') {
                if (!ui->hcursor) ui->hshow = false;
                sprintf(buf,"G%d",xi);
                return dupstr(buf);
            }
            if (button == 'V') {
                if (!ui->hcursor) ui->hshow = false;
                sprintf(buf,"V%d",xi);
                return dupstr(buf);
            }
            if (button == 'Z') {
                if (!ui->hcursor) ui->hshow = false;
                sprintf(buf,"Z%d",xi);
                return dupstr(buf);
            }
            if (button == '\b') {
                if (!ui->hcursor) ui->hshow = false;
                sprintf(buf,"E%d",xi);
                return dupstr(buf);
            }
        }
    }

    if (ui->hshow && ui->hpencil) {
        xi = state->common->xinfo[ui->hx + ui->hy*(state->common->params.w+2)];
        if (xi >= 0 && !state->common->fixed[xi]) {
            if (button == 'G') {
                sprintf(buf,"g%d",xi);
                return dupstr(buf);
            }
            if (button == 'V') {
                sprintf(buf,"v%d",xi);
                return dupstr(buf);
            }
            if (button == 'Z') {
                sprintf(buf,"z%d",xi);
                return dupstr(buf);
            }
            if (button == '\b') {
                sprintf(buf,"E%d",xi);
                if (!ui->hcursor) {
                    ui->hpencil = false;
                    ui->hshow = false;
                }
                return dupstr(buf);
            }
        }
    }

    if (!ui->hshow && (button == 'G' || button == 'V' || button == 'Z' || button == '\b')) {
        if (ui->hhint == button) ui->hhint = ' ';
        else ui->hhint = button;
        return UI_UPDATE;
    }

    if (gx > 0 && gx < ds->w+1 && gy > 0 && gy < ds->h+1) {
        xi = state->common->xinfo[gx+gy*(state->common->params.w+2)];
        if (xi >= 0 && !state->common->fixed[xi]) {
            g = state->guess[xi];
            if (!ui->hshow) {
                if (((button == LEFT_RELEASE && !swapped) || 
                     (button == LEFT_BUTTON && swapped)) &&
                     (!ui->hdrag && (ui->hhint != ' '))) {
                    sprintf(buf, "%c%d", (ui->hhint == '\b') ? 'E' : ui->hhint, xi);
                    return dupstr(buf);
                }
                else if (button == LEFT_BUTTON) {
                    if (ui->hhint != ' ') {
                        ui->hdrag = false;
                    }
                    else {
                        ui->hshow = true;
                        ui->hpencil = false;
                        ui->hcursor = false;
                        ui->hx = gx; ui->hy = gy;
                    }
                    return UI_UPDATE;
                }
                else if (button == RIGHT_BUTTON && g == 7) {
                    if (ui->hhint != ' ') {
                        sprintf(buf,"%c%d",(ui->hhint == '\b') ? 'E' : ui->hhint+32, xi);
                        return dupstr(buf);
                    }
                    ui->hshow = true;
                    ui->hpencil = true;
                    ui->hcursor = false;
                    ui->hx = gx; ui->hy = gy;
                    return UI_UPDATE;
                }
                else if (button == LEFT_DRAG) {
                    ui->hdrag = true;
                }
            }
            else if (ui->hshow) {
                if (button == LEFT_BUTTON) {
                    if (!ui->hpencil) {
                        if (gx == ui->hx && gy == ui->hy) {
                            ui->hshow = false;
                            ui->hpencil = false;
                            ui->hcursor = false;
                            ui->hx = 0; ui->hy = 0;
                            return UI_UPDATE;
                        }
                        else {
                            ui->hshow = true;
                            ui->hpencil = false;
                            ui->hcursor = false;
                            ui->hx = gx; ui->hy = gy;
                            return UI_UPDATE;
                        }
                    }
                    else {
                        ui->hshow = true;
                        ui->hpencil = false;
                        ui->hcursor = false;
                        ui->hx = gx; ui->hy = gy;
                        return UI_UPDATE;
                    }
                }
                else if (button == RIGHT_BUTTON) {
                    if (!ui->hpencil && g == 7) {
                        ui->hshow = true;
                        ui->hpencil = true;
                        ui->hcursor = false;
                        ui->hx = gx; ui->hy = gy;
                        return UI_UPDATE;
                    }
                    else {
                        if (gx == ui->hx && gy == ui->hy) {
                            ui->hshow = false;
                            ui->hpencil = false;
                            ui->hcursor = false;
                            ui->hx = 0; ui->hy = 0;
                            return UI_UPDATE;
                        }
                        else if (g == 7) {
                            ui->hshow = true;
                            ui->hpencil = true;
                            ui->hcursor = false;
                            ui->hx = gx; ui->hy = gy;
                            return UI_UPDATE;
                        }
                    }
                }
            }
        }
    } else if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        if (is_clue(state, gx, gy)) {
            sprintf(buf, "D%d,%d", gx, gy);
            return dupstr(buf);
        }
        ui->hshow = false;
        ui->hpencil = false;
        ui->hhint = ' ';
        ui->hdrag = false;
        return UI_UPDATE;
    }

    return NULL;
}

static bool check_numbers_draw(game_state *state, int *guess) {
    bool valid, filled;
    int i,x,y,xy;

    for (i=0;i<3;i++)
        state->monster_counts[i] = 0;
    for (i=0;i<state->common->num_total;i++) {
        if (guess[i] == 1) state->monster_counts[0]++;
        if (guess[i] == 2) state->monster_counts[1]++;
        if (guess[i] == 4) state->monster_counts[2]++;
    }

    valid = true;
    filled = ((state->monster_counts[0]+state->monster_counts[1]+state->monster_counts[2]) >=
              state->common->num_total);

    if (state->monster_counts[0] > state->common->num_ghosts ||
        (filled && state->monster_counts[0] != state->common->num_ghosts) ) {
        valid = false;
        state->count_errors[0] = true;
        for (x=1;x<state->common->params.w+1;x++)
            for (y=1;y<state->common->params.h+1;y++) {
                xy = x+y*(state->common->params.w+2);
                if (state->common->xinfo[xy] >= 0 &&
                    guess[state->common->xinfo[xy]] == 1)
                    state->cell_errors[xy] = true;
            }
    }
    if (state->monster_counts[1] > state->common->num_vampires ||
        (filled && state->monster_counts[1] != state->common->num_vampires) ) {
        valid = false;
        state->count_errors[1] = true;
        for (x=1;x<state->common->params.w+1;x++)
            for (y=1;y<state->common->params.h+1;y++) {
                xy = x+y*(state->common->params.w+2);
                if (state->common->xinfo[xy] >= 0 &&
                    guess[state->common->xinfo[xy]] == 2)
                    state->cell_errors[xy] = true;
            }
    }
    if (state->monster_counts[2] > state->common->num_zombies ||
        (filled && state->monster_counts[2] != state->common->num_zombies) )  {
        valid = false;
        state->count_errors[2] = true;
        for (x=1;x<state->common->params.w+1;x++)
            for (y=1;y<state->common->params.h+1;y++) {
                xy = x+y*(state->common->params.w+2);
                if (state->common->xinfo[xy] >= 0 &&
                    guess[state->common->xinfo[xy]] == 4)
                    state->cell_errors[xy] = true;
            }
    }

    return valid;
}

static bool check_path_solution(game_state *state, int p) {
    int i;
    bool mirror;
    int count;
    bool correct;
    int unfilled;

    int sightings_start, sightings_end;

    sightings_start = state->common->paths[p].sightings_start;
    sightings_end   = state->common->paths[p].sightings_end;

    count = 0;
    mirror = false;
    correct = true;

    unfilled = 0;
    for (i=0;i<state->common->paths[p].length;i++) {
        if (state->common->paths[p].p[i] == -1) mirror = true;
        else {
            if (state->guess[state->common->paths[p].p[i]] == 1 && mirror)
                count++;
            else if (state->guess[state->common->paths[p].p[i]] == 2 && !mirror)
                count++;
            else if (state->guess[state->common->paths[p].p[i]] == 4)
                count++;
            else if (state->guess[state->common->paths[p].p[i]] == 7)
                unfilled++;
        }
    }

    if ((sightings_start >= 0) && (count > sightings_start || (count + unfilled) < sightings_start)) {
        correct = false;
        state->hint_errors[state->common->paths[p].grid_start] = true;
    }

    count = 0;
    mirror = false;
    unfilled = 0;
    for (i=state->common->paths[p].length-1;i>=0;i--) {
        if (state->common->paths[p].p[i] == -1) mirror = true;
        else {
            if (state->guess[state->common->paths[p].p[i]] == 1 && mirror)
                count++;
            else if (state->guess[state->common->paths[p].p[i]] == 2 && !mirror)
                count++;
            else if (state->guess[state->common->paths[p].p[i]] == 4)
                count++;
            else if (state->guess[state->common->paths[p].p[i]] == 7)
                unfilled++;
        }
    }

    if ((sightings_end >= 0) && (count > sightings_end || count + unfilled < sightings_end)) {
        correct = false;
        state->hint_errors[state->common->paths[p].grid_end] = true;
    }

    if (!correct) {
        for (i=0;i<state->common->paths[p].length;i++)
            state->cell_errors[state->common->paths[p].xy[i]] = true;
    }

    return correct;
}

static game_state *execute_move(const game_state *state, const char *move)
{
    int x,y,n,p,i;
    char c;
    bool correct;
    bool solver;

    game_state *ret = dup_game(state);
    solver = false;

    while (*move) {
        c = *move;
        if (c == 'S') {
            move++;
            solver = true;
        }
        if (c == 'G' || c == 'V' || c == 'Z' || c == 'E' ||
            c == 'g' || c == 'v' || c == 'z') {
            move++;
            sscanf(move, "%d%n", &x, &n);
            if (c == 'G') ret->guess[x] = !solver && (ret->guess[x] == 1) ? 7 : 1;
            if (c == 'V') ret->guess[x] = !solver && (ret->guess[x] == 2) ? 7 : 2;
            if (c == 'Z') ret->guess[x] = !solver && (ret->guess[x] == 4) ? 7 : 4;
            if (c == 'E' && ret->guess[x] == 7)
                ret->pencils[x] = 0;
            if (c == 'E' && ret->guess[x] != 7)
                ret->guess[x] = 7; 
            if (c == 'g') ret->pencils[x] ^= 1;
            if (c == 'v') ret->pencils[x] ^= 2;
            if (c == 'z') ret->pencils[x] ^= 4;
            move += n;
        }
        if (c == 'D' && sscanf(move + 1, "%d,%d%n", &x, &y, &n) == 2 &&
            is_clue(ret, x, y)) {
            ret->hints_done[clue_index(ret, x, y)] ^= 1;
            move += n + 1;
        }
        if (c == 'M') {
            /*
             * Fill in absolutely all pencil marks in unfilled
             * squares, for those who like to play by the rigorous
             * approach of starting off in that state and eliminating
             * things.
             */
            for (i = 0; i < ret->common->num_total; i++)
                if (ret->guess[i] == 7)
                    ret->pencils[i] = 7;
            move++;
        }
        if (*move == ';') move++;
    }

    correct = true;

    for (i=0;i<ret->common->wh;i++) ret->cell_errors[i] = false;
    for (i=0;i<2*ret->common->num_paths;i++) ret->hint_errors[i] = false;
    for (i=0;i<3;i++) ret->count_errors[i] = false;

    if (!check_numbers_draw(ret,ret->guess)) correct = false;

    for (p=0;p<state->common->num_paths;p++)
        if (!check_path_solution(ret,p)) correct = false;

    for (i=0;i<state->common->num_total;i++)
        if (!(ret->guess[i] == 1 || ret->guess[i] == 2 ||
              ret->guess[i] == 4)) correct = false;

    ret->solved = correct;
    ret->cheated = solver;

    return ret;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

#define PREFERRED_TILE_SIZE 64

static void game_compute_size(const game_params *params, int tilesize,
                              int *x, int *y)
{
    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = 2*BORDER+(params->w+2)*TILESIZE;
    *y = 2*BORDER+(params->h+3)*TILESIZE;
    return;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
    return;
}

#define COLOUR(ret, i, r, g, b)     ((ret[3*(i)+0] = (r)), (ret[3*(i)+1] = (g)), (ret[3*(i)+2] = (b)))

static float *game_colours(frontend *fe, int *ncolours)
{
    int i;
    float *ret = snewn(3 * NCOLOURS, float);

    for (i=0;i<3;i++) {
        ret[COL_BACKGROUND * 3 + i] = 1.0F;
        ret[COL_GRID       * 3 + i] = 0.0F;
        ret[COL_TEXT       * 3 + i] = 0.0F;
        ret[COL_ERROR      * 3 + i] = 0.25F;
        ret[COL_ERROR_TEXT * 3 + i] = 0.75F;
        ret[COL_HIGHLIGHT  * 3 + i] = 0.75F;
        ret[COL_GHOST      * 3 + i] = 1.0F;
        ret[COL_VAMPIRE    * 3 + i] = 0.9F;
        ret[COL_ZOMBIE     * 3 + i] = 0.7F;
        ret[COL_DONE       * 3 + i] = 0.75F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    int i;
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->started = ds->solved = false;
    ds->w = state->common->params.w;
    ds->h = state->common->params.h;
    for (i=0;i<3;i++) {
        ds->count_errors[i] = false;
        ds->monster_counts[i] = 0;
    }
    ds->monsters = snewn(state->common->num_total,int);
    for (i=0;i<(state->common->num_total);i++)
        ds->monsters[i] = 7;
    ds->pencils = snewn(state->common->num_total,unsigned char);
    for (i=0;i<state->common->num_total;i++)
        ds->pencils[i] = 0;

    ds->cell_errors = snewn(state->common->wh,bool);
    for (i=0;i<state->common->wh;i++)
        ds->cell_errors[i] = false;
    ds->hint_errors = snewn(2*state->common->num_paths,bool);
    for (i=0;i<2*state->common->num_paths;i++)
        ds->hint_errors[i] = false;
    ds->hints_done = snewn(2 * state->common->num_paths, bool);
    memset(ds->hints_done, 0,
           2 * state->common->num_paths * sizeof(bool));

    ds->hshow = false;
    ds->hpencil = false;
    ds->hx = ds->hy = 0;
    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds) {
    sfree(ds->hints_done);
    sfree(ds->hint_errors);
    sfree(ds->cell_errors);
    sfree(ds->pencils);
    sfree(ds->monsters);
    sfree(ds);
    return;
}

static void draw_cell_background(drawing *dr, game_drawstate *ds,
                                 const game_state *state, const game_ui *ui,
                                 int x, int y) {

    bool hon;
    int dx,dy;
    dx = BORDER+(x* ds->tilesize)+(TILESIZE/2);
    dy = BORDER+(y* ds->tilesize)+(TILESIZE/2)+TILESIZE;

    hon = (ui->hshow && x == ui->hx && y == ui->hy);
    draw_rect(dr,dx-(TILESIZE/2)+1,dy-(TILESIZE/2)+1,TILESIZE-1,TILESIZE-1,(hon && !ui->hpencil) ? COL_HIGHLIGHT : COL_BACKGROUND);

    if (hon && ui->hpencil) {
        int coords[6];
        coords[0] = dx-(TILESIZE/2)+1;
        coords[1] = dy-(TILESIZE/2)+1;
        coords[2] = coords[0] + TILESIZE/2;
        coords[3] = coords[1];
        coords[4] = coords[0];
        coords[5] = coords[1] + TILESIZE/2;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);
    }

    draw_update(dr,dx-(TILESIZE/2)+1,dy-(TILESIZE/2)+1,TILESIZE-1,TILESIZE-1);

    return;
}

static void draw_circle_or_point(drawing *dr, int cx, int cy, int radius,
                                 int colour)
{
    if (radius > 0)
        draw_circle(dr, cx, cy, radius, colour, colour);
    else
        draw_rect(dr, cx, cy, 1, 1, colour);
}

static void draw_monster(drawing *dr, game_drawstate *ds, int x, int y,
                         int tilesize, int monster)
{
    int black = COL_TEXT;
    float thl = 3.0;
    int thc = 3;

    if (monster == 1) {                /* ghost */
        int poly[80], i;
        int s = 2*tilesize/5;
        
        clip(dr,x-(tilesize/2)+2,y-(tilesize/2)+2,tilesize-3,tilesize/2+1);
        draw_circle(dr,x,y,s+2, black, black);
        draw_circle(dr,x,y,s-1, COL_GHOST,black);
        unclip(dr);

        clip(dr,x-(tilesize/2)+2,y,tilesize-3,tilesize-(tilesize/2)-1);

        i=0;
        poly[i++] = x-s      ; poly[i++] = y-2;
        poly[i++] = x-s;     ; poly[i++] = y+s-s/3;
        poly[i++] = x+s+1    ; poly[i++] = y+s-s/3;
        poly[i++] = x+s+1    ; poly[i++] = y-2;
        draw_polygon(dr, poly, i/2, COL_GHOST, COL_GHOST);

        i=0;
        poly[i++] = x-s;     ; poly[i++] = y+s-s/3;
        poly[i++] = x-s;     ; poly[i++] = y+s;
        poly[i++] = x-s+1*s/3; poly[i++] = y+s-s/3;
        draw_polygon(dr, poly, i/2, COL_GHOST, COL_GHOST);

        i=0;
        poly[i++] = x-s+1*s/3; poly[i++] = y+s-s/3;
        poly[i++] = x-s+2*s/3; poly[i++] = y+s;
        poly[i++] = x-s+3*s/3; poly[i++] = y+s-s/3;
        draw_polygon(dr, poly, i/2, COL_GHOST, COL_GHOST);

        i=0;
        poly[i++] = x-s+3*s/3; poly[i++] = y+s-s/3;
        poly[i++] = x-s+4*s/3; poly[i++] = y+s;
        poly[i++] = x-s+5*s/3; poly[i++] = y+s-s/3;
        draw_polygon(dr, poly, i/2, COL_GHOST, COL_GHOST);

        i=0;
        poly[i++] = x-s+5*s/3; poly[i++] = y+s-s/3;
        poly[i++] = x-s+6*s/3; poly[i++] = y+s;
        poly[i++] = x+s+1;     poly[i++] = y-2;
        draw_polygon(dr, poly, i/2, COL_GHOST, COL_GHOST);
        
        draw_thick_line(dr, thl+0.5, x-s, y-2, x-s, y+s, black);
        draw_thick_line(dr, thl+0.5, x-s,       y+s,     x-s+1*s/3, y+s-s/3, black);
        draw_thick_line(dr, thl+0.5, x-s+1*s/3, y+s-s/3, x-s+2*s/3, y+s, black);
        draw_thick_line(dr, thl+0.5, x-s+2*s/3, y+s,     x-s+3*s/3, y+s-s/3, black);
        draw_thick_line(dr, thl+0.5, x-s+3*s/3, y+s-s/3, x-s+4*s/3, y+s, black);
        draw_thick_line(dr, thl+0.5, x-s+4*s/3, y+s,     x-s+5*s/3, y+s-s/3, black);
        draw_thick_line(dr, thl+0.5, x-s+5*s/3, y+s-s/3, x-s+6*s/3, y+s, black);
        draw_thick_line(dr, thl+0.5, x-s+6*s/3, y+s,     x+s+1,     y-2, black);

        unclip(dr);

        draw_circle(dr,x-tilesize/6,y-tilesize/12,tilesize/10, black,black);
        draw_circle(dr,x-tilesize/6,y-tilesize/12,tilesize/10-thc, COL_BACKGROUND,black);

        draw_circle(dr,x+tilesize/6,y-tilesize/12,tilesize/10, black,black);
        draw_circle(dr,x+tilesize/6,y-tilesize/12,tilesize/10-thc, COL_BACKGROUND,black);

        draw_circle_or_point(dr,x-tilesize/6+1+tilesize/48,y-tilesize/12, tilesize/48,black);
        draw_circle_or_point(dr,x+tilesize/6+1+tilesize/48,y-tilesize/12, tilesize/48,black);

    } else if (monster == 2) {         /* vampire */
        int poly[80], i;

        clip(dr,x-(tilesize/2)+2,y-(tilesize/2)+2,tilesize-3,tilesize/2);
        draw_circle(dr,x,y,2*tilesize/5,black,black);
        unclip(dr);

        clip(dr,x-(tilesize/2)+2,y-(tilesize/2)+2,tilesize/2+1,tilesize/2);
        draw_circle(dr,x-tilesize/7,y,2*tilesize/5-tilesize/7, black,black);
        draw_circle(dr,x-tilesize/7,y,2*tilesize/5-tilesize/7-thc, COL_VAMPIRE,black);
        unclip(dr);
        clip(dr,x,y-(tilesize/2)+2,tilesize/2+1,tilesize/2);
        draw_circle(dr,x+tilesize/7,y,2*tilesize/5-tilesize/7, black,black);
        draw_circle(dr,x+tilesize/7,y,2*tilesize/5-tilesize/7-thc, COL_VAMPIRE,black);
        unclip(dr);

        clip(dr,x-(tilesize/2)+2,y,tilesize-3,tilesize/2);
        draw_circle(dr,x,y,2*tilesize/5, black,black);
        draw_circle(dr,x,y,2*tilesize/5-thc, COL_VAMPIRE,black);
        unclip(dr);

        draw_circle(dr, x-tilesize/7, y-tilesize/16, tilesize/16+thc, black, black);
        draw_circle(dr, x-tilesize/7, y-tilesize/16, tilesize/16, COL_BACKGROUND, black);

        draw_circle(dr, x+tilesize/7, y-tilesize/16, tilesize/16+thc, black, black);
        draw_circle(dr, x+tilesize/7, y-tilesize/16, tilesize/16, COL_BACKGROUND, black);

        draw_circle_or_point(dr, x-tilesize/7, y-tilesize/16, tilesize/48, black);
        draw_circle_or_point(dr, x+tilesize/7, y-tilesize/16, tilesize/48, black);

        clip(dr, x-(tilesize/2)+2, y+tilesize/8, tilesize-3, tilesize/4);

        i = 0;
        poly[i++] = x-3*tilesize/16; poly[i++] = y+1*tilesize/8;
        poly[i++] = x-2*tilesize/16; poly[i++] = y+7*tilesize/24;
        poly[i++] = x-1*tilesize/16; poly[i++] = y+1*tilesize/8;
        draw_polygon(dr, poly, i/2, COL_BACKGROUND, COL_BACKGROUND);
        draw_thick_line(dr, thl, poly[0], poly[1], poly[2], poly[3], black);
        draw_thick_line(dr, thl, poly[2], poly[3], poly[4], poly[5], black);
        i = 0;
        poly[i++] = x+3*tilesize/16; poly[i++] = y+1*tilesize/8;
        poly[i++] = x+2*tilesize/16; poly[i++] = y+7*tilesize/24;
        poly[i++] = x+1*tilesize/16; poly[i++] = y+1*tilesize/8;
        draw_polygon(dr, poly, i/2, COL_BACKGROUND, COL_BACKGROUND);
        draw_thick_line(dr, thl, poly[0], poly[1], poly[2], poly[3], black);
        draw_thick_line(dr, thl, poly[2], poly[3], poly[4], poly[5], black);

        draw_circle(dr, x, y-tilesize/5, 2*tilesize/5, black, black);
        draw_circle(dr, x, y-tilesize/5, 2*tilesize/5-thc, COL_VAMPIRE, black);
        unclip(dr);

    } else if (monster == 4) {         /* zombie */
        draw_circle(dr,x,y,2*tilesize/5, black,black);
        draw_circle(dr,x,y,2*tilesize/5-thc, COL_ZOMBIE,black);

        draw_thick_line(dr, thl, x-tilesize/7-tilesize/16, y-tilesize/12-tilesize/16,
                      x-tilesize/7+tilesize/16, y-tilesize/12+tilesize/16, black);

        draw_thick_line(dr, thl,x-tilesize/7+tilesize/16, y-tilesize/12-tilesize/16,
                      x-tilesize/7-tilesize/16, y-tilesize/12+tilesize/16, black);

        draw_thick_line(dr, thl, x+tilesize/7-tilesize/16, y-tilesize/12-tilesize/16,
                      x+tilesize/7+tilesize/16, y-tilesize/12+tilesize/16, black);

        draw_thick_line(dr, thl, x+tilesize/7+tilesize/16, y-tilesize/12-tilesize/16,
                      x+tilesize/7-tilesize/16, y-tilesize/12+tilesize/16, black);

        clip(dr, x-tilesize/5, y+tilesize/6, 2*tilesize/5+1, tilesize/2);
        draw_circle(dr, x-tilesize/15, y+tilesize/6, tilesize/12, black, black);
        draw_circle(dr, x-tilesize/15, y+tilesize/6, tilesize/12-thc, COL_BACKGROUND, black);
        unclip(dr);

        draw_thick_line(dr, thl, x-tilesize/5, y+tilesize/6, x+tilesize/5, y+tilesize/6, black);
    }

    draw_update(dr,x-(tilesize/2)+2,y-(tilesize/2)+2,tilesize-3,tilesize-3);
}

static void draw_monster_count(drawing *dr, game_drawstate *ds,
                               const game_state *state, int c) {
    int dx,dy,nw;
    int mx,cx,mw,cw;
    char buf[8];

    dy = TILESIZE/4;
    dx = BORDER+(ds->w+2)*TILESIZE/2;
    nw = ((state->common->num_ghosts > 9) || 
          (state->common->num_vampires > 9 ) ||
          (state->common->num_zombies > 9)) ? 5 : 3;
    switch (c) {
      case 0:
        sprintf(buf,"%d/%d",state->common->num_ghosts, state->monster_counts[0]);
        dx = BORDER + (ds->w+2)*TILESIZE/2 - 5 - 2*TILESIZE/3 - nw*TILESIZE/4;
        break;
      case 1:
        sprintf(buf,"%d/%d",state->common->num_vampires, state->monster_counts[1]);
        dx = BORDER + (ds->w+2)*TILESIZE/2;
        break;
      case 2:
        sprintf(buf,"%d/%d",state->common->num_zombies, state->monster_counts[2]);
        dx = BORDER + (ds->w+2)*TILESIZE/2 + 5 + 2*TILESIZE/3 + nw*TILESIZE/4;
        break;
    }
    mx = dx-2*TILESIZE/3;
    mw = 2*TILESIZE/3;
    cx = dx;
    cw = nw*TILESIZE/4;

    draw_rect(dr, mx, dy, mw, TILESIZE, COL_BACKGROUND);
    draw_monster(dr, ds, mx+mw/2, dy+TILESIZE/2, mw, 1<<c);
    clip(dr, cx, dy+TILESIZE/4, cw, TILESIZE/2);
    draw_rect(dr, cx, dy+TILESIZE/4, cw, TILESIZE/2,
              (state->count_errors[c] ? COL_ERROR : COL_BACKGROUND));
    draw_text(dr, cx+cw/2, dy+TILESIZE/2, FONT_VARIABLE, 3*TILESIZE/8,
              ALIGN_HCENTRE|ALIGN_VCENTRE, (state->count_errors[c] ? COL_ERROR_TEXT : COL_TEXT), buf);
    unclip(dr);
    draw_update(dr, mx, dy, mw+cw, TILESIZE);
    return;
}

static void draw_path_hint(drawing *dr, game_drawstate *ds,
                           const struct game_params *params,
                           int hint_index, int hint) {
    int x, y, color, dx, dy, text_dx, text_dy, text_size;
    char buf[4];

    if (ds->hints_done[hint_index])
        color = COL_DONE;
    else
        color = COL_TEXT;

    range2grid(hint_index, params->w, params->h, &x, &y);
    /* Upper-left corner of the "tile" */
    dx = BORDER + x * TILESIZE;
    dy = BORDER + y * TILESIZE + TILESIZE;
    /* Center of the "tile" */
    text_dx = dx + TILESIZE / 2;
    text_dy = dy +  TILESIZE / 2;
    /* Avoid wiping out the borders of the puzzle */
    dx += 2;
    dy += 2;
    text_size = TILESIZE - 3;

    if (hint >= 0) sprintf(buf,"%d", hint);
    else           sprintf(buf," ");
    draw_rect(dr, dx, dy, text_size, text_size, ds->hint_errors[hint_index] ? COL_ERROR : COL_BACKGROUND);
    draw_text(dr, text_dx, text_dy, FONT_FIXED, TILESIZE / 2,
              ALIGN_HCENTRE | ALIGN_VCENTRE, (ds->hint_errors[hint_index] ? COL_ERROR_TEXT : color), buf);
    draw_update(dr, dx, dy, text_size, text_size);

    return;
}

static void draw_mirror(drawing *dr, game_drawstate *ds,
                        const game_state *state, int x, int y, int mirror) {
    int dx,dy,mx1,my1,mx2,my2;
    dx = BORDER+(x* ds->tilesize)+(TILESIZE/2);
    dy = BORDER+(y* ds->tilesize)+(TILESIZE/2)+TILESIZE;

    if (mirror == CELL_MIRROR_L) {
        mx1 = dx-(TILESIZE/4);
        my1 = dy-(TILESIZE/4);
        mx2 = dx+(TILESIZE/4);
        my2 = dy+(TILESIZE/4);
    }
    else {
        mx1 = dx-(TILESIZE/4);
        my1 = dy+(TILESIZE/4);
        mx2 = dx+(TILESIZE/4);
        my2 = dy-(TILESIZE/4);
    }
    draw_thick_line(dr,(float)(TILESIZE/16),mx1,my1,mx2,my2, COL_TEXT);
    draw_update(dr,dx-(TILESIZE/2)+1,dy-(TILESIZE/2)+1,TILESIZE-1,TILESIZE-1);

    return;
}

static void draw_big_monster(drawing *dr, game_drawstate *ds,
                             const game_state *state, int x, int y, int monster)
{
    int dx,dy;
    dx = BORDER+(x* ds->tilesize)+(TILESIZE/2);
    dy = BORDER+(y* ds->tilesize)+(TILESIZE/2)+TILESIZE;
    draw_monster(dr, ds, dx, dy, 3*TILESIZE/4, monster);
    return;
}

/*
static void draw_pencils(drawing *dr, game_drawstate *ds,
                         const game_state *state, int x, int y, int pencil)
{
    int dx, dy;
    int monsters[4];
    int i, j, px, py;
    dx = BORDER+(x* ds->tilesize)+(TILESIZE/4);
    dy = BORDER+(y* ds->tilesize)+(TILESIZE/4)+TILESIZE;

    for (i = 0, j = 1; j < 8; j *= 2)
        if (pencil & j)
            monsters[i++] = j;
    while (i < 4)
        monsters[i++] = 0;

    for (py = 0; py < 2; py++)
        for (px = 0; px < 2; px++)
            if (monsters[py*2+px]) {
                draw_monster(dr, ds, dx + TILESIZE/2 * px, dy + TILESIZE/2 * py,
                             TILESIZE/2, monsters[py*2+px]);
            }
    draw_update(dr,dx-(TILESIZE/4)+2,dy-(TILESIZE/4)+2,
                (TILESIZE/2)-3,(TILESIZE/2)-3);

    return;
}
*/
static void draw_pencils(drawing *dr, game_drawstate *ds,
                         const game_state *state, int x, int y, int pencil)
{
    int dx, dy;
    dx = BORDER+(x* ds->tilesize)+(TILESIZE/4);
    dy = BORDER+(y* ds->tilesize)+(TILESIZE/4)+TILESIZE;

    if (pencil & 1)
        draw_monster(dr, ds, dx,              dy,              TILESIZE/2, 1);
    if (pencil & 2)
        draw_monster(dr, ds, dx + TILESIZE/2, dy,              TILESIZE/2, 2);
    if (pencil & 4)
        draw_monster(dr, ds, dx,              dy + TILESIZE/2, TILESIZE/2, 4);

    draw_update(dr,dx-(TILESIZE/4)+2,dy-(TILESIZE/4)+2,
                (TILESIZE/2)-3,(TILESIZE/2)-3);
    return;
}

static bool is_hint_stale(const game_drawstate *ds, const game_state *state, int index)
{
    bool ret = false;
    if (!ds->started) ret = true;

    if (ds->hint_errors[index] != state->hint_errors[index]) {
        ds->hint_errors[index] = state->hint_errors[index];
        ret = true;
    }

    if (ds->hints_done[index] != state->hints_done[index]) {
        ds->hints_done[index] = state->hints_done[index];
        ret = true;
    }

    return ret;
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int i,j,x,y,xy;
    int xi, c;
    bool stale, hchanged;

    char buf[48];
    /* Draw status bar */
    sprintf(buf, "%s",
            state->cheated ? "Auto-solved." :
            state->solved  ? "COMPLETED!" : "");
    status_bar(dr, buf);

    /* Draw static grid components at startup */
    if (!ds->started) {
        draw_rect(dr, BORDER+TILESIZE-1, BORDER+2*TILESIZE-1,
                  (ds->w)*TILESIZE +3, (ds->h)*TILESIZE +3, COL_GRID);
        for (i=0;i<ds->w;i++)
            for (j=0;j<ds->h;j++)
                draw_rect(dr, BORDER+(ds->tilesize*(i+1))+1,
                          BORDER+(ds->tilesize*(j+2))+1, ds->tilesize-1,
                          ds->tilesize-1, COL_BACKGROUND);
        draw_update(dr, 0, 0, 2*BORDER+(ds->w+2)*TILESIZE,
                    2*BORDER+(ds->h+3)*TILESIZE);
    }

    hchanged = false;
    if (ds->hx != ui->hx || ds->hy != ui->hy ||
        ds->hshow != ui->hshow || ds->hpencil != ui->hpencil)
        hchanged = true;

    /* Draw monster count hints */

    for (i=0;i<3;i++) {
        stale = false;
        if (!ds->started) stale = true;
        if (ds->count_errors[i] != state->count_errors[i]) {
            stale = true;
            ds->count_errors[i] = state->count_errors[i];
        }
        if (ds->monster_counts[i] != state->monster_counts[i]) {
            stale = true;
            ds->monster_counts[i] = state->monster_counts[i];
        }
        if (stale) {
            draw_monster_count(dr, ds, state, i);
        }
    }

    /* Draw path count hints */
    for (i=0;i<state->common->num_paths;i++) {
        struct path *path = &state->common->paths[i];

        if (is_hint_stale(ds, state, path->grid_start)) {
            draw_path_hint(dr, ds, &state->common->params, path->grid_start,
                           path->sightings_start);
        }

        if (is_hint_stale(ds, state, path->grid_end)) {
            draw_path_hint(dr, ds, &state->common->params, path->grid_end,
                           path->sightings_end);
        }
    }

    /* Draw puzzle grid contents */
    for (x = 1; x < ds->w+1; x++)
        for (y = 1; y < ds->h+1; y++) {
            stale = false;
            xy = x+y*(state->common->params.w+2);
            xi = state->common->xinfo[xy];
            c = state->common->grid[xy];

            if (!ds->started) stale = true;

            if (hchanged) {
                if ((x == ui->hx && y == ui->hy) ||
                    (x == ds->hx && y == ds->hy))
                    stale = true;
            }

            if (xi >= 0 && (state->guess[xi] != ds->monsters[xi]) ) {
                stale = true;
                ds->monsters[xi] = state->guess[xi];
            }

            if (xi >= 0 && (state->pencils[xi] != ds->pencils[xi]) ) {
                stale = true;
                ds->pencils[xi] = state->pencils[xi];
            }

            if (state->cell_errors[xy] != ds->cell_errors[xy]) {
                stale = true;
                ds->cell_errors[xy] = state->cell_errors[xy];
            }

            if (stale) {
                draw_cell_background(dr, ds, state, ui, x, y);
                if (xi < 0)
                    draw_mirror(dr, ds, state, x, y, c);
                else if (state->guess[xi] == 1 || state->guess[xi] == 2 ||
                         state->guess[xi] == 4)
                    draw_big_monster(dr, ds, state, x, y, state->guess[xi]);
                else
                    draw_pencils(dr, ds, state, x, y, state->pencils[xi]);
            }
        }

    ds->hx = ui->hx; ds->hy = ui->hy;
    ds->hshow = ui->hshow;
    ds->hpencil = ui->hpencil;
    ds->started = true;
    return;
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
    return state->solved;
}

static bool game_timing_state(const game_state *state, game_ui *ui)
{
    return true;
}

#ifdef COMBINED
#define thegame undead
#endif

static const char rules[] = "You are given a grid of squares, some of which contain diagonal mirrors. Every square which is not a mirror must be filled with one of three types of undead monster: a ghost, a vampire, or a zombie:\n\n"
"- Vampires can be seen directly, but are invisible when reflected in mirrors.\n"
"- Ghosts can be seen in mirrors, but are invisible when looked at directly.\n"
"- Zombies are visible by any means.\n\n"
"You are told the total number of each type of monster in the grid. Also around the edge of the grid are written numbers, which indicate how many monsters can be seen if you look into the grid along a row or column starting from that position. (The diagonal mirrors are reflective on both sides).\n"
"If your reflected line of sight crosses the same monster more than once, the number will count it each time it is visible, not just once.\n\n\n"
"This puzzle was contributed by Steffen Bauer.\n"
"The monster icons were designed by Simon Tatham.";

const struct game thegame = {
    "Undead", "games.undead", "undead", rules,
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
    game_request_keys,
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
    is_key_highlighted,
    game_status,
    false, false, NULL, NULL,
    true,                 /* wants_statusbar */
    false, game_timing_state,
    REQUIRE_RBUTTON,                     /* flags */
};

