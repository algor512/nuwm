 /*
 *  Copyright (c) 2022, Vitor Figueiredo
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

#define TABLENGTH(x) (sizeof(x)/sizeof(*x))

// Types visible from config.h
typedef union {
    const char** com;
    const int i;
} Arg;

struct Key {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg *arg);
    const Arg arg;
};

// Functions visible from config.h (public)
static void change_desktop(const Arg *);
static void client_to_desktop(const Arg *);
static void kill_client(const Arg *);
static void move_down(const Arg *);
static void move_up(const Arg *);
static void next_win(const Arg *);
static void prev_win(const Arg *);
static void quit(const Arg *);
static void resize_master(const Arg *);
static void spawn(const Arg *);
static void swap_master(const Arg *);
static void switch_mode(const Arg *);

#include "config.h"
#define VACUUM (2 * (BORDER + GAP))

// Types not visible from config.h (public)
typedef struct Client Client;
struct Client {
    Client *next, *prev;
    Window win;
    int isfull, isfloat;
};

typedef struct Desktop Desktop;
struct Desktop{
    int master_size, mode;
    Client *head, *current;
};

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };
enum { NET_SUPPORTED, NET_FULLSCREEN, NET_WM_STATE, NET_ACTIVE, NET_COUNT };

// Functions not visible from config.h (private)
static void add_window(Window);
static void clientmessage(XEvent *);
static void configurerequest(XEvent *);
static void destroynotify(XEvent *);
static void die(const char *);
static unsigned long getcolor(const char *);
static void grabkeys(void);
static void keypress(XEvent *);
static void maprequest(XEvent *);
static Client *nexttiled(Client *);
static void remove_window(Window);
static void save_desktop(int);
static void select_desktop(int);
static void send_kill_signal(Window);
static void setfullscreen(Client *, int);
static void setup(void);
static void sigchld(int);
static void start(void);
static void tile(void);
static void update_current(void);
static int xerror(Display *, XErrorEvent *);
static int xerrorstart(Display *, XErrorEvent *);
static Client *wintoclient(Window);

// Global variables
static Display *dis;
static int bool_quit;
static int current_desktop;
static int master_size, mode;
static int screen, sh, sw;
static unsigned int win_focus, win_unfocus;
static Window root;
static Client *head, *current;
static Atom wmatoms[WM_COUNT], netatoms[NET_COUNT];
static int (*xerrorxlib)(Display *, XErrorEvent *);

// Events array
static void (*events[LASTEvent])(XEvent *e) = {
    [ClientMessage]    = clientmessage,
    [ConfigureRequest] = configurerequest,
    [DestroyNotify]    = destroynotify,
    [KeyPress]         = keypress,
    [MapRequest]       = maprequest,
};

// Desktop array
static Desktop desktops[DESKTOPS_SIZE];

// Implementation of public functions
void change_desktop(const Arg *arg)
{
    if (arg->i == current_desktop)
        return;

    Client *c;

    // Unmap all window
    for (c = head; c ;c = c->next)
        XUnmapWindow(dis, c->win);

    // Save current "properties"
    save_desktop(current_desktop);

    // Take "properties" from the new desktop
    select_desktop(arg->i);

    // Map all windows
    for (c = head; c; c = c->next)
        XMapWindow(dis, c->win);

    tile();
    update_current();
}

void client_to_desktop(const Arg *arg)
{
    if (arg->i == current_desktop || current == NULL)
        return;

    Window tmp_win = current->win;
    int tmp_current_desktop = current_desktop;

    // Save current desktop to avoid seg fault in *
    save_desktop(current_desktop);

    // Add client to desktop
    select_desktop(arg->i);
    add_window(tmp_win);
    save_desktop(arg->i);

    // Remove client from current desktop
    select_desktop(tmp_current_desktop);
    remove_window(current->win); // *

    tile();
    update_current();
}

void kill_client(const Arg *arg)
{
    if (current != NULL) {
        XEvent ke;
        ke.type = ClientMessage;
        ke.xclient.window = current->win;
        ke.xclient.format = 32;
        ke.xclient.message_type = wmatoms[WM_PROTOCOLS];
        ke.xclient.data.l[0] = wmatoms[WM_DELETE_WINDOW];
        ke.xclient.data.l[1] = CurrentTime;
        XSendEvent(dis, current->win, False, NoEventMask, &ke);
        send_kill_signal(current->win);
    }
}

void move_down(const Arg *arg)
{
    if (current == NULL || current->next == NULL || current->win == head->win || current->prev == NULL)
        return;

    Window tmp = current->win;
    current->win = current->next->win;
    current->next->win = tmp;
    //keep the moved window activated
    next_win(NULL);
    tile();
    update_current();
}

void move_up(const Arg *arg)
{
    if (current == NULL || current->prev == head || current->win == head->win)
        return;

    Window tmp = current->win;
    current->win = current->prev->win;
    current->prev->win = tmp;
    prev_win(NULL);
    tile();
    update_current();
}

void next_win(const Arg *arg)
{
    Client *c;

    if (current != NULL && head != NULL) {
        if(current->next == NULL)
            c = head;
        else
            c = current->next;

        current = c;
        update_current();
    }
}

void prev_win(const Arg *arg)
{
    Client *c;

    if (current != NULL && head != NULL) {
        if (current->prev == NULL)
            for (c = head; c->next; c = c->next);
        else
            c = current->prev;

        current = c;
        update_current();
    }
}

void quit(const Arg *arg)
{
    Window root_return, parent;
    Window *children;
    int i;
    unsigned int nchildren;
    XEvent ev;

    /*
     * if a client refuses to terminate itself,
     * we kill every window remaining the brutal way.
     * Since we're stuck in the while(nchildren > 0) { ... } loop
     * we can't exit through the main method.
     * This all happens if MOD+q is pushed a second time.
     */
    if (bool_quit == 1) {
        XUngrabKey(dis, AnyKey, AnyModifier, root);
        XDestroySubwindows(dis, root);
        printf("nuwm: Thanks for using!\n");
        XCloseDisplay(dis);
        die("forced shutdown");
    }

    bool_quit = 1;
    XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
    for (i = 0; i < nchildren; ++i)
        send_kill_signal(children[i]);

    //keep alive until all windows are killed
    while (nchildren > 0) {
        XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
        XNextEvent(dis, &ev);
        if(events[ev.type])
            events[ev.type](&ev);
    }

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    printf("nuwm: Thanks for using!\n");
}

