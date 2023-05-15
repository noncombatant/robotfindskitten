# About

robotfindskitten is a very fun adventure game for robots and humans. There are
[many versions of the game](http://robotfindskitten.org/), but this one is mine.

Part of my intent was for it to be a decent learning vehicle for people new to
the C programming language.

I based this version on a version by Alexey Toptygin, but simplified it and
re-cast it in a modern style. It also sheds all vestiges of the GNU Autotools
build system. I noticed that the build system boilerplate and infrastructure was
2 orders of magnitude larger than the code itself. That seemed wrong.

By perusing the Git history, you can see the steps I followed to get from
Alexey’s original to this version. Thank you, Alexey! And thank you to Leonard
Richardson, who originated robotfindskitten.

## Building And Installing

This version of robotfindskitten works on POSIX operating systems (tested on
Ubuntu and macOS) with reasonably modern C compilers (tested with Clang).

You will need a C compiler. On macOS, get Xcode. On Ubuntu and other
Debian-based systems, run `sudo apt install build-essential`. That should do the
trick.

The `ncurses` library is a C code library for manipulating the display in
Terminal windows, and robotfindskitten depends on it. robotfindskitten will not
compile properly unless your system has the `ncurses` header files. They come
with Xcode on macOS, and you can install them on Ubuntu or other Debian-based
systems with the command `sudo apt install libncurses-dev`.

To build and run robotfindskitten, open a Terminal window and simply type
`make`. To run, type `./robotfindskitten`. Like so:

```
git clone https://github.com/noncombatant/robotfindskitten.git
cd robotfindskitten
make
./robotfindskitten
```

To install, do the preceding and then copy the robotfindskitten binary to a
place in your `$PATH`. A good place might be `$HOME/bin`, `/usr/games`, or
`/usr/local/games`.

## Learning C With robotfindskitten

In my experience, a good way to learn how to learn a programming language is to
interleave the activities of reading tutorial books, reading code written in the
language, and writing code in the language. The best first book on programming
in C is probably _The C Programming Language_ by Kernighan and Ritchie. (It is
also one of the best technical books of any kind!) This implementation of
robotfindskitten is intended to be as easy to read as C can be. (Let me know if
I succeeded or failed!) So, that covers the reading part of learning.

For the writing part of learning, try your hand at modifying robotfindskitten to
change the game play in ways you find fun. Here are some ideas:

  * Add some more non-kitten item descriptions
  * Add functionality to read additional non-kitten item descriptions from a
    filename provided on the command line
  * Add command line options to set the size of the game field (currently, the
    game uses the entire screen)
  * Develop a scoring system, and display the score on the screen (e.g., as
    Robot sees more non-kitten items, its Zen Wisdom increases)
  * Enable Kitten and the non-kitten items to move about on the game field,
    rather than staying stationary
  * ...whatever else seems fun to you!

### Basics Of Reading C

I assume that most people reading this are coming from a background programming
in a very high-level language like Python, JavaScript, or Ruby. C is
significantly different than those languages in several ways:

  * Before you can run a program written in C, you must first _compile_
    (translate) the C code into code that your computer can understand.
  * Each variable must be declared to refer to a value of a particular _type_,
    such as `char` (character), `int` (integer), `double` (floating-point
    number), and so on.
  * C distinguishes between _values_ of particular types and _pointers to
    values_. Pointers are declared with the `*` symbol; `int foo;` means “`foo`
    is an integer”, while `int* bar;` means “`bar` is a pointer to an integer”.
  * The unary operator `&` (address-of) gets you a pointer to a value. Given an
    `int foo`, the expression `&foo` evaluates to the address of `foo`, and the
    type of this expression is `int*`.
  * The unary operator `*` (dereference) follows an address to get the value it
    refers to. Given an `int* wump`, `*wump` evaluates to whatever `int` value
    `wump` is currently pointing to.
  * You can declare arrays, but the size of arrays is fixed at compile time (!).
    `char noodles[12];` means “`noodles` is an array of 12 characters”. `int
    flarb[] = { 1, 2, 3 };` means “`flarb` is an array of integers, and it is
    large enough to hold the following items: ...”.
  * Declarations may also come with various [_type
    qualifiers_](https://en.wikipedia.org/wiki/Type_qualifier), such as
    `static`, `const`, and so on. At first, you can mostly ignore them. Their
    meaning will become more apparent (and more important) as you get more
    experience with C.

The above is the bare minimum background you’ll need to start skimming C code.
I’ve left a lot out, for the sake of brevity and to avoid repeating material you
can learn from more authoritative sources like Kernighan and Ritchie.

The biggest immediate difference between C and (e.g.) JavaScript is that the
programmer must declare the names and types of variables before using them, and
that there are both value types and pointer types.

Most of the rest of C grammar will probably look familiar to JavaScript
programmers, and somewhat familiar to Python programmers.

You probably won’t understand everything right away, and that is OK. Like Robot,
you will find your Kitten with patience and in due time.

### Understanding The C Standard Library

So much for the grammar of C. What about the vocabulary? Like all programming
languages, C has a standard library of functions and data types that programmers
can use and build on. The standard library is documented in Unix’ _manual pages_
(or “man pages” for short).

On macOS with Xcode, the manual pages are part of the Xcode install; on Ubuntu
and other Debian-based systems, you may have to install a package to get them:
`sudo apt install manpages-dev`.

To read the manual page for a given function, open a Terminal window and type
`man 3 foo` at the shell prompt, where `foo` is the name of a C library function
you want to read about. To page up and down, use the arrow keys, `b`, `f`, the
Space Bar, and so on. To quit reading the manual page, press `q` to quit.

The 3 in `man 3 foo` refers to the _chapter_ of the Unix manual: chapter 3 is
for C library functions. Chapter 1 is for command-line programs, and Chapter 2
is for kernel system calls. (See `man 1 man` for more information.)

A good manual page to read is `man 3 getopt`. `getopt` is a C library function
that parses the command line, and on most systems the manual page includes an
example of how to use it. Also read use of `getopt` in `robotfindskitten.c`.

Here are some more good manual pages to read (all in chapter 3), that might help
you when implementing changes to robotfindskitten:

  * `calloc` (and `free`), for creating (and destroying) dynamically-sized
     arrays
  * `fopen`, `fread`, `fwrite`, `fclose`, and `getline`, for working with files

### A Note On Names

You’ll notice when reading the code that identifiers (variable names, function
names, type names, et c.) seem to come from multiple naming conventions. That is
because they do. The difference is that ‘legacy’ code (such as the C standard
library, and the ncurses library) comes from the 1970s and 1980s, when memory
was expensive and text editors did not have fancy features like autocomplete.
Thus, you get these all-lowercase and pathologically terse names like `errno`
(error number), `printf` (print formatted string), and `mvaddch` (move and add
character). Memory was so expensive back then that early C compilers were not
required to recognize more than the first 6 characters of an identifier, so all
identifiers in a program had to be unique in their first 6 characters. Bonkers!

Now that computers are cheap and text editors are good, we can afford to write
code that looks like it means something. Hence the ‘modern’ naming conventions
used in this code:

  * `GlobalName` (including functions like `GetRandomIcon` and `InitializeGame`)
  * `local_name` (including `struct` members and function-local variables)
  * `i` and `j` for loop iterator variables

### Further Reading

[“Writing Programs With NCURSES” by Raymond and
Ben-Halim](http://invisible-island.net/ncurses/ncurses-intro.html) describes the
ncurses library and how to use it, in depth.

_The Practice Of Programming_ by Kernighan and Pike is an excellent discussion
of ‘modern’ programming practices.

_Managing Projects With make_ (2nd edition) by Oram and Talbott describes the
workings of the `Makefile` and the `make` program.

_lex & yacc_ (2nd edition) by Brown, Levine, and Mason describes how you can
parse complex input languages in C.

_Expert C Programming: Deep C Secrets_ by van der Linden is an amazingly great
(and hilarious) look into how C works (and, doesn’t work) under the hood.
