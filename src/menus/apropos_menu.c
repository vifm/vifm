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

#include "apropos_menu.h"

#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* malloc() free() */
#include <string.h> /* strdup() strchr() strlen() */

#include "../cfg/config.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../macros.h"
#include "../running.h"
#include "../status.h"
#include "menus.h"

int
show_apropos_menu(FileView *view, const char args[])
{
	char *cmd;
	int save_msg;
	custom_macro_t macros[] =
	{
		{ .letter = 'a', .value = args, .uses_left = 1 },
	};

	static menu_info m;
	init_menu_info(&m, APROPOS, format_str("No matches for \'%s\'", args));
	m.args = strdup(args);
	m.title = format_str(" Apropos %s ", args);

	status_bar_message("apropos...");

	cmd = expand_custom_macros(cfg.apropos_prg, ARRAY_LEN(macros), macros);
	save_msg = capture_output_to_menu(view, cmd, &m);
	free(cmd);
	return save_msg;
}

void
execute_apropos_cb(menu_info *m)
{
	char *line;
	char *man_page;
	char *free_this;
	char *num_str;
	char command[256];

	free_this = man_page = line = strdup(m->items[m->pos]);
	if(free_this == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if((num_str = strchr(line, '(')))
	{
		int z = 0;

		num_str++;
		while(num_str[z] != ')')
		{
			z++;
			if(z > 40)
			{
				free(free_this);
				return;
			}
		}

		num_str[z] = '\0';
		line = strchr(line, ' ');
		if(line != NULL)
		{
			line[0] = '\0';

			snprintf(command, sizeof(command), "man %s %s", num_str, man_page);

			curr_stats.auto_redraws = 1;
			shellout(command, 0, 1);
			curr_stats.auto_redraws = 0;
		}
	}
	free(free_this);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