void resize_master(const Arg *arg)
{
    if (!arg || !arg->i)
        return;

    int new_size = master_size + arg->i;
    if (50 < new_size && new_size < sw - 50) {
        master_size = new_size;
        tile();
    }
}

void spawn(const Arg *arg)
{
    if (fork() == 0) {
        if (fork() == 0) {
            if (dis)
                close(ConnectionNumber(dis));

            setsid();
            execvp(((char**)arg->com)[0], (char**)arg->com);
        }
        exit(0);
    }
}

void swap_master(const Arg *arg)
{
    Window tmp;

    if (head != NULL && current != NULL && current != head && mode == 0) {
        tmp = head->win;
        head->win = current->win;
        current->win = tmp;
        current = head;

        tile();
        update_current();
    }
}

void switch_mode(const Arg *arg)
{
    mode = 1 - mode;
    tile();
    update_current();
}

// Implementation of private functions
void add_window(Window w)
{
    Client *c, *t;

    if (!(c = (Client *)calloc(1, sizeof(Client))))
        die("calloc");

    if (head == NULL) {
        c->next = NULL;
        c->prev = NULL;
        c->win = w;
        head = c;
    }
    else {
        for (t = head; t->next; t = t->next);

        c->next = NULL;
        c->prev = t;
        c->win = w;

        t->next = c;
    }

    current = c;
}

void clientmessage(XEvent *e)
{
    Client *c;
    XClientMessageEvent *ev = &e->xclient;
    if ((c = wintoclient(ev->window)) == NULL)
        return;
    if (ev->message_type == netatoms[NET_WM_STATE]
            && ((unsigned)ev->data.l[1] == netatoms[NET_FULLSCREEN] || (unsigned)ev->data.l[2] == netatoms[NET_FULLSCREEN]))
        setfullscreen(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !c->isfull)));
}

void configurerequest(XEvent *e)
{
    // Paste from DWM, thx again \o/
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dis, ev->window, ev->value_mask, &wc);
    XSync(dis, False);
}

void destroynotify(XEvent *e)
{
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    if ((c = wintoclient(ev->window)) != NULL) {
        remove_window(ev->window);
        tile();
        update_current();
    }

}

void die(const char *e)
{
    fprintf(stderr, "nuwm: %s\n", e);
    exit(1);
}

unsigned long getcolor(const char *color)
{
    XColor c;
    Colormap map = DefaultColormap(dis, screen);

    if (!XAllocNamedColor(dis, map, color, &c, &c))
        die("error parsing color");

    return c.pixel;
}

