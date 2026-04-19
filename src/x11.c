
/* src/x11.c - X11 window management and event loop */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "config.h"
#include "muhhmenu.h"

/* ── internal helpers ────────────────────────────────────────────────────── */

int effective_width(void) {
  return menu.width_override > 0 ? menu.width_override : menu_width;
}

static int effective_max_items(void) {
  return menu.max_items_override > 0 ? menu.max_items_override : max_items;
}

static int item_height(void) { return menu.drw->fonts->h + 4; }

static int input_height(void) { return item_height() + 2; }

static int panel_height(void) {
  /* input row + N item rows */
  return input_height() + effective_max_items() * item_height();
}

static int subpanel_height(void) {
  /* submenus have no input row */
  return effective_max_items() * item_height();
}

/* ── window creation ─────────────────────────────────────────────────────── */

static Window create_window(int x, int y, int w, int h) {
  XSetWindowAttributes wa;
  unsigned long mask;

  wa.override_redirect = True;
  wa.background_pixel = menu.scheme[SchemeNorm][ColBg].pixel;
  wa.border_pixel = menu.scheme[SchemeSel][ColBg].pixel;
  wa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                  ButtonReleaseMask | PointerMotionMask;

  mask = CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask;

  Window win = XCreateWindow(
      menu.dpy, menu.root, x, y, (unsigned)w, (unsigned)h, (unsigned)border_px,
      DefaultDepth(menu.dpy, menu.screen), CopyFromParent,
      DefaultVisual(menu.dpy, menu.screen), mask, &wa);

  if (!win)
    die("muhhmenu: cannot create window");

  return win;
}

/* ── input grabbing ──────────────────────────────────────────────────────── */

