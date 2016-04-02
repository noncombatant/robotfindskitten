/*
 *  Copyright (C) 2004-2005 Alexey Toptygin <alexeyt@freeshell.org>
 *  Based on sources by Leonard Richardson and others.
 *
 *  Refactored (...defactored?) by Chris Palmer <https://noncombatant.org> on
 *  1 April 2016.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or EXISTENCE OF KITTEN.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "messages.h"

#define SYSTEM_NKI_DIR "/usr/share/games/robotfindskitten"
#define USER_NKI_DIR ".robotfindskitten"
#define PACKAGE_VERSION "1"

#define NKI_EXT "nki"

#define DEFAULT_NUM_BOGUS 20

/* option flags for state.options */
#define OPTION_HAS_COLOR 0x01
#define OPTION_DISPLAY_INTRO 0x02

#define DEFAULT_OPTIONS (OPTION_DISPLAY_INTRO)

/* bits returned from test() */
#define BROBOT 0x01
#define BKITTEN 0x02
#define BBOGUS 0x04

/* Nethack keycodes */
#define NETHACK_down 'j'
#define NETHACK_DOWN 'J'
#define NETHACK_up 'k'
#define NETHACK_UP 'K'
#define NETHACK_left 'h'
#define NETHACK_LEFT 'H'
#define NETHACK_right 'l'
#define NETHACK_RIGHT 'L'
#define NETHACK_ul 'y'
#define NETHACK_UL 'Y'
#define NETHACK_ur 'u'
#define NETHACK_UR 'U'
#define NETHACK_dl 'b'
#define NETHACK_DL 'B'
#define NETHACK_dr 'n'
#define NETHACK_DR 'N'

/* When numlock is on, the keypad generates numbers */
#define NUMLOCK_UL '7'
#define NUMLOCK_UP '8'
#define NUMLOCK_UR '9'
#define NUMLOCK_LEFT '4'
#define NUMLOCK_RIGHT '6'
#define NUMLOCK_DL '1'
#define NUMLOCK_DOWN '2'
#define NUMLOCK_DR '3'

/* EMACS keycodes */
#define CTRL(key) ((key)&0x1f)
#define EMACS_NEXT CTRL('N')
#define EMACS_PREVIOUS CTRL('P')
#define EMACS_BACKWARD CTRL('B')
#define EMACS_FORWARD CTRL('F')

/* allocate the messages array in chunks of this size */
#define MSGS_ALLOC_CHUNK 32

/* miscellaneous
 * I'm paranoid about collisions with curses in the KEY_ namespace */
#define MYKEY_REDRAW CTRL('L')
#define MYKEY_q 'q'
#define MYKEY_Q 'Q'

/* size of header area above frame and playing field */
#define HEADSIZE 2

/* thickness of frame - can be 1 or 0, 0 suppreses framing */
#define FRAME 1

/* magic index of white color pair */
#define WHITE 7

/* special indices in the items array */
#define ROBOT 0
#define KITTEN 1
#define BOGUS 2

typedef struct {
  int x;
  int y;
  unsigned int color;
  bool bold;
  bool reverse;
  chtype character;
} screen_object;

typedef struct {
  int lines;
  int cols;
  unsigned int options;
  size_t num_items;
  size_t num_messages;
  size_t num_messages_alloc;
  screen_object* items;
  char** messages;
} game_state;

static game_state state;

static void add_message(const char* message) {
  if (state.num_messages_alloc <= state.num_messages) {
    char** nmess = calloc(state.num_messages + MSGS_ALLOC_CHUNK, sizeof(char*));
    state.num_messages_alloc = state.num_messages + MSGS_ALLOC_CHUNK;
    memcpy(nmess, state.messages, state.num_messages * sizeof(char*));
    free(state.messages);
    state.messages = nmess;
  }
  state.messages[state.num_messages] = strdup(message);
  state.num_messages++;
}

static void read_messages(void) {
  size_t i;

  state.messages = 0;
  state.num_messages = 0;
  state.num_messages_alloc = 0;

  for (i = 0; i < BOGUS; ++i) {
    add_message("");
  }

  for (i = 0; i < sizeof(Messages) / sizeof(Messages[0]); ++i) {
    add_message(Messages[i]);
  }
}

static void randomize_messages(void) {
  for (size_t i = BOGUS; i < (state.num_messages - 1); i++) {
    size_t j = i + (random() % (state.num_messages - i));
    if (i != j) {
      char* temp = state.messages[i];
      state.messages[i] = state.messages[j];
      state.messages[j] = temp;
    }
  }
}

/* convenient macros */
#define randx() (FRAME + (random() % (state.cols - FRAME * 2)))
#define randy() \
  (HEADSIZE + FRAME + (random() % (state.lines - HEADSIZE - FRAME * 2)))
