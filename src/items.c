/* src/items.c - stdin parsing and fuzzy matching */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "muhhmenu.h"

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void item_init(Item *item) {
  memset(item, 0, sizeof(Item));
  item->frecency = 0.0;
  item->fuzzy = 0.0;
}

/* parse a single metadata field "key=value" into the item.
 * returns 1 if recognised, 0 if unknown (silently ignored). */
static int parse_field(Item *item, const char *key, const char *val) {
  if (!strcmp(key, "id")) {
    strncpy(item->id, val, sizeof(item->id) - 1);
    item->id[sizeof(item->id) - 1] = '\0';
  } else if (!strcmp(key, "action")) {
    strncpy(item->actions[ActionPrimary], val, sizeof(item->actions[0]) - 1);
    item->actions[ActionPrimary][sizeof(item->actions[0]) - 1] = '\0';
  } else if (!strcmp(key, "shift")) {
    strncpy(item->actions[ActionSecondary], val, sizeof(item->actions[0]) - 1);
    item->actions[ActionSecondary][sizeof(item->actions[0]) - 1] = '\0';
  } else if (!strcmp(key, "ctrl")) {
    strncpy(item->actions[ActionTertiary], val, sizeof(item->actions[0]) - 1);
    item->actions[ActionTertiary][sizeof(item->actions[0]) - 1] = '\0';
  } else if (!strcmp(key, "children")) {
    item->has_children = atoi(val) > 0 ? 1 : 0;
  } else {
    return 0;
  }
  return 1;
}

/* parse one line into an Item.
 * format: [key=val\t]* label
 * a line with no tabs is treated as a plain label. */
static void parse_line(const char *line, Item *item) {
  char buf[MAX_LABEL + MAX_ID * MAX_ACTIONS + 64];
  char *p, *tab, *eq;
  char key[64], val[MAX_ID];
  size_t klen, vlen;

  item_init(item);

  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  p = buf;

  /* if no tab exists this is a plain label line */
  if (!strchr(p, '\t')) {
    strncpy(item->label, p, sizeof(item->label) - 1);
    item->label[sizeof(item->label) - 1] = '\0';
    goto done;
  }

  /* consume tab-separated key=value fields until we hit the label.
   * the label is the last tab-separated token — it contains no '='. */
  while ((tab = strchr(p, '\t')) != NULL) {
    *tab = '\0';

    eq = strchr(p, '=');
    if (!eq) {
      /* no '=' means this token is the label */
      strncpy(item->label, p, sizeof(item->label) - 1);
      item->label[sizeof(item->label) - 1] = '\0';
      p = tab + 1;
      break;
    }

    klen = (size_t)(eq - p);
    if (klen >= sizeof(key))
      klen = sizeof(key) - 1;
    memcpy(key, p, klen);
    key[klen] = '\0';

    vlen = strlen(eq + 1);
    if (vlen >= sizeof(val))
      vlen = sizeof(val) - 1;
    memcpy(val, eq + 1, vlen);
    val[vlen] = '\0';

    parse_field(item, key, val);
    p = tab + 1;
  }

  /* anything remaining after the last tab is the label */
  if (*p != '\0') {
    strncpy(item->label, p, sizeof(item->label) - 1);
    item->label[sizeof(item->label) - 1] = '\0';
  }

done:
  /* if no explicit id was given, use the label as the frecency key */
  if (item->id[0] == '\0') {
    strncpy(item->id, item->label, sizeof(item->id) - 1);
    item->id[sizeof(item->id) - 1] = '\0';
  }
}

/* ── fuzzy matching ──────────────────────────────────────────────────────── */

/* score a single character match based on position context.
 * higher is better. */
static double char_score(const char *label, int label_pos, int prev_match) {
  double score = 1.0;
  char c = label[label_pos];
  char prev = label_pos > 0 ? label[label_pos - 1] : '\0';

  /* word boundary — start of label, or after space/separator */
  if (label_pos == 0 || prev == ' ' || prev == '-' || prev == '_' ||
      prev == '/' || prev == '.') {
    score += 8.0;
  }

  /* consecutive match with previous query character */
  if (prev_match && label_pos > 0)
    score += 5.0;

  /* prefer lowercase matches to avoid case noise */
  if (islower((unsigned char)c))
    score += 0.5;

  return score;
}

int items_fuzzy_match(const char *label, const char *query, int *positions,
                      int *match_count) {
  int qi, li;
  int qlen, llen;
  int prev_match;
  double score, total;

  if (!query || query[0] == '\0') {
    if (match_count)
      *match_count = 0;
    return 1; /* empty query matches everything */
  }

  qlen = (int)strlen(query);
  llen = (int)strlen(label);
  total = 0.0;
  qi = 0;
  prev_match = 0;

  for (li = 0; li < llen && qi < qlen; li++) {
    if (tolower((unsigned char)label[li]) ==
        tolower((unsigned char)query[qi])) {
      score = char_score(label, li, prev_match);
      total += score;
      if (positions)
        positions[qi] = li;
      qi++;
      prev_match = 1;
    } else {
      prev_match = 0;
    }
  }

  /* not all query characters matched */
  if (qi < qlen) {
    if (match_count)
      *match_count = 0;
    return 0;
  }

  if (match_count)
    *match_count = qlen;
  return (int)total;
}

/* ── scoring ─────────────────────────────────────────────────────────────── */

void items_score(Panel *p, const char *query) {
  Item *item;
  int match, positions[MAX_LABEL];
  int match_count;
  int vi;

  vi = 0;

  for (item = p->items; item != NULL; item = item->next) {
    memset(positions, 0, sizeof(positions));
    match_count = 0;

    match = items_fuzzy_match(item->label, query, positions, &match_count);
    if (match) {
      item->fuzzy = (double)match + item->frecency * 0.3;
      memcpy(item->match_positions, positions, match_count * sizeof(int));
      item->match_count = match_count;
      if (vi < p->count)
        p->visible[vi++] = item;
    } else {
      item->fuzzy = 0.0;
      item->match_count = 0;
    }
  }

  p->vcount = vi;
  p->sel = 0;

  /* sort visible items by score descending — insertion sort */
  for (int i = 1; i < p->vcount; i++) {
    Item *key = p->visible[i];
    int j = i - 1;
    while (j >= 0 && p->visible[j]->fuzzy < key->fuzzy) {
      p->visible[j + 1] = p->visible[j];
      j--;
    }
    p->visible[j + 1] = key;
  }
}

/* ── stdin reading ─────────────────────────────────────────────────────────
 */

void items_read_stdin(void) {
  char line[MAX_LABEL + MAX_ID * MAX_ACTIONS + 64];
  Item *item, *tail;
  size_t len;

  menu.allitems = NULL;
  menu.item_count = 0;
  tail = NULL;

  while (fgets(line, sizeof(line), stdin) != NULL) {
    /* strip trailing newline */
    len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
      len--;
    }

    /* skip empty lines */
    if (len == 0)
      continue;

    item = ecalloc(1, sizeof(Item));
    parse_line(line, item);

    if (tail)
      tail->next = item;
    else
      menu.allitems = item;

    tail = item;
    menu.item_count++;
  }

  if (ferror(stdin))
    die("muhhmenu: error reading stdin:");
}
