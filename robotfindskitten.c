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

static const char Version[] = "2.7182818";

/* Option flags for GameState.options. */
#define OPTION_HAS_COLOR 0x01
#define OPTION_DISPLAY_INTRO 0x02

/* Bits returned from test. */
#define BROBOT 0x01
#define BKITTEN 0x02
#define BBOGUS 0x04

/* Nethack keycodes. */
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

/* When numlock is on, the keypad generates numbers. */
#define NUMLOCK_UL '7'
#define NUMLOCK_UP '8'
#define NUMLOCK_UR '9'
#define NUMLOCK_LEFT '4'
#define NUMLOCK_RIGHT '6'
#define NUMLOCK_DL '1'
#define NUMLOCK_DOWN '2'
#define NUMLOCK_DR '3'

/* Emacs keycodes. */
#define CTRL(key) ((key)&0x1f)
#define EMACS_NEXT CTRL('N')
#define EMACS_PREVIOUS CTRL('P')
#define EMACS_BACKWARD CTRL('B')
#define EMACS_FORWARD CTRL('F')

/* miscellaneous
 * I'm paranoid about collisions with curses in the KEY_ namespace */
#define MYKEY_REDRAW CTRL('L')
#define MYKEY_q 'q'
#define MYKEY_Q 'Q'

/* Size of header area above frame and playing field. */
static const int HeaderSize = 2;
static const int FrameThickness = 1;

/* Index of white color pair. */
static const unsigned int White = 7;

static const char WinMessage[] = "You found kitten! Way to go, robot!";

typedef struct {
  int x;
  int y;
  unsigned int color;
  bool bold;
  bool reverse;
  chtype character;
} screen_object;

static struct {
  int lines;
  int columns;
  unsigned int options;
  size_t item_count;
  size_t message_count;
  size_t message_count_alloc;
  screen_object* items;
  char** messages;
} GameState;

/* Special indices in the GameState.items array. */
static const size_t Robot = 0;
static const size_t Kitten = 1;
static const size_t Bogus = 2;

static void add_message(const char* message) {
  static const size_t AllocationGrowth = 32;
  if (GameState.message_count_alloc <= GameState.message_count) {
    char** nmess =
        calloc(GameState.message_count + AllocationGrowth, sizeof(char*));
    GameState.message_count_alloc = GameState.message_count + AllocationGrowth;
    memcpy(nmess, GameState.messages, GameState.message_count * sizeof(char*));
    free(GameState.messages);
    GameState.messages = nmess;
  }
  GameState.messages[GameState.message_count] = strdup(message);
  GameState.message_count++;
}

static void initialize_messages(void) {
  GameState.messages = NULL;
  GameState.message_count = GameState.message_count_alloc = 0;
  for (size_t i = 0; i < Bogus; ++i) {
    add_message("");
  }
  for (size_t i = 0; i < sizeof(Messages) / sizeof(Messages[0]); ++i) {
    add_message(Messages[i]);
  }
}

static void randomize_messages(void) {
  for (size_t i = Bogus; i < (GameState.message_count - 1); ++i) {
    size_t j = i + (random() % (GameState.message_count - i));
    if (i != j) {
      char* temp = GameState.messages[i];
      GameState.messages[i] = GameState.messages[j];
      GameState.messages[j] = temp;
    }
  }
}

static int random_x(void) {
  return FrameThickness + (random() % (GameState.columns - FrameThickness * 2));
}

static int random_y() {
  return HeaderSize + FrameThickness +
         (random() % (GameState.lines - HeaderSize - FrameThickness * 2));
}

static bool random_bold(void) {
  return random() % 2 ? true : false;
}

static unsigned int random_color(void) {
  return random() % 6 + 1;
}

static chtype random_character(void) {
  chtype c;
  do {
    c = (random() % ('~' - '!' + 1) + '!');
  } while (c == '#');
  return c;
}

static bool object_equal(const screen_object a, const screen_object b) {
  return a.x == b.x && a.y == b.y;
}

static size_t test(int y, int x, unsigned int* bnum) {
  for (size_t i = 0; i < GameState.item_count; ++i) {
    if (GameState.items[i].x == x && GameState.items[i].y == y) {
      *bnum = i;
      if (Robot == i) {
        return BROBOT;
      } else if (Kitten == i) {
        return BKITTEN;
      } else {
        return BBOGUS;
      }
    }
  }
  return 0;
}

static void finish(int sig) {
  (void)endwin();
  exit(sig);
}

