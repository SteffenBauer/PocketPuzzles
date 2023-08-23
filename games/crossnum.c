/*
 * CrossNum: Implementation of the Kakuro puzzle 
 *
 * Copyright (C) 2012-2022 Anders Holst (anders.holst@ri.se) and Simon Tatham.
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
    COL_USER,
    COL_HIGHLIGHT,
    COL_ERROR,
    COL_ERROR_USER,
    COL_PENCIL,
    COL_ODDBG,
    COL_EVENBG,
    NCOLOURS
};

#define MAXBIT 0x80000
#define MAXNUM 20

typedef signed char digit;

struct game_params {
    int size;
    int oddeven_mode;
    int nosame_mode;
    int max;
    int diff;
};

struct clues {
    int refcount;
    int w, h;
    int oddeven;
    unsigned char *playable;
    int *hclues, *vclues;
    midend *me;
};

struct game_state {
    const game_params *par;
    struct clues *clues;
    digit *grid;
    long *pencil;               /* bitmaps using bits 1<<1..1<<n */
    int completed, cheated;
};

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);

    ret->size = 5;
    ret->oddeven_mode = 0;
    ret->nosame_mode = 0;
    ret->max = 9;
    ret->diff = 2;

    return ret;
}

const static struct game_params crossnum_presets[] = {
  {3, 0, 0, 9, 1},
  {5, 0, 0, 9, 2},
  {7, 0, 0, 9, 2},
  {7, 0, 0, 9, 3},
  {7, 0, 0, 9, 4},
  {7, 0, 0, 9, 5},
  {9, 0, 0, 9, 3},
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(crossnum_presets))
        return false;

    ret = snew(game_params);
    *ret = crossnum_presets[i]; /* structure copy */

    sprintf(buf, "%dx%d", ret->size+1, ret->size+1);
    if (ret->oddeven_mode)
      sprintf(buf+strlen(buf), ", odd/even");
    else if (ret->nosame_mode)
      sprintf(buf+strlen(buf), ", unique");
    sprintf(buf+strlen(buf), ", %s",
            (ret->diff==1 ? "easy" :
             ret->diff==2 ? "normal" :
             ret->diff==3 ? "tricky" :
             ret->diff==4 ? "hard" : "extreme"));

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
    *ret = *params;               /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
    char const *p = string;

    params->size = atoi(p)-1;
    while (*p && isdigit((unsigned char)*p)) p++;

    params->oddeven_mode = params->nosame_mode = 0;
    params->max = 9;
    params->diff = 3;
    if (*p == ',') {
      p++;
      params->max = atoi(p);
      while (*p && isdigit((unsigned char)*p)) p++;
    }
    if (*p == 'M')
      p++, params->oddeven_mode = 1;
    else if (*p == 'N')
      p++, params->nosame_mode = 1;
    if (*p == 'D') {
      p++;
      params->diff = atoi(p);
      if (params->diff > 5 || params->diff < 1) params->diff = 3;
      while (*p && isdigit((unsigned char)*p)) p++;
    }
}

static char *encode_params(const game_params *params, bool full)
{
    char ret[80];

    sprintf(ret, "%d,%d%sD%d", params->size+1, params->max,
            (params->oddeven_mode ? "M" : params->nosame_mode ? "N" : ""),
            params->diff);

    return dupstr(ret);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];
    int ind = 0;

    ret = snewn(5, config_item);

    ret[ind].name = "Grid size";
    ret[ind].type = C_STRING;
    sprintf(buf, "%d", params->size+1);
    ret[ind].u.string.sval = dupstr(buf);

    ind++;
    ret[ind].name = "Mode";
    ret[ind].type = C_CHOICES;
    ret[ind].u.choices.choicenames = ":Standard:Odd/even:Unique";
    ret[ind].u.choices.selected = (params->oddeven_mode ? 1 : params->nosame_mode ? 2 : 0);

    ind++;
    ret[ind].name = "Difficulty";
    ret[ind].type = C_CHOICES;
    ret[ind].u.choices.choicenames = ":Easy:Normal:Tricky:Hard:Extreme";
    ret[ind].u.choices.selected = params->diff-1;

    ind++;
    ret[ind].name = NULL;
    ret[ind].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);
    int ind = 0;

    ret->size = atoi(cfg[ind++].u.string.sval)-1;
    ret->max = 9;
    ret->oddeven_mode = (cfg[ind].u.choices.selected == 1);
    ret->nosame_mode = (cfg[ind].u.choices.selected == 2);
    ind++;
    ret->diff = cfg[ind++].u.choices.selected + 1;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->nosame_mode) {
      if (params->size < 3 || params->size > 9)
        return "In unique mode, grid size must be between 4 and 10";
    } else {
      if (params->size < 3 || params->size >= 12)
        return "Grid size must be between 4 and 12";
    }
    return NULL;
}

/* ----------------------------------------------------------------------
 * Game generation code.
 */

/* Alternative levels: Trivial 1.0-1.5, Normal 1.5-3.375, Hard 3.375-5.0, Extreme 10 */
const static float difflevels[] = {0.0, 1.25, 2.5, 3.75, 5.5, 10.0};

/* constants found by extensive empirical testing - don't change without verifying */
const static int iterlimit_normal[] = {0, 0, 0, 300, 400, 500, 600, 840, 1280, 1530, 6200, 11000, 15000};
const static int iterlimit_nosame[] = {0, 0, 0, 300, 400, 500, 600, 840, 2720, 3870, 13000, 0, 0};
const static int estlimit_normal[] = {0, 0, 0, 40, 40, 40, 40, 35, 50, 60, 65, 85, 100};
const static int estlimit_nosame[] = {0, 0, 0, 40, 40, 40, 40, 45, 80, 85, 105, 0, 0};


#define ITER_LIMIT (kb->par->nosame_mode ? iterlimit_nosame[kb->par->size] : iterlimit_normal[kb->par->size])
#define EST_LIMIT (kb->par->nosame_mode ? estlimit_nosame[kb->par->size] : estlimit_normal[kb->par->size])
#define GENBAD_LIMIT_0 2000
#define GENBAD_LIMIT 1000
#define MUTATION_RATE 1


typedef struct Slot {
    char n;
    long poss;
    long origmask;
    struct Run *run[2];
} Slot;

typedef struct Run {
    int s;
    int r;
    long poss;
    Slot **slots;
    int nslots;
    int dir; /* 0 vertical, 1 horizontal */
    int srem;
    int done;
} Run;

typedef struct CrossNumBoard {
    const game_params *par;
    Slot **slots;
    Run **runs;
    int nslots, nruns;
    char *candidate;
    long iter;
    long itermax;
    long quickret;
    long samesol;
    int oddeven;
    int phase;
    float estlimit;
    char* samecache;
    long* accvec;
} CrossNumBoard;

static int bit_count(long bits)
{
  int c;
  for (c=0; bits; c+=(bits&1), bits>>=1);
  return c;
}

static int bit_nth(long bits, int n)
{
  int c, i;
  for (c=-1, i=1; bits; i++, bits>>=1)
    if ((c+=(bits&1)) == n)
      return i;
  return -1;
}

static long bit_mk(int val)
{
  return (1L<<(val-1));
}

/*
static int bit_sum(long bits)
{
  int i, s=0;
  for (i=1; bits; i++, bits>>=1)
    if (bits&1)
       s+=i;
  return s;
}
*/

static void findrange(long bits, int sum, int ns, int* mn, int* mx)
{
  long b;
  int i, k, kk, s1, s2, n1, n2;
  if (ns == 1) {
    if (sum <= MAXNUM && (bits & bit_mk(sum)))
      *mn = *mx = sum;
    else
      *mn = 0, *mx = -1;
    return;
  } else {
    for (b=1, n1=1; b<=MAXBIT && !(bits&b); b<<=1, n1++);
    for (b=MAXBIT, n2=MAXNUM; b && !(bits&b); b>>=1, n2--);
    s1 = n1;
    s2 = n2;
    for (i=1, b>>=1, k=n2-1; b && i<ns-1; b>>=1, k--)
      if (bits&b) {
        s2 += k;
        i++;
      }
    if (k <= 0) {
      *mn = 0, *mx = -1;
      return;
    }
    if (sum-s2 <= n1)
      *mn = n1;
    else if (sum-s2 > k) {
      *mn = 0, *mx = -1;
      return;
    } else {
      kk = k, k = sum-s2;
      for (b=1L<<(k-1); k<=kk && !(bits&b); b<<=1, k++);
      if (k>kk) {
        *mn = 0, *mx = -1;
        return;
      }
      *mn = k;
    }
    for (k=n1+1, b=1L<<n1; k<=n2 && i<ns-1; b<<=1, k++)
      if (bits&b) {
        s1 += k;
        i++;
      }
    if (sum-s1 >= n2)
      *mx = n2;
    else if (sum-s1 < k) {
      *mn = 0, *mx = -1;
      return;
    } else {
      k = sum-s1;
      for (b=1L<<(k-1); k>=*mn && !(bits&b); b>>=1, k--);
      if (k<*mn) {
        *mn = 0, *mx = -1;
        return;
      }
      *mx = k;
    }
  }
}

static Slot *new_Slot(CrossNumBoard *kb, int ind)
{
    int i;
    Slot *sl = snew(Slot);
    const game_params *p = kb->par;

    sl->n = -1;
    if (p->oddeven_mode) {
      int odd = ((kb->oddeven == 2 ? 0 : 1) + ind + (p->size%2 ? 0 : ind/p->size))%2;
      sl->origmask = (odd ^ (p->max % 2) ? (1L<<p->max)/3 : (2L<<p->max)/3);
    } else
      sl->origmask = (1L << p->max) - 1;
    sl->poss = sl->origmask;
    for (i=0; i<2; i++)
        sl->run[i] = 0;

    return sl;
}

static Run *new_Run(int ns, int d, int nn)
{
    int i;
    Run *r = snew(Run);

    r->srem = r->nslots = ns;
    r->dir = d;
    r->s = r->r = nn;
    r->poss = 0;
    r->done = 0;
    r->slots = snewn(ns, Slot *);
    for (i=0; i<ns; i++)
        r->slots[i] = 0;

    return r;
}

static void delete_Run(Run *r)
{
    sfree(r->slots);
    sfree(r);
}

static void take(Slot* sl, int n)
{
  int i, j;
  long m;
  sl->n = n;
  m = bit_mk(n);
  for (i=0; i<2; i++)
    if (sl->run[i]) {
      sl->run[i]->srem--;
      sl->run[i]->r -= n;
      sl->run[i]->poss &= ~m;
      for (j=0; j<sl->run[i]->nslots; j++)
        if (sl->run[i]->slots[j] != sl)
          sl->run[i]->slots[j]->poss &= ~m;
    }
}

static void untake(Slot* sl)
{
  int i, j;
  long m;
  Slot* s0;
  m = bit_mk(sl->n);
  for (i=0; i<2; i++)
    if (sl->run[i]) {
      sl->run[i]->srem++;
      sl->run[i]->r += sl->n;
      sl->run[i]->poss |= m;
      for (j=0; j<sl->run[i]->nslots; j++) {
        s0 = sl->run[i]->slots[j];
        if (s0 != sl)
          s0->poss |= (s0->run[1-i] ? s0->run[1-i]->poss & s0->origmask & m : s0->origmask & m);
      }
    }
  sl->n = -1;
}

