.PHONY: all clean firmware-4

PBSDK ?= ../SDK-B288/usr
CROSS ?= arm-obreey-linux-gnueabi
ARCHFLAGS ?= -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
EXTRAFLAGS ?=

LIB = $(PBSDK)/lib
CC = LD_LIBRARY_PATH=$(LIB) $(PBSDK)/bin/$(CROSS)-gcc
STRIP = $(PBSDK)/bin/$(CROSS)-strip
PBRES = $(PBSDK)/bin/pbres

CFLAGS = $(EXTRAFLAGS) -DCOMBINED -std=c99 -DNDEBUG -fsigned-char -fomit-frame-pointer -fPIC -O2 $(ARCHFLAGS) -linkview -lfreetype -lm -D_XOPEN_SOURCE=632 -DVERSION=\"$(GIT_VERSION)\"

GIT_VERSION := "0.8.2-nightly"

UTILSRCS := $(wildcard utils/*.c)
UTILOBJS := $(UTILSRCS:%.c=%.o)

GAMESRCS := $(wildcard games/*.c)
GAMEOBJS := $(GAMESRCS:%.c=%.o)

FRONTENDSRCS := $(wildcard frontend/*.c)
FRONTENDOBJS := $(FRONTENDSRCS:%.c=%.o)
FRONTENDHDRS := $(wildcard include/frontend/*.h)

HEADERS := $(wildcard include /*.h)

ICONS := $(wildcard icons/*/*.bmp)

all: build/SGTPuzzles.app

firmware-4:
	$(MAKE) --no-print-directory all CROSS=arm-none-linux-gnueabi ARCHFLAGS= EXTRAFLAGS="-I$(PBSDK)/include -L$(PBSDK)/lib"

build/SGTPuzzles.app: icons/icons.c $(GAMEOBJS) $(FRONTENDOBJS) $(UTILOBJS)
	mkdir -p ./build
	LC_ALL=C $(CC) -s icons/icons.c $(GAMEOBJS) $(FRONTENDOBJS) $(UTILOBJS) -I./include -o ./build/SGTPuzzles.app $(CFLAGS)
	LC_ALL=C $(STRIP) ./build/SGTPuzzles.app

icons/icons.c: $(ICONS)
	LC_ALL=C $(PBRES) -c ./icons/icons.c -4 $(ICONS)

frontend/%.o: frontend/%.c $(HEADERS) $(FRONTENDHDRS)
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

games/%.o: games/%.c $(HEADERS)
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

utils/%.o: utils/%.c $(HEADERS)
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

clean:
	rm -f build/puzzles.app icons/icons.c games/*.o utils/*.o frontend/*.o

