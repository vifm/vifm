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
#include "../ui.h"
#include "../undo.h"
#include "menus.h"

#include "undolist_menu.h"

int
show_undolist_menu(FileView *view, int with_details)
{
	char **p;

	static menu_info m;
	m.top = 0;
	m.current = get_undolist_pos(with_details) + 1;
	m.len = 0;
	m.pos = m.current - 1;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = UNDOLIST;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Undolist ");
	m.args = NULL;
	m.items = NULL;
	m.data = NULL;

	m.items = undolist(with_details);
	p = m.items;
	while(*p++ != NULL)
		m.len++;

	if(!m.len)
	{
		m.items = (char **)realloc(m.items, sizeof(char *) * 1);
		m.items[0] = strdup(" Undolist is empty ");
		m.len = 1;
	}
	else
	{
		size_t len;

		m.items[m.len] = strdup("list end");
		m.len++;

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
