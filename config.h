#ifndef CONFIG_H
#define CONFIG_H

// Mod (Mod1 == alt, Mod4 == super)
#define MOD          Mod4Mask
#define MASTER_SIZE  0.55

// Colors
#define FOCUS   "rgb:bc/57/66"
#define UNFOCUS "rgb:88/88/88"

const char* dmenucmd[] = { "dmenu_run", NULL };
const char* termcmd[]  = { "st", NULL };

#define SHCMD(cmd) { .cmd = (const char* []){ "/bin/sh", "-c", cmd, NULL }}
#define DESKTOPCHANGE(KEY, TAG) \
    {  MOD,             KEY,   change_desktop,    { .i = TAG }}, \
    {  MOD|ShiftMask,   KEY,   client_to_desktop, { .i = TAG }},

static struct Key keys[] = {
    // MOD              KEY                         FUNCTION        ARGS
    {  MOD,             XK_p,                       spawn,          { .com = dmenucmd }},
    {  MOD,             XK_Return,                  spawn,          { .com = termcmd  }},
    {  MOD,             XK_h,                       decrease,       { NULL }},
    {  MOD,             XK_l,                       increase,       { NULL }},
    {  MOD,             XK_q,                       kill_client,    { NULL }},
    {  MOD,             XK_j,                       next_win,       { NULL }},
    {  MOD,             XK_k,                       prev_win,       { NULL }},
    {  MOD|ShiftMask,   XK_j,                       move_down,      { NULL }},
    {  MOD|ShiftMask,   XK_k,                       move_up,        { NULL }},
    {  MOD|ShiftMask,   XK_Return,                  swap_master,    { NULL }},
    {  MOD,             XK_space,                   switch_mode,    { NULL }},
    {  MOD|ShiftMask,   XK_q,                       quit,           { NULL }}
       DESKTOPCHANGE(   XK_0,                                       0)
       DESKTOPCHANGE(   XK_1,                                       1)
       DESKTOPCHANGE(   XK_2,                                       2)
       DESKTOPCHANGE(   XK_3,                                       3)
       DESKTOPCHANGE(   XK_4,                                       4)
       DESKTOPCHANGE(   XK_5,                                       5)
       DESKTOPCHANGE(   XK_6,                                       6)
};

#endif // CONFIG_H
