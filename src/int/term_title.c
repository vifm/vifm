/* vifm
 * Copyright (C) 2011 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "term_title.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <term.h>
#endif

#if !defined(_WIN32) && defined(HAVE_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef DYN_X11
#include <dlfcn.h> /* dlopen() dlsym() dlclose() */
#endif
#endif

/* Headers above define these macros which messes up config.h. */
#ifdef lines
# undef lines
#endif
#ifdef columns
# undef columns
#endif

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* stdout fflush() */
#include <stdlib.h> /* atol() free() */
#include <string.h> /* strcmp() */

#include "../cfg/config.h"
#include "../utils/env.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../vifm.h"

/* Kind of title we're working with. */
typedef enum
{
	TK_ABSENT,  /* No title support. */
	TK_REGULAR, /* Normal title that can be saved and restored. */
	TK_SCREEN,  /* GNU screen compatible. */
}
TitleKind;

enum
{
	/* Limit on the length of title message (variable part of the title).  It
	 * would be smaller if it wasn't specified for a UTF-8 string. */
	MAX_TITLE_MSG_LEN = 700,
};

static void ensure_initialized(void);
static TitleKind query_title_kind(void);
TSTATIC TitleKind title_kind_for_termenv(const char term[]);
static void apply_term_guess(TitleKind kind);
static void save_term_title(void);
static void restore_term_title(void);
#if !defined(_WIN32) && defined(HAVE_X11)
static int get_x11_disp_and_win(Display **disp, Window *win);
static void get_x11_window_title(Display *disp, Window win, char *buf,
		size_t buf_len);
static int x_error_check(Display *dpy, XErrorEvent *error_event);
#endif
static void set_terminal_title(const char path[]);

#if !defined(_WIN32) && defined(HAVE_X11)
#ifdef DYN_X11

static void try_dynload_xlib(void);
static int dynload_xlib(void);
static void set_xlib_stubs(void);
static void unload_xlib(void);

static Display *XOpenDisplayStub(_Xconst char name[]);
static Status XGetWMNameStub(Display *display, Window w,
	XTextProperty* text_prop_return);
static XErrorHandler XSetErrorHandlerStub(XErrorHandler handler);
static int XFreeStub(void* data);

/* Handle of dynamically loaded xlib. */
static void *xlib_handle;

/* Indirection level for functions of the xlib. */
static typeof(XOpenDisplay) *XOpenDisplayWrapper;
static typeof(XGetWMName) *XGetWMNameWrapper;
static typeof(XSetErrorHandler) *XSetErrorHandlerWrapper;
static typeof(XFree) *XFreeWrapper;

#else

/* Indirection level for functions of the xlib. */
static typeof(XOpenDisplay) *XOpenDisplayWrapper = &XOpenDisplay;
static typeof(XGetWMName) *XGetWMNameWrapper = &XGetWMName;
static typeof(XSetErrorHandler) *XSetErrorHandlerWrapper = &XSetErrorHandler;
static typeof(XFree) *XFreeWrapper = &XFree;

#endif
#endif

/* Holds state of the unit. */
static struct
{
	int initialized; /* Whether state has already been initialized. */
	TitleKind kind;  /* Type of title output. */

#ifndef _WIN32
	char *tsl; /* Sequence that starts title. */
	char *fsl; /* Sequence that ends title. */
#endif

	/* Original title or an empty string if failed to determine. */
#ifndef _WIN32
	char title[512];
#else
	wchar_t title[512];
#endif
}
title_state;

int
term_title_restorable(void)
{
	ensure_initialized();

	return (title_state.title[0] != '\0');
}

void
term_title_update(const char title_part[])
{
	ensure_initialized();

	if(title_state.kind == TK_ABSENT)
	{
		return;
	}

	if(title_part == NULL)
	{
		restore_term_title();
	}
	else if(cfg.set_title)
	{
		char *escaped = escape_unreadable(title_part);
		set_terminal_title(escaped);
		free(escaped);
	}
}

/* Makes sure that global title_state structure is initialized with valid
 * values. */
