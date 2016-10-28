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
#include <string.h> /* strdup() strlen() */

#include "../ui/ui.h"
#include "../utils/string_array.h"
#include "../undo.h"
#include "menus.h"

int
show_undolist_menu(FileView *view, int with_details)
{
	char **p;

	static menu_data_t m;
	init_menu_data(&m, view, strdup("Undolist"), strdup("Undolist is empty"));

	m.items = undolist(with_details);
	p = m.items;
	while(*p++ != NULL)
	{
		++m.len;
	}

	move_to_menu_pos(get_undolist_pos(with_details), m.state);

	if(m.len > 0)
	{
		size_t len;

		m.len = add_to_string_array(&m.items, m.len, 1, "list end");

		/* Add current position mark to menu item. */
		len = (m.items[m.pos] != NULL) ? strlen(m.items[m.pos]) : 0;
		m.items[m.pos] = realloc(m.items[m.pos], len + 1 + 1);
		memmove(m.items[m.pos] + 1, m.items[m.pos], len + 1);
		m.items[m.pos][0] = '*';
	}

	return display_menu(m.state, view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
