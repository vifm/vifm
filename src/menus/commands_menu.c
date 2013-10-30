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
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* malloc() free() */
#include <string.h> /* strdup() strlen() */
#include <wchar.h> /* wcscmp() */

#include "../engine/cmds.h"
#include "../modes/menu.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../ui.h"
#include "menus.h"

/* Minimal length of command name column. */
#define CMDNAME_COLUMN_MIN_WIDTH 10

static int command_khandler(struct menu_info *m, wchar_t keys[]);

int
show_commands_menu(FileView *view)
{
	char **list;
	int i;
	int cmdname_width = CMDNAME_COLUMN_MIN_WIDTH;

	static menu_info m;
	init_menu_info(&m, COMMAND, strdup("No commands set"));
	m.key_handler = command_khandler;

	m.title = strdup(" Command ------ Action ");

	list = list_udf();

	m.len = -1;
	while(list[++m.len] != NULL)
	{
		const size_t cmdname_len = strlen(list[m.len]);
		if(cmdname_len > cmdname_width)
		{
			cmdname_width = cmdname_len;
		}

		m.len++;
		assert(list[m.len] != NULL && "Broken list of user-defined commands.");
	}
	m.len /= 2;

	m.items = malloc(sizeof(char *)*m.len);
	for(i = 0; i < m.len; i++)
	{
		m.items[i] = format_str("%-*s %s", cmdname_width, list[i*2], list[i*2 + 1]);
	}

	free_string_array(list, m.len*2);

	return display_menu(&m, view);
}

static int
command_khandler(struct menu_info *m, wchar_t keys[])
{
	if(wcscmp(keys, L"dd") == 0) /* remove element */
	{
		char cmd_buf[512];

		break_at(m->items[m->pos], ' ');
		snprintf(cmd_buf, sizeof(cmd_buf), "delcommand %s", m->items[m->pos]);
		execute_cmdline_command(cmd_buf);

		remove_from_string_array(m->items, m->len, m->pos);
		if(m->matches != NULL)
		{
			if(m->matches[m->pos])
				m->matching_entries--;
			memmove(m->matches + m->pos, m->matches + m->pos + 1,
					sizeof(int)*((m->len - 1) - m->pos));
		}
		m->len--;
		draw_menu(m);

		move_to_menu_pos(m->pos, m);
		return 1;
	}
	return -1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