static void initialize(size_t item_count) {
  GameState.items = calloc(Bogus + item_count, sizeof(screen_object));

  (void)signal(SIGINT, finish);

  /* set up (n)curses */
  (void)initscr();
  (void)nonl();
  (void)noecho();
  (void)cbreak();
  (void)intrflush(stdscr, false);
  (void)keypad(stdscr, true);

  GameState.lines = LINES;
  GameState.columns = COLS;
  if (((GameState.lines - HeaderSize - FrameThickness) * GameState.columns) <
      (int)(item_count + 2)) {
    (void)endwin();
    (void)fprintf(stderr, "Screen too small to fit all objects!\n");
    exit(EXIT_FAILURE);
  }

  GameState.items[Robot].character = (chtype)'#';
  GameState.items[Robot].bold = false; /* we are a timid robot */
  GameState.items[Robot].reverse = false;
  GameState.items[Robot].y = random_y();
  GameState.items[Robot].x = random_x();

  GameState.items[Kitten].character = random_character();
  GameState.items[Kitten].bold = random_bold();
  GameState.items[Kitten].reverse = false;
  do {
    GameState.items[Kitten].y = random_y();
    GameState.items[Kitten].x = random_x();
  } while (object_equal(GameState.items[Robot], GameState.items[Kitten]));

  for (size_t i = Bogus; i < Bogus + item_count; ++i) {
    GameState.items[i].character = random_character();
    GameState.items[i].bold = random_bold();
    GameState.items[i].reverse = false;
    while (true) {
      GameState.items[i].y = random_y();
      GameState.items[i].x = random_x();
      if (object_equal(GameState.items[Robot], GameState.items[i]))
        continue;
      if (object_equal(GameState.items[Kitten], GameState.items[i]))
        continue;
      size_t j;
      for (j = 0; j < i; ++j) {
        if (object_equal(GameState.items[j], GameState.items[i])) {
          break;
        }
      }
      if (j == i) {
        break;
      }
    }
  }
  GameState.item_count = Bogus + item_count;

  (void)start_color();
  if (has_colors() && (COLOR_PAIRS > 7)) {
    GameState.options |= OPTION_HAS_COLOR;
    (void)init_pair(1, COLOR_GREEN, COLOR_BLACK);
    (void)init_pair(2, COLOR_RED, COLOR_BLACK);
    (void)init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    (void)init_pair(4, COLOR_BLUE, COLOR_BLACK);
    (void)init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    (void)init_pair(6, COLOR_CYAN, COLOR_BLACK);
    (void)init_pair(7, COLOR_WHITE, COLOR_BLACK);
    (void)bkgd((chtype)COLOR_PAIR(White));

    GameState.items[Robot].color = White;
    GameState.items[Kitten].color = random_color();
    for (size_t i = Bogus; i < GameState.item_count; ++i) {
      GameState.items[i].color = random_color();
    }
  } else {
    GameState.options &= ~OPTION_HAS_COLOR;
  }
}

