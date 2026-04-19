/* muhhmenu.h - core types and state for muhhmenu
 * single include for all translation units
 */

#ifndef MUHHMENU_H
#define MUHHMENU_H

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "drw.h"
#include "util.h"

/* ── constants ──────────────────────────────────────────────────────────── */

#define MUHHMENU_VERSION "0.1"
#define MAX_ACTIONS 3
#define MAX_QUERY 256
#define MAX_LABEL 512
#define MAX_ID 256
#define MAX_PANELS 8 /* max submenu depth */

/* ── enums ──────────────────────────────────────────────────────────────── */

enum { SchemeNorm, SchemeSel, SchemeHint, SchemeMatch, SchemeLast };

enum {
  ActionPrimary = 0,
  ActionSecondary = 1,
  ActionTertiary = 2,
};

enum {
  MenuTop,
  MenuCenter,
  MenuBottom,
};

/* ── item ───────────────────────────────────────────────────────────────── */

typedef struct Item Item;

struct Item {
  char label[MAX_LABEL];             /* display text                      */
  char id[MAX_ID];                   /* frecency key, defaults to label   */
  char actions[MAX_ACTIONS][MAX_ID]; /* primary, secondary, tertiary    */
  int has_children;                  /* 1 if item opens a submenu         */
  int child_start;                   /* index into items array            */
  int child_count;
  double frecency;                /* precomputed frecency score        */
  double fuzzy;                   /* fuzzy match score for current query */
  int match_positions[MAX_LABEL]; /* which chars are matched       */
  int match_count;
  Item *next; /* linked list within a panel        */
};

/* ── panel ──────────────────────────────────────────────────────────────── */

/* one menu panel — root or a submenu */
typedef struct {
  Window win;
  int x, y, w, h;
  Item *items;    /* head of item linked list          */
  int count;      /* total items                       */
  Item **visible; /* sorted/filtered view              */
  int vcount;     /* visible item count                */
  int sel;        /* selected index into visible       */
  int scroll;     /* top visible row index             */
  int parent_sel; /* which item in parent opened this  */
} Panel;

/* ── global state ───────────────────────────────────────────────────────── */

typedef struct {
  /* x11 */
  Display *dpy;
  Window root;
  int screen;
  int sw, sh; /* screen dimensions                 */
  Drw *drw;
  Clr **scheme;
  int lrpad;

  /* panels */
  Panel panels[MAX_PANELS];
  int panel_count; /* how many panels are open          */

  /* input */
  char query[MAX_QUERY];
  int query_len;
  int cursor; /* caret position in query           */

  /* items backing store */
  Item *allitems; /* all parsed items                  */
  int item_count;

  /* frecency */
  void *frecency_db; /* sqlite3* handle                   */

  /* hover timer — for submenu open delay */
  int hover_item; /* item index mouse is over          */
  struct timespec hover_since;

  /* misc */
  int running;
  int result_action;

  /* result */
  Item *selected;

  /* runtime overrides */
  char prompt[64];
  int max_items_override;
  int width_override;
} MenuState;

extern MenuState menu;

/* ── macros ─────────────────────────────────────────────────────────────── */

#define ROOTPANEL() (&menu.panels[0])
#define TOPPANEL() (&menu.panels[menu.panel_count - 1])
#define TEXTW(x) (drw_fontset_getwidth(menu.drw, (x)) + menu.lrpad)

/* ── key binding ─────────────────────────────────────────────────────────── */

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(void);
} Key;

/* ── function declarations ──────────────────────────────────────────────── */

/* state.c */
void state_init(void);
void state_reset_query(void);
void state_update_filter(Panel *p);

/* items.c */
void items_read_stdin(void);
void items_score(Panel *p, const char *query);
int items_fuzzy_match(const char *label, const char *query, int *positions,
                      int *match_count);

/* x11.c */
void x11_init(void);
void x11_run(void);
Panel *x11_open_panel(int x, int y, int w, int h, int parent_sel);
void x11_close_panel(Panel *p);
void submenu_position(Panel *parent, int parent_item_idx, int *out_x,
                      int *out_y);
int effective_width(void);
int panel_at(int px, int py);

/* draw.c */
void draw_panel(Panel *p);
void draw_all(void);

/* input.c */
void input_keypress(XKeyEvent *e);
void input_buttonpress(XButtonEvent *e);
void input_motion(XMotionEvent *e);
void action_exit(void);
void action_confirm(void);
void action_confirm_alt(void);
void action_confirm_ctrl(void);
void action_next(void);
void action_prev(void);
void action_child(void);
void action_parent(void);
void action_backspace(void);
void action_word_del(void);
void action_clear(void);
void action_paste(void);
void action_cursor_start(void);
void action_cursor_end(void);

/* frecency.c */
void frecency_init(void);
double frecency_score(const char *id);
void frecency_record(const char *id);
void frecency_close(void);

#endif /* MUHHMENU_H */
