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
#include <string.h> /* strdup() */

#include "../compat/reallocarray.h"
#include "../ui/ui.h"
#include "../version.h"
#include "menus.h"

int
show_vifm_menu(view_t *view)
{
	static menu_data_t m;
	int len;
	/* Version information menu always contains at least one item. */
	menus_init_data(&m, view, strdup("Vifm Information"), NULL);

	len = fill_version_info(NULL, /*include_stats=*/1);
	m.items = reallocarray(NULL, len, sizeof(char *));
	m.len = fill_version_info(m.items, /*include_stats=*/1);

	return menus_enter(m.state, view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
