/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "bmarks_menu.h"

#include <string.h> /* strdup() */

#include "../ui/ui.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../bmarks.h"
#include "../status.h"
#include "menus.h"

static int execute_bmarks_cb(FileView *view, menu_info *m);
static KHandlerResponse bmarks_khandler(menu_info *m, const wchar_t keys[]);
static void bmarks_cb(const char path[], const char tags[], time_t timestamp,
		void *arg);

int
show_bmarks_menu(FileView *view, const char tags[], int go_on_single_match)
{
	static menu_info m;
	init_menu_info(&m, strdup("Bookmarks"), strdup("No bookmarks found"));
	m.execute_handler = &execute_bmarks_cb;
	m.key_handler = &bmarks_khandler;

	if(is_null_or_empty(tags))
	{
		bmarks_list(&bmarks_cb, &m);
	}
	else
	{
		bmarks_find(tags, &bmarks_cb, &m);
	}

	if(go_on_single_match && m.len == 1)
	{
		goto_selected_file(view, m.items[m.pos], 0);
		reset_popup_menu(&m);
		return curr_stats.save_msg;
	}

	return display_menu(&m, view);
}

/* Callback for listings of bookmarks. */
static void
bmarks_cb(const char path[], const char tags[], time_t timestamp, void *arg)
{
	menu_info *m = arg;
	char *const line = format_str("%s: %s", replace_home_part_strict(path), tags);
	m->len = put_into_string_array(&m->items, m->len, line);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_bmarks_cb(FileView *view, menu_info *m)
{
	goto_selected_file(view, m->items[m->pos], 0);
	return 0;
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
bmarks_khandler(menu_info *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"dd") == 0)
	{
		break_at(m->items[m->pos], ':');
		bmarks_remove(m->items[m->pos]);
		remove_current_item(m);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