static void mi_setup(Run *r, const game_params* par, int *numind, int **si, int **ii, int **mm, int **mn, int **mx)
{
    int i, j;
    Run* r0;
    *numind = r->srem;
    *si = snewn(*numind, int);
    *ii = snewn(*numind, int);
    *mm = snewn(*numind, int);
    *mx = snewn(*numind, int);
    *mn = snewn(*numind, int);
    for (i=0, j=0; i<r->nslots; i++)
      if (r->slots[i]->n == -1) {
        r0 = r->slots[i]->run[1-r->dir];
        if (r0)
          findrange(r0->poss, r0->r, r0->srem, (*mn)+j, (*mx)+j);
        else
          (*mn)[j] = 1, (*mx)[j] = par->max;
        (*si)[j++] = i;
      }
}

static int mi_first(Run *r, const game_params* par, int numind, int *si, int *ii, int *mm, int *mn, int *mx, int kk)
{
  int i, j, k, bt = 0;
  long m;
  Slot *s0, *sl;
  Run* r0;
  if (kk >= numind)
    return 1;
  for (i=kk; i<numind; i++) {
    if (bt)
      bt = 0;
    else {
      findrange(r->poss, r->r, r->srem, ii+i, mm+i);
      if (ii[i] < mn[i]) ii[i] = mn[i];
      if (mm[i] > mx[i]) mm[i] = mx[i];
    }

    while (ii[i] <= mm[i] && !(r->slots[si[i]]->poss & bit_mk(ii[i])))
      ii[i]++;
    if (ii[i] <= mm[i]) {
/*      take(r->slots[si[i]], ii[i]);*/
      sl = r->slots[si[i]];
      sl->n = ii[i];
      m = bit_mk(ii[i]);
      for (k=0; k<2; k++)
        if ((r0 = sl->run[k])) {
          r0->srem--;
          r0->r -= sl->n;
          r0->poss &= ~m;
          for (j=0; j<r0->nslots; j++)
            if (r0->slots[j] != sl)
              r0->slots[j]->poss &= ~m;
        }
    } else {
      if (i == kk)
        return 0;
      else {
/*        untake(r->slots[si[i-1]]);*/
        sl = r->slots[si[i-1]];
        m = bit_mk(sl->n);
        for (k=0; k<2; k++)
          if ((r0 = sl->run[k])) {
            r0->srem++;
            r0->r += sl->n;
            r0->poss |= m;
            for (j=0; j<r0->nslots; j++) {
              s0 = r0->slots[j];
              if (s0 != sl)
                s0->poss |= (s0->run[1-k] ? s0->run[1-k]->poss & s0->origmask & m : s0->origmask & m);
            }
          }
        sl->n = -1;
        ii[i-1]++;
        i-=2;
        bt = 1;
      }
    }
  }
  return 1;
}

static int mi_next(Run *r, const game_params* par, int numind, int *si, int *ii, int *mm, int *mn, int *mx)
{
  int i, j, k;
  long m;
  Slot *s0, *sl;
  Run* r0;
  for (i=numind-1; i>=0; i--) {
/*    untake(r->slots[si[i]]); */
    sl = r->slots[si[i]];
    m = bit_mk(sl->n);
    for (k=0; k<2; k++)
      if ((r0 = sl->run[k])) {
        r0->srem++;
        r0->r += sl->n;
        r0->poss |= m;
        for (j=0; j<r0->nslots; j++) {
          s0 = r0->slots[j];
          if (s0 != sl)
            s0->poss |= (s0->run[1-k] ? s0->run[1-k]->poss & s0->origmask & m : s0->origmask & m);
        }
      }
    sl->n = -1;
    ii[i]++;
    while (ii[i] <= mm[i]) {
      if (sl->poss & bit_mk(ii[i])) {
/*        take(r->slots[si[i]], ii[i]);*/
        sl->n = ii[i];
        m = bit_mk(ii[i]);
        for (k=0; k<2; k++)
          if ((r0 = sl->run[k])) {
            r0->srem--;
            r0->r -= sl->n;
            r0->poss &= ~m;
            for (j=0; j<r0->nslots; j++)
              if (r0->slots[j] != sl)
                r0->slots[j]->poss &= ~m;
          }
        if (mi_first(r, par, numind, si, ii, mm, mn, mx, i+1)) {
          return 1;
        } else {
/*          untake(r->slots[si[i]]);*/
          for (k=0; k<2; k++)
            if ((r0 = sl->run[k])) {
              r0->srem++;
              r0->r += sl->n;
              r0->poss |= m;
              for (j=0; j<r0->nslots; j++) {
                s0 = r0->slots[j];
                if (s0 != sl)
                  s0->poss |= (s0->run[1-k] ? s0->run[1-k]->poss & s0->origmask & m : s0->origmask & m);
              }
            }
          sl->n = -1;
        }
      }
      ii[i]++;
    }
  }
  return 0;
}

static void mi_abort(Run *r, int numind, int *si)
{
    int i;
    for (i=numind-1; i>=0; i--) {
      untake(r->slots[si[i]]);
    }
}

static CrossNumBoard *new_CrossNumBoard(const game_params* p)
{
    CrossNumBoard *kb = snew(CrossNumBoard);
    kb->par = p;
    kb->slots = 0;
    kb->runs = 0;
    kb->nslots = 0;
    kb->nruns = 0;
    kb->candidate = snewn((p->size+1)*(p->size+1) + 2, char);
    kb->iter = 0;
    kb->quickret = 0;
    kb->phase = 0;
    kb->estlimit = 0;
    kb->oddeven = 0;
    kb->samesol = 0;
    kb->samecache = (p->nosame_mode ? snewn(1L << p->max, char) : 0);
    kb->accvec = snewn(p->size*p->size, long);
    return kb;
}

static void clean(CrossNumBoard *kb)
{
    int i;
    for (i=0; i<kb->nruns; i++)
        delete_Run(kb->runs[i]);
    for (i=0; i<kb->nslots; i++)
      if (kb->slots[i])
        sfree(kb->slots[i]);
    sfree(kb->runs);
    sfree(kb->slots);
    kb->nslots = 0;
    kb->nruns = 0;
    kb->runs = 0;
    kb->slots = 0;
    kb->oddeven = 0;
}

static void delete_CrossNumBoard(CrossNumBoard *kb)
{
    clean(kb);
    if (kb->candidate)
        sfree(kb->candidate);
    if (kb->samecache)
        sfree(kb->samecache);
    if (kb->accvec)
        sfree(kb->accvec);
    sfree(kb);
}

static void reset(CrossNumBoard *kb)
{
  int i;
  Run* r0;
  long mask = (1L<<kb->par->max) - 1;
  for (i=0; i<kb->nslots; i++)
    if (kb->slots[i]) {
      kb->slots[i]->n = -1;
      kb->slots[i]->poss = kb->slots[i]->origmask;
    }
  for (i=0; i<kb->nruns; i++) {
    r0 = kb->runs[i];
    r0->r = r0->s;
    r0->poss = mask;
    r0->srem = r0->nslots;
    r0->done = 0;
  }
}

static void check_connected_mark(Run* r, int oddeven)
{
  int i, m1=0, m2=0;
  r->done = 2;
  for (i=0; i<r->nslots; i++) {
    if (r->slots[i]->run[1-r->dir]) {
      if (!r->slots[i]->run[1-r->dir]->done)
        check_connected_mark(r->slots[i]->run[1-r->dir], oddeven);
    } else if (oddeven) {
      if (i%2)
        m1++;
      else
        m2++;
    } else
      m1++;
  }
  if (m1>1 || m2>1)
    r->done = 4;
}

static int check_correct_form(CrossNumBoard* kb)
{
  int i, maxlen, ok;
  int off1 = kb->par->size - 1;
  int off2 = kb->nslots - kb->par->size;
  for (i=0; i<kb->nslots; i++) {
    if (kb->slots[i]) {
      if (kb->slots[i]->run[0] == 0 && kb->slots[i]->run[1] == 0)
        return 0;
    } else if (i<off2 && (i+1)%kb->par->size) {
      if (!kb->slots[i+1] && !kb->slots[i+kb->par->size] && !kb->slots[i+kb->par->size+1])
        return 0;
    }
  }
  for (i=0; i<off1; i++) {
    if ((!kb->slots[i] && !kb->slots[i+1]) ||
        (!kb->slots[i+off2] && !kb->slots[i+off2+1]) ||
        (!kb->slots[i*kb->par->size] && !kb->slots[(i+1)*kb->par->size]) ||
        (!kb->slots[i*kb->par->size+off1] && !kb->slots[(i+1)*kb->par->size+off1]))
      return 0;
  }
  check_connected_mark(kb->runs[0], kb->par->oddeven_mode);
  ok = 1;
  maxlen = (kb->par->oddeven_mode ? kb->par->max-kb->par->max%2 : kb->par->max);
  for (i=0; i<kb->nruns; i++) {
    if (kb->runs[i]->done != 2 ||
        kb->runs[i]->nslots > maxlen)
      ok = 0;
    else if (kb->par->nosame_mode && kb->runs[i]->nslots == kb->par->max)
      maxlen = kb->par->max - 1;
    kb->runs[i]->done = 0;
  }
  return ok;
}

static void setup_runs(CrossNumBoard *kb)
{
    int i, j, k, cnt, cnt2;
    Run* r;
    for (i=0, cnt=0; i<kb->nslots; i++)
      if (!kb->slots[i])
        cnt++;
    kb->runs = snewn(2*(cnt+kb->par->size), Run *);
    cnt = 0;
    for (j=0; j<kb->par->size; j++) {
        cnt2 = 0;
        for (i=0; i<=kb->par->size; i++) {
            if (i==kb->par->size || kb->slots[i+kb->par->size*j] == 0) {
                if (cnt2>1) {
                    kb->runs[cnt++] = r = new_Run(cnt2, 1, 0);
                    for (k=0; k<cnt2; k++) {
                        r->slots[k] = kb->slots[i+kb->par->size*j-cnt2+k];
                        kb->slots[i+kb->par->size*j-cnt2+k]->run[1] = r;
                    }
                }
                cnt2 = 0;
            } else {
                cnt2++;
            }
        }
    }
    for (i=0; i<kb->par->size; i++) {
        cnt2 = 0;
        for (j=0; j<=kb->par->size; j++) {
            if (j==kb->par->size || kb->slots[i+kb->par->size*j] == 0) {
                if (cnt2>1) {
                    kb->runs[cnt++] = r = new_Run(cnt2, 0, 0);
                    for (k=0; k<cnt2; k++) {
                        r->slots[k] = kb->slots[i+kb->par->size*(j-cnt2+k)];
                        kb->slots[i+kb->par->size*(j-cnt2+k)]->run[0] = r;
                    }
                }
                cnt2 = 0;
            } else {
                cnt2++;
            }
        }
    }
    kb->nruns = cnt;
}

