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

#include "commands_menu.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t wchar_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() strlen() */
#include <wchar.h> /* wcscmp() */

#include "../compat/reallocarray.h"
#include "../engine/cmds.h"
#include "../modes/menu.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../cmd_core.h"
#include "menus.h"

/* Minimal length of command name column. */
#define CMDNAME_COLUMN_MIN_WIDTH 10

static int execute_commands_cb(FileView *view, menu_info *m);
static KHandlerResponse commands_khandler(menu_info *m, const wchar_t keys[]);

int
show_commands_menu(FileView *view)
{
	char **list;
	int i;
	size_t cmdname_width = CMDNAME_COLUMN_MIN_WIDTH;

	static menu_info m;
	init_menu_info(&m, strdup("Command ------ Action"),
			strdup("No commands set"));
	m.execute_handler = &execute_commands_cb;
	m.key_handler = &commands_khandler;

	list = list_udf();

	m.len = -1;
	while(list[++m.len] != NULL)
	{
		const size_t cmdname_len = strlen(list[m.len]);
		if(cmdname_len > cmdname_width)
		{
			cmdname_width = cmdname_len;
		}

		++m.len;
		assert(list[m.len] != NULL && "Broken list of user-defined commands.");
	}
	m.len /= 2;

	m.items = (m.len != 0) ? reallocarray(NULL, m.len, sizeof(char *)) : NULL;
	for(i = 0; i < m.len; i++)
	{
		m.items[i] = format_str("%-*s %s", (int)cmdname_width, list[i*2],
				list[i*2 + 1]);
	}

	free_string_array(list, m.len*2);

	return display_menu(&m, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_commands_cb(FileView *view, menu_info *m)
{
	break_at(m->items[m->pos], ' ');
	exec_command(m->items[m->pos], view, CIT_COMMAND);
	return 0;
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
commands_khandler(menu_info *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"dd") == 0) /* Remove element. */
	{
		char cmd_buf[512];

		break_at(m->items[m->pos], ' ');
		snprintf(cmd_buf, sizeof(cmd_buf), "delcommand %s", m->items[m->pos]);
		execute_cmdline_command(cmd_buf);

		remove_current_item(m);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
