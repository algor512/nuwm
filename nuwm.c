/*
 *  Copyright (c) 2022-2024, Alexey Gorelov
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
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/XF86keysym.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define TABLENGTH(x) (sizeof(x)/sizeof(*x))

#define LOG(fmt, ...) fprintf(stderr, "%s:%d:%s " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

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

struct Rule {
	const char *class;
	const int isfloat;
	const int isfull;
	const int ignore_unmaps;
};


// Functions visible from config.h (public)
static void change_desktop(const Arg *);
static void client_to_desktop(const Arg *);
static void kill_client(const Arg *);
static void next_win(const Arg *);
static void prev_win(const Arg *);
static void quit(const Arg *);
static void resize_master(const Arg *);
static void smart_hjkl(const Arg *);
static void spawn(const Arg *);
static void swap_master(const Arg *);
static void switch_mode(const Arg *);
static void toggle_float(const Arg *);
static void write_debug(const Arg *);

#include "config.h"

// Types not visible from config.h (public)
typedef struct Client Client;
struct Client {
	Client *next;
	Window win;
	int isfull, isfloat;
	int ignore_unmaps;
	int force_full;

	int x, y, w, h; /* to save position of floating windows */
};

typedef struct Desktop Desktop;
struct Desktop{
	int master_size, mode;
	Client *head, *current;
};

enum { MONOCLE, VSTACK, HSTACK, MODE };
enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };
enum { NET_SUPPORTED, NET_WM_CHECK, NET_FULLSCREEN, NET_WM_STATE, NET_ACTIVE, NET_CLIENT_LIST, NET_COUNT };

// Global variables
static Display *dis;
static int bool_quit;
static int screen, sh, sw;
static Window root, wmcheckwin;

static int current_desktop;
static Desktop desktops[DESKTOPS_SIZE];

static unsigned int win_focus, win_unfocus;
static Atom wmatoms[WM_COUNT], netatoms[NET_COUNT];
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int ignored_modifiers_mask = 0;


// Event handlers
static void buttonpress(XEvent *e);
static void clientmessage(XEvent *);
static void configurerequest(XEvent *);
static void destroynotify(XEvent *);
static void unmapnotify(XEvent *);
static void keypress(XEvent *);
static void maprequest(XEvent *);

static void (*events[LASTEvent])(XEvent *e) = {
	[ClientMessage]    = clientmessage,
	[ConfigureRequest] = configurerequest,
	[DestroyNotify]    = destroynotify,
	[UnmapNotify]      = unmapnotify,
	[KeyPress]         = keypress,
	[MapRequest]       = maprequest,
	[ButtonPress]      = buttonpress,
};

// Private functions
static void copy_client(Client *, int);
static void cleanup();
static void die(const char *);
static unsigned long getcolor(const char *);
static Atom getprop(Window, Atom prop);
static void grabkeys(void);
static void move_resize_floating(Client *, int, int, int, int);
static void remove_client(Client *, int);
static void send_kill_signal(Window);
static void setfullscreen(Client *, int);
static void setup(void);
static void sigchld(int);
static void start(void);
static void tile(void);
static void update_focus(void);
static void write_info(void);
static int xerror(Display *, XErrorEvent *);
static int xerrorstart(Display *, XErrorEvent *);
static int wintoclient(Window, Client **, int *);

// Implementation of public functions
void change_desktop(const Arg *arg)
{
	if (arg->i < 1 || arg->i == current_desktop) return;
	LOG("change desktop: %d -> %d", current_desktop, arg->i);

	for (int i = 1; i < DESKTOPS_SIZE; ++i) {
		if (i == arg->i) continue;
		for (Client *c = desktops[i].head; c != NULL; c = c->next) {
			if (i != current_desktop || desktops[i].current != c) XMoveWindow(dis, c->win, 0, sh + 5);
		}
	}
	if (desktops[current_desktop].current != NULL) XMoveWindow(dis, desktops[current_desktop].current->win, 0, sh + 5);
	current_desktop = arg->i;

	tile();
	write_info();
}

