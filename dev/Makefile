
# You can define this path to point at your tools if you need to
# TOOLPATH = /opt/gcc/bin
CC := $(TOOLPATH)$(CC)
# You can manually set this to `gtk-config' or `pkg-config gtk+-1.2'
# (depending on what works on your system) if you want to enforce
# building with GTK 1.2, or you can set it to `pkg-config gtk+-2.0'
# if you want to enforce 2.0. The default is to try 2.0 and fall back
# to 1.2 if it isn't found.
GTK_CONFIG = sh -c 'pkg-config gtk+-2.0 $$0 2>/dev/null || gtk-config $$0'

CFLAGS := -O2 -Wall -Wno-deprecated-declarations -g -I./ -I../include/ `$(GTK_CONFIG) --cflags` \
		$(CFLAGS)
XLIBS = `$(GTK_CONFIG) --libs` -lm
ULIBS = -lm

all: abcd ascent binary blackbox boats bricks bridges clusters creek crossnum cube \
		dominosa fifteen filling flip flood galaxies guess inertia keen lightup \
		loopy magnets map mathrax mines mosaic net palisade pattern pearl pegs \
		range rect rome salad samegame signpost singles sixteen slant solo \
		spokes sticks tents towers tracks twiddle undead unequal unruly \
		untangle walls

abcd: abcd.o drawing.o gtk.o malloc.o midend.o \
		misc.o no-icon.o printing.o ps.o random.o version.o
	$(CC) -o $@ abcd.o drawing.o gtk.o malloc.o midend.o \
		misc.o no-icon.o printing.o ps.o random.o version.o $(XLFLAGS) \
		$(XLIBS)

ascent: ascent.o no-icon.o drawing.o gtk.o malloc.o \
		matching.o midend.o misc.o printing.o ps.o random.o \
		version.o
	$(CC) -o $@ ascent.o no-icon.o drawing.o gtk.o malloc.o \
		matching.o midend.o misc.o printing.o ps.o random.o \
		version.o  $(XLFLAGS) $(XLIBS)

binary: drawing.o binary.o no-icon.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o binary.o no-icon.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

blackbox: blackbox.o no-icon.o drawing.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ blackbox.o no-icon.o drawing.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

boats: boats.o no-icon.o drawing.o dsf.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ boats.o no-icon.o drawing.o dsf.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

bricks: bricks.o no-icon.o drawing.o  \
		gtk.o malloc.o midend.o misc.o printing.o ps.o random.o \
		version.o
	$(CC) -o $@ bricks.o no-icon.o drawing.o \
		gtk.o malloc.o midend.o misc.o printing.o ps.o random.o \
		version.o  $(XLFLAGS) $(XLIBS)

bridges: bridges.o no-icon.o drawing.o dsf.o findloop.o \
		gtk.o malloc.o midend.o misc.o printing.o ps.o random.o \
		version.o
	$(CC) -o $@ bridges.o no-icon.o drawing.o dsf.o findloop.o \
		gtk.o malloc.o midend.o misc.o printing.o ps.o random.o \
		version.o  $(XLFLAGS) $(XLIBS)

clusters: clusters.o no-icon.o drawing.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ clusters.o no-icon.o drawing.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

creek: creek.o no-icon.o drawing.o dsf.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ creek.o no-icon.o drawing.o dsf.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

crossnum: drawing.o gtk.o crossnum.o no-icon.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o gtk.o crossnum.o no-icon.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

cube: cube.o no-icon.o drawing.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ cube.o no-icon.o drawing.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o version.o  $(XLFLAGS) \
		$(XLIBS)

dominosa: dominosa.o no-icon.o drawing.o dsf.o findloop.o \
		gtk.o laydomino.o malloc.o midend.o misc.o printing.o ps.o \
		random.o sort.o version.o
	$(CC) -o $@ dominosa.o no-icon.o drawing.o dsf.o findloop.o \
		gtk.o laydomino.o malloc.o midend.o misc.o printing.o ps.o \
		random.o sort.o version.o  $(XLFLAGS) $(XLIBS)

