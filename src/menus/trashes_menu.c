/* vifm
 * Copyright (C) 2014 xaizek.
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

#include "trashes_menu.h"

#include <stdint.h> /* uint64_t */
#include <string.h> /* strchr() strdup() */

#include "../ui/ui.h"
#include "../utils/cancellation.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../fops_misc.h"
#include "../trash.h"
#include "menus.h"

static char * format_item(const char trash_dir[], int calc_size);
static int execute_trashes_cb(view_t *view, menu_data_t *m);
static KHandlerResponse trashes_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);

int
show_trashes_menu(view_t *view, int calc_size)
{
	char **trashes;
	int ntrashes;
	int i;

	static menu_data_t m;
	menus_init_data(&m, view,
			format_str("%sNon-empty trash directories", calc_size ? "[  size] " : ""),
			strdup("No non-empty trash directories found"));

	m.execute_handler = &execute_trashes_cb;
	m.key_handler = &trashes_khandler;
	m.extra_data = calc_size;

	trashes = trash_list_trashes(&ntrashes);

	show_progress(NULL, 0);
	for(i = 0; i < ntrashes; i++)
	{
		char *const item = format_item(trashes[i], calc_size);
		m.len = put_into_string_array(&m.items, m.len, item);
	}

	free_string_array(trashes, ntrashes);

	return menus_enter(&m, view);
}

/* Formats single menu item.  Returns pointer to newly allocated string. */
static char *
format_item(const char trash_dir[], int calc_size)
{
	char msg[PATH_MAX + 1];
	uint64_t size;
	char size_str[64];

	if(!calc_size)
	{
		return strdup(trash_dir);
	}

	snprintf(msg, sizeof(msg), "Calculating size of %s...", trash_dir);
	show_progress(msg, 1);

	size = fops_dir_size(trash_dir, 1, &no_cancellation);

	size_str[0] = '\0';
	friendly_size_notation(size, sizeof(size_str), size_str);

	return format_str("[%8s] %s", size_str, trash_dir);
}

/* Callback that is called when menu item is selected.  Return non-zero to stay
 * in menu mode and zero otherwise. */
static int
execute_trashes_cb(view_t *view, menu_data_t *m)
{
	const char *const item = m->items[m->pos];
	const char *const trash_dir = m->extra_data ? strchr(item, ']') + 2 : item;
	menus_goto_dir(view, trash_dir);
	return 0;
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
trashes_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"dd") == 0)
	{
		const char *const item = m->items[m->pos];
		const char *trash_dir = m->extra_data ? strchr(item, ']') + 2 : item;
		trash_empty(trash_dir);
		menus_remove_current(m->state);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
