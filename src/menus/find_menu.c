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
#include "../macros.h"
#include "../ui.h"
#include "menus.h"

#include "find_menu.h"

int
show_find_menu(FileView *view, int with_path, const char args[])
{
	char cmd_buf[256];
	char *files;
	int were_errors;

	static menu_info m;
	init_menu_info(&m, FIND);

	snprintf(cmd_buf, sizeof(cmd_buf), "find %s", args);
	m.title = strdup(cmd_buf);

	if(view->selected_files > 0)
		files = expand_macros(view, "%f", NULL, NULL);
	else
		files = strdup(".");

	if(args[0] == '-')
		snprintf(cmd_buf, sizeof(cmd_buf), "find %s %s", files, args);
	else if(with_path)
		snprintf(cmd_buf, sizeof(cmd_buf), "find %s", args);
	else
	{
		char *escaped_args = escape_filename(args, 0);
		snprintf(cmd_buf, sizeof(cmd_buf),
				"find %s -type d \\( ! -readable -o ! -executable \\) -prune -o "
				"-name %s -print", files, escaped_args);
		free(escaped_args);
	}
	free(files);

	status_bar_message("find...");

	were_errors = capture_output_to_menu(view, cmd_buf, &m);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No files found");
		return 1;
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
