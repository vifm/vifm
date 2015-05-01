/* vifm
 * Copyright (C) 2014 xaizek.
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

#include "trashes_menu.h"

#include <string.h> /* strdup() */

#include "../ui/ui.h"
#include "../utils/string_array.h"
#include "../trash.h"
#include "menus.h"

static int execute_trashes_cb(FileView *view, menu_info *m);

int
show_trashes_menu(FileView *view)
{
	char **trashes;
	int ntrashes;
	int i;

	static menu_info m;
	init_menu_info(&m, TRASHES_MENU,
			strdup("No non-empty trash directories found"));

	m.title = strdup(" Non-empty trash directories ");
	m.execute_handler = &execute_trashes_cb;

	trashes = list_trashes(&ntrashes);

	for(i = 0; i < ntrashes; i++)
	{
		m.len = add_to_string_array(&m.items, m.len, 1, trashes[i]);
	}

	free_string_array(trashes, ntrashes);

	return display_menu(&m, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_trashes_cb(FileView *view, menu_info *m)
{
	goto_selected_directory(view, m->items[m->pos]);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
