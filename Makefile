CFLAGS = -Wall -Wextra -Werror -O0
LDFLAGS = -lncurses

robotfindskitten:

play: robotfindskitten
	-./robotfindskitten

clean:
	rm robotfindskitten
