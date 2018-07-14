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

#include "colorscheme_menu.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() strcmp() */

#include "../compat/fs_limits.h"
#include "../cfg/config.h"
#include "../ui/color_scheme.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "menus.h"

static int execute_colorscheme_cb(view_t *view, menu_data_t *m);

int
show_colorschemes_menu(view_t *view)
{
	static menu_data_t m;
	menus_init_data(&m, view, strdup("Choose the default Color Scheme"),
			strdup("No color schemes found"));
	m.execute_handler = &execute_colorscheme_cb;

	m.items = cs_list(&m.len);

	safe_qsort(m.items, m.len, sizeof(*m.items), &strossorter);

	/* It's safe to set m.pos to negative value, since menus.c handles this
	 * correctly. */
#ifndef _WIN32
	m.pos = string_array_pos(m.items, m.len, cfg.cs.name);
#else
	m.pos = string_array_pos_case(m.items, m.len, cfg.cs.name);
#endif

	return menus_enter(m.state, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_colorscheme_cb(view_t *view, menu_data_t *m)
{
	cs_load_primary(m->items[m->pos]);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
