/* muhhmenu.c - muhhmenu entry point */

#define _POSIX_C_SOURCE 200809L

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "muhhmenu.h"

/* global state - single instance, accessed everywhere via menu.something */
MenuState menu;

static void sigchld_handler(int signo) {
  (void)signo;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

static void setup_signals(void) {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sa.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &sa, NULL);
}

static void usage(void) {
  fputs("usage: muhhmenu [-v] [-p prompt] [-l lines] [-w width]\n", stderr);
  exit(1);
}

static void output_result(void) {
  Item *item = menu.selected;
  const char *out;

  /* if caller gave an action value for this slot, print that.
   * otherwise fall back to the item label. */
  if (item->actions[menu.result_action][0] != '\0')
    out = item->actions[menu.result_action];
  else
    out = item->label;

  switch (menu.result_action) {
  case ActionPrimary:
    printf("%s\n", out);
    break;
  case ActionSecondary:
    printf("%s\tshift\n", out);
    break;
  case ActionTertiary:
    printf("%s\tctrl\n", out);
    break;
  }
}

int main(int argc, char *argv[]) {
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("muhhmenu: warning: no locale support\n", stderr);

  setup_signals();

  /* set sentinel values before arg parsing */
  menu.result_action = -1;
  menu.selected = NULL;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-v")) {
      puts("muhhmenu-" VERSION);
      return 0;
    } else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
      strncpy(menu.prompt, argv[++i], sizeof(menu.prompt) - 1);
    } else if (!strcmp(argv[i], "-l") && i + 1 < argc) {
      menu.max_items_override = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "-w") && i + 1 < argc) {
      menu.width_override = atoi(argv[++i]);
    } else {
      usage();
    }
  }

  /* init order:
   * 1. stdin    - read all items before opening the window
   * 2. display  - open X connection
   * 3. x11      - window, fonts, grab keyboard and pointer
   * 4. frecency - open db, score items against history
   * 5. state    - set up root panel, apply initial sort
   * 6. draw     - render before entering event loop
   * 7. run      - event loop, blocks until selection or cancel
   */
  items_read_stdin();

  menu.dpy = XOpenDisplay(NULL);
  if (!menu.dpy)
    die("muhhmenu: cannot open display");

  x11_init();
  frecency_init();
  state_init();
  draw_all();
  x11_run();

  /* x11_run returns when the user confirms or cancels.
   * selected is NULL on cancel (Escape or click outside). */
  if (menu.selected) {
    output_result();
    frecency_record(menu.selected->id);
  }

  frecency_close();

  return menu.selected ? 0 : 1;
}
