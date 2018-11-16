CFLAGS = -Weverything -Werror -O0 -ansi -pedantic -std=c11 -Wno-padded
LDFLAGS = -lncurses

robotfindskitten: robotfindskitten.c

play: robotfindskitten
	-./robotfindskitten

clean:
	rm robotfindskitten
