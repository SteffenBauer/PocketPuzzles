#ifndef POCKETPUZZLES_HEADER
#define POCKETPUZZLES_HEADER

ifont *font;
static const int kFontSize = 32;
static const int chooser_x_num = 5;

static int start_y_top;
static int height_top;
static int start_y_chooser;
static int height_chooser;

static int icon_size;
static int button_size;
static int chooser_size;
static int chooser_padding;

static int init_tap_x = 0;
static int init_tap_y = 0;

extern ibitmap icon_home, icon_home_tap, 
               game_ascent, game_blackbox, game_boats, game_bridges, game_clusters,
               game_cube, game_dominosa, game_fifteen, game_filling, game_flip,
               game_flood, game_galaxies, game_guess, game_inertia, game_keen,
               game_lightup, game_loopy, game_magnets, game_map, game_mathrax,
               game_mines, game_net, game_netslide, game_palisade, game_pattern,
               game_pearl, game_pegs, game_range, game_rect, game_rome,
               game_salad, game_samegame, game_signpost, game_singles, game_sixteen,
               game_slant, game_solo, game_spokes, game_tents, game_towers,
               game_tracks, game_twiddle, game_undead, game_unequal, game_unruly,
               game_untangle;

typedef struct button {
    bool active;
    int posx;
    int posy;
    int size;
    ibitmap *bitmap;
    ibitmap *bitmap_tap;
    ibitmap *bitmap_disabled;
} BUTTON;

BUTTON bt_home = { false, 0, 0, 0, &icon_home, &icon_home_tap, NULL};

typedef struct chooser {
    struct button button;
    char *title;
    //struct game *thegame;
} CHOOSER;

static int num_games = 30;
static CHOOSER bt_chooser[] = {
    {{ false, 0, 0, 0, &game_ascent,     NULL, NULL}, "Ascent"},
    {{ false, 0, 0, 0, &game_blackbox,   NULL, NULL}, "Blackbox"},
    {{ false, 0, 0, 0, &game_boats,      NULL, NULL}, "Boats"},
    {{ false, 0, 0, 0, &game_bridges,    NULL, NULL}, "Bridges"},
    {{ false, 0, 0, 0, &game_clusters,   NULL, NULL}, "Clusters"},
    {{ false, 0, 0, 0, &game_cube,       NULL, NULL}, "Cube"},
    {{ false, 0, 0, 0, &game_dominosa,   NULL, NULL}, "Dominosa"},
    {{ false, 0, 0, 0, &game_fifteen,    NULL, NULL}, "Fifteen"},
    {{ false, 0, 0, 0, &game_filling,    NULL, NULL}, "Filling"},
    {{ false, 0, 0, 0, &game_flip,       NULL, NULL}, "Flip"},
    {{ false, 0, 0, 0, &game_flood,      NULL, NULL}, "Flood"},
    {{ false, 0, 0, 0, &game_galaxies,   NULL, NULL}, "Galaxies"},
    {{ false, 0, 0, 0, &game_guess,      NULL, NULL}, "Guess"},
    {{ false, 0, 0, 0, &game_inertia,    NULL, NULL}, "Inertia"},
    {{ false, 0, 0, 0, &game_keen,       NULL, NULL}, "Keen"},
    {{ false, 0, 0, 0, &game_lightup,    NULL, NULL}, "Lightup"},
    {{ false, 0, 0, 0, &game_loopy,      NULL, NULL}, "Loopy"},
    {{ false, 0, 0, 0, &game_magnets,    NULL, NULL}, "Magnets"},
    {{ false, 0, 0, 0, &game_map,        NULL, NULL}, "Map"},
    {{ false, 0, 0, 0, &game_mathrax,    NULL, NULL}, "Mathrax"},
    {{ false, 0, 0, 0, &game_mines,      NULL, NULL}, "Mines"},
    {{ false, 0, 0, 0, &game_net,        NULL, NULL}, "Net"},
    {{ false, 0, 0, 0, &game_netslide,   NULL, NULL}, "Netslide"},
    {{ false, 0, 0, 0, &game_palisade,   NULL, NULL}, "Palisade"},
    {{ false, 0, 0, 0, &game_pattern,    NULL, NULL}, "Pattern"},
    {{ false, 0, 0, 0, &game_pearl,      NULL, NULL}, "Pearl"},
    {{ false, 0, 0, 0, &game_pegs,       NULL, NULL}, "Pegs"},
    {{ false, 0, 0, 0, &game_range,      NULL, NULL}, "Range"},
    {{ false, 0, 0, 0, &game_rect,       NULL, NULL}, "Rect"},
    {{ false, 0, 0, 0, &game_salad,      NULL, NULL}, "Salad"},
    {{ false, 0, 0, 0, &game_samegame,   NULL, NULL}, "Samegame"},
    {{ false, 0, 0, 0, &game_signpost,   NULL, NULL}, "Signpost"},
    {{ false, 0, 0, 0, &game_singles,    NULL, NULL}, "Singles"},
    {{ false, 0, 0, 0, &game_sixteen,    NULL, NULL}, "Sixteen"},
    {{ false, 0, 0, 0, &game_slant,      NULL, NULL}, "Slant"},
    {{ false, 0, 0, 0, &game_solo,       NULL, NULL}, "Solo"},
    {{ false, 0, 0, 0, &game_spokes,     NULL, NULL}, "Spokes"},
    {{ false, 0, 0, 0, &game_tents,      NULL, NULL}, "Tents"},
    {{ false, 0, 0, 0, &game_towers,     NULL, NULL}, "Towers"},
    {{ false, 0, 0, 0, &game_tracks,     NULL, NULL}, "Tracks"},
    {{ false, 0, 0, 0, &game_twiddle,    NULL, NULL}, "Twiddle"},
    {{ false, 0, 0, 0, &game_undead,     NULL, NULL}, "Undead"},
    {{ false, 0, 0, 0, &game_unequal,    NULL, NULL}, "Unequal"},
    {{ false, 0, 0, 0, &game_unruly,     NULL, NULL}, "Unruly"},
    {{ false, 0, 0, 0, &game_untangle,   NULL, NULL}, "Untangle"}
};

static bool coord_in_button(int x, int y, BUTTON *button);
static void button_to_normal(BUTTON *button);
static void button_to_tapped(BUTTON *button);

static void setupChooserLayout();
static void drawChooserTop();
static void setupMenuButtons();
static void setupChooserButtons();
static void drawChooserButtons();

static void tap(int x, int y);
static void long_tap(int x, int y);
static void release(int x, int y);

static void setupApp();
static void exitApp();

#endif

