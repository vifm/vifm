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

#include "locate_menu.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../macros.h"
#include "menus.h"

static int execute_locate_cb(FileView *view, menu_info *m);

int
show_locate_menu(FileView *view, const char args[])
{
	enum { M_a, };

	char *cmd;
	char *margs;
	int save_msg;
	custom_macro_t macros[] = {
		[M_a] = { .letter = 'a', .value = args, .uses_left = 1, .group = -1 },
	};

	static menu_info m;
	margs = (args[0] == '-') ? strdup(args) : shell_like_escape(args, 0);
	init_menu_info(&m, format_str("Locate %s", margs), strdup("No files found"));
	m.args = margs;
	m.execute_handler = &execute_locate_cb;
	m.key_handler = &filelist_khandler;

	status_bar_message("locate...");

	cmd = expand_custom_macros(cfg.locate_prg, ARRAY_LEN(macros), macros);
	save_msg = capture_output_to_menu(view, cmd, 0, &m);
	free(cmd);

	return save_msg;
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_locate_cb(FileView *view, menu_info *m)
{
	goto_selected_file(view, m->items[m->pos], 0);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