static void import_answer(CrossNumBoard *kb, const char *str)
{
    int i, j;
    char ch;
    Slot *s;
    int changed = 0;

    /* First make sure the structure is correct, then implant the numbers */ 
    for (i=0; i<kb->nslots; i++) {
        s = kb->slots[i];
        ch = str[i + i/kb->par->size + kb->par->size + 3];
        if (ch >= '0' && ch <= '0' + MAXNUM) {
          if (!s) {
            changed = 1;
            kb->slots[i] = new_Slot(kb, i);
          }
        } else {
          if (s) {
            changed = 1;
            sfree(s);
            kb->slots[i] = 0;
          }
        }
    }
    if (changed) {
      for (i=0; i<kb->nruns; i++)
        delete_Run(kb->runs[i]);
      sfree(kb->runs);
      for (i=0; i<kb->par->size*kb->par->size; i++) {
        if (kb->slots[i])
          for (j=0; j<2; j++)
            kb->slots[i]->run[j] = 0;
      }
      setup_runs(kb);
    }
    reset(kb);
    for (i=0; i<kb->nslots; i++) {
        s = kb->slots[i];
        ch = str[i + i/kb->par->size + kb->par->size + 3];
        if (s)
          take(s, ch - '0');
    }
    for (i=0; i<kb->nruns; i++) {
      kb->runs[i]->s = 0;
      for (j=0; j<kb->runs[i]->nslots; j++)
        kb->runs[i]->s += kb->runs[i]->slots[j]->n;
    }
}

static void export_answer(CrossNumBoard *kb, char *str)
{
    int i;
    Slot *s;
    str[0] = 'S';
    for (i=1; i<=(kb->par->size+1)*(kb->par->size+1); i++)
        str[i] = '\\';
    str[i] = 0;
    for (i=0; i<kb->nslots; i++) {
        s = kb->slots[i];
        if (s)
          str[i + i/kb->par->size + kb->par->size + 3] = '0' + s->n;
    }
}

typedef struct { int v, h; } pair;

static pair *get_clues(CrossNumBoard *kb)
{
    int i, j;
    int sz = (kb->par->size+1);
    int n = sz*sz;
    pair *clues = snewn(n, pair);
    for (j=0; j<sz; j++)
      for (i=0; i<sz; i++) {
        if (j>0 && i>0 && kb->slots[(i-1)+(sz-1)*(j-1)])
          clues[i+sz*j].h = clues[i+sz*j].v = -2;
        else {
        if (j>0 && i != sz-1 &&
            kb->slots[i+(sz-1)*(j-1)] && kb->slots[i+(sz-1)*(j-1)]->run[1])
          clues[i+sz*j].h = kb->slots[i+(sz-1)*(j-1)]->run[1]->s;
        else
          clues[i+sz*j].h = -1;
        if (i>0 && j != sz-1 &&
            kb->slots[(i-1)+(sz-1)*j] && kb->slots[(i-1)+(sz-1)*j]->run[0])
          clues[i+sz*j].v = kb->slots[(i-1)+(sz-1)*j]->run[0]->s;
        else 
          clues[i+sz*j].v = -1;
        }
      }
    return clues;
}

static void set_clues(CrossNumBoard *kb, struct clues* cl)
{
  int i, j, k;
  int cnt=0, cnt2;
  Run* r;
  int n = kb->par->size*kb->par->size;
  int sz = (kb->par->size+1);
  clean(kb);
  kb->oddeven = cl->oddeven;
  kb->nslots = n;
  kb->slots = snewn(n, Slot *);
  for (j=sz, i=0; i<n; i++, j++) {
    if (j%sz == 0) j++;
    if (!cl->playable[j]) {
      kb->slots[i] = 0;
      cnt++;
    } else {
      kb->slots[i] = new_Slot(kb, i);
    }
  }
  kb->runs = snewn(2*(cnt+kb->par->size), Run *);
  cnt = 0;
  for (j=0; j<sz; j++) {
    for (i=0; i<sz; i++) {
      if (cl->hclues[j*sz+i] != -1 && j>0) {
        for (k=i+1, cnt2=0; k<sz && cl->playable[j*sz+k]; k++, cnt2++);
        kb->runs[cnt++] = r = new_Run(cnt2, 1, cl->hclues[j*sz+i]);
        for (k=0; k<cnt2; k++) {
          r->slots[k] = kb->slots[(j-1)*(sz-1) + (i+k)];
          r->slots[k]->run[1] = r;
        }
      }
      if (cl->vclues[j*sz+i] != -1 && i>0) {
        for (k=j+1, cnt2=0; k<sz && cl->playable[k*sz+i]; k++, cnt2++);
        kb->runs[cnt++] = r = new_Run(cnt2, 0, cl->vclues[j*sz+i]);
        for (k=0; k<cnt2; k++) {
          r->slots[k] = kb->slots[(j+k)*(sz-1) + (i-1)];
          r->slots[k]->run[0] = r;
        }
      }
    }
  }          
  kb->nruns = cnt;
  reset(kb);
}

static void mutate_chain(CrossNumBoard* kb, random_state *rs, Slot* sl)
{
  int n1, nn1, n2, nn2, tmp, dir, i, j;
  int full;
  long tmpmask;
  Run* r0;
  Slot* s0;
  nn1 = sl->n;
  if (nn1 == -1) {
    full = 0;
    if (kb->par->oddeven_mode) {
      tmp = random_upto(rs, bit_count(sl->origmask));
      nn2 = bit_nth(sl->origmask, tmp);
    } else
      nn2 = random_upto(rs, kb->par->max) + 1;
  } else {
    full = 1;
    if (kb->par->oddeven_mode) {
      tmp = random_upto(rs, bit_count(sl->origmask) - 1);
      nn2 = bit_nth(sl->origmask & ~bit_mk(nn1), tmp);
    } else {
      nn2 = random_upto(rs, kb->par->max-1) + 1;
      if (nn1<=nn2) nn2+=1;
    }
    untake(sl);
  }
  for (i=0; i<2; i++) {
    s0 = sl;
    dir = i;
    n2 = nn2;
    if (full)
      n1 = nn1;
    else if (s0->run[dir]) {
      tmp = bit_count(s0->origmask & s0->run[dir]->poss);
      tmp = random_upto(rs, tmp);
      n1 = bit_nth(s0->origmask & s0->run[dir]->poss, tmp);
    } else {
      if (kb->par->oddeven_mode) {
        tmp = random_upto(rs, bit_count(s0->origmask));
        n1 = bit_nth(s0->origmask, tmp);
      } else
        n1 = random_upto(rs, kb->par->max);
    }
    while (1) {
      r0 = s0->run[dir];
      if (!r0 || (r0->poss & bit_mk(n2))) {
        if (s0 != sl)
          take(s0, n2);
        break;
      }
      for (j=0; j<r0->nslots && r0->slots[j]->n != n2; j++);
      if (j >= r0->nslots) {
        /* This should never happen if everything is correct */
        break;
      }
      untake(r0->slots[j]);
      if (s0 != sl)
        take(s0, n2);
      tmpmask = r0->slots[j]->poss & ~(bit_mk(n1) | bit_mk(n2));
      if (tmpmask) {
        if (full) {
          tmp = bit_count(tmpmask);
          tmp = random_upto(rs, tmp);
          tmp = bit_nth(tmpmask, tmp);
          take(r0->slots[j], tmp);
        }
        break;
      }
      tmp = n2, n2 = n1, n1 = tmp;
      dir = 1-dir;
      s0 = r0->slots[j];
    }
  }
  take(sl, nn2);
}

static void randomize_squares(CrossNumBoard *kb, random_state *rs, long *av)
{
  int i, j, mn, mncnt, tmp;
  while (1) {
    mn = 32;
    mncnt = 0;
    for (i=0; i<kb->nslots; i++) {
      if (!kb->slots[i] || kb->slots[i]->n != -1) continue;
      tmp = bit_count(kb->slots[i]->poss);
      if (tmp < mn) {
        mn = tmp;
        mncnt = 1;
      } else if (tmp == mn)
        mncnt++;
    }
    if (mncnt == 0)
      break;
    j = random_upto(rs, mncnt);
    for (i=0; i<kb->nslots; i++) {
      if (!kb->slots[i] || kb->slots[i]->n != -1) continue;
      tmp = bit_count(kb->slots[i]->poss);
      if (tmp == mn) {
        if (j == 0) {
          break;
        } else
          j--;
      }
    }
    if (mn == 0) {
      mutate_chain(kb, rs, kb->slots[i]);
    } else {
      long ltmp;
      if (av && (ltmp = kb->slots[i]->poss & ~av[i])) {
        j = random_upto(rs, bit_count(ltmp));
        j = bit_nth(ltmp, j);
      } else {
        j = random_upto(rs, mn);
        /* Set the j:th slot with mn possibilities and set it to its n:th possible value */
        j = bit_nth(kb->slots[i]->poss, j);
      }
      take(kb->slots[i], j);
    }
  }
}

static void mutate_squares(CrossNumBoard* kb, random_state *rs, int m, long* av)
{
  int i, j, k, ns;
  for (ns=0, i=0; i<kb->nslots; i++)
    if (kb->slots[i])
      ns++;
  for (j=0; j<m; j++) {
    i = random_upto(rs, ns-j);
    for (k=0; k<kb->nslots; k++)
      if (kb->slots[k] && kb->slots[k]->n != -1) {
        if (i == 0)
          break;
        i--;
      }
    untake(kb->slots[k]);
  }
  randomize_squares(kb, rs, av);
}

