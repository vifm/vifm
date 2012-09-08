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
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strdup() strlen() */

#include "../utils/path.h"
#include "../macros.h"
#include "../ui.h"
#include "menus.h"

#include "grep_menu.h"

int
show_grep_menu(FileView *view, const char *args, int invert)
{
	char title_buf[256];
	char *files;
	int were_errors;
	const char *inv_str = invert ? "-v" : "";
	const char grep_cmd[] = "grep -n -H -I -r";
	char *cmd;

	static menu_info m;
	init_menu_info(&m, GREP);

	snprintf(title_buf, sizeof(title_buf), "grep %s", args);
	m.title = strdup(title_buf);

	if(view->selected_files > 0)
		files = expand_macros(view, "%f", NULL, NULL);
	else
		files = strdup(".");

	if(args[0] == '-')
	{
		size_t var_len = strlen(inv_str) + 1 + strlen(args) + 1 + strlen(files);
		cmd = malloc(sizeof(grep_cmd) + 1 + var_len);
		sprintf(cmd, "%s %s %s %s", grep_cmd, inv_str, args, files);
	}
	else
	{
		size_t var_len;
		char *escaped_args;
		escaped_args = escape_filename(args, 0);
		var_len = strlen(inv_str) + 1 + strlen(escaped_args) + 1 + strlen(files);
		cmd = malloc(sizeof(grep_cmd) + 1 + var_len);
		sprintf(cmd, "%s %s %s %s", grep_cmd, inv_str, escaped_args, files);
		free(escaped_args);
	}
	free(files);

	status_bar_message("grep...");

	were_errors = capture_output_to_menu(view, cmd, &m);
	free(cmd);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No matches found");
		return 1;
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
