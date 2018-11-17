// Copyright © 2004 – 2005 Alexey Toptygin <alexeyt@freeshell.org>. Based on
// sources by Leonard Richardson and others.
//
// Refactored (...defactored?) by Chris Palmer <https://noncombatant.org> on 1
// April 2016.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// EXISTENCE OF KITTEN. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place, Suite 330, Boston, MA  02111-1307  USA

#include <assert.h>
#include <dirent.h>
#include <getopt.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "non_kitten_items.h"

static const char Version[] = "2.71828182";
static const char Introduction[] =
    "By the illustrious Leonard Richardson © 1997, 2000.\n"
    "Written originally for the Nerth Pork robotfindskitten contest.\n"
    "\n"
    "In this game, you are robot (#). Your job is to find kitten. This task\n"
    "is complicated by the existence of various things which are not kitten.\n"
    "Robot must touch items to determine if they are kitten or not. The game\n"
    "ends when robotfindskitten. Alternatively, you may end the game by\n"
    "pressing the q key or a good old-fashioned Control-C.\n"
    "\n"
    "You can move using the arrow keys, the Emacs movement control sequences,\n"
    "the vi and Nethack movement keys, or the number keypad.\n"
    "\n"
    "Press any key to start.\n";
static const char WinMessage[] = "You found kitten! Way to go, robot!";

static const size_t DefaultItemCount = 20;

#define CONTROL(key) ((key)&0x1f)

typedef enum {
  NetHack_down = 'j',
  NetHack_DOWN = 'J',
  NetHack_up = 'k',
  NetHack_UP = 'K',
  NetHack_left = 'h',
  NetHack_LEFT = 'H',
  NetHack_right = 'l',
  NetHack_RIGHT = 'L',
  NetHack_up_left = 'y',
  NetHack_UP_LEFT = 'Y',
  NetHack_up_right = 'u',
  NetHack_UP_RIGHT = 'U',
  NetHack_down_left = 'b',
  NetHack_DOWN_LEFT = 'B',
  NetHack_down_right = 'n',
  NetHack_DOWN_RIGHT = 'N',

  NumLock_UP_LEFT = '7',
  NumLock_UP = '8',
  NumLock_UP_RIGHT = '9',
  NumLock_LEFT = '4',
  NumLock_RIGHT = '6',
  NumLock_DOWN_LEFT = '1',
  NumLock_DOWN = '2',
  NumLock_DOWN_RIGHT = '3',

  Emacs_NEXT = CONTROL('N'),
  Emacs_PREVIOUS = CONTROL('P'),
  Emacs_BACKWARD = CONTROL('B'),
  Emacs_FORWARD = CONTROL('F'),

  Key_RedrawScreen = CONTROL('L'),
  Key_quit = 'q',
  Key_QUIT = 'Q',
} KeyCode;

static const int HeaderSize = 2;
static const int FrameThickness = 1;
static const unsigned int White = 7;

typedef struct {
  int x;
  int y;
  unsigned int color;
  bool bold;
  bool reverse;
  chtype character;
} ScreenObject;

static struct {
  int lines;
  int columns;
  bool screen_has_color;
  size_t item_count;
  size_t message_count;
  ScreenObject items[MessageCount];
  char** messages;
} GameState;

// Special indices in the GameState.items array.
static const size_t Robot = 0;
static const size_t Kitten = 1;
static const size_t Bogus = 2;

static void InitializeMessages(void) {
  GameState.messages = Messages;
  GameState.message_count = MessageCount;
  for (size_t i = Bogus; i < (GameState.message_count - 1); ++i) {
    size_t j = i + ((size_t)random() % (GameState.message_count - i));
    if (i != j) {
      char* temp = GameState.messages[i];
      GameState.messages[i] = GameState.messages[j];
      GameState.messages[j] = temp;
    }
  }
  assert(GameState.message_count > 0);
  assert(0 == strcmp("", GameState.messages[Robot]));
  assert(0 == strcmp("", GameState.messages[Kitten]));
}

static int RandomX(void) {
  return FrameThickness + (random() % (GameState.columns - FrameThickness * 2));
}

static int RandomY(void) {
  return HeaderSize + FrameThickness +
         (random() % (GameState.lines - HeaderSize - FrameThickness * 2));
}

static bool RandomBold(void) {
  return random() % 2 ? true : false;
}

static unsigned int RandomColor(void) {
  return random() % 6 + 1;
}

