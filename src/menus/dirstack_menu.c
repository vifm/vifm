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

#include "dirstack_menu.h"

#include <string.h> /* strdup() */

#include "../ui/ui.h"
#include "../utils/string_array.h"
#include "../dir_stack.h"
#include "menus.h"

static int execute_dirstack_cb(view_t *view, menu_data_t *m);

int
show_dirstack_menu(view_t *view)
{
	static menu_data_t m;
	/* Directory stack always contains at least one item (current directories). */
	menus_init_data(&m, view, strdup("Directory Stack"), NULL);
	m.execute_handler = &execute_dirstack_cb;

	m.items = dir_stack_list();
	m.len = count_strings(m.items);

	return menus_enter(&m, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_dirstack_cb(view_t *view, menu_data_t *m)
{
	if(m->items[m->pos][0] != '-')
	{
		int pos = 0;
		int i;

		for(i = 0; i < m->pos; ++i)
		{
			if(m->items[i][0] == '-')
			{
				++pos;
			}
		}
		dir_stack_rotate(pos);
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