static void draw(const screen_object* o) {
  assert(curscr != NULL);
  if ((GameState.options & OPTION_HAS_COLOR) != 0) {
    attr_t new = COLOR_PAIR(o->color);
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

static void message(const char* message) {
  int y, x;
  getyx(curscr, y, x);
  if ((GameState.options & OPTION_HAS_COLOR) != 0) {
    attrset(COLOR_PAIR(White));
  }
  (void)move(1, 0);
  (void)clrtoeol();
  (void)move(1, 0);
  (void)printw("%.*s", GameState.columns, message);
  (void)move(y, x);
  (void)refresh();
}

static void draw_screen() {
  if ((GameState.options & OPTION_HAS_COLOR) != 0) {
    attrset(COLOR_PAIR(White));
  }
  (void)clear();
  (void)mvaddch(HeaderSize, 0, ACS_ULCORNER);
  (void)mvaddch(HeaderSize, COLS - 1, ACS_URCORNER);
  (void)mvaddch(LINES - 1, 0, ACS_LLCORNER);
  (void)mvaddch(LINES - 1, COLS - 1, ACS_LRCORNER);
  for (unsigned int i = 1; i < (unsigned int)COLS - 1; ++i) {
    (void)mvaddch(HeaderSize, (int)i, ACS_HLINE);
    (void)mvaddch(LINES - 1, (int)i, ACS_HLINE);
  }
  for (unsigned int i = FrameThickness + HeaderSize;
       i < (unsigned int)LINES - 1; ++i) {
    (void)mvaddch((int)i, 0, ACS_VLINE);
    (void)mvaddch((int)i, COLS - 1, ACS_VLINE);
  }
  (void)move(0, 0);
  (void)printw("robotfindskitten %s\n\n", Version);
  for (size_t i = 0; i < GameState.item_count; ++i) {
    (void)move(GameState.items[i].y, GameState.items[i].x);
    draw(&GameState.items[i]);
  }
  (void)move(GameState.items[Robot].y, GameState.items[Robot].x);
  if ((GameState.options & OPTION_HAS_COLOR) != 0)
    (void)attrset(COLOR_PAIR(White));
  (void)refresh();
}

static void handle_resize(void) {
  int xbound = 0, ybound = 0;
  unsigned int i;
  for (i = 0; i < GameState.item_count; ++i) {
    if (GameState.items[i].x > xbound)
      xbound = GameState.items[i].x;
    if (GameState.items[i].y > ybound)
      ybound = GameState.items[i].y;
  }

  /* has the resize hidden any items? */
  if (xbound >= COLS - FrameThickness * 2 ||
      ybound >= HeaderSize + LINES - FrameThickness * 2) {
    (void)endwin();
    (void)fprintf(stderr,
                  "You crushed the simulation. And robot. And kitten.\n");
    exit(EXIT_FAILURE);
  }

  GameState.lines = LINES;
  GameState.columns = COLS;
  draw_screen();
}

static void instructions(void) {
  (void)clear();
  (void)move(0, 0);
  (void)printw("robotfindskitten %s\n", Version);
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
  if (getch() == KEY_RESIZE) {
    handle_resize();
  }
  (void)clear();
}

static void play_animation(bool fromright) {
  int i, animation_meet;
  screen_object robot;
  screen_object kitten;
  chtype kitty;

  (void)move(1, 0);
  (void)clrtoeol();
  animation_meet = (COLS / 2);

  memcpy(&kitten, &GameState.items[Kitten], sizeof kitten);
  memcpy(&robot, &GameState.items[Robot], sizeof robot);
  robot.reverse = true;

  kitty = GameState.items[Kitten].character;
  for (i = 4; i > 0; i--) {
    GameState.items[Robot].character = (chtype)' ';
    GameState.items[Kitten].character = (chtype)' ';

    (void)move(GameState.items[Robot].y, GameState.items[Robot].x);
    draw(&GameState.items[Robot]);
    (void)move(GameState.items[Kitten].y, GameState.items[Kitten].x);
    draw(&GameState.items[Kitten]);

    GameState.items[Robot].character = (chtype)'#';
    GameState.items[Kitten].character = kitty;
    GameState.items[Robot].y = 1;
    GameState.items[Kitten].y = 1;
    if (fromright) {
      GameState.items[Robot].x = animation_meet + i;
      GameState.items[Kitten].x = animation_meet - i + 1;
    } else {
      GameState.items[Robot].x = animation_meet - i + 1;
      GameState.items[Kitten].x = animation_meet + i;
    }

    (void)move(kitten.y, kitten.x);
    draw(&kitten);
    (void)move(robot.y, robot.x);
    draw(&robot);

    (void)move(GameState.items[Robot].y, GameState.items[Robot].x);
    draw(&GameState.items[Robot]);
    (void)move(GameState.items[Kitten].y, GameState.items[Kitten].x);
    draw(&GameState.items[Kitten]);
    (void)move(GameState.items[Robot].y, GameState.items[Robot].x);
    (void)refresh();
    (void)sleep(1);
  }
  message(WinMessage);
  (void)curs_set(0);
  (void)sleep(1);
}

static void main_loop(void) {
  int ch, x, y;
  unsigned int bnum = 0;
  bool fromright;

  fromright = false;

  while ((ch = getch()) != 0) {
    y = GameState.items[Robot].y;
    x = GameState.items[Robot].x;
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
    if ((y < HeaderSize + FrameThickness) ||
        (y >= GameState.lines - FrameThickness) || (x < FrameThickness) ||
        (x >= GameState.columns - FrameThickness)) {
      continue;
    }

    /* let's see where we've landed */
    switch (test(y, x, &bnum)) {
      case 0:
        /* robot moved */
        GameState.items[Robot].character = (chtype)' ';
        draw(&GameState.items[Robot]);
        GameState.items[Robot].y = y;
        GameState.items[Robot].x = x;
        GameState.items[Robot].character = (chtype)'#';
        (void)move(y, x);
        draw(&GameState.items[Robot]);
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
        message(GameState.messages[bnum]);
        break;
      default:
        message("Well, that was unexpected...");
        break;
    }
  }
}

int main(int count, char** arguments) {
  int seed = time(0);
  size_t item_count = 20;

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
        (void)printf("robotfindskitten: %s\n", Version);
        exit(EXIT_SUCCESS);
      case 'h':
      case '?':
      default:
        (void)printf("usage: %s [-n nitems] [-s seed] [-V]\n", arguments[0]);
        exit(EXIT_SUCCESS);
    }
  }

  GameState.options = OPTION_DISPLAY_INTRO;
  srandom(seed);
  initialize_messages();
  assert(GameState.message_count > 0);
  randomize_messages();

  initialize(item_count <= GameState.message_count ? item_count
                                                   : GameState.message_count);

  instructions();
  draw_screen();
  main_loop();
  finish(EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
