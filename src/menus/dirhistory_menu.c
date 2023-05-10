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
#include "../utils/test_helpers.h"
#include "../filelist.h"
#include "menus.h"

static int execute_dirhistory_cb(view_t *view, menu_data_t *m);
TSTATIC strlist_t list_dir_history(view_t *view, int *pos);

int
show_history_menu(view_t *view)
{
	strlist_t list;

	static menu_data_t m;
	menus_init_data(&m, view, strdup("Directory History"),
			strdup("History disabled or empty"));

	m.execute_handler = &execute_dirhistory_cb;

	list = list_dir_history(view, &m.pos);
	m.items = list.items;
	m.len = list.nitems;

	return menus_enter(&m, view);
}

/* Lists directory history of the view.  Puts current position into *pos.
 * Returns the list. */
TSTATIC strlist_t
list_dir_history(view_t *view, int *pos)
{
	int i;
	int need_cleanup = 0;
	strlist_t list = {};

	*pos = 0;

	for(i = 0; i < view->history_num && i < cfg.history_len; ++i)
	{
		int j;
		if(view->history[i].dir[0] == '\0')
			break;

		/* Ignore all appearances of a directory except for the last one. */
		for(j = i + 1; j < view->history_num && j < cfg.history_len; ++j)
			if(stroscmp(view->history[i].dir, view->history[j].dir) == 0)
				break;
		if(j < view->history_num && j < cfg.history_len)
			continue;

		if(!is_valid_dir(view->history[i].dir))
		{
			/* "Mark" directory as non-existent. */
			update_string(&view->history[i].dir, NULL);
			need_cleanup = 1;
			continue;
		}

		if(stroscmp(view->history[i].dir, flist_get_dir(view)) == 0)
		{
			/* Change the current dir to reflect the current file. */
			(void)replace_string(&view->history[i].file, get_current_file_name(view));
			*pos = list.nitems;
		}

		list.nitems = add_to_string_array(&list.items, list.nitems,
				view->history[i].dir);
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
				view->history[i].timestamp = (time_t)-1;
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

	/* Reverse order in which items appear and adjust position. */
	for(i = 0; i < list.nitems/2; ++i)
	{
		char *t = list.items[i];
		list.items[i] = list.items[list.nitems - 1 - i];
		list.items[list.nitems - 1 - i] = t;
	}
	*pos = list.nitems - 1 - *pos;

	return list;
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_dirhistory_cb(view_t *view, menu_data_t *m)
{
	menus_goto_dir(view, m->items[m->pos]);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
