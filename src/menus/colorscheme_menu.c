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

#include <dirent.h> /* DIR dirent opendir() readdir() closedir() */

#include <limits.h> /* PATH_MAX */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() strcmp() */

#include "../cfg/config.h"
#include "../modes/menu.h"
#include "../utils/string_array.h"
#include "menus.h"

#include "colorscheme_menu.h"

void
show_colorschemes_menu(FileView *view)
{
	DIR *dir;
	struct dirent *d;
	char colors_dir[PATH_MAX];

	static menu_info m;
	init_menu_info(&m, COLORSCHEME);
	m.title = strdup(" Choose the default Color Scheme ");

	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", cfg.config_dir);

	dir = opendir(colors_dir);
	if(dir == NULL)
	{
		free(m.title);
		return;
	}

	while((d = readdir(dir)) != NULL)
	{
#ifndef _WIN32
		if(d->d_type != DT_REG && d->d_type != DT_LNK)
			continue;
#endif

		if(d->d_name[0] == '.')
			continue;

		m.len = add_to_string_array(&m.items, m.len, 1, d->d_name);
		if(strcmp(d->d_name, cfg.cs.name) == 0)
		{
			m.current = m.len;
			m.pos = m.len - 1;
		}
	}
	closedir(dir);

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
