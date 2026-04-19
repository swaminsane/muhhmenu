/* src/input.c - keyboard and mouse input handling */

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

static int effective_max_items(void) {
  return menu.max_items_override > 0 ? menu.max_items_override : max_items;
}

static int item_height(void) { return menu.drw->fonts->h + 4; }

/* returns the topmost panel — the one receiving keyboard input */
static Panel *active_panel(void) { return TOPPANEL(); }

/* confirm selection from panel p with a given action slot */
static void confirm(Panel *p, int action) {
  if (p->vcount == 0)
    return;

  menu.selected = p->visible[p->sel];
  menu.result_action = action;
  menu.running = 0;
}

/* close all submenus above index 0 */
static void close_all_submenus(void) {
  while (menu.panel_count > 1)
    x11_close_panel(TOPPANEL());
}

/* open submenu for item at index idx in panel p */
static void open_submenu(Panel *p, int idx) {
  Panel *sub;
  Item *item;
  int x, y, w, h;
  int max;

  if (idx < 0 || idx >= p->vcount)
    return;

  item = p->visible[idx];
  if (!item->has_children)
    return;

  /* close any existing submenu first */
  if (menu.panel_count > 1)
    x11_close_panel(TOPPANEL());

  submenu_position(p, idx, &x, &y);

  /* child item list starts at child_start in allitems.
   * walk to that offset. */
  Item *child = menu.allitems;
  for (int i = 0; i < item->child_start && child; i++)
    child = child->next;

  if (!child)
    return;

  max = item->child_count > 0 ? item->child_count : effective_max_items();

  w = effective_width();
  h = menu.drw->fonts->h + 4; /* recompute panel height */
  h = h + effective_max_items() * h + h;

  sub = x11_open_panel(x, y, w, h, idx);
  if (!sub)
    return;

  sub->items = child;
  sub->count = item->child_count;
  sub->visible =
      ecalloc((size_t)(max > 0 ? max : effective_max_items()), sizeof(Item *));

  /* apply frecency and build initial visible list */
  Item *it;
  int vi = 0;
  for (it = sub->items; it && vi < sub->count; it = it->next)
    sub->visible[vi++] = it;
  sub->vcount = vi;
  sub->sel = 0;
  sub->scroll = 0;

  (void)max; /* used above */
}

/* scroll panel p so sel stays in view */
static void scroll_to_sel(Panel *p) {
  int rows = effective_max_items();

  if (p->sel < p->scroll)
    p->scroll = p->sel;
  if (p->sel >= p->scroll + rows)
    p->scroll = p->sel - rows + 1;
}

/* ── query editing ───────────────────────────────────────────────────────── */

static void query_insert(const char *s, int len) {
  if (menu.query_len + len >= MAX_QUERY)
    return;

  /* shift right to make room at cursor */
  memmove(menu.query + menu.cursor + len, menu.query + menu.cursor,
          (size_t)(menu.query_len - menu.cursor));

  memcpy(menu.query + menu.cursor, s, (size_t)len);
  menu.query_len += len;
  menu.cursor += len;
  menu.query[menu.query_len] = '\0';

  state_update_filter(ROOTPANEL());

  /* close submenus when query changes */
  close_all_submenus();
}

static void query_backspace(void) {
  if (menu.cursor == 0)
    return;

  memmove(menu.query + menu.cursor - 1, menu.query + menu.cursor,
          (size_t)(menu.query_len - menu.cursor));
  menu.cursor--;
  menu.query_len--;
  menu.query[menu.query_len] = '\0';

  state_update_filter(ROOTPANEL());
  close_all_submenus();
}

static void query_delete_word(void) {
  int start;

  if (menu.cursor == 0)
    return;

  start = menu.cursor - 1;

  /* skip trailing spaces */
  while (start > 0 && menu.query[start] == ' ')
    start--;

  /* skip back to previous space */
  while (start > 0 && menu.query[start - 1] != ' ')
    start--;

  int deleted = menu.cursor - start;
  memmove(menu.query + start, menu.query + menu.cursor,
          (size_t)(menu.query_len - menu.cursor));

  menu.cursor -= deleted;
  menu.query_len -= deleted;
  menu.query[menu.query_len] = '\0';

  state_update_filter(ROOTPANEL());
  close_all_submenus();
}

static void query_clear(void) {
  state_reset_query();
  close_all_submenus();
}

static void query_paste(void) {
  /* request clipboard selection — response arrives as SelectionNotify.
   * for now we request XA_PRIMARY (middle-click selection buffer). */
  XConvertSelection(menu.dpy, XA_PRIMARY, XA_STRING, XA_STRING,
                    menu.panels[0].win, CurrentTime);
  /* SelectionNotify is handled in x11_run — paste arrives async */
}