fifteen: drawing.o fifteen.o no-icon.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o fifteen.o no-icon.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

filling: drawing.o dsf.o filling.o no-icon.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o dsf.o filling.o no-icon.o gtk.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

flip: drawing.o flip.o no-icon.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o tree234.o version.o
	$(CC) -o $@ drawing.o flip.o no-icon.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o tree234.o version.o  \
		$(XLFLAGS) $(XLIBS)

flood: drawing.o flood.o no-icon.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o flood.o no-icon.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o version.o  $(XLFLAGS) \
		$(XLIBS)

galaxies: drawing.o dsf.o galaxies.o no-icon.o gtk.o \
		malloc.o midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o dsf.o galaxies.o no-icon.o gtk.o \
		malloc.o midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

guess: drawing.o gtk.o guess.o no-icon.o malloc.o midend.o \
		misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o gtk.o guess.o no-icon.o malloc.o midend.o \
		misc.o printing.o ps.o random.o version.o  $(XLFLAGS) \
		$(XLIBS)

inertia: drawing.o gtk.o inertia.o no-icon.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o gtk.o inertia.o no-icon.o malloc.o \
		midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

keen: drawing.o dsf.o gtk.o keen.o no-icon.o latin.o malloc.o \
		matching.o midend.o misc.o printing.o ps.o random.o \
		tree234.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o keen.o no-icon.o latin.o \
		malloc.o matching.o midend.o misc.o printing.o ps.o random.o \
		tree234.o version.o  $(XLFLAGS) $(XLIBS)

lightup: combi.o drawing.o gtk.o lightup.o no-icon.o \
		malloc.o midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ combi.o drawing.o gtk.o lightup.o no-icon.o \
		malloc.o midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

loopy: drawing.o dsf.o grid.o gtk.o loopgen.o loopy.o \
		no-icon.o malloc.o midend.o misc.o printing.o \
		ps.o random.o tree234.o version.o
	$(CC) -o $@ drawing.o dsf.o grid.o gtk.o loopgen.o loopy.o \
		no-icon.o malloc.o midend.o misc.o printing.o \
		ps.o random.o tree234.o version.o  $(XLFLAGS) $(XLIBS)

magnets: drawing.o gtk.o laydomino.o magnets.o no-icon.o \
		malloc.o midend.o misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o gtk.o laydomino.o magnets.o no-icon.o \
		malloc.o midend.o misc.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

map: drawing.o dsf.o gtk.o malloc.o map.o no-icon.o midend.o \
		misc.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o malloc.o map.o no-icon.o midend.o \
		misc.o printing.o ps.o random.o version.o  $(XLFLAGS) \
		$(XLIBS)

mathrax: drawing.o gtk.o latin.o malloc.o matching.o mathrax.o \
		no-icon.o midend.o misc.o printing.o ps.o random.o \
		tree234.o version.o
	$(CC) -o $@ drawing.o gtk.o latin.o malloc.o matching.o mathrax.o \
		no-icon.o midend.o misc.o printing.o ps.o random.o \
		tree234.o version.o  $(XLFLAGS) $(XLIBS)

mines: drawing.o gtk.o malloc.o midend.o mines.o no-icon.o \
		misc.o printing.o ps.o random.o tree234.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o mines.o no-icon.o \
		misc.o printing.o ps.o random.o tree234.o version.o  \
		$(XLFLAGS) $(XLIBS)

mosaic: drawing.o gtk.o malloc.o midend.o misc.o mosaic.o \
		no-icon.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o mosaic.o \
		no-icon.o printing.o ps.o random.o version.o  $(XLFLAGS) \
		$(XLIBS)

