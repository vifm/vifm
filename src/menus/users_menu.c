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

#include "../status.h"
#include "../ui.h"
#include "menus.h"

#include "users_menu.h"

void
show_user_menu(FileView *view, const char command[], int navigate)
{
	static menu_info m;
	int were_errors;
	const int menu_type = navigate ? USER_NAVIGATE : USER;
	init_menu_info(&m, menu_type);

	m.title = strdup(command);

	were_errors = capture_output_to_menu(view, command, &m);
	if(!were_errors && m.len < 1)
	{
		status_bar_error("No results found");
		curr_stats.save_msg = 1;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