/* ── navigation ──────────────────────────────────────────────────────────── */

static void nav_next(Panel *p) {
  if (p->vcount == 0)
    return;
  p->sel = (p->sel + 1) % p->vcount;
  scroll_to_sel(p);
}

static void nav_prev(Panel *p) {
  if (p->vcount == 0)
    return;
  p->sel = (p->sel - 1 + p->vcount) % p->vcount;
  scroll_to_sel(p);
}

static void nav_child(Panel *p) {
  if (p->vcount == 0)
    return;
  if (p->visible[p->sel]->has_children)
    open_submenu(p, p->sel);
}

static void nav_parent(void) {
  if (menu.panel_count > 1)
    x11_close_panel(TOPPANEL());
}

/* ── action handlers called from key table ───────────────────────────────── */

void action_exit(void) { menu.running = 0; }
void action_confirm(void) { confirm(active_panel(), ActionPrimary); }
void action_confirm_alt(void) { confirm(active_panel(), ActionSecondary); }
void action_confirm_ctrl(void) { confirm(active_panel(), ActionTertiary); }
void action_next(void) { nav_next(active_panel()); }
void action_prev(void) { nav_prev(active_panel()); }
void action_child(void) { nav_child(active_panel()); }
void action_parent(void) { nav_parent(); }
void action_backspace(void) { query_backspace(); }
void action_word_del(void) { query_delete_word(); }
void action_clear(void) { query_clear(); }
void action_paste(void) { query_paste(); }
void action_cursor_start(void) { menu.cursor = 0; }
void action_cursor_end(void) { menu.cursor = menu.query_len; }

/* ── public api ──────────────────────────────────────────────────────────── */

void input_keypress(XKeyEvent *e) {
  char buf[32];
  KeySym sym;
  int len;
  size_t i;

  len = XLookupString(e, buf, sizeof(buf) - 1, &sym, NULL);
  buf[len] = '\0';

  /* check key table first */
  for (i = 0; i < LENGTH(keys); i++) {
    if (sym == keys[i].keysym &&
        (keys[i].mod == 0 || (e->state & keys[i].mod)) && keys[i].func) {
      keys[i].func();
      return;
    }
  }

  /* printable character — insert into query */
  if (len > 0 && (unsigned char)buf[0] >= 32)
    query_insert(buf, len);
}

void input_buttonpress(XButtonEvent *e) {
  int pi, row, item_idx;
  Panel *p;

  /* click outside all panels — close */
  pi = panel_at(e->x_root, e->y_root);
  if (pi < 0) {
    menu.running = 0;
    return;
  }

  p = &menu.panels[pi];

  /* close any submenus opened from a higher panel */
  while (menu.panel_count - 1 > pi)
    x11_close_panel(TOPPANEL());

  /* which row was clicked — subtract input row at top */
  row = (e->y_root - p->y - item_height()) / item_height();

  if (row < 0) {
    /* clicked the input field — focus is already there, nothing to do */
    return;
  }

  item_idx = p->scroll + row;
  if (item_idx >= p->vcount)
    return;

  p->sel = item_idx;

  switch (e->button) {
  case Button1:
    /* left click — primary action */
    confirm(p, ActionPrimary);
    break;
  case Button2:
    /* middle click — secondary action */
    confirm(p, ActionSecondary);
    break;
  case Button3:
    /* right click — open action menu (stub for now) */
    /* TODO: inline action picker */
    break;
  case Button4:
    /* scroll up */
    nav_prev(p);
    break;
  case Button5:
    /* scroll down */
    nav_next(p);
    break;
  }
}

void input_motion(XMotionEvent *e) {
  int pi, row, item_idx;
  Panel *p;
  struct timespec now;

  pi = panel_at(e->x_root, e->y_root);
  if (pi < 0)
    return;

  p = &menu.panels[pi];

  row = (e->y_root - p->y - item_height()) / item_height();
  if (row < 0 || row >= effective_max_items())
    return;

  item_idx = p->scroll + row;
  if (item_idx >= p->vcount)
    return;

  /* update selection on hover */
  if (p->sel != item_idx) {
    p->sel = item_idx;

    /* reset hover timer on new item */
    menu.hover_item = item_idx;
    clock_gettime(CLOCK_MONOTONIC, &menu.hover_since);
  }

  /* check if hover delay has elapsed for submenu opening */
  if (p->visible[item_idx]->has_children) {
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms = (now.tv_sec - menu.hover_since.tv_sec) * 1000 +
                      (now.tv_nsec - menu.hover_since.tv_nsec) / 1000000;

    if (elapsed_ms >= hover_ms && menu.hover_item == item_idx)
      open_submenu(p, item_idx);
  }
}
