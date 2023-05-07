/* vifm
 * Copyright (C) 2023 xaizek.
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

#include "chistory_menu.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../utils/str.h"
#include "../utils/string_array.h"
#include "menus.h"

static int execute_stash_cb(struct view_t *view, menu_data_t *m);

int
show_chistory_menu(struct view_t *view)
{
	static menu_data_t m;
	menus_init_data(&m, view, strdup("Item count -- Menu title"),
			strdup("No saved menus"));
	m.execute_handler = &execute_stash_cb;
	m.menu_context = 1;

	int i = 0, current;
	const menu_data_t *stash;
	while((stash = menus_get_stash(i++, &current)) != NULL)
	{
		char *item = format_str("%12d    %s", stash->len, stash->title);
		if(item == NULL)
		{
			continue;
		}

		if(put_into_string_array(&m.items, m.len, item) != m.len + 1)
		{
			free(item);
			continue;
		}

		++m.len;
		if(current)
		{
			m.pos = m.len - 1;
		}
	}

	return menus_enter(m.state, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_stash_cb(struct view_t *view, menu_data_t *m)
{
	menus_unstash_at(m->state, m->pos);
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
