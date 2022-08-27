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

#ifdef BROKEN_WIDE_CURSES

#include "curses.h"

#include <curses.h>

#include <stdlib.h> /* free() */
#include <wchar.h> /* wchar_t */

#include "../utils/str.h"

int
compat_wget_wch(WINDOW *w, wint_t *wc)
{
	*wc = wgetch(w);
	return ((char)*wc == ERR) ? ERR : (*wc >= KEY_MIN ? KEY_CODE_YES : OK);
}

int
compat_waddwstr(WINDOW *w, const wchar_t wstr[])
{
	char *const str = to_multibyte(wstr);
	const int result = waddstr(w, str);
	free(str);
	return result;
}

int
compat_mvwaddwstr(WINDOW *w, int y, int x, const wchar_t wstr[])
{
	char *const str = to_multibyte(wstr);
	const int result = mvwaddstr(w, y, x, str);
	free(str);
	return result;
}

#endif /* BROKEN_WIDE_CURSES */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
