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
#include <string.h> /* strdup() */

#include "../utils/path.h"
#include "../ui.h"
#include "menus.h"

#include "locate_menu.h"

int
show_locate_menu(FileView *view, const char args[])
{
	char cmd_buf[256];
	int were_errors;

	static menu_info m;
	init_menu_info(&m, LOCATE);
	m.args = (args[0] == '-') ? strdup(args) : escape_filename(args, 0);

	snprintf(cmd_buf, sizeof(cmd_buf), "locate %s", m.args);
	m.title = strdup(cmd_buf);

	status_bar_message("locate...");

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
