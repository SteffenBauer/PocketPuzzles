#ifndef POCKETPUZZLES_HEADER
#define POCKETPUZZLES_HEADER

static ifont *font;
static const int kFontSize = 32;
static const int chooser_cols = 5;
static const int chooser_rows = 5;
static int current_chooserpage;
static int chooser_lastpage;

static int control_num = 2;

typedef struct panel {
    int starty;
    int height;
} PANEL;

static struct {
    bool with_statusbar;
    PANEL menu;
    PANEL maincanvas;
    PANEL maincanvas_sb;
    PANEL buttonpanel;
    PANEL buttonpanel_sb;
    PANEL statusbar;
} layout;

static int icon_size;
static int control_size;
static int control_padding;
static int chooser_size;
static int chooser_padding;

static int init_tap_x = 0;
static int init_tap_y = 0;

extern ibitmap icon_home, icon_home_tap,
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

typedef enum {
    BTN_MENU,
    BTN_CHOOSER,
    BTN_CTRL
} BUTTONTYPE;

typedef struct button {
    bool active;
    BUTTONTYPE type;
    int posx;
    int posy;
    int page;
    int size;
    ibitmap *bitmap;
    ibitmap *bitmap_tap;
    ibitmap *bitmap_disabled;
    char *title;
    // struct game *thegame;
} BUTTON;

static BUTTON btn_home = { false, BTN_MENU, 0, 0, 0, 0, &icon_home, &icon_home_tap, NULL, NULL};

static BUTTON btn_prev = { false, BTN_CTRL, 0, 0, 0, 0, &bt_prev, NULL, NULL, NULL};
static BUTTON btn_next = { false, BTN_CTRL, 0, 0, 0, 0, &bt_next, NULL, NULL, NULL};

static const int num_games = 48;
static BUTTON btn_chooser[] = {
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_abcd,       NULL, NULL, "ABCD"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_ascent,     NULL, NULL, "Ascent"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_blackbox,   NULL, NULL, "Blackbox"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_boats,      NULL, NULL, "Boats"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_bridges,    NULL, NULL, "Bridges"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_clusters,   NULL, NULL, "Clusters"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_cube,       NULL, NULL, "Cube"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_dominosa,   NULL, NULL, "Dominosa"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_fifteen,    NULL, NULL, "Fifteen"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_filling,    NULL, NULL, "Filling"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_flip,       NULL, NULL, "Flip"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_flood,      NULL, NULL, "Flood"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_galaxies,   NULL, NULL, "Galaxies"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_guess,      NULL, NULL, "Guess"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_inertia,    NULL, NULL, "Inertia"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_keen,       NULL, NULL, "Keen"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_lightup,    NULL, NULL, "Lightup"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_loopy,      NULL, NULL, "Loopy"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_magnets,    NULL, NULL, "Magnets"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_map,        NULL, NULL, "Map"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_mathrax,    NULL, NULL, "Mathrax"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_mines,      NULL, NULL, "Mines"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_net,        NULL, NULL, "Net"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_netslide,   NULL, NULL, "Netslide"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_palisade,   NULL, NULL, "Palisade"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_pattern,    NULL, NULL, "Pattern"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_pearl,      NULL, NULL, "Pearl"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_pegs,       NULL, NULL, "Pegs"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_range,      NULL, NULL, "Range"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_rect,       NULL, NULL, "Rect"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_rome,       NULL, NULL, "Rome"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_salad,      NULL, NULL, "Salad"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_samegame,   NULL, NULL, "Samegame"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_signpost,   NULL, NULL, "Signpost"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_singles,    NULL, NULL, "Singles"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_sixteen,    NULL, NULL, "Sixteen"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_slant,      NULL, NULL, "Slant"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_solo,       NULL, NULL, "Solo"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_spokes,     NULL, NULL, "Spokes"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_tents,      NULL, NULL, "Tents"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_towers,     NULL, NULL, "Towers"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_tracks,     NULL, NULL, "Tracks"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_twiddle,    NULL, NULL, "Twiddle"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_undead,     NULL, NULL, "Undead"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_unequal,    NULL, NULL, "Unequal"},

    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_unruly,     NULL, NULL, "Unruly"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_untangle,   NULL, NULL, "Untangle"},
    { false, BTN_CHOOSER, 0, 0, 0, 0, &game_walls,      NULL, NULL, "Walls"},
};
static bool coord_in_button(int x, int y, BUTTON *button);
static void button_to_normal(BUTTON *button, bool update);
static void button_to_tapped(BUTTON *button);
static void button_to_cleared(BUTTON *button, bool update);

static void setupLayout();
static void drawChooserTop();
static void setupMenuButtons();
static void setupChooserButtons();
static void showChooserPage();
static void drawChooserButtons(int page);
static void drawChooserControlButtons(int page);

static void tap(int x, int y);
static void long_tap(int x, int y);
static void release(int x, int y);

static void setupApp();
static void exitApp();

#endif