static int mutate_structure(CrossNumBoard* kb, random_state *rs, int pos)
{
  int i, j, k, cnt;
  long mask, confl;
  Run** newruns;
  if (kb->slots[pos]) {
    Run *r0, *r1;
    /* first, check that it seems ok */
    if (((pos+1)%kb->par->size && !kb->slots[pos+1]) ||
        (pos%kb->par->size && !kb->slots[pos-1]) || 
        (pos < kb->par->size*(kb->par->size-1) && !kb->slots[pos+kb->par->size]) ||
        (pos >= kb->par->size && !kb->slots[pos-kb->par->size]))
      return 0;
    /* add a clue square */
    newruns = snewn(kb->nruns + 2, Run *);
    mask = (1L<<kb->par->max) - 1;
    for (i=0, cnt=0; i<kb->nruns; i++)
      if (kb->runs[i] != kb->slots[pos]->run[0] && kb->runs[i] != kb->slots[pos]->run[1])
        newruns[cnt++] = kb->runs[i];
    for (j=0; j<2; j++) {
      r0 = kb->slots[pos]->run[j];
      if (!r0) continue;
      for (k=0; k<r0->nslots && r0->slots[k] != kb->slots[pos]; k++);
      if (k>1) {
        r1 = new_Run(k, j, 0);
        r1->srem = 0;
        r1->poss = mask;
        for (i=0; i<k; i++) {
          r1->slots[i] = r0->slots[i];
          r1->slots[i]->run[r0->dir] = r1;
          r1->poss &= ~bit_mk(r1->slots[i]->n);
        }
        for (i=0; i<k; i++) {
          r1->slots[i]->poss = ((r1->slots[i]->origmask &
                                 r1->poss &
                                 (r1->slots[i]->run[1-j] ? r1->slots[i]->run[1-j]->poss : -1)) |
                                bit_mk(r1->slots[i]->n));
        }
        newruns[cnt++] = r1;
      } else if (k==1)
        r0->slots[0]->run[r0->dir] = 0; 
      if (k<r0->nslots-2) {
        r1 = new_Run(r0->nslots-k-1, j, 0);
        r1->srem = 0;
        r1->poss = mask;
        for (i=0; i<r0->nslots-k-1; i++) {
          r1->slots[i] = r0->slots[i+k+1];
          r1->slots[i]->run[r0->dir] = r1;
          r1->poss &= ~bit_mk(r1->slots[i]->n);
        }
        for (i=0; i<r0->nslots-k-1; i++) {
          r1->slots[i]->poss = ((r1->slots[i]->origmask &
                                 r1->poss &
                                 (r1->slots[i]->run[1-j] ? r1->slots[i]->run[1-j]->poss : -1)) |
                                bit_mk(r1->slots[i]->n));
        }
        newruns[cnt++] = r1;
      } else if (k==r0->nslots-2)
        r0->slots[r0->nslots-1]->run[r0->dir] = 0; 
      delete_Run(kb->slots[pos]->run[j]);
    }
    sfree(kb->runs);
    kb->runs = newruns;
    kb->nruns = cnt;
    sfree(kb->slots[pos]);
    kb->slots[pos] = 0;
    return 1;
  } else {
    Run *r0, *r1, *r2;
    Slot *s1, *s2, *s3, *s4;
    int mxlen = (kb->par->oddeven_mode ? kb->par->max-kb->par->max%2 : kb->par->max);
    mask = (1L<<kb->par->max) - 1;
    s1 = (pos >= kb->par->size ? kb->slots[pos-kb->par->size] : 0);
    s2 = (pos < kb->par->size*(kb->par->size-1) ? kb->slots[pos+kb->par->size] : 0);
    s3 = (pos%kb->par->size ? kb->slots[pos-1] : 0);
    s4 = ((pos+1)%kb->par->size ? kb->slots[pos+1] : 0);
    /* Check resulting run length */
    if ((1 + (!s1 ? 0 : s1->run[0] ? s1->run[0]->nslots : 1) + (!s2 ? 0 : s2->run[0] ? s2->run[0]->nslots : 1) > mxlen) ||
        (1 + (!s3 ? 0 : s3->run[1] ? s3->run[1]->nslots : 1) + (!s4 ? 0 : s4->run[1] ? s4->run[1]->nslots : 1) > mxlen))
      return 0;
    /* remove a clue square */
    newruns = snewn(kb->nruns + 2, Run *);
    for (i=0, cnt=0; i<kb->nruns; i++)
      if (kb->runs[i] != (s1 ? s1->run[0] : 0) && kb->runs[i] != (s2 ? s2->run[0] : 0) &&
          kb->runs[i] != (s3 ? s3->run[1] : 0) && kb->runs[i] != (s4 ? s4->run[1] : 0))
        newruns[cnt++] = kb->runs[i];
    kb->slots[pos] = new_Slot(kb, pos);
    for (j=0; j<2; j++, s1=s3, s2=s4) {
      if (s1 || s2) {
        r1 = (s1 ? s1->run[j] : 0);
        r2 = (s2 ? s2->run[j] : 0);
        r0 = new_Run(1 + (r1 ? r1->nslots : s1 ? 1 : 0) + (r2 ? r2->nslots : s2 ? 1 : 0), j, 0);
        r0->poss = mask;
        k = 0;
        if (r1)
          for (i=0; i<r1->nslots; i++, k++)
            r0->slots[k] = r1->slots[i];
        else if (s1)
          r0->slots[k++] = s1;
        r0->slots[k++] = kb->slots[pos];
        if (r2)
          for (i=0; i<r2->nslots; i++, k++)
            r0->slots[k] = r2->slots[i];
        else if (s2)
          r0->slots[k++] = s2;
        confl = mask & ((r1 ? ~r1->poss : s1 && s1->n != -1 ? bit_mk(s1->n) : 0) &
                        (r2 ? ~r2->poss : s2 && s2->n != -1 ? bit_mk(s2->n) : 0));
        for (i=0; i<k; i++) {
          if (r0->slots[i]->n != -1) {
            if (confl & bit_mk(r0->slots[i]->n))
              untake(r0->slots[i]);
            else
              r0->poss &= ~bit_mk(r0->slots[i]->n);
          }
          r0->slots[i]->run[j] = r0;
        }
        for (i=0; i<k; i++) {
          r0->slots[i]->poss = ((r0->slots[i]->origmask &
                                 r0->poss &
                                 (r0->slots[i]->run[1-j] ? r0->slots[i]->run[1-j]->poss : -1)) |
                                (r0->slots[i]->n != -1 ? bit_mk(r0->slots[i]->n) : 0));
        }
        r0->srem = 0;
        newruns[cnt++] = r0;
        if (r1)
          delete_Run(r1);
        if (r2)
          delete_Run(r2);
      } else
        kb->slots[pos]->run[j] = 0; 
    }
    sfree(kb->runs);
    kb->runs = newruns;
    kb->nruns = cnt;
    randomize_squares(kb, rs, 0);
    return 1;
  }
}

static int contains_same(CrossNumBoard *kb)
{
  int i;
  for (i=0; i<kb->nruns; i++)
    kb->samecache[kb->runs[i]->poss] = 0;
  for (i=0; i<kb->nruns; i++)
    if (kb->samecache[kb->runs[i]->poss]++)
      return 1;
  return 0;
}

static void eliminate_same(CrossNumBoard *kb, random_state *rs)
{
  int i, ii, j1, off, change, lim = 50;
  do {
    change = 0;
    for (i=0; i<kb->nruns; i++)
      kb->samecache[kb->runs[i]->poss] = 0;
    for (i=0; i<kb->nruns; i++)
      kb->samecache[kb->runs[i]->poss]++;
    off = random_upto(rs, kb->nruns);
    for (ii=0,i=off; ii<kb->nruns; ii++,i=(ii+off)%kb->nruns)
      if (kb->samecache[kb->runs[i]->poss] > 1) {
        change = 1;
        for (j1=0; j1<kb->runs[i]->nslots; j1++)
          if (kb->runs[i]->slots[j1]->n != -1)
            untake(kb->runs[i]->slots[j1]);
        randomize_squares(kb, rs, 0);
        break;
      }
  } while (change && lim--);
}

static void maybe_split_run(CrossNumBoard *kb, random_state *rs, int nsq, int pos, int dir)
{
  int tmp;
  while (nsq > kb->par->max) {
    tmp = min(nsq-2, kb->par->max+1);
    tmp = nsq - 2 - random_upto(rs, tmp-2);
    if (dir) {
      sfree(kb->slots[pos-tmp]);
      kb->slots[pos-tmp] = 0;
    } else {
      sfree(kb->slots[pos-tmp*kb->par->size]);
      kb->slots[pos-tmp*kb->par->size] = 0;
    }
    nsq = tmp-1;
  }
}

static void randomize_structure(CrossNumBoard *kb, random_state *rs)
{
  int i, j, k, ncl, nsq;
  int n = kb->par->size*kb->par->size;
  while (1) {
    /* Randomize the structure */
    ncl = (kb->par->size < 5 ? 0 : kb->par->size*2-9);
    if (n>5)
      ncl = ncl + random_upto(rs, n/5 - ncl);
    for (j=0; j<ncl; j++) {
      i = random_upto(rs, n);
      if (kb->slots[i] == 0) {
        j--;
        continue;
      }
      sfree(kb->slots[i]);
      kb->slots[i] = 0;
    }
    if (kb->par->size > kb->par->max) {
      /* Make sure no run is too long */
      for (i=0,k=0; i<kb->par->size; i++) {
        nsq = 0;
        for (j=0; j<kb->par->size; j++,k++) {
          if (kb->slots[k])
            nsq++;
          else {
            maybe_split_run(kb, rs, nsq, k, 1);
            nsq = 0;
          }
        }
        maybe_split_run(kb, rs, nsq, k, 1);
      }
      for (i=0,k=i; i<kb->par->size; i++,k=i) {
        nsq = 0;
        for (j=0; j<kb->par->size; j++,k+=kb->par->size) {
          if (kb->slots[k])
            nsq++;
          else {
            maybe_split_run(kb, rs, nsq, k, 0);
            nsq = 0;
          }
        }
        maybe_split_run(kb, rs, nsq, k, 0);
      }
    }
    setup_runs(kb);
    if (check_correct_form(kb))
      break;
    for (i=0; i<kb->nruns; i++)
      delete_Run(kb->runs[i]);
    sfree(kb->runs);
    for (i=0; i<n; i++) {
      if (!kb->slots[i])
        kb->slots[i] = new_Slot(kb, i);
      else {
        for (j=0; j<2; j++)
          kb->slots[i]->run[j] = 0;
      }
    }
  }
}

static char* randomize_answer(CrossNumBoard *kb, random_state *rs)
{
  int i, j;
  int n = kb->par->size*kb->par->size;
  clean(kb);
  kb->nslots = n;
  kb->slots = snewn(n, Slot *);
  if (kb->par->oddeven_mode)
    kb->oddeven = 1 + random_upto(rs, 2);
  for (i=0; i<n; i++)
    kb->slots[i] = new_Slot(kb, i);
  randomize_structure(kb, rs);
  /* Fill in the slots */
  reset(kb);
  randomize_squares(kb, rs, 0);

  if (kb->par->nosame_mode && contains_same(kb))
    eliminate_same(kb, rs);

  for (i=0; i<kb->nruns; i++) {
    kb->runs[i]->s = 0;
    for (j=0; j<kb->runs[i]->nslots; j++)
      kb->runs[i]->s += kb->runs[i]->slots[j]->n;
  }
  
  export_answer(kb, kb->candidate);
  return dupstr(kb->candidate);
}

static char *mutate_answer(CrossNumBoard *kb, random_state *rs, int m, long* av)
{
  int i, j, lim=3;

  if (random_upto(rs, 20) < 2) {
    i = random_upto(rs, kb->nslots);
    while (lim && !mutate_structure(kb, rs, i))
      lim--;
    i = random_upto(rs, kb->nslots);
    while (lim && !mutate_structure(kb, rs, i))
      lim--;
  } else {
    mutate_squares(kb, rs, m, av);
  }
  if (kb->par->nosame_mode && contains_same(kb))
    eliminate_same(kb, rs);

  for (i=0; i<kb->nruns; i++) {
    kb->runs[i]->s = 0;
    for (j=0; j<kb->runs[i]->nslots; j++)
      kb->runs[i]->s += kb->runs[i]->slots[j]->n;
  }
  export_answer(kb, kb->candidate);
  return dupstr(kb->candidate);
}