static chtype RandomCharacter(void) {
  chtype c;
  do {
    c = (random() % ('~' - '!' + 1) + '!');
  } while (c == '#');
  return c;
}

static bool ScreenObjectEqual(const ScreenObject a, const ScreenObject b) {
  return a.x == b.x && a.y == b.y;
}

typedef enum {
  TouchTestResultNone,
  TouchTestResultRobot,
  TouchTestResultKitten,
  TouchTestResultNonKitten,
} TouchTestResult;

static TouchTestResult TouchTest(int y, int x, size_t* item_number) {
  for (size_t i = 0; i < GameState.item_count; ++i) {
    if (GameState.items[i].x == x && GameState.items[i].y == y) {
      *item_number = i;
      if (Robot == i) {
        return TouchTestResultRobot;
      } else if (Kitten == i) {
        return TouchTestResultKitten;
      } else {
        return TouchTestResultNonKitten;
      }
    }
  }
  return 0;
}

static noreturn void Finish(int signal) {
  endwin();
  exit(signal);
}

static void InitializeGame(size_t item_count) {
  InitializeMessages();
  item_count = GameState.message_count ? item_count : GameState.message_count;
  GameState.item_count = Bogus + item_count;

  signal(SIGINT, Finish);

  // Set up (n)curses.
  initscr();
  nonl();
  noecho();
  cbreak();
  intrflush(stdscr, false);
  keypad(stdscr, true);

  GameState.lines = LINES;
  GameState.columns = COLS;
  if (((GameState.lines - HeaderSize - FrameThickness) * GameState.columns) <
      (int)(item_count + 2)) {
    endwin();
    fprintf(stderr, "Screen too small to fit all objects!\n");
    exit(EXIT_FAILURE);
  }

  GameState.items[Robot].character = (chtype)'#';
  GameState.items[Robot].bold = false;  // We are a timid robot.
  GameState.items[Robot].reverse = false;
  GameState.items[Robot].y = RandomY();
  GameState.items[Robot].x = RandomX();

  GameState.items[Kitten].character = RandomCharacter();
  GameState.items[Kitten].bold = RandomBold();
  GameState.items[Kitten].reverse = false;
  do {
    GameState.items[Kitten].y = RandomY();
    GameState.items[Kitten].x = RandomX();
  } while (ScreenObjectEqual(GameState.items[Robot], GameState.items[Kitten]));

  for (size_t i = Bogus; i < GameState.item_count; ++i) {
    GameState.items[i].character = RandomCharacter();
    GameState.items[i].bold = RandomBold();
    GameState.items[i].reverse = false;
    while (true) {
      GameState.items[i].y = RandomY();
      GameState.items[i].x = RandomX();
      if (ScreenObjectEqual(GameState.items[Robot], GameState.items[i])) {
        continue;
      }
      if (ScreenObjectEqual(GameState.items[Kitten], GameState.items[i])) {
        continue;
      }
      size_t j;
      for (j = 0; j < i; ++j) {
        if (ScreenObjectEqual(GameState.items[j], GameState.items[i])) {
          break;
        }
      }
      if (j == i) {
        break;
      }
    }
  }

  GameState.screen_has_color = false;
  start_color();
  if (has_colors() && (COLOR_PAIRS > 7)) {
    GameState.screen_has_color = true;
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    bkgd((chtype)COLOR_PAIR(White));

    GameState.items[Robot].color = White;
    GameState.items[Kitten].color = RandomColor();
    for (size_t i = Bogus; i < GameState.item_count; ++i) {
      GameState.items[i].color = RandomColor();
    }
  }
}

static void Draw(const ScreenObject* o) {
  assert(curscr != NULL);
  if (GameState.screen_has_color) {
    attr_t new = COLOR_PAIR(o->color);
    if (o->bold) {
      new |= A_BOLD;
    }
    if (o->reverse) {
      new |= A_REVERSE;
    }
    attrset(new);
  }
  addch(o->character);
}

static void ShowMessage(const char* message) {
  int y, x;
  getyx(curscr, y, x);
  if (GameState.screen_has_color) {
    attrset(COLOR_PAIR(White));
  }
  move(1, 0);
  clrtoeol();
  move(1, 0);
  printw("%.*s", GameState.columns, message);
  move(y, x);
  refresh();
}