void client_to_desktop(const Arg *arg)
{
	if (arg->i == current_desktop || desktops[current_desktop].current == NULL) return;

	Client *current = desktops[current_desktop].current;
	LOG("client to desktop: %d -> %d, client = %p", current_desktop, arg->i, (void *) current);

	XMoveWindow(dis, current->win, 0, sh + 5);
	copy_client(current, arg->i);
	remove_client(current, current_desktop);

	tile();
	write_info();
}

void kill_client(const Arg *arg)
{
	if (desktops[current_desktop].current != NULL) {
		Window win = desktops[current_desktop].current->win;
		LOG("kill window %lu", win);
		XEvent ke;
		ke.type = ClientMessage;
		ke.xclient.window = win;
		ke.xclient.format = 32;
		ke.xclient.message_type = wmatoms[WM_PROTOCOLS];
		ke.xclient.data.l[0] = wmatoms[WM_DELETE_WINDOW];
		ke.xclient.data.l[1] = CurrentTime;
		XSendEvent(dis, win, False, NoEventMask, &ke);
		send_kill_signal(win);
	}
}

void next_win(const Arg *arg)
{
	Client *current = desktops[current_desktop].current;
	Client *head = desktops[current_desktop].head;
	if (current == NULL || head == NULL || current->isfull) return;

	Client *next = current->next;
	if (next == NULL) next = head;

	desktops[current_desktop].current = next;
	update_focus();
}

void prev_win(const Arg *arg)
{
	Client *current = desktops[current_desktop].current;
	Client *head = desktops[current_desktop].head;
	if (current == NULL || head == NULL || current->isfull) return;

	Client *prev = head;
	for (; prev->next != current && prev->next != NULL; prev = prev->next);
	if (current != head && prev->next == NULL) die("something wrong with client list");
	desktops[current_desktop].current = prev;
	update_focus();
}

void quit(const Arg *arg)
{
	bool_quit = 1;
}

void resize_master(const Arg *arg)
{
	if (!arg || !arg->i) return;

	desktops[current_desktop].master_size = MIN(MAX(desktops[current_desktop].master_size + arg->i, 10), 90);
	tile();
}

void smart_hjkl(const Arg *arg)
{
	Client *current = desktops[current_desktop].current;
	if (current != NULL && current->isfloat) {
		XWindowAttributes wa;
		XGetWindowAttributes(dis, current->win, &wa);

		int x = wa.x, y = wa.y;
		int w = wa.width, h = wa.height;
		switch (arg->i) {
		case XK_h: x -= MOVE_STEP; break;
		case XK_j: y += MOVE_STEP; break;
		case XK_k: y -= MOVE_STEP; break;
		case XK_l: x += MOVE_STEP; break;
		case XK_H: w -= RESIZE_STEP; break;
		case XK_J: h += RESIZE_STEP; break;
		case XK_K: h -= RESIZE_STEP; break;
		case XK_L: w += RESIZE_STEP; break;
		}
		move_resize_floating(current, x, y, w, h);
		return;
	}

	Arg inc_arg = { .i = 10 };
	Arg dec_arg = { .i = -10 };
	switch (desktops[current_desktop].mode) {
	case MONOCLE:
		if (arg->i == XK_l || arg->i == XK_j) {
			next_win(NULL);
		} else if (arg->i == XK_h || arg->i == XK_k) {
			prev_win(NULL);
		}
		break;
	case HSTACK:
		switch (arg->i) {
		case XK_h: prev_win(NULL); break;
		case XK_j: resize_master(&inc_arg); break;
		case XK_k: resize_master(&dec_arg); break;
		case XK_l: next_win(NULL); break;
		}
		break;
	case VSTACK:
		switch (arg->i) {
		case XK_h: resize_master(&dec_arg); break;
		case XK_j: next_win(NULL); break;
		case XK_k: prev_win(NULL); break;
		case XK_l: resize_master(&inc_arg); break;
		}
		break;
	}
}

