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

#include "undolist_menu.h"

#include <stddef.h> /* size_t */
#include <stdlib.h> /* realloc() */
#include <string.h> /* memmove() strdup() strlen() */
#include <wchar.h> /* wcscmp() */

#include "../ui/ui.h"
#include "../utils/string_array.h"
#include "../undo.h"
#include "menus.h"

static KHandlerResponse undolist_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);
static void set_mark(menu_data_t *m, int pos);
static void unset_mark(menu_data_t *m, int pos);

int
show_undolist_menu(view_t *view, int with_details)
{
	static menu_data_t m;
	menus_init_data(&m, view, strdup("Undolist"), strdup("Undolist is empty"));
	m.key_handler = &undolist_khandler;
	m.extra_data = with_details;

	m.items = un_get_list(with_details);
	m.len = count_strings(m.items);

	/* Add additional entry before setting position. */
	if(m.len > 0)
	{
		m.len = add_to_string_array(&m.items, m.len, " <<< list end >>>");
	}

	menus_set_pos(m.state, un_get_list_pos(with_details));
	set_mark(&m, m.pos);

	return menus_enter(m.state, view);
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
undolist_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"r") == 0)
	{
		const int with_details = m->extra_data;

		unset_mark(m, un_get_list_pos(with_details));

		un_set_pos(m->pos, with_details);
		set_mark(m, un_get_list_pos(with_details));

		menus_partial_redraw(m->state);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* Adds current position mark to a menu item. */
static void
set_mark(menu_data_t *m, int pos)
{
	if(m->len != 0 && m->items[pos] != NULL)
	{
		m->items[pos][0] = '*';
	}
}

/* Removes current position mark from a menu item. */
static void
unset_mark(menu_data_t *m, int pos)
{
	if(m->len != 0 && m->items[pos] != NULL)
	{
		m->items[pos][0] = ' ';
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
