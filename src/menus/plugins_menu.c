/* vifm
 * Copyright (C) 2020 xaizek.
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

#include "plugins_menu.h"

#include <string.h> /* strdup() */

#include "../compat/reallocarray.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/menu.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../plugins.h"
#include "menus.h"

static KHandlerResponse plugs_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);

/* Plugins menu description. */
static menu_data_t plugs_m;

int
show_plugins_menu(view_t *view)
{
	menus_init_data(&plugs_m, view, strdup("Plugins"),
			strdup("There are no plugins"));
	plugs_m.key_handler = &plugs_khandler;

	int i;
	const plug_t *plug;
	for(i = 0; plugs_get(curr_stats.plugs, i, &plug); ++i)
	{
		const char *status = (plug->status == PLS_SUCCESS ? "loaded" : "failed");
		char *item = format_str("[%6s] %s", status, plug->path);
		plugs_m.len = put_into_string_array(&plugs_m.items, plugs_m.len, item);

		plugs_m.void_data = reallocarray(plugs_m.void_data, plugs_m.len,
				sizeof(*plugs_m.void_data));
		plugs_m.void_data[plugs_m.len - 1] = (void *)plug;
	}

	return menus_enter(plugs_m.state, view);
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
plugs_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"gf") == 0)
	{
		const plug_t *plug = m->void_data[m->pos];
		(void)menus_goto_file(m, view, plug->path, 0);
		return KHR_CLOSE_MENU;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