static long count_possibilities(const game_params* par, Run* run, long limit)
{
  int numind;
  int* si;
  int* ii;
  int* mm;
  int* mn;
  int* mx;
  long count = 0;
  mi_setup(run, par, &numind, &si, &ii, &mm, &mn, &mx);
  if (!mi_first(run, par, numind, si, ii, mm, mn, mx, 0)) {
    sfree(si);
    sfree(ii);
    sfree(mm);
    sfree(mn);
    sfree(mx);
    return count;
  }
  do {
    count++;
    if (count > limit) {
      mi_abort(run, numind, si);
      break;
    }
  } while (mi_next(run, par, numind, si, ii, mm, mn, mx));
  sfree(si);
  sfree(ii);
  sfree(mm);
  sfree(mn);
  sfree(mx);
  return count;
}

static float estimate_possibilities(const game_params* par, Run* run)
{
  int mn0, mx0, mn, mx, len, k, kk;
  float prod;
  int* ranges;
  Run* r;
  findrange(run->poss, run->s, run->nslots, &mn0, &mx0);
  ranges = snewn(run->nslots, int);
  for (k=0; k<run->nslots; k++) {
    r = run->slots[k]->run[1-run->dir]; 
    if (r)
      findrange(r->poss, r->s, r->nslots, &mn, &mx);
    else
      mn = 1, mx = par->max;
    len = min(mx, mx0) - max(mn, mn0) + 1;
    for (kk=k-1; kk>=0 && ranges[kk] > len; kk--)
      ranges[kk+1] = ranges[kk];
    ranges[kk+1] = len;
  }
  len = mx0 - mn0 + 1;
  prod = 1.0;
  for (k=0; k<run->nslots-1; k++, len--)
    prod *= min(len, ranges[k]);
  sfree(ranges);
  return log(prod);
}

static Run *select_run(CrossNumBoard *kb)
{
    int i, best = -1;
    for (i=0; i<kb->nruns; i++) {
        if (!kb->runs[i]->done &&
            (best == -1 || kb->runs[i]->srem < kb->runs[best]->srem))
            best = i;
    }
    if (best == -1)
        return 0;
    else if (kb->phase == 3) {
      int b, i;
      long m, mn;
      b = best;
      mn = m = count_possibilities(kb->par, kb->runs[best], 10000);
      for (i=0; i<kb->nruns; i++) {
        if (i == best) continue;
        if (kb->runs[i]->done) continue;
        m = count_possibilities(kb->par, kb->runs[i], mn);
        if (m < mn) mn = m, b = i;
      }
      return kb->runs[b];
    } else
      return kb->runs[best];
}

static long count_internal(CrossNumBoard *kb)
{
  int numind, i;
    int *si;
    int *ii;
    int *mm;
    int* mn;
    int* mx;
    long sol, s;
    Run *run = select_run(kb);
    if (!run) {
      if (kb->par->nosame_mode && contains_same(kb)) {
        /* invalid solution with two equal number sets */
        kb->samesol++;
        return 0;
      } else {
        /* we have a solution and are done */
        for (i=0; i<kb->nslots; i++)
          if (kb->slots[i])
            kb->accvec[i] |= bit_mk(kb->slots[i]->n);
        export_answer(kb, kb->candidate);
        return 1;
      }
    }
    mi_setup(run, kb->par, &numind, &si, &ii, &mm, &mn, &mx);
    if (!mi_first(run, kb->par, numind, si, ii, mm, mn, mx, 0)) {
        sfree(si);
        sfree(ii);
        sfree(mm);
        sfree(mn);
        sfree(mx);
        return 0;
    }
    sol = 0;
    run->done = 1;
    do {
        s = count_internal(kb);
        kb->iter++;
        if (s >= kb->itermax || kb->iter >= kb->itermax) {
          sol = kb->itermax;
          mi_abort(run, numind, si);
          break;
        } else
          sol += s;
        if (kb->quickret && sol > kb->quickret) {
          mi_abort(run, numind, si);
          break;
        }
    } while (mi_next(run, kb->par, numind, si, ii, mm, mn, mx));
    run->done = 0;
    sfree(si);
    sfree(ii);
    sfree(mm);
    sfree(mn);
    sfree(mx);
    return sol;
}

static void count_solutions(CrossNumBoard *kb, const char *str, long limit, long *sol, long *it)
{
    int i, nsok = 1;
    float lp = 0.0;
    kb->iter = 0;
    kb->quickret = limit;
    for (i=0; i<kb->nslots; i++)
      kb->accvec[i] = 0;
    import_answer(kb, str);
    if (kb->par->nosame_mode) {
      kb->samesol = 0;
      nsok = !contains_same(kb);
    }
    if (!check_correct_form(kb) || (kb->phase != 1 && !nsok)) {
      *sol = -1;
    } else if (kb->phase == 1) {
      reset(kb);
      for (i=0; i<kb->nruns; i++)
        lp += estimate_possibilities(kb->par, kb->runs[i]);
      if (lp < kb->estlimit && lp*128 + kb->itermax < limit && nsok) {
        *sol = count_internal(kb);
        if (*sol < kb->itermax) {
          if (kb->par->nosame_mode) {
            if (kb->samesol < *sol) *sol = 2 * *sol - kb->samesol;
            if (*sol > kb->itermax/2) *sol = (*sol+kb->itermax)/3;
          }
          kb->phase = 2;
        } else
          *sol = kb->itermax + (int)(lp*128);
      } else
        *sol = kb->itermax + (int)(lp*128);
    } else {
      reset(kb);
      *sol = count_internal(kb);
      if (*sol < kb->itermax && kb->par->nosame_mode) {
        if (kb->samesol < *sol) *sol = 2 * *sol - kb->samesol;
        if (*sol > kb->itermax/2) *sol = (*sol+kb->itermax)/3;
      }
    }
    *it = kb->iter;
}

static int diffcloser(float diff2, float diff1, int level)
{
  float mid = (difflevels[level] + difflevels[level-1])*0.5;
  return ((diff2 > mid ? diff2 - mid : mid - diff2) < (diff1 > mid ? diff1 - mid : mid - diff1));
}

static int diffwithin(float diff, int level)
{
  return (diff >= difflevels[level-1] && diff <= difflevels[level]);
}

static pair *simple_evolve(random_state *rs, const game_params *par, char** answer, int* oddeven, float* hard)
{
    CrossNumBoard *kb = new_CrossNumBoard(par);
    char *vec1, *vec2;
    long val1, val2;
    long iter1, iter2;
    float diff1, diff2;
    int gen, genbad;
    pair *ret;
    long* accvec1;
    int i, notyet;

    accvec1 = snewn(kb->par->size*kb->par->size, long);
    vec1 = randomize_answer(kb, rs);
    vec2 = NULL;
    kb->phase = 1;
    kb->itermax = ITER_LIMIT;
    kb->estlimit = EST_LIMIT;
    notyet = 1;
    count_solutions(kb, vec1, 0, &val1, &iter1);
    diff1 = ((float)iter1) / (kb->nruns * sqrt(par->nosame_mode ? kb->samesol + val1 : val1));
    for (i=0; i<kb->nslots; i++)
      accvec1[i] = kb->accvec[i];
    gen = 1;
    genbad = 0;
    while (val1 != 1 || notyet) {
        gen++;
        if (val1 <= 0)
          vec2 = randomize_answer(kb, rs);
        else {
          import_answer(kb, vec1);
          vec2 = mutate_answer(kb, rs, MUTATION_RATE, kb->phase == 1 ? 0 : accvec1);
          while (!strcmp(vec1, vec2)) {
            sfree(vec2);
            vec2 = mutate_answer(kb, rs, MUTATION_RATE, kb->phase == 1 ? 0 : accvec1);
          }
        }
        count_solutions(kb, vec2, (val1 < 0 ? 0 : val1), &val2, &iter2);
        diff2 = ((float)iter2) / (kb->nruns * sqrt(par->nosame_mode ? kb->samesol + val2 : val2));
        if (val2 > 0 && (val1 < 0 || val2 < val1 ||
                         (val2 == 1 && diffcloser(diff2, diff1, par->diff)) ||
                         (val2 == val1 && iter2 < iter1))) {
            sfree(vec1);
            val1 = val2, iter1 = iter2;
            diff1 = diff2;
            vec1 = vec2;
            for (i=0; i<kb->nslots; i++)
              accvec1[i] = kb->accvec[i];
            genbad = 0;
            if (val1 == 1 && !diffwithin(diff1, par->diff))
              notyet = 1;
            else
              notyet = 0;
        } else {
            sfree(vec2);
            genbad++;
        }
        if (kb->phase == 2 && val1 < 10) {
          kb->phase = 3;
        }
        if (genbad >= (kb->phase == 1 ? GENBAD_LIMIT_0 : GENBAD_LIMIT)) {
          genbad = 0;
          gen += 1;
          sfree(vec1);
          vec1 = randomize_answer(kb, rs);
          kb->phase = 1;
          kb->estlimit = EST_LIMIT;
          count_solutions(kb, vec1, 0, &val1, &iter1);
          notyet = 1;
        }
    }
    *hard = (par->nosame_mode ? ((float)iter1) / (kb->nruns * sqrt(kb->samesol + 1)) : ((float)iter1) / kb->nruns);
    ret = get_clues(kb);
    *answer = dupstr(kb->candidate);
    *oddeven = (par->oddeven_mode ? kb->oddeven : 0);
    sfree(vec1);
    sfree(accvec1);
    delete_CrossNumBoard(kb);
    return ret;
}

static char *new_game_desc(const game_params *params, random_state *rs,
               char **aux, bool interactive)
{
    pair *clues;
    char *buf, *p, *ret;
    float hard;
    int oe;
    int n, i, run;

    clues = simple_evolve(rs, params, aux, &oe, &hard);

    n = (params->size+1) * (params->size+1);
    buf = snewn(n * 24, char);

    p = buf;
    if (params->oddeven_mode) {
      if (oe == 2)
        *p++ = 'E';
      else
        *p++ = 'O';
    }
    run = 0;
    for (i = 0; i < n+1; i++) {
        if (i < n && clues[i].h == -2 && clues[i].v == -2) {
            run++;
        } else {
            while (run > 0) {
                int thisrun = run > 26 ? 26 : run;
                *p++ = 'a' + (thisrun-1);
                run -= thisrun;
            }
            if (i < n) {
                if (clues[i].h != -1 && clues[i].v != -1)
                    p += sprintf(p, "B%d.%d", clues[i].h, clues[i].v);
                else if (clues[i].h != -1)
                    p += sprintf(p, "H%d", clues[i].h);
                else if (clues[i].v != -1)
                    p += sprintf(p, "V%d", clues[i].v);
                else
                    *p++ = 'X';
            }
        }
    }
    *p = '\0';

    ret = dupstr(buf);

    sfree(buf);
    sfree(clues);
    return ret;
}

/* ----------------------------------------------------------------------
 * Main game UI.
 */

static const char *validate_desc(const game_params *params, const char *desc)
{
    int wanted = (params->size+1) * (params->size+1);
    int n = 0;

    if (params->oddeven_mode) {
      if (*desc != 'O' && *desc != 'E')
        return "Expected 'O' or 'E' in the beginning when oddeven mode";
      desc++;
    }
    while (n < wanted && *desc) {
        char c = *desc++;
        if (c == 'X') {
            n++;
        } else if (c >= 'a' && c < 'z') {
            n += 1 + (c - 'a');
        } else if (c == 'B') {
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            if (*desc != '.')
                return "Expected a '.' after number following 'B'";
            desc++;
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            n++;
        } else if (c == 'H' || c == 'V') {
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            n++;
        } else {
            return "Unexpected character in grid description";
        }
    }

    if (n > wanted || *desc)
        return "Too much data to fill grid";
    else if (n < wanted)
        return "Not enough data to fill grid";

    return NULL;
}