void spawn(const Arg *arg)
{
	if (fork() == 0) {
		if (fork() == 0) {
			if (dis) close(ConnectionNumber(dis));

			// redirect annoying outputs to /dev/null
			int fdnull = open("/dev/null", O_WRONLY);
			if (fdnull < 0) die("cannot open /dev/null");
			if (dup2(fdnull, STDOUT_FILENO) < 0) die("cannot redirect stdout to /dev/null");
			if (dup2(fdnull, STDERR_FILENO) < 0) die("cannot redirect stderr to /dev/null");
			if (fdnull > 2) close(fdnull);

			setsid();
			execvp(((char**)arg->com)[0], (char**)arg->com);
		}
		exit(0);
	}
}

void swap_master(const Arg *arg)
{
	Client *head = desktops[current_desktop].head;
	Client *current = desktops[current_desktop].current;

	if (head == NULL || current == NULL || current == head) return;

	Client *prev = head;
	for (; prev->next != current && prev->next != NULL; prev = prev->next);
	if (prev->next == NULL) die("something wrong with client list");

	prev->next = head;

	// exchange prev/next pointers
	Client *tmp = current->next;
	current->next = head->next;
	head->next = tmp;
	desktops[current_desktop].head = current;

	tile();
}

void switch_mode(const Arg *arg)
{
	desktops[current_desktop].mode = (desktops[current_desktop].mode + 1) % MODE;
	tile();
	write_info();
}

void toggle_float(const Arg *arg)
{
	Client *current = desktops[current_desktop].current;
	if (current == NULL) return;

	current->isfloat = !current->isfloat;
	if (current->isfloat) {
		move_resize_floating(current, sw - 480 - 2*BORDER, sh - 360 - 2*BORDER, 480, 360);
		XSetWindowBorderWidth(dis, current->win, BORDER);
	}
	tile();
}

void write_debug(const Arg *unused)
{
	for (int i = 1; i < DESKTOPS_SIZE; ++i) {
		LOG("desktop = %d", i);
		for (Client *c = desktops[i].head; c != NULL; c = c->next) {
			int is_cur = (c == desktops[i].current);
			unsigned long next = c->next != NULL ? c->next->win : 0;
			LOG("\twindow %lu: current = %d, float = %d, next = %lu", c->win, is_cur, c->isfloat, next);
		}
	}
}

// Implementation of event handlers
void buttonpress(XEvent *e)
{
	Client *c = NULL;
	int desktop;
	if (wintoclient(e->xbutton.window, &c, &desktop) && desktop == current_desktop
	    && c != desktops[current_desktop].current) {
		desktops[current_desktop].current = c;
		update_focus();
	}
}

void clientmessage(XEvent *e)
{
	XClientMessageEvent *ev = &e->xclient;
	Client *c = NULL;
	int desktop;

	if (!wintoclient(ev->window, &c, &desktop)) return;
	if (ev->message_type == netatoms[NET_WM_STATE]
	    && ((unsigned)ev->data.l[1] == netatoms[NET_FULLSCREEN] || (unsigned)ev->data.l[2] == netatoms[NET_FULLSCREEN])) {
		setfullscreen(c, (ev->data.l[0] == 1 || (ev->data.l[0] == 2 && !c->isfull)));
		if (c->force_full) {
			setfullscreen(c, 1);
		}
	}
}

void configurerequest(XEvent *e)
{
	// Paste from DWM, thx again \o/
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	LOG("configure request, win=%lu", ev->window);
	XWindowChanges wc = {
		.x = ev->x,
		.y = ev->y,
		.width = ev->width,
		.height = ev->height,
		.border_width = ev->border_width,
		.sibling = ev->above,
		.stack_mode = ev->detail,
	};
	XConfigureWindow(dis, ev->window, ev->value_mask, &wc);

	Client *c;
	if (wintoclient(ev->window, &c, NULL) && c->isfloat && !c->isfull) {
		move_resize_floating(c, ev->x, ev->y, ev->width, ev->height);
	}
	XSync(dis, False);
	tile();
}

