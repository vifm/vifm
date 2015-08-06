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

#include "users_menu.h"

#include <string.h> /* strdup() */

#include "../ui/ui.h"
#include "menus.h"

static int execute_users_cb(FileView *view, menu_info *m);

int
show_user_menu(FileView *view, const char command[], int navigate)
{
	static menu_info m;
	init_menu_info(&m, strdup(command), strdup("No results found"));
	m.extra_data = navigate;

	m.execute_handler = &execute_users_cb;
	if(navigate)
	{
		m.key_handler = &filelist_khandler;
	}

	return capture_output_to_menu(view, command, 1, &m);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_users_cb(FileView *view, menu_info *m)
{
	const int navigate = m->extra_data;
	if(navigate)
	{
		goto_selected_file(view, m->items[m->pos], 0);
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
