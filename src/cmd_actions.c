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
#include "menus/grep_menu.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/str.h"

int
act_grep(const char args[], int invert)
{
	static char *last_args;
	static int last_invert;

	if(args != NULL)
	{
		(void)replace_string(&last_args, args);
		last_invert = invert;
	}
	else if(last_args == NULL)
	{
		ui_sb_err("Nothing to repeat");
		return CMDS_ERR_CUSTOM;
	}

	int inv = last_invert;
	if(args == NULL && invert)
	{
		inv = !inv;
	}

	return show_grep_menu(curr_view, last_args, inv) != 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
