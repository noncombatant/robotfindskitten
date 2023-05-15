CC = clang
CFLAGS = -Weverything -Werror -O3 -std=c2x -Wno-padded -Wno-poison-system-directories -Wno-declaration-after-statement
LDFLAGS = -lncurses

play: robotfindskitten
	-./robotfindskitten -n3

clean:
	rm robotfindskitten
