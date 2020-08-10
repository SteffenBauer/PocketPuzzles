#ifndef POCKETPUZZLES_CHOOSER_HEADER
#define POCKETPUZZLES_CHOOSER_HEADER
#include "common.h"

struct layout chooserlayout;
int current_chooserpage;
int chooser_lastpage;

static int chooser_cols = 5;
static int chooser_rows = 5;

static int control_num = 2;
static int control_padding;
static int chooser_padding;

extern const struct game abcd, ascent, blackbox, boats, bridges,
                         clusters, cube, dominosa, fifteen, filling,
                         flip, flood, galaxies, guess, inertia,
                         keen, lightup, loopy, magnets, map,
                         mathrax, mines, net, netslide, palisade,
                         pattern, pearl, pegs, range, rect,
                         rome, salad, samegame, signpost, singles,
                         sixteen, slant, solo, spokes, tents,
                         towers, tracks, twiddle, undead, unequal,
                         unruly, untangle, walls;

extern ibitmap icon_home, icon_home_tap, icon_redraw, icon_redraw_tap,
               game_abcd, game_ascent, game_blackbox, game_boats, game_bridges,
               game_clusters, game_cube, game_dominosa, game_fifteen, game_filling,
               game_flip, game_flood, game_galaxies, game_guess, game_inertia,
               game_keen, game_lightup, game_loopy, game_magnets, game_map,
               game_mathrax, game_mines, game_net, game_netslide, game_palisade,
               game_pattern, game_pearl, game_pegs, game_range, game_rect,
               game_rome, game_salad, game_samegame, game_signpost, game_singles,
               game_sixteen, game_slant, game_solo, game_spokes, game_tents,
               game_towers, game_tracks, game_twiddle, game_undead, game_unequal,
               game_unruly, game_untangle, game_walls,
               bt_prev, bt_next;

static BUTTON btn_home = { false, BTN_MENU, 0, 0, 0, 0, &icon_home, &icon_home_tap, NULL, NULL};
static BUTTON btn_draw = { false, BTN_MENU, 0, 0, 0, 0, &icon_redraw, &icon_redraw_tap, NULL, NULL};
static BUTTON btn_prev = { false, BTN_CTRL, 0, 0, 0, 0, &bt_prev, NULL, NULL, NULL};
static BUTTON btn_next = { false, BTN_CTRL, 0, 0, 0, 0, &bt_next, NULL, NULL, NULL};

static int num_games;
static BUTTON btn_chooser[] = {
//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_abcd,       NULL, NULL, &abcd},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_ascent,     NULL, NULL, &ascent},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_blackbox,   NULL, NULL, &blackbox},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_boats,      NULL, NULL, &boats},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_bridges,    NULL, NULL, &bridges},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_clusters,   NULL, NULL, &clusters},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_cube,       NULL, NULL, &cube},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_dominosa,   NULL, NULL, &dominosa},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_fifteen,    NULL, NULL, &fifteen},
//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_filling,    NULL, NULL, &filling},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_flip,       NULL, NULL, &flip},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_flood,      NULL, NULL, &flood},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_galaxies,   NULL, NULL, &galaxies},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_guess,      NULL, NULL, &guess},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_inertia,    NULL, NULL, &inertia},

//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_keen,       NULL, NULL, &keen},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_lightup,    NULL, NULL, &lightup},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_loopy,      NULL, NULL, &loopy},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_magnets,    NULL, NULL, &magnets},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_map,        NULL, NULL, &map},

//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_mathrax,    NULL, NULL, &mathrax},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_mines,      NULL, NULL, &mines},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_net,        NULL, NULL, &net},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_netslide,   NULL, NULL, &netslide},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_palisade,   NULL, NULL, &palisade},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_pattern,    NULL, NULL, &pattern},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_pearl,      NULL, NULL, &pearl},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_pegs,       NULL, NULL, &pegs},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_range,      NULL, NULL, &range},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_rect,       NULL, NULL, &rect},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_rome,       NULL, NULL, &rome},
//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_salad,      NULL, NULL, &salad},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_samegame,   NULL, NULL, &samegame},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_signpost,   NULL, NULL, &signpost},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_singles,    NULL, NULL, &singles},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_sixteen,    NULL, NULL, &sixteen},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_slant,      NULL, NULL, &slant},
//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_solo,       NULL, NULL, &solo},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_spokes,     NULL, NULL, &spokes},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_tents,      NULL, NULL, &tents},

//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_towers,     NULL, NULL, &towers},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_tracks,     NULL, NULL, &tracks},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_twiddle,    NULL, NULL, &twiddle},
//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_undead,     NULL, NULL, &undead},
//    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_unequal,    NULL, NULL, &unequal},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_unruly,     NULL, NULL, &unruly},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_untangle,   NULL, NULL, &untangle},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_walls,      NULL, NULL, &walls},
    { false, BTN_NULL, 0, 0, 0, 0, NULL, NULL, NULL, NULL}
};

static void chooserSetupMenuButtons();
static void chooserSetupChooserButtons();
static void chooserSetupControlButtons();

static void chooserDrawMenu();
static void chooserDrawChooserButtons(int page);
static void chooserDrawControlButtons(int page);

void chooserInit();
void chooserShowPage();
void chooserTap(int x, int y);
void chooserLongTap(int x, int y);
void chooserDrag(int x, int y);
void chooserRelease(int x, int y);

#endif
