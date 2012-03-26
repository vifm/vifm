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

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* malloc() realloc() */
#include <string.h> /* strdup() */

#include "../modes/menu.h"
#include "../utils/fs.h"
#include "../filelist.h"
#include "../ui.h"
#include "menus.h"

#include "volumes_menu.h"

void
show_volumes_menu(FileView *view)
{
	TCHAR c;
	TCHAR vol_name[MAX_PATH];
	TCHAR file_buf[MAX_PATH];

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = VOLUMES;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = strdup(" Mounted Volumes ");
	m.args = NULL;
	m.items = NULL;
	m.data = NULL;

	for(c = TEXT('a'); c < TEXT('z'); c++)
	{
		if(drive_exists(c))
		{
			TCHAR drive[] = TEXT("?:\\");
			drive[0] = c;
			if(GetVolumeInformation(drive, vol_name, MAX_PATH, NULL, NULL, NULL,
					file_buf, MAX_PATH))
			{
				m.items = (char **)realloc(m.items, sizeof(char *) * (m.len + 1));
				m.items[m.len] = (char *)malloc((MAX_PATH + 5) * sizeof(char));

				snprintf(m.items[m.len], MAX_PATH, "%s  %s ", drive, vol_name);
				m.len++;
			}
		}
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(0, &m);
	enter_menu_mode(&m, view);
}

void
execute_volumes_cb(FileView *view, menu_info *m)
{
	char buf[4];
	snprintf(buf, 4, "%s", m->items[m->pos]);

	if(change_directory(view, buf) < 0)
		return;

	load_dir_list(view, 0);
	move_to_list_pos(view, 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