net: drawing.o dsf.o findloop.o gtk.o malloc.o midend.o misc.o \
		net.o no-icon.o printing.o ps.o random.o tree234.o \
		version.o
	$(CC) -o $@ drawing.o dsf.o findloop.o gtk.o malloc.o midend.o \
		misc.o net.o no-icon.o printing.o ps.o random.o tree234.o \
		version.o  $(XLFLAGS) $(XLIBS)

palisade: divvy.o drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		palisade.o no-icon.o printing.o ps.o random.o \
		version.o
	$(CC) -o $@ divvy.o drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		palisade.o no-icon.o printing.o ps.o random.o \
		version.o  $(XLFLAGS) $(XLIBS)

pattern: drawing.o gtk.o malloc.o midend.o misc.o pattern.o \
		no-icon.o printing.o ps.o random.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o pattern.o \
		no-icon.o printing.o ps.o random.o version.o  \
		$(XLFLAGS) $(XLIBS)

pearl: drawing.o dsf.o grid.o gtk.o loopgen.o malloc.o midend.o \
		misc.o pearl.o no-icon.o printing.o ps.o \
		random.o tdq.o tree234.o version.o
	$(CC) -o $@ drawing.o dsf.o grid.o gtk.o loopgen.o malloc.o midend.o \
		misc.o pearl.o no-icon.o printing.o ps.o \
		random.o tdq.o tree234.o version.o  $(XLFLAGS) $(XLIBS)

pegs: drawing.o gtk.o malloc.o midend.o misc.o pegs.o \
		no-icon.o printing.o ps.o random.o tree234.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o pegs.o \
		no-icon.o printing.o ps.o random.o tree234.o version.o  \
		$(XLFLAGS) $(XLIBS)

range: drawing.o dsf.o gtk.o malloc.o midend.o misc.o printing.o \
		ps.o random.o range.o no-icon.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o range.o no-icon.o version.o  \
		$(XLFLAGS) $(XLIBS)

rect: drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o rect.o no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o rect.o no-icon.o version.o  $(XLFLAGS) $(XLIBS)

rome: drawing.o dsf.o gtk.o malloc.o midend.o misc.o printing.o \
		ps.o random.o rome.o no-icon.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o rome.o no-icon.o version.o  \
		$(XLFLAGS) $(XLIBS)

salad: drawing.o gtk.o latin.o malloc.o matching.o midend.o \
		misc.o printing.o ps.o random.o salad.o no-icon.o \
		tree234.o version.o
	$(CC) -o $@ drawing.o gtk.o latin.o malloc.o matching.o midend.o \
		misc.o printing.o ps.o random.o salad.o no-icon.o \
		tree234.o version.o  $(XLFLAGS) $(XLIBS)

samegame: drawing.o gtk.o malloc.o midend.o misc.o printing.o \
		ps.o random.o samegame.o no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o samegame.o no-icon.o version.o  $(XLFLAGS) \
		$(XLIBS)

signpost: drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o signpost.o no-icon.o \
		version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o signpost.o no-icon.o \
		version.o  $(XLFLAGS) $(XLIBS)

singles: drawing.o dsf.o gtk.o latin.o malloc.o matching.o \
		midend.o misc.o printing.o ps.o random.o singles.o \
		no-icon.o tree234.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o latin.o malloc.o matching.o \
		midend.o misc.o printing.o ps.o random.o singles.o \
		no-icon.o tree234.o version.o  $(XLFLAGS) $(XLIBS)

sixteen: drawing.o gtk.o malloc.o midend.o misc.o printing.o \
		ps.o random.o sixteen.o no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o sixteen.o no-icon.o version.o  $(XLFLAGS) \
		$(XLIBS)

slant: drawing.o dsf.o findloop.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o slant.o no-icon.o version.o
	$(CC) -o $@ drawing.o dsf.o findloop.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o slant.o no-icon.o \
		version.o  $(XLFLAGS) $(XLIBS)

