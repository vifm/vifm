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
#endif

#if !defined(_WIN32) && defined(HAVE_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef DYN_X11
#include <dlfcn.h> /* dlopen() dlsym() dlclose() */
#endif
#endif

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* atol() */

#include "utils/env.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/string_array.h"

static struct
{
	int initialized;
	int supported;
#ifndef _WIN32
	char title[512];
#else
	wchar_t title[512];
#endif
}title_state;

static void check_title_supported();
static void save_term_title();
static void restore_term_title();
#if !defined(_WIN32) && defined(HAVE_X11)
static int get_x11_disp_and_win(Display **disp, Window *win);
static void get_x11_window_title(Display *disp, Window win, char *buf,
		size_t buf_len);
static int x_error_check(Display *dpy, XErrorEvent *error_event);
#endif
static void set_terminal_title(const char *path);

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

void
set_term_title(const char *title_part)
{
	if(!title_state.initialized)
	{
		check_title_supported();
		if(title_state.supported)
			save_term_title();
		title_state.initialized = 1;
	}
	if(!title_state.supported)
		return;

	if(title_part == NULL)
	{
		restore_term_title();
	}
	else
	{
		set_terminal_title(title_part);
	}
}

/* checks if we can alter terminal emulator title and writes result to
 * title_state.supported */
static void
check_title_supported()
{
#ifdef _WIN32
	title_state.supported = 1;
#else
	/* this list was taken from ranger's sources */
	static char *TERMINALS_WITH_TITLE[] = {
		"xterm", "xterm-256color", "rxvt", "rxvt-256color", "rxvt-unicode",
		"aterm", "Eterm", "screen", "screen-256color"
	};

	title_state.supported = is_in_string_array(TERMINALS_WITH_TITLE,
			ARRAY_LEN(TERMINALS_WITH_TITLE), env_get("TERM"));
#endif
}

/* stores current terminal title into title_state.title */
static void
save_term_title()
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

	/* use X to determine current window title */
	if(get_x11_disp_and_win(&x11_display, &x11_window))
		get_x11_window_title(x11_display, x11_window, title_state.title,
				sizeof(title_state.title));
#endif
#endif
}

/* restores terminal title from title_state.title */
static void
restore_term_title()
{
#ifdef _WIN32
	if(title_state.title[0] != L'\0')
		SetConsoleTitleW(title_state.title);
#else
	if(title_state.title[0] != '\0')
		printf("\033]2;%s\007", title_state.title);

#if defined(HAVE_X11) && defined(DYN_X11)
	unload_xlib();
#endif
#endif
}

#if !defined(_WIN32) && defined(HAVE_X11)
/* loads X specific variables */
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

/* gets terminal title using X */
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
	snprintf(buf, buf_len, "%s", text_prop.value);
	XFreeWrapper((void *)text_prop.value);
}

/* callback function for reporting X errors, should return 0 on success */
static int
x_error_check(Display *dpy, XErrorEvent *error_event)
{
	return 0;
}
#endif

/* does real job on setting terminal title */
static void
set_terminal_title(const char *path)
{
#ifdef _WIN32
	wchar_t buf[2048];
	my_swprintf(buf, ARRAY_LEN(buf), L"%" WPRINTF_MBSTR L" - VIFM", path);
	SetConsoleTitleW(buf);
#else
	printf("\033]2;%s - VIFM\007", path);
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
	xlib_handle = dlopen("libX11.so", RTLD_NOW);
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
	(void)dlclose(xlib_handle);
	xlib_handle = NULL;
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
/* vim: set cinoptions+=t0 : */
