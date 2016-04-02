About
=====

robotfindskitten is a very fun adventure game for robots and humans. There are
many versions of the game (you can see them all at
[robotfindskitten.org](http://robotfindskitten.org/)), but this one is
intended to be a decent learning vehicle for people new to the C programming
language.

It is based on a C implementation by Alexey Toptygin, but the implementation
is simplified and re-cast in a modern style. It also sheds all vestiges of the
GNU Autotools build system. In fact, I was motivated to do this because I
noticed that the build system boilerplate and infrastructure was 2 orders of
magnitude larger than the code itself. That seemed wrong, so I fixed it.

Learning C With robotfindskitten
================================

I will write this section later. TODO.

Building And Installing
=======================

This version of robotfindskitten works on POSIX operating systems (tested on
Ubuntu and Mac OS X) with reasonably modern C compilers (tested with Clang).

You will need a C compiler. On Mac OS X, get Xcode. On Ubuntu and other
Debian-based systems, run `sudo apt-get install build-essential`. That should
do the trick.

robotfindskitten will not compile properly unless the ncurses headers are
installed. They are installed by default on Mac OS X, and you can install them
on Ubuntu or other Debian-based systems with the command `apt-get install
libncurses-dev`.

To compile, make sure you have the ncurses libraries installed, and simply
type `make`. To run, type `./robotfindskitten`.

To install, do the preceeding and then copy the robotfindskitten binary to a
place in your `$PATH`. A good place might be `$HOME/bin`, `/usr/games`, or
`/usr/local/games`.