solo: divvy.o drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o solo.o no-icon.o version.o
	$(CC) -o $@ divvy.o drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o solo.o no-icon.o version.o  \
		$(XLFLAGS) $(XLIBS)

spokes: drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o spokes.o no-icon.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o spokes.o no-icon.o version.o  \
		$(XLFLAGS) $(XLIBS)

sticks: drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o sticks.o no-icon.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o sticks.o no-icon.o version.o  \
		$(XLFLAGS) $(XLIBS)

tents: drawing.o dsf.o gtk.o malloc.o matching.o midend.o misc.o \
		printing.o ps.o random.o tents.o no-icon.o version.o
	$(CC) -o $@ drawing.o dsf.o gtk.o malloc.o matching.o midend.o \
		misc.o printing.o ps.o random.o tents.o no-icon.o \
		version.o  $(XLFLAGS) $(XLIBS)

towers: drawing.o gtk.o latin.o malloc.o matching.o midend.o \
		misc.o printing.o ps.o random.o towers.o no-icon.o \
		tree234.o version.o
	$(CC) -o $@ drawing.o gtk.o latin.o malloc.o matching.o midend.o \
		misc.o printing.o ps.o random.o towers.o no-icon.o \
		tree234.o version.o  $(XLFLAGS) $(XLIBS)

tracks: drawing.o dsf.o findloop.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o tracks.o no-icon.o \
		version.o
	$(CC) -o $@ drawing.o dsf.o findloop.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o tracks.o no-icon.o \
		version.o  $(XLFLAGS) $(XLIBS)

twiddle: drawing.o gtk.o malloc.o midend.o misc.o printing.o \
		ps.o random.o twiddle.o no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o twiddle.o no-icon.o version.o  $(XLFLAGS) \
		$(XLIBS)

undead: drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o undead.o no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o undead.o no-icon.o version.o  $(XLFLAGS) \
		$(XLIBS)

unequal: drawing.o gtk.o latin.o malloc.o matching.o midend.o \
		misc.o printing.o ps.o random.o tree234.o unequal.o \
		no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o latin.o malloc.o matching.o midend.o \
		misc.o printing.o ps.o random.o tree234.o unequal.o \
		no-icon.o version.o  $(XLFLAGS) $(XLIBS)

unruly: drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o unruly.o no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o unruly.o no-icon.o version.o  $(XLFLAGS) \
		$(XLIBS)

untangle: drawing.o gtk.o malloc.o midend.o misc.o printing.o \
		ps.o random.o tree234.o untangle.o no-icon.o version.o
	$(CC) -o $@ drawing.o gtk.o malloc.o midend.o misc.o printing.o ps.o \
		random.o tree234.o untangle.o no-icon.o version.o  \
		$(XLFLAGS) $(XLIBS)

walls: drawing.o dsf.o findloop.o gtk.o malloc.o midend.o misc.o \
		printing.o ps.o random.o version.o walls.o no-icon.o
	$(CC) -o $@ drawing.o dsf.o findloop.o gtk.o malloc.o midend.o \
		misc.o printing.o ps.o random.o version.o walls.o \
		no-icon.o  $(XLFLAGS) $(XLIBS)



