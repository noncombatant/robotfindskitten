CC = clang
CFLAGS = -Weverything -Werror -O3 -std=c2x -Wno-padded -Wno-poison-system-directories -Wno-declaration-after-statement
LDFLAGS = -lncurses

robotfindskitten: robotfindskitten.c

play: robotfindskitten
	-./robotfindskitten

clean:
	rm robotfindskitten
