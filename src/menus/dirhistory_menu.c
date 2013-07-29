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

#include "dirhistory_menu.h"

#include <stdio.h> /* snprintf() */
#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../modes/menu.h"
#include "../utils/fs.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../ui.h"
#include "menus.h"

int
show_history_menu(FileView *view)
{
	int i;
	static menu_info m;

	init_menu_info(&m, DIRHISTORY, strdup("History disabled or empty"));

	m.title = strdup(" Directory History ");

	for(i = 0; i < view->history_num && i < cfg.history_len; i++)
	{
		int j;
		if(view->history[i].dir[0] == '\0')
			break;
		for(j = i + 1; j < view->history_num && j < cfg.history_len; j++)
			if(stroscmp(view->history[i].dir, view->history[j].dir) == 0)
				break;
		if(j < view->history_num && j < cfg.history_len)
			continue;
		if(!is_valid_dir(view->history[i].dir))
			continue;

		/* Change the current dir to reflect the current file. */
		if(stroscmp(view->history[i].dir, view->curr_dir) == 0)
		{
			(void)replace_string(&view->history[i].file,
					view->dir_entry[view->list_pos].name);
			m.pos = m.len;
		}

		m.len = add_to_string_array(&m.items, m.len, 1, view->history[i].dir);
	}

	/* Reverse order in which items appear. */
	for(i = 0; i < m.len/2; i++)
	{
		char *t = m.items[i];
		m.items[i] = m.items[m.len - 1 - i];
		m.items[m.len - 1 - i] = t;
	}
	m.pos = m.len - 1 - m.pos;

	return display_menu(&m, view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