void destroynotify(XEvent *e)
{
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	Client *c = NULL;
	int desktop;

	LOG("destroynotify win=%lu", ev->window);
	if (wintoclient(ev->window, &c, &desktop)) {
		remove_client(c, desktop);
	}
	tile();
}

void unmapnotify(XEvent *e)
{
	XUnmapEvent *ev = &e->xunmap;
	Client *c;
	int desktop;

	LOG("unmapnotify win=%lu", ev->window);
	if (!wintoclient(ev->window, &c, &desktop)) return;
	if (c->ignore_unmaps) return;
	remove_client(c, desktop);
	tile();
}

void keypress(XEvent *e)
{
	int i;
	XKeyEvent ke = e->xkey;
	KeySym keysym = XKeycodeToKeysym(dis, ke.keycode, 0);
	int state = ke.state & ~ignored_modifiers_mask;

	for (i = 0; i < TABLENGTH(keys); ++i) {
		if (keys[i].keysym == keysym && keys[i].mod == state) {
			keys[i].function(&(keys[i].arg));
		}
	}
}

void maprequest(XEvent *e)
{
	XMapRequestEvent *ev = &e->xmaprequest;
	LOG("maprequest win=%lu", ev->window);

	XSetWindowBorderWidth(dis, ev->window, BORDER);
	XMapWindow(dis, ev->window);

	XWindowAttributes attrs = {0};
	XGetWindowAttributes(dis, ev->window, &attrs);

	if (wintoclient(ev->window, NULL, NULL) || attrs.override_redirect) {
		return;
	}

	// FIXME: realise getatomprop(Client *c, Atom prop) and
	// if getatomprop(c, netatom[NetWMState]) == NetWMFullscreen we set it to be fullscreen
	Client c = { .win = ev->window };
	XClassHint cls = {0, 0};
	if (XGetClassHint(dis, c.win, &cls)) {
		for (int i = 0; i < TABLENGTH(rules); i++) {
			if (strstr(cls.res_class, rules[i].class) || strstr(cls.res_name, rules[i].class)) {
				c.isfloat = rules[i].isfloat;
				c.isfull = rules[i].isfull;
				c.force_full = rules[i].isfull;
				c.ignore_unmaps = rules[i].ignore_unmaps;
				break;
			}
		}
	}

	if (getprop(c.win, netatoms[NET_WM_STATE]) == netatoms[NET_FULLSCREEN]) {
		c.isfull = 1;
	}

	if (cls.res_class) XFree(cls.res_class);
	if (cls.res_name) XFree(cls.res_name);
	if (c.isfull) {
		setfullscreen(&c, 1);
	} else if (c.isfloat) {
		c.x = attrs.x;
		c.y = attrs.y;
		c.w = attrs.width;
		c.h = attrs.height;
	}

	copy_client(&c, current_desktop);

	tile();
	write_info();
}

// Implementation of private functions
void copy_client(Client *c, int desktop)
{
	Client *new = calloc(sizeof(*c), 1);
	*new = *c;
	LOG("copy client client=%p -> desktop=%d, new client=%p", (void *) c, desktop, (void *) new);

	Client *current = desktops[desktop].current;
	if (current == NULL) {
		new->next = desktops[desktop].head;
		desktops[desktop].head = new;
	} else if (current == desktops[desktop].head) {
		new->next = current;
		desktops[desktop].head = new;
	} else {
		Client *prev = desktops[desktop].head;
		for (; prev->next != current && prev->next != NULL; prev = prev->next);
		if (prev->next == NULL) die("something wrong with clients list");
		prev->next = new;
		new->next = current;
	}

	desktops[desktop].current = new;
}

