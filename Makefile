.PHONY = all clean


PBSDK ?= ../../pocketbook-sdk5
CC = $(PBSDK)/bin/arm-obreey-linux-gnueabi-gcc
PBRES = $(PBSDK)/bin/pbres
CFLAGS = -DCOMBINED -std=c99 -DNDEBUG -fsigned-char -fomit-frame-pointer -fPIC -O2 -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp -linkview -lfreetype -lm -D_XOPEN_SOURCE=632

UTILSRCS := $(wildcard utils/*.c)
UTILOBJS := $(UTILSRCS:%.c=%.o)

GAMESRCS := $(wildcard games/*.c)
GAMEOBJS := $(GAMESRCS:%.c=%.o)

FRONTENDSRCS := $(wildcard frontend/*.c)
FRONTENDOBJS := $(FRONTENDSRCS:%.c=%.o)

all: build/puzzles.app

build/puzzles.app: pocketpuzzles.c pocketpuzzles.h icons/icons.c
	LC_ALL=C $(CC) -s pocketpuzzles.c icons/icons.c -I./include -o build/puzzles.app $(CFLAGS)

icons/icons.c: ./icons/*.bmp
	LC_ALL=C $(PBRES) -c ./icons/icons.c -4 ./icons/*.bmp

frontend/%.o: frontend/%.c
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

games/%.o: games/%.c
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

utils/%.o: utils/%.c
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

clean:
	rm -f build/puzzles.app icons/icons.c games/*.o utils/*.o frontend/*.o

