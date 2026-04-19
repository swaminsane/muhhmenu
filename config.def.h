/* config.def.h - default configuration for muhhmenu */

/* ── appearance ─────────────────────────────────────────────────────────── */

static const char *font = "monospace:size=10";
static const int max_items = 10;   /* max visible rows              */
static const int menu_width = 600; /* window width in px            */
static const int position = MenuCenter;
static const int submenu_gap = 4; /* px between parent and submenu */
static const int border_px = 1;   /* border width, 0 to disable    */
static const int hover_ms = 150;  /* delay before submenu opens    */

/* ── colors ──────────────────────────────────────────────────────────────── */

static const char *colors[][2] = {
    /*                       fg           bg        */
    [SchemeNorm] = {"#eeeeee", "#111111"},
    [SchemeSel] = {"#111111", "#4a90d9"},
    [SchemeHint] = {"#666666", "#111111"},
    [SchemeMatch] = {"#4a90d9", "#111111"},
};

/* ── keybindings ─────────────────────────────────────────────────────────── */

#define MODKEY Mod1Mask /* Alt */

static const Key keys[] = {
    /* modifier   keysym          function */
    {0, XK_Escape, action_exit},
    {0, XK_Return, action_confirm},
    {ShiftMask, XK_Return, action_confirm_alt},
    {ControlMask, XK_Return, action_confirm_ctrl},
    {0, XK_Up, action_prev},
    {0, XK_Down, action_next},
    {0, XK_Left, action_parent},
    {0, XK_Right, action_child},
    {0, XK_Tab, action_next},
    {ShiftMask, XK_Tab, action_prev},
    {ControlMask, XK_w, action_word_del},
    {ControlMask, XK_u, action_clear},
    {ControlMask, XK_y, action_paste},
    {ControlMask, XK_h, action_backspace},
    {0, XK_BackSpace, action_backspace},
    {0, XK_Home, action_cursor_start},
    {0, XK_End, action_cursor_end},
};
