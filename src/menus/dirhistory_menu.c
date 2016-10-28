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

#include <stddef.h> /* NULL */
#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../filelist.h"
#include "menus.h"

static int execute_dirhistory_cb(FileView *view, menu_data_t *m);

int
show_history_menu(FileView *view)
{
	int i;
	int need_cleanup;

	static menu_data_t m;
	init_menu_data(&m, view, strdup("Directory History"),
			strdup("History disabled or empty"));

	m.execute_handler = &execute_dirhistory_cb;

	need_cleanup = 0;
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
		{
			/* "Mark" directory as non-existent. */
			free(view->history[i].dir);
			view->history[i].dir = NULL;
			need_cleanup = 1;
			continue;
		}

		/* Change the current dir to reflect the current file. */
		if(stroscmp(view->history[i].dir, view->curr_dir) == 0)
		{
			(void)replace_string(&view->history[i].file, get_current_file_name(view));
			m.pos = m.len;
		}

		m.len = add_to_string_array(&m.items, m.len, 1, view->history[i].dir);
	}

	if(need_cleanup)
	{
		int i, j;

		/* Erase non existing directories from history. */
		j = 0;
		for(i = 0; i < view->history_num && i < cfg.history_len; ++i)
		{
			if(view->history[i].dir == NULL)
			{
				free(view->history[i].file);
				continue;
			}

			if(j != i)
			{
				view->history[j] = view->history[i];

				view->history[i].dir = NULL;
				view->history[i].file = NULL;
				view->history[i].rel_pos = -1;
			}

			if(view->history_pos == i)
			{
				view->history_pos = j;
			}

			++j;
		}
		view->history_num = j;
	}

	/* Reverse order in which items appear. */
	for(i = 0; i < m.len/2; i++)
	{
		char *t = m.items[i];
		m.items[i] = m.items[m.len - 1 - i];
		m.items[m.len - 1 - i] = t;
	}
	m.pos = m.len - 1 - m.pos;

	return display_menu(m.state, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_dirhistory_cb(FileView *view, menu_data_t *m)
{
	goto_selected_directory(view, m->items[m->pos]);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