static void RedrawScreen(void) {
  if (GameState.screen_has_color) {
    attrset(COLOR_PAIR(White));
  }
  clear();
  mvaddch(HeaderSize, 0, ACS_ULCORNER);
  mvaddch(HeaderSize, COLS - 1, ACS_URCORNER);
  mvaddch(LINES - 1, 0, ACS_LLCORNER);
  mvaddch(LINES - 1, COLS - 1, ACS_LRCORNER);
  for (unsigned int i = 1; i < (unsigned int)COLS - 1; ++i) {
    mvaddch(HeaderSize, (int)i, ACS_HLINE);
    mvaddch(LINES - 1, (int)i, ACS_HLINE);
  }
  for (unsigned int i = FrameThickness + HeaderSize;
       i < (unsigned int)LINES - 1; ++i) {
    mvaddch((int)i, 0, ACS_VLINE);
    mvaddch((int)i, COLS - 1, ACS_VLINE);
  }
  move(0, 0);
  printw("robotfindskitten %s\n\n", Version);
  for (size_t i = 0; i < GameState.item_count; ++i) {
    move(GameState.items[i].y, GameState.items[i].x);
    Draw(&GameState.items[i]);
  }
  move(GameState.items[Robot].y, GameState.items[Robot].x);
  if (GameState.screen_has_color) {
    attrset(COLOR_PAIR(White));
  }
  refresh();
}

static void HandleResize(void) {
  int xbound = 0, ybound = 0;
  unsigned int i;
  for (i = 0; i < GameState.item_count; ++i) {
    if (GameState.items[i].x > xbound) {
      xbound = GameState.items[i].x;
    }
    if (GameState.items[i].y > ybound) {
      ybound = GameState.items[i].y;
    }
  }

  // Has the resize hidden any items?
  if (xbound >= COLS - FrameThickness * 2 ||
      ybound >= HeaderSize + LINES - FrameThickness * 2) {
    endwin();
    fprintf(stderr, "You crushed the simulation. And robot. And kitten.\n");
    exit(EXIT_FAILURE);
  }

  GameState.lines = LINES;
  GameState.columns = COLS;
  RedrawScreen();
}

static void ShowIntroduction(void) {
  clear();
  move(0, 0);
  printw("robotfindskitten %s\n", Version);
  printw(Introduction);
  refresh();
  if (getch() == KEY_RESIZE) {
    HandleResize();
  }
  clear();
}

static void PlayAnimation(bool approach_from_right) {
  move(1, 0);
  clrtoeol();
  int animation_meet = (COLS / 2);

  ScreenObject kitten;
  memcpy(&kitten, &GameState.items[Kitten], sizeof(kitten));

  ScreenObject robot;
  memcpy(&robot, &GameState.items[Robot], sizeof(robot));
  robot.reverse = true;

  chtype kitty = GameState.items[Kitten].character;
  for (int i = 4; i > 0; --i) {
    GameState.items[Robot].character = (chtype)' ';
    GameState.items[Kitten].character = (chtype)' ';

    move(GameState.items[Robot].y, GameState.items[Robot].x);
    Draw(&GameState.items[Robot]);
    move(GameState.items[Kitten].y, GameState.items[Kitten].x);
    Draw(&GameState.items[Kitten]);

    GameState.items[Robot].character = (chtype)'#';
    GameState.items[Kitten].character = kitty;
    GameState.items[Robot].y = 1;
    GameState.items[Kitten].y = 1;
    if (approach_from_right) {
      GameState.items[Robot].x = animation_meet + i;
      GameState.items[Kitten].x = animation_meet - i + 1;
    } else {
      GameState.items[Robot].x = animation_meet - i + 1;
      GameState.items[Kitten].x = animation_meet + i;
    }

    move(kitten.y, kitten.x);
    Draw(&kitten);
    move(robot.y, robot.x);
    Draw(&robot);

    move(GameState.items[Robot].y, GameState.items[Robot].x);
    Draw(&GameState.items[Robot]);
    move(GameState.items[Kitten].y, GameState.items[Kitten].x);
    Draw(&GameState.items[Kitten]);
    move(GameState.items[Robot].y, GameState.items[Robot].x);
    refresh();
    sleep(1);
  }
  ShowMessage(WinMessage);
  curs_set(0);
  sleep(1);
}

