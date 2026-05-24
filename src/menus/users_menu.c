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

#include "../ui/ui.h"
#include "../modes/cmdline.h"
#include "../modes/menu.h"
#include "menus.h"

static int execute_users_cb(view_t *view, menu_data_t *m);
static const char * users_get_spec(const menu_data_t *m, int pos);
static KHandlerResponse users_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);

int
show_user_menu(view_t *view, const char command[], const char title[],
		MacroFlags flags)
{
	static menu_data_t m;
	menus_init_data(&m, view, strdup(title), strdup("No results found"));

	const int navigate = ma_flags_present(flags, MF_MENU_NAV_OUTPUT);

	m.extra_data = navigate;
	m.stashable = navigate;
	m.execute_handler = &execute_users_cb;
	m.key_handler = &users_khandler;

	return menus_capture(view, command, 1, &m, flags);
}

int
show_custom_menu(view_t *view, const char title[], strlist_t items,
		strlist_t specs, int with_navigation)
{
	static menu_data_t m;

	/* Avoid a dialog about the menu being empty by checking for this case
	 * beforehand. */
	if(items.nitems == 0)
	{
		goto fail;
	}

	/* Items and specs must be consistent. */
	if(specs.nitems != 0 && specs.nitems != items.nitems)
	{
		goto fail;
	}

	menus_init_data(&m, view, strdup(title), strdup("No items"));

	m.extra_data = with_navigation;
	m.stashable = with_navigation;
	m.execute_handler = &execute_users_cb;
	m.get_spec = &users_get_spec;
	m.key_handler = &users_khandler;

	m.len = items.nitems;
	m.items = items.items;
	m.data = specs.items;

	if(menus_enter(&m, view) != 0)
	{
		goto fail;
	}

	return 0;

fail:
	free_string_array(items.items, items.nitems);
	free_string_array(specs.items, specs.nitems);
	return 1;
}

int
stash_custom_menu(view_t *view, const char title[], strlist_t items,
		strlist_t specs, int with_navigation, int pos)
{
	static menu_data_t m;

	/* Avoid saving menus that couldn't be reopened later. */
	if(items.nitems == 0)
	{
		goto fail;
	}

	if(specs.nitems != 0 && specs.nitems != items.nitems)
	{
		goto fail;
	}

	menus_init_data(&m, view, strdup(title), strdup("No items"));

	m.extra_data = with_navigation;
	m.stashable = 1;
	m.execute_handler = &execute_users_cb;
	m.get_spec = &users_get_spec;
	m.key_handler = &users_khandler;

	m.len = items.nitems;
	m.items = items.items;
	m.data = specs.items;
	if(pos >= 0 && pos < m.len)
	{
		m.pos = pos;
	}

	menus_stash(&m);
	return 0;

fail:
	free_string_array(items.items, items.nitems);
	free_string_array(specs.items, specs.nitems);
	return 1;
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_users_cb(view_t *view, menu_data_t *m)
{
	const int navigate = m->extra_data;
	if(navigate)
	{
		(void)menus_goto_file(m, view, m->get_spec(m, m->pos), 0);
	}
	return 0;
}

/* Callback for querying path specification.  Must never return NULL. */
static const char *
users_get_spec(const menu_data_t *m, int pos)
{
	return (m->data != NULL ? m->data[pos] : m->items[pos]);
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
users_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	const int navigate = m->extra_data;
	if(navigate)
	{
		return menus_def_khandler(view, m, keys);
	}
	else if(wcscmp(keys, L"c") == 0)
	{
		/* Insert whole line. */
		modmenu_morph_into_cline(CLS_COMMAND, m->items[m->pos], 1);
		return KHR_MORPHED_MENU;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