void grabkeys()
{
    int i;
    KeyCode code;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    for (i = 0; i < TABLENGTH(keys); ++i)
        if ((code = XKeysymToKeycode(dis, keys[i].keysym)))
            XGrabKey(dis, code, keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);
}

void keypress(XEvent *e)
{
    int i;
    XKeyEvent ke = e->xkey;
    KeySym keysym = XKeycodeToKeysym(dis, ke.keycode, 0);

    for (i = 0; i < TABLENGTH(keys); ++i)
        if (keys[i].keysym == keysym && keys[i].mod == ke.state)
            keys[i].function(&(keys[i].arg));
}

void maprequest(XEvent *e)
{
    XMapRequestEvent *ev = &e->xmaprequest;

    // For fullscreen mplayer (and maybe some other program)
    Client *c;
    if ((c = wintoclient(ev->window)) != NULL) {
        XMapWindow(dis, ev->window);
        return;
    }

    add_window(ev->window);
    XMapWindow(dis,ev->window);
    tile();
    update_current();
}

Client *nexttiled(Client *c)
{
    for (; c && c->isfloat; c = c->next);
    return c;
}

void remove_window(Window w)
{
    Client *c;

    // CHANGE THIS UGLY CODE
    if ((c = wintoclient(w)) == NULL)
        return;

    XUnmapWindow(dis, c->win);
    if (c->prev == NULL && c->next == NULL) {
        free(head);
        head = NULL;
        current = NULL;
        return;
    }

    if (c->prev == NULL) {
        head = c->next;
        c->next->prev = NULL;
        current = c->next;
    }
    else if (c->next == NULL) {
        c->prev->next = NULL;
        current = c->prev;
    }
    else {
        c->prev->next = c->next;
        c->next->prev = c->prev;
        current = c->prev;
    }

    free(c);
}

void save_desktop(int i)
{
    desktops[i].master_size = master_size;
    desktops[i].mode = mode;
    desktops[i].head = head;
    desktops[i].current = current;
}

void select_desktop(int i)
{
    head = desktops[i].head;
    current = desktops[i].current;
    master_size = desktops[i].master_size;
    mode = desktops[i].mode;
    current_desktop = i;
}

void send_kill_signal(Window w)
{
    XEvent ke;
    ke.type = ClientMessage;
    ke.xclient.window = w;
    ke.xclient.format = 32;
    ke.xclient.message_type = wmatoms[WM_PROTOCOLS];
    ke.xclient.data.l[0] = wmatoms[WM_DELETE_WINDOW];
    ke.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, w, False, NoEventMask, &ke);
}

void setfullscreen(Client *c, int fullscreen)
{
    if (fullscreen != c->isfull)
        XChangeProperty(dis, c->win, netatoms[NET_WM_STATE], XA_ATOM, 32, PropModeReplace,
                (unsigned char*)(fullscreen ? &netatoms[NET_FULLSCREEN] : 0), fullscreen);
    if (fullscreen) {
        c->isfull = c->isfloat = 1;
        XMoveResizeWindow(dis, c->win, 0, 0, sw, sh);
        XSetWindowBorderWidth(dis, c->win, 0);
    } else {
        c->isfull = c->isfloat = 0;
        tile();
        update_current();
    }
}

