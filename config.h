#ifndef CONFIG_H
#define CONFIG_H

#include <X11/X.h>
#define MOD          Mod4Mask // Mod (Mod1 == alt, Mod4 == super)
#define MASTER_SIZE  0.55     // default master size

// Border colors and width
#define FOCUS   "rgb:bc/57/66"
#define UNFOCUS "rgb:88/88/88"
#define BORDER  2
#define GAP     6

const char* dmenucmd[] = { "dmenu_run", NULL };
const char* termcmd[]  = { "st", NULL };
const char* emacscmd[] = { "emacs", NULL };
const char* flameshotcmd[]  = { "flameshot", "gui", NULL };

const struct Rule rules[] = {
	// class    floating    xkb_lock     xkb_lock_group
	{ "Emacs",  0,          1,           0 },
};

#define DESKTOPCHANGE(KEY, TAG) \
    {  MOD,             KEY,   change_desktop,    { .i = TAG }}, \
    {  MOD|ShiftMask,   KEY,   client_to_desktop, { .i = TAG }},

#define SHCMD(cmd) { .com = (const char* []){ "/bin/sh", "-c", cmd, NULL }}

static struct Key keys[] = {
    // MOD                    KEY            FUNCTION        ARGS
	{ MOD,                    XK_r,          spawn,          {.com = dmenucmd }     },
	{ MOD,                    XK_e,          spawn,          {.com = emacscmd }     },
	{ MOD,                    XK_Return,     spawn,          {.com = termcmd }      },
	{ 0,                      XK_Print,      spawn,          {.com = flameshotcmd } },
    { MOD,                    XK_q,          kill_client,    { NULL }               },
    { MOD|ShiftMask,          XK_q,          quit,           { NULL }               },

    { MOD,                    XK_h,          resize_master,  { .i = -15 }           },
    { MOD,                    XK_l,          resize_master,  { .i = +15 }           },
    { MOD,                    XK_Tab,        next_win,       { NULL }               },
    { MOD,                    XK_f,          toggle_float,   { NULL }               },

    { MOD,                    XK_j,          move_down,      { NULL }               },
    { MOD,                    XK_k,          move_up,        { NULL }               },
    { MOD|ShiftMask,          XK_Return,     swap_master,    { NULL }               },
    { MOD,                    XK_comma,      switch_mode,    { NULL }               },
       DESKTOPCHANGE(         XK_0,                          0)
       DESKTOPCHANGE(         XK_1,                          1)
       DESKTOPCHANGE(         XK_2,                          2)
       DESKTOPCHANGE(         XK_3,                          3)
#define DESKTOPS_SIZE            4
};

#endif // CONFIG_H
