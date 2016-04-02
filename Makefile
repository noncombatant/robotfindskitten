CFLAGS = -Wall -Wextra -Werror -O0
LDFLAGS = -lncurses

robotfindskitten: robotfindskitten.c

play: robotfindskitten
	-./robotfindskitten

clean:
	rm robotfindskitten
