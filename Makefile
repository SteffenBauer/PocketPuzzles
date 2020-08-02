PBSDK ?= ../pocketbook-sdk5
CC = $(PBSDK)/bin/arm-obreey-linux-gnueabi-cc
PBRES = $(PBSDK)/bin/pbres
CFLAGS = -std=c99 -fomit-frame-pointer -linkview -lfreetype -lm -D_XOPEN_SOURCE=632

all: build/puzzles.app

build/puzzles.app: puzzles.c icons/icons.c
	LC_ALL=C $(CC) -s puzzles.c icons/icons.c -o build/puzzles.app -O2 $(CFLAGS)

icons/icons.c: ./icons/*.bmp
	$(PBRES) -c ./icons/icons.c ./icons/*.bmp

clean:
	rm -f build/puzzles.app icons/icons.c