static game_state *new_game(midend *me, const game_params *params, const char *desc)
{
    game_state *state = snew(game_state);
    int w, h, wh, i, n;

    w = h = params->size + 1;
    wh = w*h;
    state->par = params;
    state->grid = snewn(wh, digit);
    state->pencil = snewn(wh, long);
    for (i = 0; i < wh; i++) {
        state->grid[i] = -1;
        state->pencil[i] = 0;
    }
    state->completed = state->cheated = false;

    state->clues = snew(struct clues);
    state->clues->refcount = 1;
    state->clues->w = w;
    state->clues->h = h;
    state->clues->oddeven = 0;
    state->clues->playable = snewn(wh, unsigned char);
    state->clues->hclues = snewn(wh, int);
    state->clues->vclues = snewn(wh, int);
    state->clues->me = me;

    n = 0;

    if (params->oddeven_mode) {
      if (*desc == 'O')
        state->clues->oddeven = 1;
      else if (*desc == 'E')
        state->clues->oddeven = 2;
      desc++;
    }
    while (n < wh && *desc) {
        char c = *desc++;
        if (c == 'X') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = state->clues->vclues[n] = -1;
            n++;
        } else if (c >= 'a' && c < 'z') {
            int k = 1 + (c - 'a');
            while (k-- > 0) {
                state->clues->playable[n] = true;
                state->clues->hclues[n] = state->clues->vclues[n] = -1;
                n++;
            }
        } else if (c == 'B') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = atoi(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            assert(*desc == '.');
            desc++;
            state->clues->vclues[n] = atoi(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;            
            n++;
        } else if (c == 'H') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = atoi(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            state->clues->vclues[n] = -1;
            n++;
        } else if (c == 'V') {
            state->clues->playable[n] = false;
            state->clues->hclues[n] = -1;
            state->clues->vclues[n] = atoi(desc);
            while (*desc && isdigit((unsigned char)*desc))
                desc++;
            n++;
        } else {
            assert(!"This should never happen");
        }
    }

    assert(!*desc);
    assert(n == wh);

    return state;
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = snew(game_state);
    int w, wh, i;

    w = state->par->size+1;
    wh = w*w;
    ret->par = state->par;
    ret->grid = snewn(wh, digit);
    ret->pencil = snewn(wh, long);
    for (i = 0; i < wh; i++) {
        ret->grid[i] = state->grid[i];
        ret->pencil[i] = state->pencil[i];
    }
    ret->completed = state->completed;
    ret->cheated = state->cheated;

    ret->clues = state->clues;
    ret->clues->refcount++;

    return ret;
}

static void free_game(game_state *state)
{
    if (--state->clues->refcount <= 0) {
        sfree(state->clues->playable);
        sfree(state->clues->hclues);
        sfree(state->clues->vclues);
        sfree(state->clues);
    }
    sfree(state->grid);
    sfree(state->pencil);
    sfree(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
            const char *aux, const char **error) {
    if (aux)
        return dupstr(aux);
    else {
        CrossNumBoard *kb = new_CrossNumBoard(state->par);
        long sol;
        kb->iter = 0;
        kb->quickret = 2;
        kb->itermax = 10000000;
        set_clues(kb, state->clues);
        if (!check_correct_form(kb)) {
            *error = "Game is not correctly formed";
            return NULL;
        }
        sol = count_internal(kb);
        if (sol>0) {
            return dupstr(kb->candidate);
        } else {
            *error = "No solution found";
            return NULL;
        }
    }
}

struct game_ui {
    /*
     * These are the coordinates of the currently highlighted
     * square on the grid, if hshow = 1.
     */
    int hx, hy;
    /*
     * This indicates whether the current highlight is a
     * pencil-mark one or a real one.
     */
    int hpencil;
    /*
     * This indicates whether or not we're showing the highlight
     * (used to be hx = hy = -1); important so that when we're
     * using the cursor keys it doesn't keep coming back at a
     * fixed position. When hshow = 1, pressing a valid number
     * or letter key or Space will enter that number or letter in the grid.
     */
    int hshow;
    /*
     * This contains the number which gets highlighted when the user
     * presses a number when the UI is not in highlight mode.
     * 0 means that no number is currently highlighted.
     * -1 means that the erase button is currently highlighted.
     */
    int hhint;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    ui->hx = ui->hy = 0;
    ui->hpencil = ui->hshow = 0;
    ui->hhint = 0;
    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static key_label *game_request_keys(const game_params *params, const game_ui *ui, int *nkeys)
{
    int i;
    int w = params->max;

    key_label *keys = snewn(w+2, key_label);
    *nkeys = w + 2;

    for (i = 0; i < w; i++) {
        if (i<9) keys[i].button = '1' + i;
        else keys[i].button = 'a' + i - 9;

        keys[i].label = NULL;
    }
    keys[w].button = '\b';
    keys[w].label = NULL;
    keys[w+1].button = '+';
    keys[w+1].label = "Add";

    return keys;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
    return;
}

static const char *current_key_label(const game_ui *ui,
                                     const game_state *state, int button){
    if (button == '\b' && ui->hhint == -1) return "H";
    if (button < '0' || button > '9') return "";
    return ((button-'0') == ui->hhint) ? "H" : "E";
}

#define PREFERRED_TILESIZE 48
#define TILESIZE (ds->tilesize)
#define BORDER (TILESIZE / 2)
#define GRIDEXTRA max((TILESIZE / 32),1)
#define COORD(x) ((x)*TILESIZE + BORDER)
#define FROMCOORD(x) (((x)+(TILESIZE-BORDER)) / TILESIZE - 1)

#define DF_PENCIL_SHIFT 11
#define DF_ERR_HCLUE 0x0800
#define DF_ERR_VCLUE 0x0400
#define DF_ERR_SLOT 0x0c00
#define DF_HIGHLIGHT 0x0200
#define DF_HIGHLIGHT_PENCIL 0x0100
#define DF_DIGIT_MASK 0x007F
#define DF_HAS_DIGIT_MASK 0x0080

struct game_drawstate {
    int tilesize;
    int w, h;
    int started;
    long *tiles;
    long *errors;
};

static char* make_move_string(const game_state *state, int cx, int cy, int n, bool mode)
{
    char buf[80];
    int sz = state->par->size + 1;
    if (state->par->oddeven_mode && n != -1) {
        int odd = ((state->clues->oddeven == 2 ? 0 : 1) + cx + cy)%2;
        if (odd ^ (n%2))
            return MOVE_UI_UPDATE;
    }
    if (n != -1 && (n > state->par->max || n == 0))
        return MOVE_UI_UPDATE;
    if (n == -1 && (state->grid[cy*sz+cx] == -1) && (state->pencil[cy*sz+cx] == 0))
        return MOVE_UI_UPDATE;
    sprintf(buf, "%c%d,%d,%d", (char)(mode ? 'P' : 'R'), cx, cy, n);
    return dupstr(buf);
}

static char *interpret_move(const game_state *state, game_ui *ui, const game_drawstate *ds,
                int x, int y, int button, bool swapped)
{
    int sz = state->par->size + 1;
    int n;
    int tx, ty;

    button &= ~MOD_MASK;

    tx = FROMCOORD(x);
    ty = FROMCOORD(y);

    if (tx >= 0 && tx < sz && ty >= 0 && ty < sz) {
        if (((button == LEFT_RELEASE && !swapped) || 
            (button == LEFT_BUTTON && swapped)) &&
            (ui->hhint != 0)) {
            if (!state->clues->playable[ty*sz+tx])
                return NULL;
            ui->hshow = 0;
            return make_move_string(state, tx, ty, ui->hhint, false);
        }
        else if (button == LEFT_BUTTON) {
            if (ui->hhint != 0) {
                ui->hshow = 0;
            }
            else if (tx == ui->hx && ty == ui->hy && ui->hshow && ui->hpencil == 0) {
                /* left-click on the already active square */
                ui->hshow = 0;
            } else {
                /* left-click on a new square */
                ui->hx = tx;
                ui->hy = ty;
                ui->hshow = state->clues->playable[ty*sz+tx];
                ui->hpencil = 0;
            }
            return MOVE_UI_UPDATE;
        }
        if (button == RIGHT_BUTTON) {
            if (ui->hhint != 0) {
                if (!state->clues->playable[ty*sz+tx])
                    return NULL;
                return make_move_string(state, tx, ty, ui->hhint, true);
            }
            else if (tx == ui->hx && ty == ui->hy && ui->hshow && ui->hpencil) {
                /* right-click on the already active square */
                ui->hshow = 0;
                ui->hpencil = 0;
            } else {
                /* right-click on a new square */
                ui->hx = tx;
                ui->hy = ty;
                if (state->grid[ty*sz+tx] != -1 ||
                  !state->clues->playable[ty*sz+tx]) {
                    ui->hshow = 0;
                    ui->hpencil = 0;
                } else {
                    ui->hshow = 1;
                    ui->hpencil = 1;
                }
            }
            return MOVE_UI_UPDATE;
        }
    } else if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        ui->hshow = 0;
        ui->hpencil = 0;
        ui->hhint = 0;
        return MOVE_UI_UPDATE;
    }

    if (ui->hshow &&
    ((button >= '0' && button <= '9') || (button == '\b'))) {
        /*
         * Can't make pencil marks in a filled square. This can only
         * become highlighted if we're using cursor keys.
         */
        if (ui->hpencil && state->grid[ui->hy*sz+ui->hx] != -1)
            return NULL;

    /*
     * Can't do anything to an immutable square.
     */
        if (!state->clues->playable[ui->hy*sz+ui->hx])
            return NULL;
        if (button == '\b') n = -1;
        else n = button - '0';
        if (ui->hpencil == 0) ui->hshow = 0;
        return make_move_string(state, ui->hx, ui->hy, n, ui->hpencil);
    }
    if (!ui->hshow && ((button >= '0' && button <= '9') || (button == '\b'))) {
        int n = button - '0';
        if (button == '\b') n = -1;
        if (ui->hhint == n) ui->hhint = 0;
        else ui->hhint = n;
        return MOVE_UI_UPDATE;
    }
    if (button == '+') {
        return dupstr("M");
    }
    return MOVE_UNUSED;
}

typedef struct sameentry {
  long mask;
  int pos;
  struct sameentry* next;
} sameentry;

static sameentry* register_mask(long m, int p, sameentry* list)
{
  sameentry* se2;
  sameentry* se = snew(sameentry);
  se->mask = m;
  se->pos = p;
  if (!list || list->mask >= m) {
    se->next = list;
    return se;
  } else {
    for (se2 = list; se2->next && se2->next->mask < m; se2 = se2->next);
    se->next = se2->next;
    se2->next = se;
    return list;
  }
}

static bool check_errors(const game_state *state, long *errors)
{
    int sz = state->par->size + 1, a = sz*sz;
    int x, y;
    int ret = false;
    sameentry* samelist;

    if (errors)
        for (x = 0; x < a; x++) errors[x] = 0;

    samelist = 0;

    for (y = 0; y < sz; y++) {
        for (x = 0; x < sz; x++) {
            if (!state->clues->playable[y*sz+x] &&
                state->clues->hclues[y*sz+x] >= 0) {
                int clue = state->clues->hclues[y*sz+x];
                int error = false;
                int unfilled = false;
                long mask = 0;
                int xx, xxx;
                for (xx = x+1; xx<sz && state->clues->playable[y*sz+xx]; xx++) {
                    int d = state->grid[y*sz+xx];
                    if (d == -1) {
                        unfilled = true;    /* an unfilled square exists */
                    } else {
                      if (mask & bit_mk(d)) {
                        error = true;
                        if (errors)
                          for (xxx = x+1; xxx<sz && state->clues->playable[y*sz+xxx]; xxx++)
                            if (state->grid[y*sz+xxx] == d)
                              errors[y*sz+xxx] |= DF_ERR_SLOT;
                      }
                      mask |= bit_mk(d);
                      if (clue < d) {
                        error = true;
                        if (errors)
                          errors[y*sz+x] |= DF_ERR_HCLUE;
                      } else
                        clue -= d;
                    }
                }
                if (!unfilled && state->par->nosame_mode)
                  samelist = register_mask(mask, (y*sz+x), samelist);
                if (!unfilled && clue != 0) {
                  error = true;
                  if (errors)
                    errors[y*sz+x] |= DF_ERR_HCLUE;
                }
                if (error || unfilled)
                  ret = true;
            }
        }
    }

    for (x = 0; x < sz; x++) {
        for (y = 0; y < sz; y++) {
            if (!state->clues->playable[y*sz+x] &&
                state->clues->vclues[y*sz+x] >= 0) {
                int clue = state->clues->vclues[y*sz+x];
                int error = false;
                int unfilled = false;
                long mask = 0;
                int yy, yyy;
                for (yy = y+1; yy<sz && state->clues->playable[yy*sz+x]; yy++) {
                    int d = state->grid[yy*sz+x];
                    if (d == -1) {
                        unfilled = true;    /* an unfilled square exists */
                    } else {
                      if (mask & bit_mk(d)) {
                        error = true;
                        if (errors)
                          for (yyy = y+1; yyy<sz && state->clues->playable[yyy*sz+x]; yyy++)
                            if (state->grid[yyy*sz+x] == d)
                              errors[yyy*sz+x] |= DF_ERR_SLOT;
                      }
                      mask |= bit_mk(d);
                      if (clue < d) {
                        error = true;
                        if (errors)
                          errors[y*sz+x] |= DF_ERR_VCLUE;
                      } else
                        clue -= d;
                    }
                }
                if (!unfilled && state->par->nosame_mode)
                  samelist = register_mask(mask, -(y*sz+x), samelist);
                if (!unfilled && clue != 0) {
                  error = true;
                  if (errors)
                    errors[y*sz+x] |= DF_ERR_VCLUE;
                }
                if (error || unfilled)
                    ret = true;
            }
        }
    }

    if (state->par->nosame_mode) {
      int i;
      long lastmask;
      sameentry *se, *se2;
      for (se=samelist; se && se->next;) {
        if (se->mask == se->next->mask) {
          ret = true;
          lastmask = se->mask;
          if (errors) {
            for (; se && se->mask == lastmask; se=se->next) {
              if (se->pos > 0)
                for (i = se->pos+1; i%sz && state->clues->playable[i]; i++)
                  errors[i] |= DF_ERR_SLOT;
              else
                for (i = -se->pos+sz; i<a && state->clues->playable[i]; i+=sz)
                  errors[i] |= DF_ERR_SLOT;
            }
          } else
            break;
        } else
          se = se->next;
      }
      for (se2=0, se=samelist; se; se = se2) {
        se2 = se->next;
        sfree(se);
      }
    }

    return ret;
}

static game_state *execute_move(const game_state *from0, const char *move)
{
    game_state* from = (game_state*) from0;
    int sz = from->par->size + 1, a = sz*sz;
    game_state *ret;
    int x, y, i, n;

    if (move[0] == 'S') {
        ret = dup_game(from);
        ret->completed = ret->cheated = true;

        for (i = 0; i < a; i++) {
            if (!from->clues->playable[i])
                continue;
            if (move[i+1] < '0' || move[i+1] > '0'+9) {
                free_game(ret);
                return from;
            }
            ret->grid[i] = move[i+1] - '0';
            /* ret->pencil[i] = 0; */
        }

        if (move[a+1] != '\0') {
            free_game(ret);
            return from;
        }

        return ret;
    } else if ((move[0] == 'P' || move[0] == 'R') &&
        sscanf(move+1, "%d,%d,%d", &x, &y, &n) == 3 &&
        x >= 0 && x < sz && y >= 0 && y < sz && n >= -1 && n <= 9) {
        if (!from->clues->playable[y*sz+x])
            return from;

        ret = dup_game(from);
        if (move[0] == 'P') {
            if (n == -1)
                ret->pencil[y*sz+x] = 0;
            else if (n > 0)
                ret->pencil[y*sz+x] ^= 1L << n;
        } else {
            ret->grid[y*sz+x] = n;
        }
        ret->completed = !check_errors(ret, NULL);
        ret->cheated = false;
        return ret;
    } else if (move[0] == 'M') {
    /*
     * Fill in absolutely all pencil marks everywhere.
     */
        ret = dup_game(from);
        if (from->par->oddeven_mode) {
            long oddmask = ((1L<<from->par->max)/3) << 1;
            long evenmask = ((2L<<from->par->max)/3) << 1;
            for (x = 0; x < sz; x++)
            for (y = 0; y < sz; y++) {
                if (ret->grid[y*sz+x] == -1)
                    ret->pencil[y*sz+x] = (
                        ((from->clues->oddeven == 2 ? 0 : 1) + x + y)%2 ?
                             evenmask : oddmask);
            }
        } else {
            long mask = (2L<<from->par->max) - 2;
            for (i = 0; i < a; i++) {
                if (ret->grid[i] == -1)
                    ret->pencil[i] = mask;
            }
        }
        return ret;
    } else
        return from;               /* couldn't parse move string */
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

#define SIZE(w) (((w)+1) * TILESIZE + 2*BORDER)

static void game_compute_size(const game_params *params, int tilesize,
                              const game_ui *ui, int *x, int *y)
{
    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = *y = SIZE(params->size);
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
        ret[COL_BACKGROUND * 3 + i] = 1.0F;
        ret[COL_GRID       * 3 + i] = 0.0F;
        ret[COL_USER       * 3 + i] = 0.0F;
        ret[COL_HIGHLIGHT  * 3 + i] = 0.5F;
        ret[COL_ERROR      * 3 + i] = 0.25F;
        ret[COL_ERROR_USER * 3 + i] = 0.75F;
        ret[COL_PENCIL     * 3 + i] = 0.25F;
        ret[COL_EVENBG     * 3 + i] = 0.75F;
        ret[COL_ODDBG      * 3 + i] = 1.0F;
    }

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    struct game_drawstate *ds = snew(struct game_drawstate);
    int sz = state->par->size + 1, a = sz*sz;
    int i;

    ds->tilesize = 0;
    ds->w = sz;
    ds->h = sz;
    ds->started = false;
    ds->tiles = snewn(a, long);
    for (i = 0; i < a; i++)
    ds->tiles[i] = -1;
    ds->errors = snewn(a, long);

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->tiles);
    sfree(ds->errors);
    sfree(ds);
}

