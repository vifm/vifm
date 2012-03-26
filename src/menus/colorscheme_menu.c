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

#include <dirent.h> /* DIR dirent */

#include <string.h> /* strdup() strcmp() */

#include "../cfg/config.h"
#include "../modes/menu.h"
#include "menus.h"

#include "colorscheme_menu.h"

void
show_colorschemes_menu(FileView *view)
{
	int len;
	DIR *dir;
	struct dirent *d;
	char colors_dir[PATH_MAX];

	static menu_info m;
	m.top = 0;
	m.current = 0;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = COLORSCHEME;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.items = NULL;
	m.data = NULL;

	getmaxyx(menu_win, m.win_rows, len);

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

		m.items = (char **)realloc(m.items, sizeof(char *)*(m.len + 1));
		m.items[m.len] = (char *)malloc(len + 2);
		snprintf(m.items[m.len++], len, "%s", d->d_name);
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
