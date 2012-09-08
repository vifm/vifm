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

#include <stdlib.h> /* malloc() realloc() */
#include <string.h> /* strdup() strcpy() strlen() */

#include "../cfg/config.h"
#include "../modes/menu.h"
#include "../utils/string_array.h"
#include "../ui.h"
#include "menus.h"

#include "history_menu.h"

static int show_history(FileView *view, int type, int len, char *hist[],
		const char title[]);

int
show_cmdhistory_menu(FileView *view)
{
	return show_history(view, CMDHISTORY, cfg.cmd_history_num + 1,
			cfg.cmd_history, " Command Line History ");
}

int
show_prompthistory_menu(FileView *view)
{
	return show_history(view, PROMPTHISTORY, cfg.prompt_history_num + 1,
			cfg.prompt_history, " Prompt History ");
}

int
show_fsearchhistory_menu(FileView *view)
{
	return show_history(view, FSEARCHHISTORY, cfg.search_history_num + 1,
			cfg.search_history, " Search History ");
}

int
show_bsearchhistory_menu(FileView *view)
{
	return show_history(view, BSEARCHHISTORY, cfg.search_history_num + 1,
			cfg.search_history, " Search History ");
}

static int
show_history(FileView *view, int type, int len, char *hist[],
		const char title[])
{
	int i;
	static menu_info m;

	if(len <= 0)
	{
		status_bar_message("History is disabled or empty");
		return 1;
	}

	init_menu_info(&m, type);
	m.title = strdup(title);

	for(i = 0; i < len; i++)
	{
		m.len = add_to_string_array(&m.items, m.len, 1, hist[i]);
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
