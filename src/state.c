/* src/state.c - menu state management */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "muhhmenu.h"

/* ── internal helpers ────────────────────────────────────────────────────── */

static int effective_max_items(void) {
  return menu.max_items_override > 0 ? menu.max_items_override : max_items;
}

/* build the visible array for a panel from its item linked list.
 * allocates the visible pointer array — caller owns it. */
static void panel_build_visible(Panel *p) {
  Item *item;
  int i;

  p->visible = ecalloc((size_t)p->count, sizeof(Item *));

  i = 0;
  for (item = p->items; item != NULL; item = item->next) {
    if (i >= p->count)
      break;
    p->visible[i++] = item;
  }

  p->vcount = i;
  p->sel = 0;
  p->scroll = 0;
}

/* apply frecency scores to every item in a panel */
static void panel_apply_frecency(Panel *p) {
  Item *item;

  for (item = p->items; item != NULL; item = item->next)
    item->frecency = frecency_score(item->id);
}

/* clamp selection index to valid range after filter changes */
static void panel_clamp_sel(Panel *p) {
  if (p->vcount == 0) {
    p->sel = 0;
    p->scroll = 0;
    return;
  }

  if (p->sel >= p->vcount)
    p->sel = p->vcount - 1;
  if (p->sel < 0)
    p->sel = 0;

  /* adjust scroll so selected item is always visible */
  if (p->sel < p->scroll)
    p->scroll = p->sel;
  if (p->sel >= p->scroll + effective_max_items())
    p->scroll = p->sel - effective_max_items() + 1;
}

/* ── public api ──────────────────────────────────────────────────────────── */

void state_init(void) {
  Panel *root;

  /* only the root panel exists at startup */
  menu.panel_count = 1;
  root = &menu.panels[0];

  memset(root, 0, sizeof(Panel));

  root->items = menu.allitems;
  root->count = menu.item_count;

  panel_apply_frecency(root);
  panel_build_visible(root);

  /* no query yet — sort by frecency descending for initial display */
  state_update_filter(root);
}

void state_reset_query(void) {
  memset(menu.query, 0, sizeof(menu.query));
  menu.query_len = 0;
  menu.cursor = 0;

  state_update_filter(ROOTPANEL());
}

void state_update_filter(Panel *p) {
  int i, j;

  if (menu.query_len == 0) {
    /* no query — show everything, sorted by frecency */
    int vi = 0;
    Item *item;

    for (item = p->items; item != NULL; item = item->next) {
      item->match_count = 0;
      if (vi < p->count)
        p->visible[vi++] = item;
    }
    p->vcount = vi;

    /* sort by frecency descending — simple insertion sort,
     * list is nearly sorted after frecency_score already ran */
    for (i = 1; i < p->vcount; i++) {
      Item *key = p->visible[i];
      j = i - 1;
      while (j >= 0 && p->visible[j]->frecency < key->frecency) {
        p->visible[j + 1] = p->visible[j];
        j--;
      }
      p->visible[j + 1] = key;
    }
  } else {
    /* query active — fuzzy score and filter */
    items_score(p, menu.query);
  }

  panel_clamp_sel(p);
}
