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

#ifdef _WIN32
#include <windows.h>
#endif

#if !defined(_WIN32) && defined(HAVE_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include <stdlib.h> /* atol() */

#include "utils/env.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/string_array.h"

#include "term_title.h"

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
		*disp = XOpenDisplay(NULL);
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

	old_handler = XSetErrorHandler(x_error_check);
	if(!XGetWMName(disp, win, &text_prop))
	{
		(void)XSetErrorHandler(old_handler);
		return;
	}

	(void)XSetErrorHandler(old_handler);
	snprintf(buf, buf_len, "%s", text_prop.value);
	XFree((void *)text_prop.value);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