static void draw_tile(drawing *dr, game_drawstate *ds, const game_params *par,
              struct clues *clues, int x, int y, long tile)
{
    int sz = ds->w;
    int tx, ty, tw, th;
    int cx, cy, cw, ch;
    char str[64];

    tx = BORDER + x * TILESIZE + 1 + GRIDEXTRA;
    ty = BORDER + y * TILESIZE + 1 + GRIDEXTRA;

    cx = tx;
    cy = ty;
    cw = tw = TILESIZE-1-2*GRIDEXTRA;
    ch = th = TILESIZE-1-2*GRIDEXTRA;

    clip(dr, cx, cy, cw, ch);

    /* background needs erasing */
    draw_rect(dr, cx, cy, cw, ch, COL_BACKGROUND);
    if ((tile & DF_ERR_HCLUE) || (tile & DF_ERR_VCLUE) || (tile & DF_ERR_SLOT)) {
        if (clues->playable[y*sz+x])
            draw_rect(dr, cx, cy, cw, ch, (tile & DF_HIGHLIGHT) ? COL_HIGHLIGHT : COL_ERROR);
        else {
            int hclue = clues->hclues[y*sz+x], vclue = clues->vclues[y*sz+x];
            if ((x == 0 && hclue >= 0) || (y == 0 && vclue >= 0)) {
                draw_rect(dr, cx, cy, cw, ch, COL_ERROR);
            }
            else {
                int coords[6];
                if ((hclue >= 0) && (tile & DF_ERR_HCLUE)) {
                    coords[0] = cx; coords[1] = cy;
                    coords[2] = cx+cw; coords[3] = cy;
                    coords[4] = cx+cw; coords[5] = cy+ch;
                    draw_polygon(dr, coords, 3, COL_ERROR, COL_ERROR);
                }
                if ((vclue >= 0) && (tile & DF_ERR_VCLUE)) {
                    coords[0] = cx; coords[1] = cy;
                    coords[2] = cx; coords[3] = cy+ch;
                    coords[4] = cx+cw; coords[5] = cy+ch;
                    draw_polygon(dr, coords, 3, COL_ERROR, COL_ERROR);
                }
            }
        }
    }
    else if (par->oddeven_mode && clues->playable[y*sz+x] && !(tile & DF_HIGHLIGHT)) {
        int odd = ((clues->oddeven == 2 ? 0 : 1) + x + y)%2;
        draw_rect(dr, cx, cy, cw, ch, odd ? COL_ODDBG : COL_EVENBG);
    } else if (tile & DF_HIGHLIGHT) {
        draw_rect(dr, cx, cy, cw, ch, COL_HIGHLIGHT);
    }

