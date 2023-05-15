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

#define _XOPEN_SOURCE_EXTENDED

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

static const char Introduction[] =
    "This is robotfindskitten, version 2.718281828, by the illustrious\n"
    "Leonard Richardson © 1997, 2000.\n"
    "\n"
    "Written originally for the Nerth Pork robotfindskitten contest.\n"
    "\n"
    "In this game, you are Robot 🤖. Your job is to find Kitten 😺.\n"
    "Inevitably, this task is complicated by the existence of various\n"
    "items which are not Kitten. As in our world, things are rarely what\n"
    "they seem, so you must touch them to determine whether they are\n"
    "Kitten or not.\n"
    "\n"
    "The game ends when robotfindskitten. Alternatively, you may end\n"
    "the game by pressing the Q key or a good old-fashioned Control-C.\n"
    "\n"
    "You can move using the arrow keys, the Emacs movement control sequences,\n"
    "the vi and NetHack movement keys, or the number keypad.\n"
    "\n"
    "Press any key to start.\n";
static const char WinMessage[] = "You found Kitten! Way to go, Robot!";

static const size_t DefaultItemCount = 20;

#define CONTROL(key) ((key)&0x1f)

#define COUNT(a) (sizeof((a)) / sizeof((a)[0]))

typedef enum KeyCode {
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

static const int HeaderSize = 1;
static const int FrameThickness = 1;
static const unsigned int White = 7;

typedef struct Item {
  int x;
  int y;
  unsigned int color;
  char* icon;
} Item;

static struct Game {
  int lines;
  int columns;
  bool screen_has_color;
  unsigned int border_color;
  size_t item_count;
  Item items[COUNT(Messages)];
  size_t message_count;
  char** messages;
  size_t icon_count;
  char** icons;
} Game;

// Special indices in the Game.items array.
static const size_t Robot = 0;
static const size_t Kitten = 1;
static const size_t Bogus = 2;

static bool StringsEqual(const char* a, const char* b) {
  return strcmp(a, b) == 0;
}

static void Shuffle(char** array, size_t count) {
  for (size_t i = 0; i < count - 1; ++i) {
    const size_t j = i + ((size_t)random() % (count - i));
    char* temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}

static int GetRandomX(void) {
  return FrameThickness + (random() % (Game.columns - FrameThickness * 2));
}

static int GetRandomY(void) {
  return HeaderSize + FrameThickness +
         (random() % (Game.lines - HeaderSize - FrameThickness * 2));
}

static unsigned int GetRandomColor(void) {
  return ((unsigned int)random()) % 6 + 1;
}

static char* GetRandomIcon(void) {
  static size_t previous = 0;
  char* r = Icons[previous];
  previous = (previous + 1) % COUNT(Icons);
  return r;
}

static bool ItemsCoincide(const Item* a, const Item* b) {
  return a->x == b->x && a->y == b->y;
}

typedef enum {
  TouchTestResultNone,
  TouchTestResultRobot,
  TouchTestResultKitten,
  TouchTestResultNonKitten,
} TouchTestResult;

static TouchTestResult TouchTest(int y, int x, size_t* item_number) {
  for (size_t i = 0; i < Game.item_count; ++i) {
    if (Game.items[i].x == x && Game.items[i].y == y) {
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
  return TouchTestResultNone;
}

static noreturn void Finish(int signal) {
  endwin();
  exit(signal);
}

static void InitializeGame(size_t item_count) {
  Game.messages = Messages;
  Game.message_count = COUNT(Messages);
  // Shuffle only the items after the Robot and Kitten placeholders:
  Shuffle(&Game.messages[Bogus], Game.message_count - Bogus);
  // Ensure that we did that correctly:
  assert(StringsEqual("", Game.messages[Robot]));
  assert(StringsEqual("", Game.messages[Kitten]));

  Game.item_count = Bogus + item_count;

  Game.icons = Icons;
  Game.icon_count = COUNT(Icons);
  Shuffle(Game.icons, Game.icon_count);

  Game.border_color = GetRandomColor();

  // Set up (n)curses.
  initscr();
  nonl();
  noecho();
  cbreak();
  intrflush(stdscr, false);
  keypad(stdscr, true);

  Game.lines = LINES;
  Game.columns = COLS;
  if (((Game.lines - HeaderSize - FrameThickness) * Game.columns) <
      (int)(item_count + 2)) {
    endwin();
    fprintf(stderr, "Screen too small to fit all objects!\n");
    exit(EXIT_FAILURE);
  }

  Game.items[Robot].icon = "🤖";  // We are a curious robot.
  Game.items[Robot].y = GetRandomY();
  Game.items[Robot].x = GetRandomX();

  Game.items[Kitten].icon = GetRandomIcon();
  do {
    Game.items[Kitten].y = GetRandomY();
    Game.items[Kitten].x = GetRandomX();
  } while (ItemsCoincide(&Game.items[Robot], &Game.items[Kitten]));

  for (size_t i = Bogus; i < Game.item_count; ++i) {
    Game.items[i].icon = GetRandomIcon();
    while (true) {
      Game.items[i].y = GetRandomY();
      Game.items[i].x = GetRandomX();
      if (ItemsCoincide(&Game.items[Robot], &Game.items[i]) ||
          ItemsCoincide(&Game.items[Kitten], &Game.items[i])) {
        continue;
      }
      size_t j;
      for (j = 0; j < i; ++j) {
        if (ItemsCoincide(&Game.items[j], &Game.items[i])) {
          break;
        }
      }
      if (j == i) {
        break;
      }
    }
  }

  Game.screen_has_color = false;
  start_color();
  if (has_colors() && (COLOR_PAIRS > 7)) {
    Game.screen_has_color = true;
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    bkgd((chtype)COLOR_PAIR(White));
  }
}

static void DrawItem(const Item* o) {
  mvprintw(o->y, o->x, "%s", o->icon);
}

static void DrawMessage(const char* message) {
  int y, x;
  getyx(curscr, y, x);
  if (Game.screen_has_color) {
    attrset(COLOR_PAIR(White));
  }
  move(0, 0);
  clrtoeol();
  mvprintw(0, 0, "%.*s", Game.columns, message);
  move(y, x);
  refresh();
}

static void RedrawScreen(void) {
  const unsigned int attributes = COLOR_PAIR(Game.border_color) | A_BOLD;
  if (Game.screen_has_color) {
    attron(attributes);
  }
  clear();
  mvadd_wch(HeaderSize, 0, WACS_ULCORNER);
  mvadd_wch(HeaderSize, COLS - 1, WACS_URCORNER);
  mvadd_wch(LINES - 1, 0, WACS_LLCORNER);
  mvadd_wch(LINES - 1, COLS - 1, WACS_LRCORNER);
  for (int i = 1; i < COLS - 1; ++i) {
    mvadd_wch(HeaderSize, i, WACS_HLINE);
    mvadd_wch(LINES - 1, i, WACS_HLINE);
  }
  for (int i = FrameThickness + HeaderSize; i < LINES - 1; ++i) {
    mvadd_wch(i, 0, WACS_VLINE);
    mvadd_wch(i, COLS - 1, WACS_VLINE);
  }

  move(0, 0);
  if (Game.screen_has_color) {
    attroff(attributes);
  }
  for (size_t i = 0; i < Game.item_count; ++i) {
    DrawItem(&Game.items[i]);
  }
  move(Game.items[Robot].y, Game.items[Robot].x);
  if (Game.screen_has_color) {
    attrset(COLOR_PAIR(White));
  }
  refresh();
}

static void HandleResize(void) {
  int xbound = 0, ybound = 0;
  for (size_t i = 0; i < Game.item_count; ++i) {
    if (Game.items[i].x > xbound) {
      xbound = Game.items[i].x;
    }
    if (Game.items[i].y > ybound) {
      ybound = Game.items[i].y;
    }
  }

  // Has the resize hidden any items?
  if (xbound >= COLS - FrameThickness * 2 ||
      ybound >= HeaderSize + LINES - FrameThickness * 2) {
    endwin();
    fprintf(stderr, "You crushed the simulation. And robot. And kitten.\n");
    exit(EXIT_FAILURE);
  }

  Game.lines = LINES;
  Game.columns = COLS;
  RedrawScreen();
}

static void ShowIntroduction(void) {
  clear();
  move(0, 0);
  printw(Introduction);
  refresh();
  if (getch() == KEY_RESIZE) {
    HandleResize();
  }
  clear();
}

static void PlayAnimation(bool approach_from_right) {
  move(0, 0);
  clrtoeol();
  const int animation_meet = (COLS / 2);

  Item kitten;
  memcpy(&kitten, &Game.items[Kitten], sizeof(kitten));

  Item robot;
  memcpy(&robot, &Game.items[Robot], sizeof(robot));

  Game.items[Robot].y = Game.items[Kitten].y = 0;
  for (int i = 4; i > 0; --i) {
    printf("\a");

    Game.items[Robot].icon = " ";
    DrawItem(&Game.items[Robot]);
    Game.items[Kitten].icon = " ";
    DrawItem(&Game.items[Kitten]);

    Game.items[Robot].icon = "🤖";
    Game.items[Kitten].icon = "😺";
    if (approach_from_right) {
      Game.items[Robot].x = animation_meet + i;
      Game.items[Kitten].x = animation_meet - i + 1;
    } else {
      Game.items[Robot].x = animation_meet - i + 1;
      Game.items[Kitten].x = animation_meet + i;
    }

    DrawItem(&kitten);
    DrawItem(&robot);

    DrawItem(&Game.items[Robot]);
    DrawItem(&Game.items[Kitten]);
    move(Game.items[Robot].y, Game.items[Robot].x);
    refresh();
    sleep(1);
  }
  DrawMessage(WinMessage);
  curs_set(0);
  sleep(1);
}

static void MainLoop(void) {
  while (true) {
    const int ch = getch();
    if (ch == 0) {
      break;
    }

    int y = Game.items[Robot].y;
    int x = Game.items[Robot].x;
    size_t item_number = 0;
    bool approach_from_right = false;

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
        DrawMessage("Use direction keys or Q to quit.");
        break;
    }

    // It's the edge of the world as we know it...
    if ((y < HeaderSize + FrameThickness) ||
        (y >= Game.lines - FrameThickness) || (x < FrameThickness) ||
        (x >= Game.columns - FrameThickness)) {
      continue;
    }

    // Let's see where we've landed.
    switch (TouchTest(y, x, &item_number)) {
      case TouchTestResultNone:
        // Robot moved.
        Game.items[Robot].y = y;
        Game.items[Robot].x = x;
        move(y, x);
        DrawItem(&Game.items[Robot]);
        // Using RedrawScreen instead of refresh restores the icon the
        // robot touched, but is visibly slower.
        RedrawScreen();
        break;
      case TouchTestResultRobot:
        // Nothing happened.
        break;
      case TouchTestResultKitten:
        PlayAnimation(approach_from_right);
        Finish(EXIT_SUCCESS);
      case TouchTestResultNonKitten:
        DrawMessage(Game.messages[item_number]);
        break;
    }
  }
}

int main(int count, char* arguments[]) {
  signal(SIGINT, Finish);

#if defined(__APPLE__) && defined(__MACH__)
  setlocale(LC_ALL, "");
#else
  setlocale(LC_ALL, NULL);
#endif

  unsigned int seed = (unsigned int)time(0);
  size_t item_count = DefaultItemCount;
  bool options_present = false;

  while (true) {
    const int option = getopt(count, arguments, "n:s:h");
    if (-1 == option) {
      break;
    }

    switch (option) {
      case 'n': {
        item_count = (size_t)abs(atoi(optarg));
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
        printf("Usage: %s [-n item-count] [-s seed]\n", arguments[0]);
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
