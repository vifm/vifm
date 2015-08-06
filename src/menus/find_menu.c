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

#include "find_menu.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../macros.h"
#include "menus.h"

#ifdef _WIN32
#define DEFAULT_PREDICATE "-iname"
#else
#define DEFAULT_PREDICATE "-name"
#endif

static int execute_find_cb(FileView *view, menu_info *m);

int
show_find_menu(FileView *view, int with_path, const char args[])
{
	int save_msg;
	char *custom_args = NULL;
	char *targets = NULL;
	char *cmd;

	custom_macro_t macros[] = {
		{ .letter = 's', .value = NULL, .uses_left = 1, .group = -1 },
		{ .letter = 'a', .value = NULL, .uses_left = 1, .group =  1 },
		{ .letter = 'A', .value = NULL, .uses_left = 0, .group =  1 },
	};

	static menu_info m;

	if(with_path)
	{
		macros[0].value = args;
		macros[1].value = "";
		macros[2].value = "";
	}
	else
	{
		targets = prepare_targets(view);
		if(targets == NULL)
		{
			show_error_msg("Find", "Failed to setup target directory.");
			return 0;
		}

		macros[0].value = targets;
		macros[2].value = args;

		if(args[0] == '-')
		{
			macros[1].value = args;
		}
		else
		{
			char *const escaped_args = escape_filename(args, 0);
			custom_args = format_str("%s %s", DEFAULT_PREDICATE, escaped_args);
			macros[1].value = custom_args;
			free(escaped_args);
		}
	}

	init_menu_info(&m, format_str("Find %s", args), strdup("No files found"));

	m.execute_handler = &execute_find_cb;
	m.key_handler = &filelist_khandler;

	status_bar_message("find...");

	cmd = expand_custom_macros(cfg.find_prg, ARRAY_LEN(macros), macros);

	free(targets);
	free(custom_args);

	save_msg = capture_output_to_menu(view, cmd, 0, &m);
	free(cmd);

	return save_msg;
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_find_cb(FileView *view, menu_info *m)
{
	goto_selected_file(view, m->items[m->pos], 0);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
