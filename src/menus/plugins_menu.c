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

#include <assert.h> /* assert() */
#include <string.h> /* strdup() */

#include "../compat/reallocarray.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../plugins.h"
#include "menus.h"

static const char * status_to_str(PluginLoadStatus status);
static KHandlerResponse plugs_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);
static void show_plugin_log(view_t *view, menu_data_t *m, plug_t *plug);
static KHandlerResponse log_khandler(view_t *view, menu_data_t *m,
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
		const char *status = status_to_str(plug->status);
		char *item = format_str("[%7s] %s", status, plug->name);
		plugs_m.len = put_into_string_array(&plugs_m.items, plugs_m.len, item);

		plugs_m.void_data = reallocarray(plugs_m.void_data, plugs_m.len,
				sizeof(*plugs_m.void_data));
		plugs_m.void_data[plugs_m.len - 1] = (void *)plug;
	}

	return menus_enter(plugs_m.state, view);
}

/* Turns plugin status into a string.  Returns the string. */
static const char *
status_to_str(PluginLoadStatus status)
{
	switch(status)
	{
		case PLS_SUCCESS: return "loaded";
		case PLS_SKIPPED: return "skipped";
		case PLS_FAILURE: return "failed";
	}
	assert(0 && "Unhandled status value.");
	return "";
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
	else if(wcscmp(keys, L"e") == 0)
	{
		show_plugin_log(view, m, m->void_data[m->pos]);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* Shows log messages of the plugin if there is something.  Switches to a
 * separate menu description. */
static void
show_plugin_log(view_t *view, menu_data_t *m, plug_t *plug)
{
	if(is_null_or_empty(plug->log))
	{
		show_error_msg("Plugin log", "No messages to show");
	}
	else
	{
		static menu_data_t log_m;

		menus_init_data(&log_m, view, format_str("Plugin log (%s)", plug->name),
				NULL);
		log_m.key_handler = &log_khandler;
		log_m.items = break_into_lines(plug->log, plug->log_len, &log_m.len, 0);

		menus_switch_to(&log_m);
	}
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
log_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"h") == 0)
	{
		menus_switch_to(&plugs_m);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