#define randch() (random() % ('~' - '!' + 1) + '!')
#define randbold() ((random() % 2) ? true : false)
#define randcolor() (random() % 6 + 1)
#define randint(m, n) ((m) + (random() % ((n) + 1)))

static char randchar(void) {
  char ch;
  do {
    ch = randch();
  } while (ch == '#');
  return ch;
}

static bool object_equal(const screen_object a, const screen_object b) {
  return a.x == b.x && a.y == b.y;
}

static unsigned int test(int y, int x, unsigned int* bnum) {
  unsigned int i;
  for (i = 0; i < state.num_items; i++) {
    if (state.items[i].x == x && state.items[i].y == y) {
      *bnum = i;
      if (i == ROBOT)
        return BROBOT;
      else if (i == KITTEN)
        return BKITTEN;
      else
        return BBOGUS;
    }
  }

  return 0;
}

static void finish(int sig) {
  (void)endwin();
  exit(sig);
}

static void init(size_t item_count) {
  size_t i, j;

  state.items = calloc(BOGUS + item_count, sizeof(screen_object));

  (void)signal(SIGINT, finish);

  /* set up (n)curses */
  (void)initscr();
  (void)nonl();
  (void)noecho();
  (void)cbreak();
  (void)intrflush(stdscr, false);
  (void)keypad(stdscr, true);

  state.lines = LINES;
  state.cols = COLS;
  if (((state.lines - HEADSIZE - FRAME) * state.cols) < (int)(item_count + 2)) {
    (void)endwin();
    (void)fprintf(stderr, "Screen too small to fit all objects!\n");
    exit(EXIT_FAILURE);
  }

  /* set up robot */
  state.items[ROBOT].character = (chtype)'#';
  state.items[ROBOT].bold = false; /* we are a timid robot */
  state.items[ROBOT].reverse = false;
  state.items[ROBOT].y = randy();
  state.items[ROBOT].x = randx();

  /* set up kitten */
  state.items[KITTEN].character = (chtype)randchar();
  state.items[KITTEN].bold = randbold();
  state.items[KITTEN].reverse = false;
  do {
    state.items[KITTEN].y = randy();
    state.items[KITTEN].x = randx();
  } while (object_equal(state.items[ROBOT], state.items[KITTEN]));

  /* set up items */
  for (i = BOGUS; i < BOGUS + item_count; i++) {
    state.items[i].character = (chtype)randchar();
    state.items[i].bold = randbold();
    state.items[i].reverse = false;
    while (true) {
      state.items[i].y = randy();
      state.items[i].x = randx();
      if (object_equal(state.items[ROBOT], state.items[i]))
        continue;
      if (object_equal(state.items[KITTEN], state.items[i]))
        continue;
      for (j = 0; j < i; j++) {
        if (object_equal(state.items[j], state.items[i]))
          break;
      }
      if (j == i)
        break;
    }
  }
  state.num_items = BOGUS + item_count;

  /* set up colors */
  (void)start_color();
  if (has_colors() && (COLOR_PAIRS > 7)) {
    state.options |= OPTION_HAS_COLOR;
    (void)init_pair(1, COLOR_GREEN, COLOR_BLACK);
    (void)init_pair(2, COLOR_RED, COLOR_BLACK);
    (void)init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    (void)init_pair(4, COLOR_BLUE, COLOR_BLACK);
    (void)init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    (void)init_pair(6, COLOR_CYAN, COLOR_BLACK);
    (void)init_pair(7, COLOR_WHITE, COLOR_BLACK);
    (void)bkgd((chtype)COLOR_PAIR(WHITE));

    state.items[ROBOT].color = WHITE;
    state.items[KITTEN].color = randcolor();
    for (i = BOGUS; i < state.num_items; i++) {
      state.items[i].color = randcolor();
    }
  } else {
    state.options &= ~OPTION_HAS_COLOR;
  }
}

static void draw(const screen_object* o) {
  attr_t new;

  assert(curscr != NULL);
  if ((state.options & OPTION_HAS_COLOR) != 0) {
    new = COLOR_PAIR(o->color);
    if (o->bold) {
      new |= A_BOLD;
    }
    if (o->reverse) {
      new |= A_REVERSE;
    }
    (void)attrset(new);
  }
  (void)addch(o->character);
}

static void message(char* message) {
  int y, x;

  getyx(curscr, y, x);
  if ((state.options & OPTION_HAS_COLOR) != 0) {
    attrset(COLOR_PAIR(WHITE));
  }
  (void)move(1, 0);
  (void)clrtoeol();
  (void)move(1, 0);
  (void)printw("%.*s", state.cols, message);
  (void)move(y, x);
  (void)refresh();
}

