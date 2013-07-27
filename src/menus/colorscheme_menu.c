/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#include "colorscheme_menu.h"

#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() qsort() */
#include <string.h> /* strdup() strcmp() */

#include "../cfg/config.h"
#include "../modes/menu.h"
#include "../utils/fs_limits.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "menus.h"

static int sorter(const void *first, const void *second);

int
show_colorschemes_menu(FileView *view)
{
	static menu_info m;
	init_menu_info(&m, COLORSCHEME, strdup("No color schemes found"));
	m.title = strdup(" Choose the default Color Scheme ");

	m.items = list_color_schemes(&m.len);

	qsort(m.items, m.len, sizeof(*m.items), &sorter);

	/* It's safe to set m.pos to negative value, since menus.c handles this
	 * correctly. */
#ifndef _WIN32
	m.pos = string_array_pos(m.items, m.len, cfg.cs.name);
#else
	m.pos = string_array_pos_case(m.items, m.len, cfg.cs.name);
#endif

	return display_menu(&m, view);
}

/* Sorting function for qsort(). */
static int
sorter(const void *first, const void *second)
{
	const char *stra = *(const char **)first;
	const char *strb = *(const char **)second;
	return stroscmp(stra, strb);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
