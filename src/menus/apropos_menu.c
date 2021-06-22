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

#include "apropos_menu.h"

#include <ctype.h> /* isdigit() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() strchr() strlen() */

#include "../cfg/config.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../macros.h"
#include "../running.h"
#include "../status.h"
#include "menus.h"

static int execute_apropos_cb(view_t *view, menu_data_t *m);
TSTATIC int parse_apropos_line(const char line[], char section[],
		size_t section_len, char topic[], size_t topic_len);

int
show_apropos_menu(view_t *view, const char args[])
{
	char *cmd;
	int save_msg;
	custom_macro_t macros[] = {
		{ .letter = 'a', .value = args, .uses_left = 1, .group = -1 },
	};

	static menu_data_t m;
	menus_init_data(&m, view, format_str("Apropos %s", args),
			format_str("No matches for \'%s\'", args));
	m.execute_handler = &execute_apropos_cb;

	ui_sb_msg("apropos...");

	cmd = ma_expand_custom(cfg.apropos_prg, ARRAY_LEN(macros), macros, MA_NOOPT);
	save_msg = menus_capture(view, cmd, /*user_sh=*/0, &m, MF_NONE);
	free(cmd);
	return save_msg;
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_apropos_cb(view_t *view, menu_data_t *m)
{
	char section[64], topic[64];
	char command[256];
	int exit_code;

	if(parse_apropos_line(m->items[m->pos], section, sizeof(section), topic,
				sizeof(topic)) != 0)
	{
		curr_stats.save_msg = 1;
		return 1;
	}

	snprintf(command, sizeof(command), "man %s %s", section, topic);

	curr_stats.skip_shellout_redraw = 1;
	exit_code = rn_shell(command, PAUSE_NEVER, 1, SHELL_BY_APP);
	curr_stats.skip_shellout_redraw = 0;

	if(exit_code != 0)
	{
		ui_sb_errf("man view command failed with code: %d", exit_code);
		curr_stats.save_msg = 1;
	}

	return 1;
}

/* Parses apropos output line and extracts section number and topic.  On error
 * prints status bar message and returns non-zero, otherwise zero is
 * returned. */
TSTATIC int
parse_apropos_line(const char line[], char section[], size_t section_len,
		char topic[], size_t topic_len)
{
	const char *sec_l, *sec_r;
	const char *sep;

	sec_l = strchr(line, '(');
	if(sec_l == NULL)
	{
		ui_sb_err("Failed to find section number.");
		return 1;
	}

	/* Check for "([^\s()]+)" format. */
	sec_r = sec_l + 1 + strcspn(sec_l + 1, " \t()");
	if(sec_r == sec_l + 1 || *sec_r != ')')
	{
		ui_sb_err("Wrong section number format.");
		return 1;
	}

	/* sep can't be NULL as we found '(' above. */
	sep = strpbrk(line, " (");

	if(section_len == 0 || section_len - 1 < (size_t)(sec_r - (sec_l + 1)) ||
			topic_len == 0 || topic_len - 1 < (size_t)(sep - line))
	{
		ui_sb_err("Internal buffer is too small.");
		return 1;
	}

	copy_str(topic, sep - line + 1, line);
	copy_str(section, sec_r - (sec_l + 1) + 1, sec_l + 1);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
