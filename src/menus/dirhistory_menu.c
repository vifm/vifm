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

#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../modes/menu.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../ui.h"
#include "menus.h"

#include "dirhistory_menu.h"

/* Returns new value for save_msg flag */
int
show_history_menu(FileView *view)
{
	int x;
	static menu_info m;

	if(cfg.history_len <= 0)
	{
		status_bar_message("History is disabled");
		return 1;
	}

	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = DIRHISTORY;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Directory History ");
	m.args = NULL;
	m.items = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, x);

	for(x = 0; x < view->history_num && x < cfg.history_len; x++)
	{
		int y;
		if(view->history[x].dir[0] == '\0')
			break;
		for(y = x + 1; y < view->history_num && y < cfg.history_len; y++)
			if(pathcmp(view->history[x].dir, view->history[y].dir) == 0)
				break;
		if(y < view->history_num && y < cfg.history_len)
			continue;
		if(!is_valid_dir(view->history[x].dir))
			continue;

		/* Change the current dir to reflect the current file. */
		if(pathcmp(view->history[x].dir, view->curr_dir) == 0)
		{
			snprintf(view->history[x].file, sizeof(view->history[x].file),
					"%s", view->dir_entry[view->list_pos].name);
			m.pos = m.len;
		}

		m.items = realloc(m.items, sizeof(char *)*(m.len + 1));
		m.items[m.len] = strdup(view->history[x].dir);
		m.len++;
	}
	for(x = 0; x < m.len/2; x++)
	{
		char *t = m.items[x];
		m.items[x] = m.items[m.len - 1 - x];
		m.items[m.len - 1 - x] = t;
	}
	m.pos = m.len - 1 - m.pos;
	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