static void
ensure_initialized(void)
{
	if(title_state.initialized)
	{
		return;
	}

	title_state.kind = query_title_kind();
	if(title_state.kind == TK_ABSENT)
	{
		title_state.kind = title_kind_for_termenv(env_get_def("TERM", ""));
		apply_term_guess(title_state.kind);
	}

	if(title_state.kind == TK_REGULAR)
	{
		save_term_title();
	}
	title_state.initialized = 1;
}

/* Determines kind of title in a reliable, but not always available way.
 * Returns the kind. */
static TitleKind
query_title_kind(void)
{
#ifndef _WIN32
	int need_cleanup = 0;
	if(cur_term == NULL && !vifm_testing())
	{
		(void)setupterm((char *)env_get("TERM"), 1, (int *)0);
		need_cleanup = 1;
	}

	int success = 1;

	const char *tsl = tigetstr("tsl");
	if(tsl == NULL || tsl == (char *)-1)
	{
		success = 0;
	}
	else
	{
		update_string(&title_state.tsl, tsl);
	}

	const char *fsl = tigetstr("fsl");
	if(tsl == NULL || tsl == (char *)-1)
	{
		success = 0;
	}
	else
	{
		update_string(&title_state.fsl, fsl);
	}

	if(need_cleanup)
	{
		del_curterm(cur_term);
	}

	if(!success)
	{
		update_string(&title_state.tsl, NULL);
		update_string(&title_state.fsl, NULL);
		return TK_ABSENT;
	}
#endif

	return TK_REGULAR;
}

/* Guesses how we can alter terminal emulator title based on the value of $TERM.
 * Returns kind of terminal that was found. */
TSTATIC TitleKind
title_kind_for_termenv(const char term[])
{
#ifdef _WIN32
	return TK_REGULAR;
#else
	if(strcmp(term, "xterm") == 0 || starts_with_lit(term, "xterm-") ||
			strcmp(term, "rxvt") == 0 || starts_with_lit(term, "rxvt-") ||
			strcmp(term, "aterm") == 0 || strcmp(term, "Eterm") == 0 ||
			strcmp(term, "foot") == 0 || starts_with_lit(term, "foot-"))
	{
		return TK_REGULAR;
	}

	if(strcmp(term, "screen") == 0 || starts_with_lit(term, "screen-"))
	{
		return TK_SCREEN;
	}

	return TK_ABSENT;
#endif
}

/* Adjusts title_state according to guessed terminal type. */
static void
apply_term_guess(TitleKind kind)
{
#ifndef _WIN32
	switch(kind)
	{
		case TK_ABSENT:
			break;
		case TK_REGULAR:
			update_string(&title_state.tsl, "\033]2;");
			update_string(&title_state.fsl, "\007");
			break;
		case TK_SCREEN:
			update_string(&title_state.tsl, "\033k");
			update_string(&title_state.fsl, "\033\134");
			break;
	}
#endif
}

/* Stores current terminal title into title_state.title. */
static void
save_term_title(void)
{
#ifdef _WIN32
	GetConsoleTitleW(title_state.title, ARRAY_LEN(title_state.title));
#else
#ifdef HAVE_X11
	Display *x11_display = 0;
	Window x11_window = 0;

#ifdef DYN_X11
	try_dynload_xlib();
#endif

	/* Use X to determine current window title. */
	if(get_x11_disp_and_win(&x11_display, &x11_window))
		get_x11_window_title(x11_display, x11_window, title_state.title,
				sizeof(title_state.title));
#endif
#endif
}

/* Restores terminal title from title_state.title. */
static void
restore_term_title(void)
{
#ifdef _WIN32
	if(title_state.title[0] != L'\0')
		SetConsoleTitleW(title_state.title);
#else
	if(title_state.title[0] != '\0')
	{
		/* Apparently tsl can take column in some cases. */
		putp(tgoto(title_state.tsl, 0, 0));
		putp(title_state.title);
		putp(title_state.fsl);
		fflush(stdout);
	}

#if defined(HAVE_X11) && defined(DYN_X11)
	unload_xlib();
#endif
#endif
}