void cleanup()
{
	Window root_return, parent;
	Window *children;
	unsigned int nchildren;

	XDestroyWindow(dis, wmcheckwin);
	XUngrabKey(dis, AnyKey, AnyModifier, root);
	XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
	for (int i = 0; i < nchildren; ++i) send_kill_signal(children[i]);
	if (children) XFree(children);
	XSync(dis, False);

	XEvent ev;
	int attempts = 5;
	while (nchildren > 0 && attempts > 0) {
		XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);

		if (attempts == 2) {
			for (int i = 0; i < nchildren; ++i) XDestroyWindow(dis, children[i]);
			XSync(dis, False);
		}

		if (children) XFree(children);
		if (XPending(dis) > 0) {
			XNextEvent(dis, &ev);
		}
		sleep(1);
		--attempts;
	}

	XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
	for (int i = 0; i < nchildren; ++i) XKillClient(dis, children[i]);
	XFree(children);
	XCloseDisplay(dis);

	for (int i = 0; i < DESKTOPS_SIZE; ++i) {
		Client *c = desktops[i].head;
		Client *next = c->next;
		while (c != NULL) {
			free(c);
			c = next;
			next = c->next;
		}
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

	if (!XAllocNamedColor(dis, map, color, &c, &c)) die("error parsing color");

	return c.pixel;
}

Atom getprop(Window win, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dis, win, prop, 0L, sizeof(atom), False, XA_ATOM,
		                   &da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

void grabkeys()
{
	KeyCode code;

	unsigned int ignored_modifiers_num = TABLENGTH(ignored_modifiers);
	for (int i = 0; i < (1 << ignored_modifiers_num); ++i) {
		unsigned int mask = 0;
		for (int j = 0; j < ignored_modifiers_num; ++j) {
			if (i & (1 << j)) {
				mask |= ignored_modifiers[j];
			}
		}

		for (int k = 0; k < TABLENGTH(keys); ++k) {
			if ((code = XKeysymToKeycode(dis, keys[k].keysym))) {
				XGrabKey(dis, code, keys[k].mod | mask, root, True, GrabModeAsync, GrabModeAsync);
			}
		}
	}

	ignored_modifiers_mask = 0;
	for (int i = 0; i < ignored_modifiers_num; ++i) {
		ignored_modifiers_mask |= ignored_modifiers[i];
	}
}

void move_resize_floating(Client *c, int x, int y, int w, int h)
{
	w = MAX(10, MIN(w, sw - 2*BORDER));
	h = MAX(10, MIN(h, sh - 2*BORDER - BAR));
	x = MAX(0, x);
	y = MAX(BAR, y);

	int corner_x = x + w + 2*BORDER;
	int corner_y = y + h + 2*BORDER;

	if (corner_x > sw) x -= (corner_x - sw);
	if (corner_y > sh) y -= (corner_y - sh);

	c->x = x;
	c->y = y;
	c->w = w;
	c->h = h;
	XMoveResizeWindow(dis, c->win, x, y, w, h);
}

void remove_client(Client *c, int desktop)
{
	LOG("remove client=%p win=%lu desktop=%d", (void *) c, c->win, desktop);
	if (c == NULL) return;

	Client *head = desktops[desktop].head;
	if (head == c) {
		desktops[desktop].head = head->next;
	} else {
		Client *prev = desktops[desktop].head;
		for (; prev->next != c && prev->next != NULL; prev = prev->next);
		if (prev->next == NULL) die("something wrong with clients list");
		prev->next = c->next;
	}
	if (desktops[desktop].current == c) {
		desktops[desktop].current = c->next;
		if (desktops[desktop].current == NULL) desktops[desktop].current = desktops[desktop].head;
	}

	free(c);
	write_info();
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
		c->x = c->y = 0;
		c->w = sw;
		c->h = sh;
		XMoveResizeWindow(dis, c->win, 0, 0, sw, sh);
		XSetWindowBorderWidth(dis, c->win, 0);
	} else {
		c->isfull = c->isfloat = 0;
		tile();
	}
}