# Game files
abcd.o: ../games/abcd.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
ascent.o: ../games/ascent.c ../include/puzzles.h ../include/matching.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
binary.o: ../games/binary.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
blackbox.o: ../games/blackbox.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
boats.o: ../games/boats.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
bricks.o: ../games/bricks.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
bridges.o: ../games/bridges.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
clusters.o: ../games/clusters.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
creek.o: ../games/creek.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
crossnum.o: ../games/crossnum.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
cube.o: ../games/cube.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
dominosa.o: ../games/dominosa.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
fifteen.o: ../games/fifteen.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
filling.o: ../games/filling.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
flip.o: ../games/flip.c ../include/puzzles.h ../include/tree234.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
flood.o: ../games/flood.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
galaxies.o: ../games/galaxies.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
guess.o: ../games/guess.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
inertia.o: ../games/inertia.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
keen.o: ../games/keen.c ../include/puzzles.h ../include/latin.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
lightup.o: ../games/lightup.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
loopy.o: ../games/loopy.c ../include/puzzles.h ../include/tree234.h ../include/grid.h ../include/loopgen.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
magnets.o: ../games/magnets.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
map.o: ../games/map.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
mathrax.o: ../games/mathrax.c ../include/puzzles.h ../include/latin.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
mines.o: ../games/mines.c ../include/tree234.h ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
mosaic.o: ../games/mosaic.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
net.o: ../games/net.c ../include/puzzles.h ../include/tree234.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
palisade.o: ../games/palisade.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
pattern.o: ../games/pattern.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
pearl.o: ../games/pearl.c ../include/puzzles.h ../include/grid.h ../include/loopgen.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
pegs.o: ../games/pegs.c ../include/puzzles.h ../include/tree234.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
range.o: ../games/range.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
rect.o: ../games/rect.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
rome.o: ../games/rome.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
salad.o: ../games/salad.c ../include/puzzles.h ../include/latin.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
samegame.o: ../games/samegame.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
signpost.o: ../games/signpost.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
singles.o: ../games/singles.c ../include/puzzles.h ../include/latin.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
sixteen.o: ../games/sixteen.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
slant.o: ../games/slant.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
solo.o: ../games/solo.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
spokes.o: ../games/spokes.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
sticks.o: ../games/sticks.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
tents.o: ../games/tents.c ../include/puzzles.h ../include/matching.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
towers.o: ../games/towers.c ../include/puzzles.h ../include/latin.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
tracks.o: ../games/tracks.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
twiddle.o: ../games/twiddle.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
undead.o: ../games/undead.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
unequal.o: ../games/unequal.c ../include/puzzles.h ../include/latin.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
unruly.o: ../games/unruly.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
untangle.o: ../games/untangle.c ../include/puzzles.h ../include/tree234.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
walls.o: ../games/walls.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@

# Utility files
combi.o: ../utils/combi.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
divvy.o: ../utils/divvy.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
drawing.o: ../utils/drawing.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
dsf.o: ../utils/dsf.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
findloop.o: ../utils/findloop.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
grid.o: ../utils/grid.c ../include/puzzles.h ../include/tree234.h ../include/grid.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
gtk.o: ./gtk.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
latin.o: ../utils/latin.c ../include/puzzles.h ../include/tree234.h ../include/matching.h ../include/latin.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
laydomino.o: ../utils/laydomino.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
list.o: ../utils/list.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
loopgen.o: ../utils/loopgen.c ../include/puzzles.h ../include/tree234.h ../include/grid.h ../include/loopgen.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
malloc.o: ../utils/malloc.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
matching.o: ../utils/matching.c ../include/puzzles.h ../include/matching.h ../include/tree234.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
midend.o: ../utils/midend.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
misc.o: ../utils/misc.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
no-icon.o: ./no-icon.c
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
printing.o: ../utils/printing.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
ps.o: ./ps.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
random.o: ../utils/random.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
sort.o: ../utils/sort.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
tdq.o: ../utils/tdq.c ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
tree234.o: ../utils/tree234.c ../include/tree234.h ../include/puzzles.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@
version.o: ../utils/version.c ../include/version.h
	$(CC) $(COMPAT) $(FWHACK) $(CFLAGS) $(XFLAGS) -c $< -o $@

clean:
	rm -f *.o abcd ascent blackbox boats bricks bridges clusters creek crossnum cube dominosa fifteen filling flip flood galaxies guess inertia keen lightup loopy magnets map mathrax mines mosaic net palisade pattern pearl pegs range rect rome salad samegame signpost singles sixteen slant solo spokes sticks tents towers tracks twiddle undead unequal unruly untangle walls

