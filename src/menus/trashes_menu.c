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

#include "../utils/string_array.h"
#include "../trash.h"
#include "../ui.h"

int
show_trashes_menu(FileView *view)
{
	char **trashes;
	int ntrashes;
	int i;

	static menu_info m;
	init_menu_info(&m, TRASHES_MENU, strdup("No trash directories found"));

	m.title = strdup(" Trash directories ");

	trashes = list_trashes(&ntrashes);

	for(i = 0; i < ntrashes; i++)
	{
		m.len = add_to_string_array(&m.items, m.len, 1, trashes[i]);
	}

	free_string_array(trashes, ntrashes);

	return display_menu(&m, view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
