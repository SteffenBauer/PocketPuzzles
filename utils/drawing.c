/*
 * drawing.c: Intermediary between the drawing interface as
 * presented to the back end, and that implemented by the front
 * end.
 * 
 * Mostly just looks up calls in a vtable and passes them through
 * unchanged. However, on the printing side it tracks print colours
 * so the front end API doesn't have to.
 * 
 * FIXME:
 * 
 *  - I'd _like_ to do automatic draw_updates, but it's a pain for
 *    draw_text in particular. I'd have to invent a front end API
 *    which retrieved the text bounds.
 *     + that might allow me to do the alignment centrally as well?
 *        * perhaps not, because PS can't return this information,
 *          so there would have to be a special case for it.
 *     + however, that at least doesn't stand in the way of using
 *       the text bounds for draw_update, because PS doesn't need
 *       draw_update since it's printing-only. Any _interactive_
 *       drawing API couldn't get away with refusing to tell you
 *       what parts of the screen a text draw had covered, because
 *       you would inevitably need to erase it later on.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "puzzles.h"

struct drawing {
    const drawing_api *api;
    void *handle;
    float scale;
    /* `me' is only used in status_bar(), so print-oriented instances of
     * this may set it to NULL. */
    midend *me;
    char *laststatus;
};

drawing *drawing_new(const drawing_api *api, midend *me, void *handle)
{
    drawing *dr = snew(drawing);
    dr->api = api;
    dr->handle = handle;
    dr->scale = 1.0F;
    dr->me = me;
    dr->laststatus = NULL;
    return dr;
}

void drawing_free(drawing *dr)
{
    sfree(dr->laststatus);
    sfree(dr);
}

void draw_text(drawing *dr, int x, int y, int fonttype, int fontsize,
               int align, int colour, const char *text)
{
    dr->api->draw_text(dr->handle, x, y, fonttype, fontsize, align,
                       colour, text);
}

void draw_rect(drawing *dr, int x, int y, int w, int h, int colour)
{
    dr->api->draw_rect(dr->handle, x, y, w, h, colour);
}

void draw_line(drawing *dr, int x1, int y1, int x2, int y2, int colour)
{
    dr->api->draw_line(dr->handle, x1, y1, x2, y2, colour);
}

void draw_thick_line(drawing *dr, float thickness,
                     float x1, float y1, float x2, float y2, int colour)
{
    if (thickness < 1.0F)
        thickness = 1.0F;
    if (dr->api->draw_thick_line) {
        dr->api->draw_thick_line(dr->handle, thickness,
                                 x1, y1, x2, y2, colour);
    } else {
        /* We'll fake it up with a filled polygon.  The tweak to the
         * thickness empirically compensates for rounding errors, because
         * polygon rendering uses integer coordinates.
         */
        float len = sqrt((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
        float tvhatx = (x2 - x1)/len * (thickness/2 - 0.2F);
        float tvhaty = (y2 - y1)/len * (thickness/2 - 0.2F);
        int p[8];

        p[0] = x1 - tvhaty;
        p[1] = y1 + tvhatx;
        p[2] = x2 - tvhaty;
        p[3] = y2 + tvhatx;
        p[4] = x2 + tvhaty;
        p[5] = y2 - tvhatx;
        p[6] = x1 + tvhaty;
        p[7] = y1 - tvhatx;
        dr->api->draw_polygon(dr->handle, p, 4, colour, colour);
    }
}

void draw_polygon(drawing *dr, const int *coords, int npoints,
                  int fillcolour, int outlinecolour)
{
    dr->api->draw_polygon(dr->handle, coords, npoints, fillcolour,
                          outlinecolour);
}

void draw_circle(drawing *dr, int cx, int cy, int radius,
                 int fillcolour, int outlinecolour)
{
    dr->api->draw_circle(dr->handle, cx, cy, radius, fillcolour,
                         outlinecolour);
}

void draw_update(drawing *dr, int x, int y, int w, int h)
{
    if (dr->api->draw_update)
        dr->api->draw_update(dr->handle, x, y, w, h);
}

void clip(drawing *dr, int x, int y, int w, int h)
{
    dr->api->clip(dr->handle, x, y, w, h);
}

void unclip(drawing *dr)
{
    dr->api->unclip(dr->handle);
}

void start_draw(drawing *dr)
{
    dr->api->start_draw(dr->handle);
}

void end_draw(drawing *dr)
{
    dr->api->end_draw(dr->handle);
}

char *text_fallback(drawing *dr, const char *const *strings, int nstrings)
{
    int i;

    /*
     * If the drawing implementation provides one of these, use it.
     */
    if (dr && dr->api->text_fallback)
        return dr->api->text_fallback(dr->handle, strings, nstrings);

    /*
     * Otherwise, do the simple thing and just pick the first string
     * that fits in plain ASCII. It will then need no translation
     * out of UTF-8.
     */
    for (i = 0; i < nstrings; i++) {
        const char *p;

        for (p = strings[i]; *p; p++)
            if (*p & 0x80)
                break;
        if (!*p)
            return dupstr(strings[i]);
    }

    /*
     * The caller was responsible for making sure _some_ string in
     * the list was in plain ASCII.
     */
    assert(!"Should never get here");
    return NULL;               /* placate optimiser */
}

void status_bar(drawing *dr, const char *text)
{
    char *rewritten;

    if (!dr->api->status_bar)
        return;

    assert(dr->me);

    rewritten = midend_rewrite_statusbar(dr->me, text);
    if (!dr->laststatus || strcmp(rewritten, dr->laststatus)) {
        dr->api->status_bar(dr->handle, rewritten);
        sfree(dr->laststatus);
        dr->laststatus = rewritten;
    } else {
        sfree(rewritten);
    }
}

char *get_statustext(drawing *dr) {
    return dr->laststatus;
}

blitter *blitter_new(drawing *dr, int w, int h)
{
    return dr->api->blitter_new(dr->handle, w, h);
}

void blitter_free(drawing *dr, blitter *bl)
{
    dr->api->blitter_free(dr->handle, bl);
}

void blitter_save(drawing *dr, blitter *bl, int x, int y)
{
    dr->api->blitter_save(dr->handle, bl, x, y);
}

void blitter_load(drawing *dr, blitter *bl, int x, int y)
{
    dr->api->blitter_load(dr->handle, bl, x, y);
}

