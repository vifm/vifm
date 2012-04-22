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

#include <string.h> /* strdup() */

#include "../utils/path.h"
#include "../macros.h"
#include "../ui.h"
#include "menus.h"

#include "find_menu.h"

int
show_find_menu(FileView *view, int with_path, const char *args)
{
	char buf[256];
	char *files;
	int were_errors;

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = FIND;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.items = NULL;
	m.data = NULL;

	snprintf(buf, sizeof(buf), "find %s", args);
	m.title = strdup(buf);

	if(view->selected_files > 0)
		files = expand_macros(view, "%f", NULL, NULL);
	else
		files = strdup(".");

	if(args[0] == '-')
		snprintf(buf, sizeof(buf), "find %s %s", files, args);
	else if(with_path)
		snprintf(buf, sizeof(buf), "find %s", args);
	else
	{
		char *escaped_args = escape_filename(args, 0);
		snprintf(buf, sizeof(buf),
				"find %s -type d \\( ! -readable -o ! -executable \\) -prune -o "
				"-name %s -print", files, escaped_args);
		free(escaped_args);
	}
	free(files);

	were_errors = capture_output_to_menu(view, buf, &m);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No files found");
		return 1;
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