static void draw_screen() {
  unsigned int i;

  if ((state.options & OPTION_HAS_COLOR) != 0)
    attrset(COLOR_PAIR(WHITE));
  (void)clear();
#if FRAME > 0
  (void)mvaddch(HEADSIZE, 0, ACS_ULCORNER);
  (void)mvaddch(HEADSIZE, COLS - 1, ACS_URCORNER);
  (void)mvaddch(LINES - 1, 0, ACS_LLCORNER);
  (void)mvaddch(LINES - 1, COLS - 1, ACS_LRCORNER);
  for (i = 1; i < (unsigned int)COLS - 1; i++) {
    (void)mvaddch(HEADSIZE, (int)i, ACS_HLINE);
    (void)mvaddch(LINES - 1, (int)i, ACS_HLINE);
  }
  for (i = FRAME + HEADSIZE; i < (unsigned int)LINES - 1; i++) {
    (void)mvaddch((int)i, 0, ACS_VLINE);
    (void)mvaddch((int)i, COLS - 1, ACS_VLINE);
  }
#else
  for (i = 0; i < COLS; i++) {
    (void)mvaddch(HEADSIZE, (int)i, ACS_HLINE);
  }
#endif
  (void)move(0, 0);
  (void)printw("robotfindskitten %s\n\n", PACKAGE_VERSION);
  for (i = 0; i < state.num_items; i++) {
    (void)move(state.items[i].y, state.items[i].x);
    draw(&state.items[i]);
  }
  (void)move(state.items[ROBOT].y, state.items[ROBOT].x);
  if ((state.options & OPTION_HAS_COLOR) != 0)
    (void)attrset(COLOR_PAIR(WHITE));
  (void)refresh();
}

static void handle_resize(void) {
  int xbound = 0, ybound = 0;
  unsigned int i;
  for (i = 0; i < state.num_items; i++) {
    if (state.items[i].x > xbound)
      xbound = state.items[i].x;
    if (state.items[i].y > ybound)
      ybound = state.items[i].y;
  }

  /* has the resize hidden any items? */
  if (xbound >= COLS - FRAME * 2 || ybound >= HEADSIZE + LINES - FRAME * 2) {
    (void)endwin();
    (void)fprintf(stderr,
                  "You crushed the simulation. And robot. And kitten.\n");
    exit(EXIT_FAILURE);
  }

  state.lines = LINES;
  state.cols = COLS;
  draw_screen();
}

static void instructions(void) {
  (void)clear();
  (void)move(0, 0);
  (void)printw("robotfindskitten %s\n", PACKAGE_VERSION);
  (void)printw(
      "By the illustrious Leonard Richardson (C) 1997, 2000\n"
      "Written originally for the Nerth Pork robotfindskitten contest\n\n"
      "In this game, you are robot (#). Your job is to find kitten. This task\n"
      "is complicated by the existence of various things which are not "
      "kitten.\n"
      "Robot must touch items to determine if they are kitten or not. The "
      "game\n"
      "ends when robotfindskitten. Alternatively, you may end the game by "
      "hitting\n"
      "the q key or a good old-fashioned Ctrl-C.\n\n"
      "See the documentation for more information.\n\n"
      "Press any key to start.\n");
  (void)refresh();
  if (getch() == KEY_RESIZE)
    handle_resize();
  (void)clear();
}

static void play_animation(bool fromright) {
  int i, animation_meet;
  screen_object robot;
  screen_object kitten;
  chtype kitty;
#define WIN_MESSAGE "You found kitten! Way to go, robot!"

  (void)move(1, 0);
  (void)clrtoeol();
  animation_meet = (COLS / 2);

  memcpy(&kitten, &state.items[KITTEN], sizeof kitten);
  memcpy(&robot, &state.items[ROBOT], sizeof robot);
  robot.reverse = true;

  kitty = state.items[KITTEN].character;
  for (i = 4; i > 0; i--) {
    state.items[ROBOT].character = (chtype)' ';
    state.items[KITTEN].character = (chtype)' ';

    (void)move(state.items[ROBOT].y, state.items[ROBOT].x);
    draw(&state.items[ROBOT]);
    (void)move(state.items[KITTEN].y, state.items[KITTEN].x);
    draw(&state.items[KITTEN]);

    state.items[ROBOT].character = (chtype)'#';
    state.items[KITTEN].character = kitty;
    state.items[ROBOT].y = 1;
    state.items[KITTEN].y = 1;
    if (fromright) {
      state.items[ROBOT].x = animation_meet + i;
      state.items[KITTEN].x = animation_meet - i + 1;
    } else {
      state.items[ROBOT].x = animation_meet - i + 1;
      state.items[KITTEN].x = animation_meet + i;
    }

    (void)move(kitten.y, kitten.x);
    draw(&kitten);
    (void)move(robot.y, robot.x);
    draw(&robot);

    (void)move(state.items[ROBOT].y, state.items[ROBOT].x);
    draw(&state.items[ROBOT]);
    (void)move(state.items[KITTEN].y, state.items[KITTEN].x);
    draw(&state.items[KITTEN]);
    (void)move(state.items[ROBOT].y, state.items[ROBOT].x);
    (void)refresh();
    (void)sleep(1);
  }
  message(WIN_MESSAGE);
  (void)curs_set(0);
  (void)sleep(1);
}

