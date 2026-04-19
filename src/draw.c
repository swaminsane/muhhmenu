/* draw.c - muhhmenu rendering */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "muhhmenu.h"

/* ── helpers ────────────────────────────────────────────────────────────── */

static int item_height(void) { return menu.drw->fonts->h + 2; }

static int input_height(void) { return item_height() + 2; }

static int panel_width(void) {
  return menu.width_override > 0 ? menu.width_override : menu_width;
}

static int panel_max_items(void) {
  return menu.max_items_override > 0 ? menu.max_items_override : max_items;
}

/* ── cursor drawing ─────────────────────────────────────────────────────── */

static void draw_cursor(int x, int y, int h) {
  drw_setscheme(menu.drw, menu.scheme[SchemeNorm]);
  drw_rect(menu.drw, x, y + 2, 1, h - 4, 1, 0);
}

/* ── input field ────────────────────────────────────────────────────────── */

static void draw_input(Panel *p) {
  int x = 0, w = p->w;
  int h = input_height();
  char *q = menu.query;

  drw_setscheme(menu.drw, menu.scheme[SchemeNorm]);
  drw_rect(menu.drw, 0, 0, w, h, 1, 1);

  /* prompt */
  if (menu.prompt[0]) {
    drw_setscheme(menu.drw, menu.scheme[SchemeSel]);
    x = drw_text(menu.drw, x, 0, TEXTW(menu.prompt), h, menu.lrpad / 2,
                 menu.prompt, 0);
  }

  /* query text */
  drw_setscheme(menu.drw, menu.scheme[SchemeNorm]);
  drw_text(menu.drw, x, 0, w - x, h, menu.lrpad / 2, q, 0);

  /* cursor — compute pixel position of caret */
  if (p == TOPPANEL()) {
    char tmp[MAX_QUERY];
    int cursor_x = x + menu.lrpad / 2;
    if (menu.cursor > 0) {
      strncpy(tmp, q, menu.cursor);
      tmp[menu.cursor] = '\0';
      cursor_x += drw_fontset_getwidth(menu.drw, tmp);
    }
    draw_cursor(cursor_x, 0, h);
  }
}

/* ── single item ────────────────────────────────────────────────────────── */

static void draw_item(Panel *p, int idx, int y) {
  Item *it = p->visible[idx];
  int h = item_height();
  int w = p->w;
  int sel = (idx == p->sel);
  int x = 0;

  /* background */
  drw_setscheme(menu.drw, menu.scheme[sel ? SchemeSel : SchemeNorm]);
  drw_rect(menu.drw, 0, y, w, h, 1, 1);

  /* highlight matched characters */
  if (!sel && it->match_count > 0 && menu.query[0]) {
    /* draw label char by char, switching scheme on matched positions */
    char buf[2] = {0};
    int px = menu.lrpad / 2;
    int mi = 0; /* match position index */
    int len = (int)strlen(it->label);
    int i;

    for (i = 0; i < len; i++) {
      int is_match = (mi < it->match_count && it->match_positions[mi] == i);
      if (is_match) {
        drw_setscheme(menu.drw, menu.scheme[SchemeMatch]);
        mi++;
      } else {
        drw_setscheme(menu.drw, menu.scheme[SchemeNorm]);
      }
      buf[0] = it->label[i];
      int cw = drw_fontset_getwidth(menu.drw, buf);
      drw_text(menu.drw, px, y, cw + 1, h, 0, buf, 0);
      px += cw;
    }
  } else {
    /* plain text draw */
    drw_setscheme(menu.drw, menu.scheme[sel ? SchemeSel : SchemeNorm]);
    drw_text(menu.drw, x, y, w, h, menu.lrpad / 2, it->label, 0);
  }

  /* child indicator ▶ */
  if (it->has_children) {
    const char *arrow = "▶";
    int aw = TEXTW(arrow);
    drw_setscheme(menu.drw, menu.scheme[sel ? SchemeSel : SchemeHint]);
    drw_text(menu.drw, w - aw, y, aw, h, 0, arrow, 0);
  }
}

/* ── scrollbar ──────────────────────────────────────────────────────────── */

static void draw_scrollbar(Panel *p) {
  int max = panel_max_items();
  int ih = item_height();
  int ih_s = input_height();

  if (p->vcount <= max)
    return; /* no scrollbar needed */

  int track_h = max * ih;
  int track_y = ih_s;
  int bar_h = track_h * max / p->vcount;
  int bar_y = track_y + track_h * p->scroll / p->vcount;
  int bar_x = p->w - 3;

  drw_setscheme(menu.drw, menu.scheme[SchemeHint]);
  drw_rect(menu.drw, bar_x, bar_y, 2, bar_h, 1, 0);
}

/* ── full panel ─────────────────────────────────────────────────────────── */

void draw_panel(Panel *p) {
  int ih = input_height();
  int itemh = item_height();
  int max = panel_max_items();
  int i;

  /* clear */
  drw_setscheme(menu.drw, menu.scheme[SchemeNorm]);
  drw_rect(menu.drw, 0, 0, p->w, p->h, 1, 1);

  /* only root panel gets input field */
  if (p == ROOTPANEL())
    draw_input(p);

  /* items */
  int visible = p->vcount - p->scroll;
  if (visible > max)
    visible = max;

  for (i = 0; i < visible; i++) {
    int item_idx = p->scroll + i;
    int y = (p == ROOTPANEL() ? ih : 0) + i * itemh;
    draw_item(p, item_idx, y);
  }

  /* empty state */
  if (p->vcount == 0 && p == ROOTPANEL()) {
    drw_setscheme(menu.drw, menu.scheme[SchemeHint]);
    drw_text(menu.drw, 0, ih, p->w, itemh, menu.lrpad / 2, "no matches", 0);
  }

  draw_scrollbar(p);

  drw_map(menu.drw, p->win, 0, 0, p->w, p->h);
}

/* ── draw everything ────────────────────────────────────────────────────── */

void draw_all(void) {
  int i;
  for (i = 0; i < menu.panel_count; i++)
    draw_panel(&menu.panels[i]);
}
