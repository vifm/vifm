/* vifm
 * Copyright (C) 2013 xaizek.
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

#include "trash_menu.h"

#include <string.h> /* strdup() */

#include "../utils/string_array.h"
#include "../trash.h"

int
show_trash_menu(FileView *view)
{
	int i;

	static menu_info m;
	init_menu_info(&m, TRASH_MENU, strdup("No files in trash"));

	m.title = strdup(" Original paths of files in trash ");

	for(i = 0; i < nentries; i++)
	{
		const trash_entry_t *const entry = &trash_list[i];
		m.len = add_to_string_array(&m.items, m.len, 1, entry->path);
	}

	return display_menu(&m, view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
