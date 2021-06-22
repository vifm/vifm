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
#include "../running.h"
#include "menus.h"

static int execute_locate_cb(view_t *view, menu_data_t *m);

int
show_locate_menu(view_t *view, const char args[])
{
	enum { M_a, M_u, M_U, };

	char *cmd;
	char *margs;
	int save_msg;
	custom_macro_t macros[] = {
		[M_a] = { .letter = 'a', .value = args, .uses_left = 1, .group = -1 },

		[M_u] = { .letter = 'u', .value = "",   .uses_left = 1, .group = -1 },
		[M_U] = { .letter = 'U', .value = "",   .uses_left = 1, .group = -1 },
	};

	static menu_data_t m;
	margs = (args[0] == '-') ? strdup(args) : shell_like_escape(args, 0);
	menus_init_data(&m, view, format_str("Locate %s", margs),
			strdup("No files found"));
	free(margs);

	m.stashable = 1;
	m.execute_handler = &execute_locate_cb;
	m.key_handler = &menus_def_khandler;

	cmd = ma_expand_custom(cfg.locate_prg, ARRAY_LEN(macros), macros, MA_NOOPT);

	MacroFlags flags = MF_NONE;
	if(macros[M_u].explicit_use)
	{
		ma_flags_set(&flags, MF_CUSTOMVIEW_OUTPUT);
	}
	if(macros[M_U].explicit_use)
	{
		ma_flags_set(&flags, MF_VERYCUSTOMVIEW_OUTPUT);
	}

	ui_sb_msg("locate...");
	save_msg = menus_capture(view, cmd, /*user_sh=*/0, &m, flags);
	free(cmd);

	return save_msg;
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_locate_cb(view_t *view, menu_data_t *m)
{
	(void)menus_goto_file(m, view, m->items[m->pos], 0);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
