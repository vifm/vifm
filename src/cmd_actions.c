/* vifm
 * Copyright (C) 2023 xaizek.
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

#include "cmd_actions.h"

#include "engine/cmds.h"
#include "menus/all.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/str.h"
#include "utils/test_helpers.h"

TSTATIC void act_drop_state(void);

/* Holds static state. */
static struct
{
	/* For act_find(). */
	struct
	{
		char *last_args;   /* Last arguments passed to the command. */
		int includes_path; /* Whether last_args contains path to search in. */
	}
	find;

	/* For act_grep(). */
	struct
	{
		char *last_args; /* Last arguments passed to the command. */
		int last_invert; /* Last inversion flag passed to the command. */
	}
	grep;
}
act_state;

int
act_find(const char args[], int argc, char *argv[])
{
	if(argc > 0)
	{
		if(argc == 1)
			act_state.find.includes_path = 0;
		else if(is_dir(argv[0]))
			act_state.find.includes_path = 1;
		else
			act_state.find.includes_path = 0;

		(void)replace_string(&act_state.find.last_args, args);
	}
	else if(act_state.find.last_args == NULL)
	{
		ui_sb_err("Nothing to repeat");
		return CMDS_ERR_CUSTOM;
	}

	return show_find_menu(curr_view, act_state.find.includes_path,
			act_state.find.last_args) != 0;
}

int
act_grep(const char args[], int invert)
{
	if(args != NULL)
	{
		(void)replace_string(&act_state.grep.last_args, args);
		act_state.grep.last_invert = invert;
	}
	else if(act_state.grep.last_args == NULL)
	{
		ui_sb_err("Nothing to repeat");
		return CMDS_ERR_CUSTOM;
	}

	int inv = act_state.grep.last_invert;
	if(args == NULL && invert)
	{
		inv = !inv;
	}

	return show_grep_menu(curr_view, act_state.grep.last_args, inv) != 0;
}

TSTATIC void
act_drop_state(void)
{
	update_string(&act_state.find.last_args, NULL);
	act_state.find.includes_path = 0;

	update_string(&act_state.grep.last_args, NULL);
	act_state.grep.last_invert = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
