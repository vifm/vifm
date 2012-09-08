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

#include <stdlib.h> /* realloc() */
#include <string.h> /* strdup() strlen() */

#include "../modes/menu.h"
#include "../utils/string_array.h"
#include "../ui.h"
#include "../undo.h"
#include "menus.h"

#include "undolist_menu.h"

int
show_undolist_menu(FileView *view, int with_details)
{
	char **p;

	static menu_info m;
	init_menu_info(&m, UNDOLIST);
	m.current = get_undolist_pos(with_details) + 1;
	m.pos = m.current - 1;
	m.title = strdup(" Undolist ");

	m.items = undolist(with_details);
	p = m.items;
	while(*p++ != NULL)
		m.len++;

	if(m.len == 0)
	{
		m.len = add_to_string_array(&m.items, m.len, 1, " Undolist is empty ");
	}
	else
	{
		size_t len;

		m.len = add_to_string_array(&m.items, m.len, 1, "list end");

		/* Add current position mark to menu item. */
		len = (m.items[m.pos] != NULL) ? strlen(m.items[m.pos]) : 0;
		m.items[m.pos] = realloc(m.items[m.pos], len + 1 + 1);
		memmove(m.items[m.pos] + 1, m.items[m.pos], len + 1);
		m.items[m.pos][0] = '*';
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