static void MainLoop(void) {
  int x, y;
  size_t item_number = 0;
  bool approach_from_right = false;

  while (true) {
    int ch = getch();
    if (0 == ch) {
      break;
    }
    y = GameState.items[Robot].y;
    x = GameState.items[Robot].x;
    switch (ch) {
      case NetHack_UP_LEFT:
      case NetHack_up_left:
      case NumLock_UP_LEFT:
      case KEY_A1:
      case KEY_HOME:
        --y;
        --x;
        approach_from_right = true;
        break;
      case Emacs_PREVIOUS:
      case NetHack_UP:
      case NetHack_up:
      case NumLock_UP:
      case KEY_UP:
        // approach_from_right: special case
        --y;
        approach_from_right = true;
        break;
      case NetHack_UP_RIGHT:
      case NetHack_up_right:
      case NumLock_UP_RIGHT:
      case KEY_A3:
      case KEY_PPAGE:
        --y;
        ++x;
        break;
      case Emacs_BACKWARD:
      case NetHack_LEFT:
      case NetHack_left:
      case NumLock_LEFT:
      case KEY_LEFT:
        --x;
        approach_from_right = true;
        break;
      case Emacs_FORWARD:
      case NetHack_RIGHT:
      case NetHack_right:
      case NumLock_RIGHT:
      case KEY_RIGHT:
        ++x;
        approach_from_right = false;
        break;
      case NetHack_DOWN_LEFT:
      case NetHack_down_left:
      case NumLock_DOWN_LEFT:
      case KEY_C1:
      case KEY_END:
        ++y;
        --x;
        approach_from_right = true;
        break;
      case Emacs_NEXT:
      case NetHack_DOWN:
      case NetHack_down:
      case NumLock_DOWN:
      case KEY_DOWN:
        ++y;
        break;
      case NetHack_DOWN_RIGHT:
      case NetHack_down_right:
      case NumLock_DOWN_RIGHT:
      case KEY_C3:
      case KEY_NPAGE:
        ++y;
        ++x;
        break;
      case Key_QUIT:
      case Key_quit:
        Finish(EXIT_FAILURE);
      case Key_RedrawScreen:
        RedrawScreen();
        break;
      case KEY_RESIZE:
        HandleResize();
        break;
      default:
        ShowMessage("Invalid input: Use direction keys or q.");
        break;
    }

    // It's the edge of the world as we know it...
    if ((y < HeaderSize + FrameThickness) ||
        (y >= GameState.lines - FrameThickness) || (x < FrameThickness) ||
        (x >= GameState.columns - FrameThickness)) {
      continue;
    }

    // Let's see where we've landed.
    switch (TouchTest(y, x, &item_number)) {
      case TouchTestResultNone:
        // Robot moved.
        GameState.items[Robot].character = (chtype)' ';
        Draw(&GameState.items[Robot]);
        GameState.items[Robot].y = y;
        GameState.items[Robot].x = x;
        GameState.items[Robot].character = (chtype)'#';
        move(y, x);
        Draw(&GameState.items[Robot]);
        move(y, x);
        refresh();
        break;
      case TouchTestResultRobot:
        // Nothing happened.
        break;
      case TouchTestResultKitten:
        PlayAnimation(approach_from_right);
        Finish(EXIT_SUCCESS);
      case TouchTestResultNonKitten:
        ShowMessage(GameState.messages[item_number]);
        break;
    }
  }
}

int main(int count, char* arguments[]) {
#if defined(__APPLE__) && defined(__MACH__)
  setlocale(LC_ALL, "");
#else
  setlocale(LC_ALL, NULL);
#endif

  unsigned int seed = (unsigned int)time(0);
  size_t item_count = DefaultItemCount;
  bool options_present = false;

  while (true) {
    int option = getopt(count, arguments, "n:s:h");
    if (-1 == option) {
      break;
    }

    switch (option) {
      case 'n': {
        int i = atoi(optarg);
        if (i <= 0) {
          fprintf(stderr, "Argument must be positive.\n");
          exit(EXIT_FAILURE);
        }
        item_count = (size_t)i;
        options_present = true;
        break;
      }
      case 's':
        seed = (unsigned int)atoi(optarg);
        options_present = true;
        break;
      case 'h':
      case '?':
      default:
        printf("usage: %s [-n item-count] [-s seed]\n", arguments[0]);
        exit(EXIT_SUCCESS);
    }
  }

  srandom(seed);
  InitializeGame(item_count);
  if (!options_present) {
    ShowIntroduction();
  }
  RedrawScreen();
  MainLoop();
  Finish(EXIT_SUCCESS);
}
