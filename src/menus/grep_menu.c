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

#include "grep_menu.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../macros.h"
#include "../ui.h"
#include "menus.h"

int
show_grep_menu(FileView *view, const char args[], int invert)
{
	char *targets;
	int save_msg;
	char *cmd;
	char *escaped_args = NULL;

	custom_macro_t macros[] =
	{
		{ .letter = 'i', .value = NULL, .uses_left = 1 },
		{ .letter = 'a', .value = NULL, .uses_left = 1 },
		{ .letter = 's', .value = NULL, .uses_left = 1 },
	};

	static menu_info m;
	init_menu_info(&m, GREP, strdup("No matches found"));

	m.title = format_str(" Grep %s ", args);

	targets = get_cmd_target();
	macros[0].value = invert ? "-v" : "";
	macros[1].value = args;
	macros[2].value = targets;
	if(args[0] != '-')
	{
		escaped_args = escape_filename(args, 0);
		macros[1].value = escaped_args;
	}

	cmd = expand_custom_macros(cfg.grep_prg, ARRAY_LEN(macros), macros);

	free(escaped_args);
	free(targets);

	status_bar_message("grep...");
	save_msg = capture_output_to_menu(view, cmd, &m);
	free(cmd);

	return save_msg;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