void setup()
{
    if ((dis = XOpenDisplay(NULL)) == NULL)
        die("cannot open display");

    // Error handling
    xerrorxlib = XSetErrorHandler(xerrorstart);
    XSelectInput(dis, DefaultRootWindow(dis), SubstructureRedirectMask);
    XSync(dis, False);
    XSetErrorHandler(xerror);
    XSync(dis, False);

    // Install a signal
    sigchld(0);

    // Screen and root window
    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);

    // Screen width and height
    sw = XDisplayWidth(dis, screen);
    sh = XDisplayHeight(dis, screen);

    // Colors
    win_focus = getcolor(FOCUS);
    win_unfocus = getcolor(UNFOCUS);

    // thx to monsterwm
    // set up atoms for dialog/notification windows
    wmatoms[WM_PROTOCOLS]     = XInternAtom(dis, "WM_PROTOCOLS", False);
    wmatoms[WM_DELETE_WINDOW] = XInternAtom(dis, "WM_DELETE_WINDOW", False);
    netatoms[NET_SUPPORTED]   = XInternAtom(dis, "_NET_SUPPORTED", False);
    netatoms[NET_WM_STATE]    = XInternAtom(dis, "_NET_WM_STATE", False);
    netatoms[NET_ACTIVE]      = XInternAtom(dis, "_NET_ACTIVE_WINDOW", False);
    netatoms[NET_FULLSCREEN]  = XInternAtom(dis, "_NET_WM_STATE_FULLSCREEN", False);

    // propagate EWMH support
    XChangeProperty(dis, root, netatoms[NET_SUPPORTED], XA_ATOM, 32,
            PropModeReplace, (unsigned char *)netatoms, NET_COUNT);

    // Shortcuts
    grabkeys();

    // Vertical stack
    mode = 0;
    bool_quit = 0;
    head = NULL;
    current = NULL;
    master_size = sw * MASTER_SIZE;

    // Set up all desktop
    for (int i = 0; i < DESKTOPS_SIZE; ++i) {
        desktops[i].master_size = master_size;
        desktops[i].mode = mode;
        desktops[i].head = head;
        desktops[i].current = current;
    }

    // Select first dekstop by default
    const Arg arg = { .i = 1 };
    current_desktop = arg.i;
    change_desktop(&arg);

    // To catch maprequest and destroynotify (if other wm running)
    XSelectInput(dis, root, SubstructureNotifyMask|SubstructureRedirectMask);
}

void sigchld(int unused)
{
    // Again, thx to dwm ;)
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        die("Can't install SIGCHLD handler");

    while (0 < waitpid(-1, NULL, WNOHANG));
}

void start()
{
    XEvent ev;

    while (!bool_quit && !XNextEvent(dis, &ev)) {
        if (ev.type < LASTEvent && events[ev.type] != NULL)
            events[ev.type](&ev);
    }
}

void tile()
{
    Client *c, *tmp_head;
    if ((c = tmp_head = nexttiled(head)) == NULL)
        return;

    int n = 0, x = GAP, y = GAP;
    int w = sw - VACUUM, h = sh - VACUUM;

    // If only one window
    if (nexttiled(tmp_head->next) == NULL)
        XMoveResizeWindow(dis, tmp_head->win, x, y, w, h);
    else {
        switch (mode) {
            case 0:
                // Master window
                w = master_size - VACUUM + GAP / 2;
                h = sh - VACUUM;
                XMoveResizeWindow(dis, tmp_head->win, x, y, w, h);

                // Stack
                for (c = nexttiled(tmp_head->next); c; c = nexttiled(c->next))
                    ++n;
                // x, w and h are constant for windows in the stack
                x = master_size + GAP / 2;
                w = sw - master_size - VACUUM + GAP / 2;
                h = (sh - 2 * n * BORDER - n * GAP - GAP) / n;
                for (c = nexttiled(tmp_head->next); c; c = nexttiled(c->next)) {
                    XMoveResizeWindow(dis, c->win, x, y, w, h);
                    y += h + VACUUM - GAP;
                }
                break;
            case 1:
                for (c = tmp_head; c; c = nexttiled(c->next))
                    XMoveResizeWindow(dis, c->win, x, y, w, h);
                break;
            default:
                break;
        }
    }
}

void update_current()
{
    Client *c;

    for (c = head; c; c = c->next)
        if (current == c) {
            // "Enable" current window
            if (!c->isfull) {
                XSetWindowBorderWidth(dis, c->win, BORDER);
                XSetWindowBorder(dis, c->win, win_focus);
            }
            XSetInputFocus(dis, c->win, RevertToParent, CurrentTime);
            XRaiseWindow(dis, c->win);
        }
        else if (!c->isfull)
            XSetWindowBorder(dis, c->win, win_unfocus);
}

int xerror(Display *dpy, XErrorEvent *ee)
{
    // thx to berrywm
    if (ee->error_code == BadWindow
            || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
            || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
            || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
            || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
            || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
            || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
            || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
            || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable)
            || (ee->request_code == 139 && ee->error_code == BadDrawable)
            || (ee->request_code == 139 && ee->error_code == 143))
        return 0;

    fprintf(stderr, "nuwm: fatal error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrorstart(Display *dpy, XErrorEvent *ee)
{
    die("another window manager is already running");
    return 1;
}

Client *wintoclient(Window w)
{
    Client *c;

    for (c = head; c != NULL; c = c->next)
        if (c->win == w)
            return c;
    return NULL;
}

int main(int argc, char **argv)
{
    setup();

#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec", NULL) == -1)
        die("pledge");
#endif

    start();
    XCloseDisplay(dis);

    return 0;
}
