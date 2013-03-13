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

#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../utils/path.h"
#include "../utils/str.h"
#include "../ui.h"
#include "menus.h"

#include "find_menu.h"

int
show_find_menu(FileView *view, int with_path, const char args[])
{
	char cmd_buf[256];
	char *targets;

	static menu_info m;
	init_menu_info(&m, FIND, strdup("No files found"));

	m.title = format_str("find %s", args);

	targets = get_cmd_target();
	if(args[0] == '-')
		snprintf(cmd_buf, sizeof(cmd_buf), "find %s %s", targets, args);
	else if(with_path)
		snprintf(cmd_buf, sizeof(cmd_buf), "find %s", args);
	else
	{
		char *escaped_args = escape_filename(args, 0);
		snprintf(cmd_buf, sizeof(cmd_buf),
				"find %s -type d \\( ! -readable -o ! -executable \\) -prune -o "
				"-name %s -print", targets, escaped_args);
		free(escaped_args);
	}
	free(targets);

	status_bar_message("find...");

	return capture_output_to_menu(view, cmd_buf, &m);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
