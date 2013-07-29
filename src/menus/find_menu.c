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
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../macros.h"
#include "../ui.h"
#include "menus.h"

#ifdef _WIN32
#define DEFAULT_PREDICATE "-iname"
#else
#define DEFAULT_PREDICATE "-name"
#endif

int
show_find_menu(FileView *view, int with_path, const char args[])
{
	int save_msg;
	char *custom_args = NULL;
	char *targets = NULL;
	char *cmd;

	custom_macro_t macros[] =
	{
		{ .letter = 's', .value = NULL, .uses_left = 1 },
		{ .letter = 'a', .value = NULL, .uses_left = 1 },
	};

	static menu_info m;
	init_menu_info(&m, FIND, strdup("No files found"));

	m.title = format_str(" Find %s ", args);

	if(with_path)
	{
		macros[0].value = args;
		macros[1].value = "";
	}
	else
	{
		targets = get_cmd_target();
		macros[0].value = targets;

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

	status_bar_message("find...");

	cmd = expand_custom_macros(cfg.find_prg, ARRAY_LEN(macros), macros);

	free(targets);
	free(custom_args);

	save_msg = capture_output_to_menu(view, cmd, &m);
	free(cmd);

	return save_msg;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
