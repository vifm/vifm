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

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../utils/path.h"
#include "../utils/str.h"
#include "../macros.h"
#include "../ui.h"
#include "menus.h"

#include "grep_menu.h"

int
show_grep_menu(FileView *view, const char args[], int invert)
{
	char *files;
	int result;
	const char *inv_str = invert ? "-v" : "";
	const char grep_cmd[] = "grep -n -H -I -r";
	char *cmd;

	static menu_info m;
	init_menu_info(&m, GREP, strdup("No matches found"));
	m.title = format_str("grep %s", args);

	if(view->selected_files > 0)
		files = expand_macros("%f", NULL, NULL);
	else
		files = strdup(".");

	if(args[0] == '-')
	{
		cmd = format_str("%s %s %s %s", grep_cmd, inv_str, args, files);
	}
	else
	{
		char *const escaped_args = escape_filename(args, 0);
		cmd = format_str("%s %s %s %s", grep_cmd, inv_str, escaped_args, files);
		free(escaped_args);
	}
	free(files);

	status_bar_message("grep...");

	result = capture_output_to_menu(view, cmd, &m);
	free(cmd);
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
