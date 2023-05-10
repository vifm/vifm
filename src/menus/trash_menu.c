/* vifm
 * Copyright (C) 2013 xaizek.
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

#include "trash_menu.h"

#include <stddef.h> /* wchar_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcscmp() */

#include "../io/ior.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/cancellation.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/string_array.h"
#include "../status.h"
#include "../trash.h"
#include "../undo.h"
#include "menus.h"

static KHandlerResponse trash_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);
static KHandlerResponse restore_current(menu_data_t *m);
static KHandlerResponse delete_current(menu_data_t *m);
static int ui_cancellation_hook(void *arg);

int
show_trash_menu(view_t *view)
{
	int i;

	static menu_data_t m;
	menus_init_data(&m, view, strdup("Original paths of files in trash"),
			strdup("No files in trash"));
	m.key_handler = &trash_khandler;

	trash_prune_dead_entries();

	for(i = 0; i < trash_list_size; ++i)
	{
		const trash_entry_t *const entry = &trash_list[i];
		if(trash_has_path(entry->trash_name))
		{
			m.len = add_to_string_array(&m.items, m.len, entry->path);
		}
	}

	return menus_enter(&m, view);
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
trash_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"r") == 0)
	{
		return restore_current(m);
	}
	else if(wcscmp(keys, L"dd") == 0)
	{
		return delete_current(m);
	}
	return KHR_UNHANDLED;
}

/* Restores current item from the trash. */
static KHandlerResponse
restore_current(menu_data_t *m)
{
	char *trash_path;
	int err;

	un_group_open("restore: ");
	un_group_close();

	/* The string is freed in trash_restore(), thus must be cloned. */
	trash_path = strdup(trash_list[m->pos].trash_name);
	err = trash_restore(trash_path);
	free(trash_path);

	if(err != 0)
	{
		const char *const orig_path = m->items[m->pos];
		ui_sb_errf("Failed to restore %s", orig_path);
		curr_stats.save_msg = 1;
		return KHR_UNHANDLED;
	}

	menus_remove_current(m->state);
	return KHR_REFRESH_WINDOW;
}

/* Deletes current item from the trash. */
static KHandlerResponse
delete_current(menu_data_t *m)
{
	io_args_t args = {
		.arg1.path = trash_list[m->pos].trash_name,

		.cancellation.hook = &ui_cancellation_hook,
	};
	ioe_errlst_init(&args.result.errors);

	ui_cancellation_enable();
	IoRes result = ior_rm(&args);
	ui_cancellation_disable();

	if(result != IO_RES_SUCCEEDED)
	{
		char *const errors = ioe_errlst_to_str(&args.result.errors);
		ioe_errlst_free(&args.result.errors);

		show_error_msg("File deletion error", errors);

		free(errors);
		return KHR_UNHANDLED;
	}

	ioe_errlst_free(&args.result.errors);
	menus_remove_current(m->state);
	return KHR_REFRESH_WINDOW;
}

/* Implementation of cancellation hook for I/O unit. */
static int
ui_cancellation_hook(void *arg)
{
	return ui_cancellation_requested();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
