CC = clang
CFLAGS = -Weverything -Werror -std=c2x \
	-O3 \
	-Wno-pre-c2x-compat \
	-Wno-padded \
	-Wno-poison-system-directories \
	-Wno-declaration-after-statement
LDFLAGS = -lncurses

play: robotfindskitten
	-./robotfindskitten

clean:
	rm robotfindskitten