void setup()
{
	LOG("setup started");
	if ((dis = XOpenDisplay(NULL)) == NULL) die("cannot open display");

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
	netatoms[NET_WM_CHECK]    = XInternAtom(dis, "_NET_SUPPORTING_WM_CHECK", False);
	netatoms[NET_WM_STATE]    = XInternAtom(dis, "_NET_WM_STATE", False);
	netatoms[NET_ACTIVE]      = XInternAtom(dis, "_NET_ACTIVE_WINDOW", False);
	netatoms[NET_CLIENT_LIST] = XInternAtom(dis, "_NET_CLIENT_LIST", False);
	netatoms[NET_FULLSCREEN]  = XInternAtom(dis, "_NET_WM_STATE_FULLSCREEN", False);

	// propagate EWMH support
	XChangeProperty(dis, root, netatoms[NET_SUPPORTED], XA_ATOM, 32,
	                PropModeReplace, (unsigned char *)netatoms, NET_COUNT);

	// create supporting window (for _NET_SUPPORTING_WM_CHECK)
	wmcheckwin = XCreateSimpleWindow(dis, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dis, wmcheckwin, netatoms[NET_WM_CHECK], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dis, root, netatoms[NET_WM_CHECK], XA_WINDOW, 32,
	                PropModeReplace, (unsigned char *) &wmcheckwin, 1);

	LOG("grab keys");
	// Shortcuts
	grabkeys();

	// Set up all desktop
	for (int i = 0; i < DESKTOPS_SIZE; ++i) {
		desktops[i].master_size = MASTER_SIZE;
		desktops[i].mode = MONOCLE;
		desktops[i].head = NULL;
		desktops[i].current = NULL;
	}

	// Select first dekstop by default
	const Arg arg = { .i = 1 };
	change_desktop(&arg);

	// To catch maprequest and destroynotify (if other wm running)
	XSelectInput(dis, root, SubstructureNotifyMask|SubstructureRedirectMask|ButtonPressMask);
}

void sigchld(int unused)
{
	// Again, thx to dwm ;)
	if (signal(SIGCHLD, sigchld) == SIG_ERR) die("can't install SIGCHLD handler");

	while (0 < waitpid(-1, NULL, WNOHANG));
}

void start()
{
	XEvent ev;

	write_info();
	while (!bool_quit && !XNextEvent(dis, &ev)) {
		LOG("event loop iteration");
		if (ev.type < LASTEvent && events[ev.type] != NULL) {
			events[ev.type](&ev);
		}
	}
}

void tile()
{
	Client *master = NULL;
	Client *current = desktops[current_desktop].current;
	int stack_size = 0;

	for (Client *c = desktops[current_desktop].head; c != NULL; c = c->next) {
		if (c->isfloat) {
			if (c->isfull) {
				XSetWindowBorderWidth(dis, c->win, 0);
			} else {
				XSetWindowBorderWidth(dis, c->win, BORDER);
			}
			XMoveResizeWindow(dis, c->win, c->x, c->y, c->w, c->h);
		} else {
			if (master == NULL) {
				master = c;
			} else {
				++stack_size;
			}
		}
	}
	if (master == NULL) {
		update_focus();
		return;
	}

	int w, h, x, y, ms;
	if (stack_size == 0) {
		XSetWindowBorderWidth(dis, master->win, 0);
		XMoveResizeWindow(dis, master->win, 0, BAR, sw, sh - BAR);
	} else {
		switch (desktops[current_desktop].mode) {
		case VSTACK:
			ms = desktops[current_desktop].master_size * (sw - 2*BORDER - GAP) / 100;

			// Master window
			w = ms - 2*BORDER;
			h = sh - 2*BORDER - BAR;
			XSetWindowBorderWidth(dis, master->win, BORDER);
			XMoveResizeWindow(dis, master->win, 0, BAR, w, h);

			x = ms + 2*BORDER + GAP;
			y = BAR;
			w = sw - ms - 4*BORDER - GAP;
			h = (sh - 2*stack_size*BORDER - (stack_size - 1)*GAP - BAR) / stack_size;
			for (Client *c = master->next; c != NULL; c = c->next) {
				if (c->isfloat) continue;
				XSetWindowBorderWidth(dis, c->win, BORDER);
				XMoveResizeWindow(dis, c->win, x, y, w, h);
				y += h + 2*BORDER + GAP;
			}
			break;
		case HSTACK:
			ms = desktops[current_desktop].master_size * (sh - BAR - 2*BORDER - GAP) / 100;

			// Master window
			w = sw - 2*BORDER;
			h = ms - 2*BORDER;
			XSetWindowBorderWidth(dis, master->win, BORDER);
			XMoveResizeWindow(dis, master->win, 0, BAR, w, h);

			x = 0;
			h = sh - ms - 4*BORDER - GAP - BAR;
			w = (sw - 2*stack_size*BORDER - (stack_size - 1)*GAP) / stack_size;
			for (Client *c = master->next; c != NULL; c = c->next) {
				if (c->isfloat) continue;
				XSetWindowBorderWidth(dis, c->win, BORDER);
				XMoveResizeWindow(dis, c->win, x, GAP + BAR + ms, w, h);
				x += w + 2*BORDER + GAP;
			}

			break;
		case MONOCLE:
			if (current != NULL && !current->isfloat) {
				XSetWindowBorderWidth(dis, current->win, 0);
				XMoveResizeWindow(dis, current->win, 0, BAR, sw, sh - BAR);
			}
			for (Client *c = master; c != NULL; c = c->next) {
				if (c->isfloat) continue;
				XSetWindowBorderWidth(dis, c->win, 0);
				XMoveResizeWindow(dis, c->win, 0, BAR, sw, sh - BAR);
			}
			break;
		default:
			break;
		}
	}
	update_focus();
}

