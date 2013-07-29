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

#include "volumes_menu.h"

#include <windows.h>

#include <stdio.h> /* snprintf() */
#include <string.h> /* strdup() */

#include "../modes/menu.h"
#include "../utils/fs.h"
#include "../utils/string_array.h"
#include "../filelist.h"
#include "../ui.h"
#include "menus.h"

int
show_volumes_menu(FileView *view)
{
	TCHAR c;
	TCHAR vol_name[MAX_PATH];
	TCHAR file_buf[MAX_PATH];

	static menu_info m;
	init_menu_info(&m, VOLUMES, strdup("No volumes mounted"));
	m.title = strdup(" Mounted Volumes ");

	for(c = TEXT('a'); c < TEXT('z'); c++)
	{
		if(drive_exists(c))
		{
			TCHAR drive[] = TEXT("?:\\");
			drive[0] = c;
			if(GetVolumeInformation(drive, vol_name, MAX_PATH, NULL, NULL, NULL,
					file_buf, MAX_PATH))
			{
				char item_buf[MAX_PATH + 5];
				snprintf(item_buf, sizeof(item_buf), "%s  %s ", drive, vol_name);
				m.len = add_to_string_array(&m.items, m.len, 1, item_buf);
			}
		}
	}

	return display_menu(&m, view);
}

void
execute_volumes_cb(FileView *view, menu_info *m)
{
	char path_buf[4];
	snprintf(path_buf, 4, "%s", m->items[m->pos]);

	if(change_directory(view, path_buf) < 0)
		return;

	load_dir_list(view, 0);
	move_to_list_pos(view, 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
