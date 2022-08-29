/* vifm
 * Copyright (C) 2015 xaizek.
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

#ifndef VIFM__COMPAT__CURSES_H__
#define VIFM__COMPAT__CURSES_H__

#include <curses.h>

#include <wchar.h> /* wchar_t wint_t */

/* Moves curses functional keys (KEY_*) to Unicode Private Use Area starting
 * from U+E000, which shouldn't clash with anything except for maybe iconic
 * fonts, but by convention they start from U+F000, so even that shouldn't
 * happen.  One value is reserved for our custom Ctrl-Space key.
 * (Solution that guarantees no clashes ever is possible, but very inconvenient
 * for implementation as it needs more than just wchar_t.) */
#define K(x) ((wchar_t)((wint_t)0xe000 + 1 + (x)))

/* OpenBSD used to have perverted ncursesw library.  It had stubs with infinite
 * loops instead of real wide functions.  As there is only a couple of wide
 * functions in use, they can be emulated on systems like that. */

#ifndef BROKEN_WIDE_CURSES

#define compat_wget_wch wget_wch
#define compat_waddwstr waddwstr
#define compat_mvwaddwstr mvwaddwstr

#else

int compat_wget_wch(WINDOW *w, wint_t *wc);

int compat_waddwstr(WINDOW *w, const wchar_t wstr[]);

int compat_mvwaddwstr(WINDOW *w, int y, int x, const wchar_t wstr[]);

#endif

#endif /* VIFM__COMPAT__CURSES_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
