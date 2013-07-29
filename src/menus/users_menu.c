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

#include "users_menu.h"

#include <string.h> /* strdup() */

#include "../status.h"
#include "../ui.h"
#include "menus.h"

int
show_user_menu(FileView *view, const char command[], int navigate)
{
	static menu_info m;
	const int menu_type = navigate ? USER_NAVIGATE : USER;
	init_menu_info(&m, menu_type, strdup("No results found"));

	m.title = strdup(command);

	return capture_output_to_menu(view, command, &m);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
