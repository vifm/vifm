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

#include <ctype.h> /* isalpha() */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../compat/fs_limits.h"
#include "../modes/menu.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../filelist.h"
#include "../flist_pos.h"
#include "menus.h"

static const char * get_dos_path(const char no_slash_drive[], char buf[],
		size_t buf_size);
static int execute_volumes_cb(view_t *view, menu_data_t *m);

int
show_volumes_menu(view_t *view)
{
	char c;
	char vol_name[PATH_MAX + 1];
	char file_buf[PATH_MAX + 1];

	static menu_data_t m;
	menus_init_data(&m, view, strdup("Mounted Volumes"),
			strdup("No volumes mounted"));
	m.execute_handler = &execute_volumes_cb;

	for(c = 'a'; c <= 'z'; ++c)
	{
		if(drive_exists(c))
		{
			const char no_slash_drive[] = { c, ':', '\0' };
			const char drive[] = { c, ':', '\\', '\0' };

			char unc_path[PATH_MAX + 1];
			DWORD unc_path_size = sizeof(unc_path);
			if(WNetGetConnectionA(no_slash_drive, unc_path, &unc_path_size) !=
					NO_ERROR)
			{
				unc_path[0] = '\0';
			}

			char dos_path_buf[PATH_MAX + 1];
			const char *dos_path = get_dos_path(no_slash_drive, dos_path_buf,
					sizeof(dos_path_buf));

			if(GetVolumeInformationA(drive, vol_name, sizeof(vol_name), NULL, NULL,
					NULL, file_buf, sizeof(file_buf)))
			{
				const char *target = (unc_path[0] != '\0' ? unc_path : dos_path);
				const char *format = (target[0] == '\0') ? "%s    %-20s"
				                                         : "%s    %-20s   ->  %s";

				char *quoted_vol_name = (vol_name[0] == '\0')
				                      ? strdup("")
				                      : format_str("\"%s\"", vol_name);

				char item_buf[PATH_MAX + 5];
				snprintf(item_buf, sizeof(item_buf), format, drive, quoted_vol_name,
						target);
				m.len = add_to_string_array(&m.items, m.len, item_buf);

				free(quoted_vol_name);
			}
		}
	}

	return menus_enter(&m, view);
}

/* Retrieves target of a drive created with `subst` command or equivalent.
 * Returns pointer within the buffer.  The returned string is empty if there is
 * no such target. */
static const char *
get_dos_path(const char no_slash_drive[], char buf[], size_t buf_size)
{
	if(QueryDosDeviceA(no_slash_drive, buf, buf_size) == 0)
	{
		buf[0] = '\0';
	}

	char *path = buf + (starts_with_lit(buf, "\\??\\") ? 4 : 0);
	if(starts_with_lit(path, "UNC\\"))
	{
		path += 2;
		*path = '\\';
	}
	else if(!isalpha(path[0]) || path[1] != ':')
	{
		path[0] = '\0';
	}

	return path;
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_volumes_cb(view_t *view, menu_data_t *m)
{
	char path_buf[4];
	copy_str(path_buf, sizeof(path_buf), m->items[m->pos]);

	if(change_directory(view, path_buf) >= 0)
	{
		load_dir_list(view, 0);
		fpos_set_pos(view, 0);
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