#if !defined(_WIN32) && defined(HAVE_X11)
/* Loads X specific variables. */
static int
get_x11_disp_and_win(Display **disp, Window *win)
{
	const char *winid;

	if(*win == 0 && (winid = env_get("WINDOWID")) != NULL)
		*win = (Window)atol(winid);

	if(*win != 0 && *disp == NULL)
		*disp = XOpenDisplayWrapper(NULL);
	if(*win == 0 || *disp == NULL)
		return 0;

	return 1;
}

/* Gets terminal title using X. */
static void
get_x11_window_title(Display *disp, Window win, char *buf, size_t buf_len)
{
	int (*old_handler)();
	XTextProperty text_prop;

	old_handler = XSetErrorHandlerWrapper(x_error_check);
	if(!XGetWMNameWrapper(disp, win, &text_prop))
	{
		(void)XSetErrorHandlerWrapper(old_handler);
		return;
	}

	(void)XSetErrorHandlerWrapper(old_handler);
	copy_str(buf, buf_len, (char *)text_prop.value);
	XFreeWrapper(text_prop.value);
}

/* Callback function for reporting X errors, should return 0 on success. */
static int
x_error_check(Display *dpy, XErrorEvent *error_event)
{
	return 0;
}
#endif

/* Does all the real work on setting terminal title. */
static void
set_terminal_title(const char path[])
{
#ifdef _WIN32
	wchar_t *utf16;
	char title[2048];
	snprintf(title, sizeof(title), "%s - VIFM", path);

	utf16 = utf8_to_utf16(title);
	SetConsoleTitleW(utf16);
	free(utf16);
#else
	char *const fmt = (title_state.kind == TK_REGULAR)
	                ? "\033]2;%.*s - VIFM\007"
	                : "\033k%.*s - VIFM\033\134";
	char *const title = format_str(fmt, MAX_TITLE_MSG_LEN, path);

	putp(title);
	fflush(stdout);

	free(title);
#endif
}

#if !defined(_WIN32) && defined(HAVE_X11) && defined(DYN_X11)

/* Tries to load xlib dynamically, falls back to stubs that do nothing in case
 * of failure. */
static void
try_dynload_xlib(void)
{
	const int xlib_dynloaded_successfully = dynload_xlib();
	if(!xlib_dynloaded_successfully)
	{
		set_xlib_stubs();
	}
}

/* Loads libX11 library.  Returns non-zero when library is loaded
 * successfully. */
static int
dynload_xlib(void)
{
	xlib_handle = dlopen("libX11.so.6", RTLD_NOW);
	if(xlib_handle == NULL)
	{
		return 0;
	}
	XOpenDisplayWrapper = dlsym(xlib_handle, "XOpenDisplay");
	XGetWMNameWrapper = dlsym(xlib_handle, "XGetWMName");
	XSetErrorHandlerWrapper = dlsym(xlib_handle, "XSetErrorHandler");
	XFreeWrapper = dlsym(xlib_handle, "XFree");
	return XOpenDisplayWrapper != NULL && XGetWMNameWrapper != NULL &&
		XSetErrorHandlerWrapper != NULL && XFreeWrapper != NULL;
}

/* Sets xlib function pointers to stubs. */
static void
set_xlib_stubs(void)
{
	XOpenDisplayWrapper = &XOpenDisplayStub;
	XGetWMNameWrapper = &XGetWMNameStub;
	XSetErrorHandlerWrapper = &XSetErrorHandlerStub;
	XFreeWrapper = &XFreeStub;
}

/* Unloads previously loaded libX11 library. */
static void
unload_xlib(void)
{
	if(xlib_handle != NULL)
	{
		(void)dlclose(xlib_handle);
		xlib_handle = NULL;
	}
}

/* XOpenDisplay() function stub, which does nothing. */
static Display *
XOpenDisplayStub(_Xconst char name[])
{
	return NULL;
}

/* XGetWMName() function stub, which does nothing. */
static Status
XGetWMNameStub(Display *display, Window w, XTextProperty* text_prop_return)
{
	return 0;
}

/* XSetErrorHandler() function stub, which does nothing. */
static XErrorHandler
XSetErrorHandlerStub(XErrorHandler handler)
{
	return NULL;
}

/* XFree() function stub, which does nothing. */
static int
XFreeStub(void* data)
{
	return 0;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
