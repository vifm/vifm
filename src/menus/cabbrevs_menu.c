/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "cabbrevs_menu.h"

#include <stddef.h> /* wchar_t */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcslen() */

#include "../engine/abbrevs.h"
#include "../engine/text_buffer.h"
#include "../modes/menu.h"
#include "../ui/ui.h"
#include "../utils/string_array.h"
#include "../utils/str.h"
#include "../bracket_notation.h"
#include "menus.h"

static KHandlerResponse commands_khandler(menu_info *m, const wchar_t keys[]);

int
show_cabbrevs_menu(FileView *view)
{
	void *state;
	const wchar_t *lhs, *rhs;
	int no_remap;

	static menu_info m;
	init_menu_info(&m, CABBREVS_MENU, strdup("No abbreviation set"));
	m.title = strdup("Abbreviation -- N -- Replacement");
	m.key_handler = &commands_khandler;

	state = NULL;
	while(vle_abbr_iter(&lhs, &rhs, &no_remap, &state))
	{
		char *const descr = describe_abbrev(lhs, rhs, no_remap, 2);
		m.len = put_into_string_array(&m.items, m.len, descr);
	}

	return display_menu(&m, view);
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
		snprintf(cmd_buf, sizeof(cmd_buf), "cunabbrev %s", m->items[m->pos]);
		execute_cmdline_command(cmd_buf);

		remove_current_item(m);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

char *
describe_abbrev(const wchar_t lhs[], const wchar_t rhs[], int no_remap,
		int offset)
{
	enum { LHS_MIN_WIDTH = 13 };
	const char map_mark = no_remap ? '*' : ' ';

	vle_textbuf *const descr = vle_tb_create();

	const size_t rhs_len = wcslen(rhs);
	size_t seq_len;
	size_t i;

	vle_tb_appendf(descr, "%-*ls %3c    ", offset + LHS_MIN_WIDTH, lhs, map_mark);

	for(i = 0U; i < rhs_len; i += seq_len)
	{
		vle_tb_append(descr, wchar_to_spec(&rhs[i], &seq_len));
	}

	vle_tb_append_line(descr, "");

	return vle_tb_release(descr);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
