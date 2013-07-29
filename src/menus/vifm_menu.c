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

#include "vifm_menu.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* malloc() */
#include <string.h> /* strdup() */

#include "../modes/menu.h"
#include "../ui.h"
#include "../version.h"
#include "menus.h"

int
show_vifm_menu(FileView *view)
{
	static menu_info m;
	int len;
	/* Version information menu always contains at least one item. */
	init_menu_info(&m, VIFM, NULL);
	m.title = strdup(" vifm information ");

	len = fill_version_info(NULL);
	m.items = malloc(sizeof(char*)*len);
	m.len = fill_version_info(m.items);

	return display_menu(&m, view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
