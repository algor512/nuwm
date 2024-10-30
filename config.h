#ifndef CONFIG_H
#define CONFIG_H

#include <X11/X.h>
#define MOD          Mod4Mask // Mod (Mod1 == alt, Mod4 == super)
#define MASTER_SIZE  55        // default master size (in percent)

/* Border colors and width */
#define FOCUS   "rgb:bc/57/66"
#define UNFOCUS "rgb:88/88/88"
#define BORDER  2
#define GAP     6
#define BAR     25

/* move/resize steps (in pixels) */
#define MOVE_STEP   40
#define RESIZE_STEP 15

const unsigned int ignored_modifiers[] = {LockMask, Mod2Mask, Mod3Mask, Mod5Mask};

const char* runcmd[] =        { "launcher", NULL };
const char* emacscmd[] =      { "emacs", NULL };
const char* termcmd[] =       { "st", "/bin/fish", NULL };
const char* screenshotcmd[] = { "screenshot", NULL };

const char* volume_raise_cmd[] =    { "wmactions", "inc-volume", NULL };
const char* volume_lower_cmd[] =    { "wmactions", "dec-volume", NULL };
const char* volume_mute_cmd[] =     { "wmactions", "mute", NULL };
const char* mic_mute_cmd[] =        { "wmactions", "mic-mute", NULL };
const char* brightness_up_cmd[] =   { "wmactions", "inc-bright", NULL };
const char* brightness_dowm_cmd[] = { "wmactions", "dec-bright", NULL };

const struct Rule rules[] = {
	// class                 floating   fullscreen   ignore unmap
	{ "7DaysToDie.x86_64",   1,         1,           1 }
};

#define DESKTOPCHANGE(KEY, TAG)                                         \
	{ MOD,             KEY,   change_desktop,    { .i = TAG }}, \
	{ MOD|ShiftMask,   KEY,   client_to_desktop, { .i = TAG }},

#define SHCMD(cmd) { .com = (const char* []){ "/bin/sh", "-c", cmd, NULL }}

static struct Key keys[] = {
	// MOD                    KEY            FUNCTION        ARGS
	{ MOD,                    XK_r,          spawn,          { .com = runcmd }      },
	{ MOD,                    XK_e,          spawn,          { .com = emacscmd }    },
	{ MOD,                    XK_Return,     spawn,          { .com = termcmd }     },
	{ 0,                      XK_Print,      spawn,          { .com = screenshotcmd }},
	{ MOD,                    XK_q,          kill_client,    { NULL }               },
	{ MOD,                    XK_Tab,        next_win,       { NULL }               },
	{ MOD,                    XK_f,          toggle_float,   { NULL }               },
	{ MOD,                    XK_space,      swap_master,    { NULL }               },
	{ MOD,                    XK_comma,      switch_mode,    { NULL }               },

	{ MOD,                    XK_h,          smart_hjkl,     { .i = XK_h }          },
	{ MOD,                    XK_j,          smart_hjkl,     { .i = XK_j }          },
	{ MOD,                    XK_k,          smart_hjkl,     { .i = XK_k }          },
	{ MOD,                    XK_l,          smart_hjkl,     { .i = XK_l }          },
	{ MOD|ShiftMask,          XK_h,          smart_hjkl,     { .i = XK_H }          },
	{ MOD|ShiftMask,          XK_j,          smart_hjkl,     { .i = XK_J }          },
	{ MOD|ShiftMask,          XK_k,          smart_hjkl,     { .i = XK_K }          },
	{ MOD|ShiftMask,          XK_l,          smart_hjkl,     { .i = XK_L }          },

	{ MOD,                    XK_d,          write_debug,    { NULL }               },
	{ Mod1Mask|ControlMask,   XK_BackSpace,  quit,           { NULL }               },
	DESKTOPCHANGE(            XK_1,                          1)
	DESKTOPCHANGE(            XK_2,                          2)
	DESKTOPCHANGE(            XK_3,                          3)
	DESKTOPCHANGE(            XK_4,                          4)
#define DESKTOPS_SIZE 10
	{ 0,                      XF86XK_AudioRaiseVolume,   spawn, { .com = volume_raise_cmd }    },
	{ 0,                      XF86XK_AudioLowerVolume,   spawn, { .com = volume_lower_cmd }    },
	{ 0,                      XF86XK_AudioMute,          spawn, { .com = volume_mute_cmd }     },
	{ 0,                      XF86XK_AudioMicMute,       spawn, { .com = mic_mute_cmd }        },
	{ 0,                      XF86XK_MonBrightnessUp,    spawn, { .com = brightness_up_cmd }   },
	{ 0,                      XF86XK_MonBrightnessDown,  spawn, { .com = brightness_dowm_cmd } },
};

#endif // CONFIG_H