void update_focus()
{
	Client *current = desktops[current_desktop].current;

	/* grab/ungrab buttons, set borders */
	for (Client *c = desktops[current_desktop].head; c != NULL; c = c->next) {
		XUngrabButton(dis, AnyButton, AnyModifier, c->win);

		if (current == c) {
			XSetWindowBorder(dis, c->win, win_focus);
			XSetInputFocus(dis, c->win, RevertToParent, CurrentTime);
			XChangeProperty(dis, root, netatoms[NET_ACTIVE], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &(c->win), 1);
		} else {
			XSetWindowBorder(dis, c->win, win_unfocus);
			XGrabButton(dis, AnyButton, AnyModifier, c->win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
		}
	}

	/* reorder windows */
	if (current != NULL) XRaiseWindow(dis, current->win);
	for (Client *c = desktops[current_desktop].head; c != NULL; c = c->next) {
		if (c->isfloat || c->isfull) XRaiseWindow(dis, c->win);
	}
	if (current != NULL && current->isfull) XRaiseWindow(dis, current->win);
}

void write_info(void)
{
	char status[512] = {0};
	int length = 0;

	XDeleteProperty(dis, root, netatoms[NET_CLIENT_LIST]);
	for (int i = 1; i < DESKTOPS_SIZE; ++i) {
		int nclients = 0;
		for (Client *c = desktops[i].head; c != NULL; c = c->next) {
			++nclients;
			XChangeProperty(dis, root, netatoms[NET_CLIENT_LIST],
			                XA_WINDOW, 32, PropModeAppend, (unsigned char *) &(c->win), 1);
		}
		length += snprintf(status + length, 512 - length, "%c:%d:%d:%d ",
		                   i == current_desktop ? '*' : '-', i, desktops[i].mode, nclients);
	}
	XStoreName(dis, root, status);
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
	    || (ee->request_code == 139 && ee->error_code == 143)) return 0;

	fprintf(stderr, "nuwm: fatal error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("another window manager is already running");
	return 1;
}

int wintoclient(Window w, Client **c, int *desktop)
{
	for (int i = 1; i < DESKTOPS_SIZE; ++i) {
		for (Client *cur = desktops[i].head; cur != NULL; cur = cur->next) {
			if (cur->win == w) {
				if (c) *c = cur;
				if (desktop) *desktop = i;
				return 1;
			}
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	setup();

#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif

	start();
	cleanup();

	return 0;
}