static void main_loop(void) {
  int ch, x, y;
  unsigned int bnum = 0;
  bool fromright;

  fromright = false;

  while ((ch = getch()) != 0) {
    y = state.items[ROBOT].y;
    x = state.items[ROBOT].x;
    switch (ch) {
      case NETHACK_UL:
      case NETHACK_ul:
      case NUMLOCK_UL:
      case KEY_A1:
      case KEY_HOME:
        y--;
        x--;
        fromright = true;
        break;
      case EMACS_PREVIOUS:
      case NETHACK_UP:
      case NETHACK_up:
      case NUMLOCK_UP:
      case KEY_UP:
        /* fromright: special case */
        y--;
        fromright = true;
        break;
      case NETHACK_UR:
      case NETHACK_ur:
      case NUMLOCK_UR:
      case KEY_A3:
      case KEY_PPAGE:
        y--;
        x++;
        break;
      case EMACS_BACKWARD:
      case NETHACK_LEFT:
      case NETHACK_left:
      case NUMLOCK_LEFT:
      case KEY_LEFT:
        x--;
        fromright = true;
        break;
      case EMACS_FORWARD:
      case NETHACK_RIGHT:
      case NETHACK_right:
      case NUMLOCK_RIGHT:
      case KEY_RIGHT:
        x++;
        fromright = false;
        break;
      case NETHACK_DL:
      case NETHACK_dl:
      case NUMLOCK_DL:
      case KEY_C1:
      case KEY_END:
        y++;
        x--;
        fromright = true;
        break;
      case EMACS_NEXT:
      case NETHACK_DOWN:
      case NETHACK_down:
      case NUMLOCK_DOWN:
      case KEY_DOWN:
        y++;
        break;
      case NETHACK_DR:
      case NETHACK_dr:
      case NUMLOCK_DR:
      case KEY_C3:
      case KEY_NPAGE:
        y++;
        x++;
        break;
      case MYKEY_Q:
      case MYKEY_q:
        finish(EXIT_FAILURE);
        break;
      case MYKEY_REDRAW:
        draw_screen();
        break;
      case KEY_RESIZE:
        handle_resize();
        break;
      default:
        message(
            "Invalid input: Use direction keys"
            " or q.");
        break;
    }

    /* it's the edge of the world as we know it... */
    if ((y < HEADSIZE + FRAME) || (y >= state.lines - FRAME) || (x < FRAME) ||
        (x >= state.cols - FRAME))
      continue;

    /* let's see where we've landed */
    switch (test(y, x, &bnum)) {
      case 0:
        /* robot moved */
        state.items[ROBOT].character = (chtype)' ';
        draw(&state.items[ROBOT]);
        state.items[ROBOT].y = y;
        state.items[ROBOT].x = x;
        state.items[ROBOT].character = (chtype)'#';
        (void)move(y, x);
        draw(&state.items[ROBOT]);
        (void)move(y, x);
        (void)refresh();
        break;
      case BROBOT:
        /* nothing happened */
        break;
      case BKITTEN:
        play_animation(fromright);
        finish(0);
        break;
      case BBOGUS:
        message(state.messages[bnum]);
        break;
      default:
        message("Well, that was unexpected...");
        break;
    }
  }
}

int main(int count, char** arguments) {
  int seed = time(0);
  size_t item_count = DEFAULT_NUM_BOGUS;

  while (true) {
    int option = getopt(count, arguments, "n:s:Vh");
    if (-1 == option) {
      break;
    }

    switch (option) {
      case 'n': {
        int i = atoi(optarg);
        if (i <= 0) {
          (void)fprintf(stderr, "Argument must be positive.\n");
          exit(EXIT_FAILURE);
        }
        item_count = i;
        break;
      }
      case 's':
        seed = atoi(optarg);
        break;
      case 'V':
        (void)printf("robotfindskitten: %s\n", PACKAGE_VERSION);
        exit(EXIT_SUCCESS);
      case 'h':
      case '?':
      default:
        (void)printf("usage: %s [-n nitems] [-s seed] [-V]\n", arguments[0]);
        exit(EXIT_SUCCESS);
    }
  }

  state.options = DEFAULT_OPTIONS;
  srandom(seed);
  read_messages();
  assert(state.num_messages > 0);
  randomize_messages();

  if (item_count > state.num_messages) {
    item_count = state.num_messages;
  }
  init(item_count);

  instructions();
  draw_screen();
  main_loop();
  finish(EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