    /* pencil-mode highlight */
    if (tile & DF_HIGHLIGHT_PENCIL) {
        int coords[6];
        coords[0] = cx;
        coords[1] = cy;
        coords[2] = cx+cw/2;
        coords[3] = cy;
        coords[4] = cx;
        coords[5] = cy+ch/2;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);
    }

    if (!clues->playable[y*sz+x]) {
        /*
         * This is an unplayable square, so draw clue(s) or
         * crosshatching.
         */
        int hclue = clues->hclues[y*sz+x], vclue = clues->vclues[y*sz+x];
        char hcbuf[20], vcbuf[20];
        int c[12];
        int i, hfs, vfs;
        float fw = (float)cw/12.0; float fw2 = (float)cw/24.0;
        float fh = (float)ch/12.0; float fh2 = (float)ch/24.0;

        sprintf(hcbuf, "%d", hclue);
        sprintf(vcbuf, "%d", vclue);
        hfs = 9*ch/24;
        vfs = 9*ch/24;
        if (hclue < 0 && vclue < 0) {
            /* unplayable square without clue - crosshatch it */
            for (i=1;i<=11;i+=2) {
                c[0] = cx;          c[1] = cy+i*fh;
                c[2] = cx+i*fw;     c[3] = cy;
                c[4] = cx+(i+1)*fw; c[5] = cy;
                c[6] = cx;          c[7] = cy+(i+1)*fh;
                draw_polygon(dr, c, 4, COL_GRID, COL_GRID);
            }
            for (i=1;i<=11;i+=2) {
                c[0] = cx+i*fw;     c[1] = cy+12*fh;
                c[2] = cx+12*fw;    c[3] = cy+i*fh;
                c[4] = cx+12*fw;    c[5] = cy+(i+1)*fh;
                c[6] = cx+(i+1)*fw; c[7] = cy+12*fh;
                draw_polygon(dr, c, 4, COL_GRID, COL_GRID);
            }
        } else if (x == 0 && hclue >= 0) {
            /* left edge horizontal clue, center it */
            draw_text(dr, cx+cw/2, cy+ch/2, FONT_FIXED, hfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_HCLUE ? COL_ERROR_USER : COL_GRID), hcbuf);
        } else if (y == 0 && vclue >= 0) {
            /* top edge vertical clue, center it */
            draw_text(dr, cx+cw/2, cy+ch/2, FONT_FIXED, vfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_VCLUE ? COL_ERROR_USER : COL_GRID), vcbuf);
        } else {
          /* Divide square in two parts, horizontal clue upper and vertical lower */
            c[0] = cx;        c[1] = cy;
            c[2] = cx+fw2;    c[3] = cy;
            c[4] = cx+cw;     c[5] = cy+ch-fh2;
            c[6] = cx+cw;     c[7] = cy+ch;
            c[8] = cx+cw-fw2; c[9] = cy+ch;
            c[10] = cx;       c[11] = cy+fh2;
            draw_polygon(dr, c, 6, COL_GRID, COL_GRID);

          if (hclue >= 0) {
            draw_text(dr, cx+3*cw/4-GRIDEXTRA, cy+ch/4, FONT_FIXED, hfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_HCLUE ? COL_ERROR_USER : COL_GRID), hcbuf);
          } else {
            for (i=1;i<=11;i+=2) {
                c[0] = cx+i*fw;      c[1] = cy;
                c[2] = cx+i*fw2;     c[3] = cy+i*fh2;
                c[4] = cx+(i+1)*fw2; c[5] = cy+(i+1)*fh2;
                c[6] = cx+(i+1)*fw;  c[7] = cy;
                draw_polygon(dr, c, 4, COL_GRID, COL_GRID);
            }
            for (i=1;i<=11;i+=2) {
                c[0] = cx+12*fw;      c[1] = cy+i*fh;
                c[2] = cx+(12+i)*fw2; c[3] = cy+(12+i)*fh2;
                c[4] = cx+(13+i)*fw2; c[5] = cy+(13+i)*fh2;
                c[6] = cx+12*fw;      c[7] = cy+(i+1)*fh;
                draw_polygon(dr, c, 4, COL_GRID, COL_GRID);
            }
          }
          if (vclue >= 0) {
            draw_text(dr, cx+cw/4+GRIDEXTRA, cy+3*ch/4, FONT_FIXED, vfs,
                      ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_VCLUE ? COL_ERROR_USER : COL_GRID), vcbuf);
          } else {
            for (i=1;i<=11;i+=2) {
                c[0] = cx;           c[1] = cy+i*fh;
                c[2] = cx+i*fw2;     c[3] = cy+i*fh2;
                c[4] = cx+(i+1)*fw2; c[5] = cy+(i+1)*fh2;
                c[6] = cx;           c[7] = cy+(i+1)*fh;
                draw_polygon(dr, c, 4, COL_GRID, COL_GRID);
            }
            for (i=1;i<=11;i+=2) {
                c[0] = cx+i*fw;      c[1] = cy+12*fh;
                c[2] = cx+(12+i)*fw2; c[3] = cy+(12+i)*fh2;
                c[4] = cx+(13+i)*fw2; c[5] = cy+(13+i)*fh2;
                c[6] = cx+(i+1)*fw;  c[7] = cy+12*fh;
                draw_polygon(dr, c, 4, COL_GRID, COL_GRID);
            }
          }
        }
    } else {
        /*
         * This is a playable square, so draw a user-entered number or
         * pencil mark.
         */

        /* new number needs drawing? */
        if (tile & DF_HAS_DIGIT_MASK) {
            sprintf(str, "%d", (int)(tile & DF_DIGIT_MASK));
            draw_text(dr, tx + cw/2, ty + ch/2,
                      FONT_VARIABLE, TILESIZE/2, ALIGN_VCENTRE | ALIGN_HCENTRE,
                      (tile & DF_ERR_SLOT ? 
                      (tile & DF_HIGHLIGHT ? COL_ERROR : 
                                             COL_ERROR_USER) : 
                                             COL_USER), str);
        } /* else if ((tile & DF_DIGIT_MASK) && !(tile & DF_HIGHLIGHT_PENCIL)) {
            sprintf(str, "%d_", (int)(tile & DF_DIGIT_MASK));
            draw_text(dr, tx + cw/2, ty + ch/2,
                      FONT_VARIABLE, TILESIZE/2, ALIGN_VCENTRE | ALIGN_HCENTRE,
                      COL_USER, str);
        } */ else {
            long rev;
            int i, j, npencil;
            int pl, pr, pt, pb;
            float vhprop;
            int ok, pw, ph, minph, minpw, pgsizex, pgsizey, fontsize;

            /* Count the pencil marks required. */
            if ((tile & DF_DIGIT_MASK)) {
                rev = 1L << ((tile & DF_DIGIT_MASK) + DF_PENCIL_SHIFT);
            } else
                rev = 0;
            /* for (i = 1, npencil = 0; i <= MAXNUM; i++)
                if ((tile ^ rev) & (1L << (i + DF_PENCIL_SHIFT)))
                    npencil++;
            if (tile & DF_DIGIT_MASK)
                npencil++; */
            npencil = 9;
            if (npencil) {

                minph = 2;
                minpw = (par->max > 9 ? 2 : 3);
                vhprop = (par->max > 9 ? 1.5 : 1.0);
                /*
                 * Determine the bounding rectangle within which we're going
                 * to put the pencil marks.
                 */
                pl = tx + GRIDEXTRA;
                pr = pl + TILESIZE - GRIDEXTRA;
                pt = ty + GRIDEXTRA;
                pb = pt + TILESIZE - GRIDEXTRA - 2; 

                /*
                 * We arrange our pencil marks in a grid layout, with
                 * the number of rows and columns adjusted to allow the
                 * maximum font size.
                 *
                 * So now we work out what the grid size ought to be.
                 */
                pw = minpw, ph = minph;
                ok = 0;
                for (fontsize=(pb-pt)/minph; fontsize>1 && !ok; fontsize--) {
                  pw = (pr - pl)/(int)(fontsize*vhprop+0.5);
                  ph = (pb - pt)/fontsize;
                  ok = (pw >= minpw && ph >= minph && npencil <= pw*ph && pw*vhprop>=ph);
                }
                pgsizey = fontsize;
                pgsizex = (int)(fontsize*vhprop+0.5);

                /*
                 * Centre the resulting figure in the square.
                 */
                pl = tx + (TILESIZE - pgsizex * pw) / 2;
                pt = ty + (TILESIZE - pgsizey * ph - 2) / 2;

                /*
                 * Now actually draw the pencil marks.
                 */
                for (i = 1, j = 0; i <= 9; i++)
                    if ((tile ^ rev) & (1L << (i + DF_PENCIL_SHIFT))) {
                        int dx = (i-1) % pw, dy = (i-1) / pw;
                        sprintf(str, "%d", i);
                        draw_text(dr, pl + pgsizex * (2*dx+1) / 2,
                                  pt + pgsizey * (2*dy+1) / 2,
                                  FONT_VARIABLE, fontsize,
                                  ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL,
                                  str);
                        j++;
                    }
                /* if (tile & DF_DIGIT_MASK) {
                  int dx = j % pw, dy = j / pw;
                  sprintf(str, "%d_", (int)(tile & DF_DIGIT_MASK));
                  draw_text(dr, pl + pgsizex * (2*dx+1) / 2,
                            pt + pgsizey * (2*dy+1) / 2,
                            FONT_VARIABLE, fontsize,
                            ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL,
                            str);
                } */
            }
        }
    }

    unclip(dr);

    draw_update(dr, cx, cy, cw, ch);
}

static void game_redraw(drawing *dr, game_drawstate *ds, const game_state *oldstate,
            const game_state *state, int dir, const game_ui *ui,
            float animtime, float flashtime)
{
    int sz = state->par->size + 1;
    int x, y;
    char buf[48];

    sprintf(buf, "%s",
            state->cheated   ? "Auto-solved." :
            state->completed ? "COMPLETED!" : "");
    status_bar(dr, buf);

    if (!ds->started) {
    /*
     * The initial contents of the window are not guaranteed and
     * can vary with front ends. To be on the safe side, all
     * games should start by drawing a big background-colour
     * rectangle covering the whole window.
     */
    draw_rect(dr, 0, 0, SIZE(sz), SIZE(sz), COL_BACKGROUND);

    /*
     * Big containing rectangle.
     */
    draw_rect(dr, COORD(0) - GRIDEXTRA, COORD(0) - GRIDEXTRA,
          sz*TILESIZE+1+GRIDEXTRA*2, sz*TILESIZE+1+GRIDEXTRA*2,
          COL_GRID);

    draw_update(dr, 0, 0, SIZE(sz), SIZE(sz));

    ds->started = true;
    }

    if (animtime)
        return;

    check_errors(state, ds->errors);

    for (y = 0; y < sz; y++) {
    for (x = 0; x < sz; x++) {
        long tile = 0L;

        tile = (long)state->pencil[y*sz+x] << DF_PENCIL_SHIFT;
        if (state->grid[y*sz+x] != -1)
            tile = state->grid[y*sz+x] | DF_HAS_DIGIT_MASK;

        if (ui->hshow && ui->hx == x && ui->hy == y) {
            tile |= (ui->hpencil ? DF_HIGHLIGHT_PENCIL : DF_HIGHLIGHT);
        }

        tile |= ds->errors[y*sz+x];

        if (ds->tiles[y*sz+x] != tile) {
            ds->tiles[y*sz+x] = tile;
            draw_tile(dr, ds, state->par, state->clues, x, y, tile);
        }
    }
    }
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
#define thegame crossnum
#endif
static const char rules[] = "Fill the grid with numbers like a crossword puzzle. The sum of each line must match the given horizontal / vertical clues. The same number cannot occur more than once in each sum.\n\n\n"
"These variations are available:\n\n"
"- Odd/Even: White cells can only have an odd number, grey cells only an even number.\n"
"- Unique: No two sums can contain the same numbers.\n"
"\n\nThis puzzle was implemented by Anders Holst.";

const struct game thegame = {
    "CrossNum", "games.crossnum", "crossnum", rules,
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