static void grab_input(void) {
  struct timespec ts = {0, 1000000}; /* 1ms */
  int i;

  for (i = 0; i < 1000; i++) {
    if (XGrabKeyboard(menu.dpy, menu.root, True, GrabModeAsync, GrabModeAsync,
                      CurrentTime) == GrabSuccess)
      break;
    nanosleep(&ts, NULL);
  }
  if (i == 1000)
    die("muhhmenu: cannot grab keyboard");

  XGrabPointer(menu.dpy, menu.root, False,
               ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
               GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

static void ungrab_input(void) {
  XUngrabKeyboard(menu.dpy, CurrentTime);
  XUngrabPointer(menu.dpy, CurrentTime);
}

/* ── panel geometry ──────────────────────────────────────────────────────── */

int panel_at(int px, int py) {
  int i;
  for (i = 0; i < menu.panel_count; i++) {
    Panel *p = &menu.panels[i];
    if (px >= p->x && px < p->x + p->w && py >= p->y && py < p->y + p->h)
      return i;
  }
  return -1;
}

void submenu_position(Panel *parent, int parent_item_idx, int *out_x,
                      int *out_y) {
  int w = effective_width();
  int h = subpanel_height();
  int ih = item_height();
  int x, y, row_y;

  /* align vertically to the trigger row */
  row_y = parent->y + input_height() + (parent_item_idx - parent->scroll) * ih;

  /* try right, flip left if overflow */
  x = parent->x + parent->w + submenu_gap;
  if (x + w > menu.sw)
    x = parent->x - w - submenu_gap;

  /* try downward from trigger row, flip up if overflow */
  y = row_y;
  if (y + h > menu.sh)
    y = row_y - h + ih;

  /* clamp to screen */
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;

  *out_x = x;
  *out_y = y;
}

/* ── public api ──────────────────────────────────────────────────────────── */

void x11_init(void) {
  int w, h, x, y;
  Panel *root;

  /* screen geometry */
  menu.screen = DefaultScreen(menu.dpy);
  menu.root = RootWindow(menu.dpy, menu.screen);
  menu.sw = DisplayWidth(menu.dpy, menu.screen);
  menu.sh = DisplayHeight(menu.dpy, menu.screen);

  /* panel dimensions */
  w = effective_width();
  h = panel_height();

  /* drawing context sized to the panel — not the full screen */
  menu.drw =
      drw_create(menu.dpy, menu.screen, menu.root, (unsigned)w, (unsigned)h);
  if (!menu.drw)
    die("muhhmenu: cannot create drawing context");

  if (!drw_fontset_create(menu.drw, &font, 1))
    die("muhhmenu: cannot load font");

  menu.lrpad = (int)menu.drw->fonts->h;

  /* color schemes — 2 colors each (fg, bg) */
  menu.scheme = ecalloc(SchemeLast, sizeof(Clr *));
  for (int i = 0; i < SchemeLast; i++) {
    menu.scheme[i] = drw_scm_create(menu.drw, colors[i], 2);
    if (!menu.scheme[i])
      die("muhhmenu: cannot create color scheme %d", i);
  }

  /* position root panel */
  switch (position) {
  case MenuTop:
    x = (menu.sw - w) / 2;
    y = 0;
    break;
  case MenuBottom:
    x = (menu.sw - w) / 2;
    y = menu.sh - h;
    break;
  default: /* MenuCenter */
    x = (menu.sw - w) / 2;
    y = (menu.sh - h) / 2;
    break;
  }

  root = &menu.panels[0];
  root->x = x;
  root->y = y;
  root->w = w;
  root->h = h;
  root->win = create_window(x, y, w, h);
  menu.panel_count = 1;

  XClassHint ch = {"muhhmenu", "muhhmenu"};
  XSetClassHint(menu.dpy, root->win, &ch);

  /* suppress _NET_WM_NAME so taskbars ignore us */
  Atom net_wm_name = XInternAtom(menu.dpy, "_NET_WM_NAME", False);
  XDeleteProperty(menu.dpy, root->win, net_wm_name);

  XMapRaised(menu.dpy, root->win);
  XSync(menu.dpy, False);

  grab_input();
}

Panel *x11_open_panel(int x, int y, int w, int h, int parent_sel) {
  Panel *p;

  if (menu.panel_count >= MAX_PANELS) {
    fprintf(stderr, "muhhmenu: max panel depth reached\n");
    return NULL;
  }

  p = &menu.panels[menu.panel_count++];
  memset(p, 0, sizeof(Panel));

  p->x = x;
  p->y = y;
  p->w = w;
  p->h = h;
  p->parent_sel = parent_sel;
  p->win = create_window(x, y, w, h);

  XMapRaised(menu.dpy, p->win);
  XSync(menu.dpy, False);

  return p;
}

void x11_close_panel(Panel *p) {
  int i;

  if (!p || !p->win)
    return;

  XUnmapWindow(menu.dpy, p->win);
  XDestroyWindow(menu.dpy, p->win);
  p->win = 0;

  free(p->visible);
  p->visible = NULL;

  /* shift panel stack down */
  i = (int)(p - menu.panels);
  if (i < 0 || i >= menu.panel_count)
    return;

  memmove(&menu.panels[i], &menu.panels[i + 1],
          (size_t)(menu.panel_count - i - 1) * sizeof(Panel));
  menu.panel_count--;

  XSync(menu.dpy, False);
}

void x11_run(void) {
  XEvent ev;

  menu.running = 1;

  while (menu.running) {
    XNextEvent(menu.dpy, &ev);

    if (XFilterEvent(&ev, None))
      continue;

    switch (ev.type) {
    case Expose:
      if (ev.xexpose.count == 0)
        draw_all();
      break;

    case KeyPress:
      input_keypress(&ev.xkey);
      draw_all();
      break;

    case ButtonPress:
      input_buttonpress(&ev.xbutton);
      if (menu.running)
        draw_all();
      break;

    case MotionNotify:
      /* coalesce — only handle the last queued motion */
      while (XCheckTypedEvent(menu.dpy, MotionNotify, &ev))
        ;
      input_motion(&ev.xmotion);
      draw_all();
      break;

    case SelectionNotify:
      /* clipboard paste result */
      if (ev.xselection.property != None) {
        Atom actual;
        int fmt;
        unsigned long n, extra;
        unsigned char *data = NULL;
        XGetWindowProperty(menu.dpy, ev.xselection.requestor,
                           ev.xselection.property, 0L, 4096L, True,
                           AnyPropertyType, &actual, &fmt, &n, &extra, &data);
        if (data) {
          /* insert pasted text into query */
          int i;
          for (i = 0; i < (int)n && menu.query_len < MAX_QUERY - 1; i++) {
            if (data[i] >= 32) {
              memmove(&menu.query[menu.cursor + 1], &menu.query[menu.cursor],
                      (size_t)(menu.query_len - menu.cursor));
              menu.query[menu.cursor++] = (char)data[i];
              menu.query[++menu.query_len] = '\0';
            }
          }
          XFree(data);
          state_update_filter(ROOTPANEL());
          draw_all();
        }
      }
      break;

    case DestroyNotify:
      menu.running = 0;
      break;
    }
  }

  ungrab_input();

  for (int i = menu.panel_count - 1; i >= 0; i--) {
    if (menu.panels[i].win) {
      XDestroyWindow(menu.dpy, menu.panels[i].win);
      free(menu.panels[i].visible);
    }
  }

  XSync(menu.dpy, False);
  XCloseDisplay(menu.dpy);
}
