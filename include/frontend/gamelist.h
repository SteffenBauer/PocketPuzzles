#ifndef POCKETPUZZLES_GAMELIST
#define POCKETPUZZLES_GAMELIST

extern const struct game abcd, ascent, blackbox, boats, bridges,
                         clusters, cube, dominosa, fifteen, filling,
                         flip, flood, galaxies, group, guess,
                         inertia, keen, lightup, loopy, magnets,
                         map, mathrax, mines, net, netslide,
                         palisade, pattern, pearl, pegs, range,
                         rect, rome, salad, samegame, signpost,
                         singles, sixteen, slant, sokoban, solo,
                         spokes, tents, towers, tracks, twiddle,
                         undead, unequal, unruly, untangle, walls;

extern ibitmap game_abcd, game_ascent, game_blackbox, game_boats, game_bridges,
               game_clusters, game_cube, game_dominosa, game_fifteen, game_filling,
               game_flip, game_flood, game_galaxies, game_group, game_guess,
               game_inertia, game_keen, game_lightup, game_loopy, game_magnets,
               game_map, game_mathrax, game_mines, game_net, game_netslide,
               game_palisade, game_pattern, game_pearl, game_pegs, game_range,
               game_rect, game_rome, game_salad, game_samegame, game_signpost,
               game_singles, game_sixteen, game_slant, game_sokoban, game_solo,
               game_spokes, game_tents, game_towers, game_tracks, game_twiddle,
               game_undead, game_unequal, game_unruly, game_untangle, game_walls;

static GAMEINFO mygames[] = {
    { &game_abcd,     &abcd},
    { &game_ascent,   &ascent},
    { &game_blackbox, &blackbox},
    { &game_boats,    &boats},
    { &game_bridges,  &bridges},

    { &game_clusters, &clusters},
//    { &game_cube,     &cube},
    { &game_dominosa, &dominosa},
//    { &game_fifteen,  &fifteen},
    { &game_filling,  &filling},

//    { &game_flip,     &flip},
//    { &game_flood,    &flood},
    { &game_galaxies, &galaxies},
    { &game_group,    &group},
//    { &game_guess,    &guess},

    { &game_inertia,  &inertia},
    { &game_keen,     &keen},
    { &game_lightup,  &lightup},
    { &game_loopy,    &loopy},
    { &game_magnets,  &magnets},

//    { &game_map,      &map},
    { &game_mathrax,  &mathrax},
    { &game_mines,    &mines},
//    { &game_net,      &net},
//    { &game_netslide, &netslide},

    { &game_palisade, &palisade},
    { &game_pattern,  &pattern},
    { &game_pearl,    &pearl},
//    { &game_pegs,     &pegs},
    { &game_range,    &range},

    { &game_rect,     &rect},
//    { &game_rome,     &rome},
    { &game_salad,    &salad},
//    { &game_samegame, &samegame},
    { &game_signpost, &signpost},

    { &game_singles,  &singles},
//    { &game_sixteen,  &sixteen},
    { &game_slant,    &slant},
    { &game_sokoban,  &sokoban},
    { &game_solo,     &solo},

    { &game_spokes,   &spokes},
    { &game_tents,    &tents},
    { &game_towers,   &towers},
    { &game_tracks,   &tracks},
//    { &game_twiddle,  &twiddle},

    { &game_undead,   &undead},
    { &game_unequal,  &unequal},
    { &game_unruly,   &unruly},
//    { &game_untangle, &untangle},
    { &game_walls,    &walls},

    { NULL, NULL}
};

#endif
