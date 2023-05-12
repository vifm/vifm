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

#include "filetypes_menu.h"

#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() strlen() */

#include "../compat/fs_limits.h"
#include "../int/file_magic.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/cmdline.h"
#include "../modes/menu.h"
#include "../ui/fileview.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../filelist.h"
#include "../flist_sel.h"
#include "../filetype.h"
#include "../running.h"
#include "../types.h"
#include "menus.h"

static const char * form_filetype_menu_entry(assoc_record_t prog,
		int descr_width);
static int execute_filetype_cb(view_t *view, menu_data_t *m);
static KHandlerResponse filetypes_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);
static void fill_menu_from_records(menu_data_t *m,
		const assoc_records_t *records);
static int max_desc_len(const assoc_records_t *records);

int
show_file_menu(view_t *view, int background)
{
	static menu_data_t m;

	int i;
	int max_len;
	assoc_records_t ft, magic;
	char *typed_name;

	dir_entry_t *const entry = get_current_entry(view);
	if(fentry_is_fake(entry))
	{
		show_error_msg("File menu", "Entry doesn't correspond to a file.");
		return 0;
	}

	typed_name = get_typed_entry_fpath(entry);
	ft = ft_get_all_programs(typed_name);
	magic = get_magic_handlers(typed_name);
	free(typed_name);

	menus_init_data(&m, view, strdup("Filetype associated commands"),
			strdup("No programs set for this filetype"));

	m.execute_handler = &execute_filetype_cb;
	m.key_handler = &filetypes_khandler;
	m.extra_data = (background ? 1 : 0);

	max_len = MAX(max_desc_len(&ft), max_desc_len(&magic));

	for(i = 0; i < ft.count; ++i)
	{
		(void)add_to_string_array(&m.data, m.len, ft.list[i].command);
		const char *entry = form_filetype_menu_entry(ft.list[i], max_len);
		m.len = add_to_string_array(&m.items, m.len, entry);
	}

#ifdef ENABLE_DESKTOP_FILES
	/* Add empty line separator if there is at least one other item of either
	 * kind. */
	if(ft.count > 0 || magic.count > 0)
	{
		(void)add_to_string_array(&m.data, m.len, NONE_PSEUDO_PROG.command);
		m.len = add_to_string_array(&m.items, m.len, "");
	}
#endif

	ft_assoc_records_free(&ft);

	for(i = 0; i < magic.count; ++i)
	{
		(void)add_to_string_array(&m.data, m.len, magic.list[i].command);
		const char *entry = form_filetype_menu_entry(magic.list[i], max_len);
		m.len = add_to_string_array(&m.items, m.len, entry);
	}

	return menus_enter(&m, view);
}

/* Returns pointer to a statically allocated buffer */
static const char *
form_filetype_menu_entry(assoc_record_t prog, int descr_width)
{
	static char result[PATH_MAX + 1];

	int found = ft_exists(prog.command);
	const char *found_msg = (found ? "[present] " : "          ");

	char descr[64];
	descr[0] = '\0';

	if(descr_width > 0)
	{
		char format[32];
		if(prog.description[0] == '\0')
		{
			snprintf(format, sizeof(format), "%%-%ds   ", descr_width);
		}
		else
		{
			snprintf(format, sizeof(format), "[%%-%ds] ", descr_width);
		}
		snprintf(descr, sizeof(descr), format, prog.description);
	}

	snprintf(result, sizeof(result), "%s%s%s", found_msg, descr, prog.command);
	return result;
}

/* Callback that is invoked when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_filetype_cb(view_t *view, menu_data_t *m)
{
	const char *const prog_str = m->data[m->pos];
	if(prog_str[0] != '\0')
	{
		int background = m->extra_data & 1;
		rn_open_with(view, prog_str, 0, background);
	}

	flist_sel_stash(view);
	redraw_view(view);
	return 0;
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
filetypes_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"c") == 0)
	{
		const char *const prog_str = m->data[m->pos];
		if(prog_str[0] != '\0')
		{
			modmenu_morph_into_cline(CLS_COMMAND, prog_str, 1);
			return KHR_MORPHED_MENU;
		}
	}
	return KHR_UNHANDLED;
}

int
show_fileprograms_menu(view_t *view, const char fname[])
{
	static menu_data_t m;

	assoc_records_t file_programs;

	menus_init_data(&m, view, format_str("Programs that match %s", fname),
			format_str("No programs match %s", fname));

	file_programs = ft_get_all_programs(fname);
	fill_menu_from_records(&m, &file_programs);
	ft_assoc_records_free(&file_programs);

	return menus_enter(&m, view);
}

int
show_fileviewers_menu(view_t *view, const char fname[])
{
	static menu_data_t m;

	assoc_records_t file_viewers;

	menus_init_data(&m, view, format_str("Viewers that match %s", fname),
			format_str("No viewers match %s", fname));

	file_viewers = ft_get_all_viewers(fname);
	fill_menu_from_records(&m, &file_viewers);
	ft_assoc_records_free(&file_viewers);

	return menus_enter(&m, view);
}

/* Fills the menu with commands from association records. */
static void
fill_menu_from_records(menu_data_t *m, const assoc_records_t *records)
{
	int i;
	const int max_len = max_desc_len(records);

	for(i = 0; i < records->count; ++i)
	{
		const char *entry = form_filetype_menu_entry(records->list[i], max_len);
		m->len = add_to_string_array(&m->items, m->len, entry);
	}
}

/* Calculates the maximum length of the description among the records.  Returns
 * the length. */
static int
max_desc_len(const assoc_records_t *records)
{
	int i;
	int max_len = 0;
	for(i = 0; i < records->count; ++i)
	{
		max_len = MAX(max_len, (int)strlen(records->list[i].description));
	}
	return max_len;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
