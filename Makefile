CC = clang
CFLAGS = -Weverything -Werror -O2 -std=c17 -Wno-padded -Wno-poison-system-directories
LDFLAGS = -lncurses

robotfindskitten: robotfindskitten.c

play: robotfindskitten
	-./robotfindskitten

clean:
	rm robotfindskitten
