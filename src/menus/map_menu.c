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

#include "map_menu.h"

#include <curses.h> /* KEY_* */

#include <stddef.h> /* wchar_t */
#include <stdlib.h> /* malloc() free() */
#include <string.h> /* strdup() strlen() strcat() */
#include <wchar.h> /* wcsncmp() wcslen() */

#include "../compat/reallocarray.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../bracket_notation.h"
#include "menus.h"

static void add_mapping_item(menu_info *m, const wchar_t map_info[]);

int
show_map_menu(FileView *view, const char mode_str[], wchar_t *list[],
		const wchar_t start[])
{
	int x;
	const size_t start_len = wcslen(start);

	static menu_info m;
	init_menu_info(&m, format_str("Mappings for %s mode", mode_str),
			strdup("No mappings found"));

	x = 0;
	while(list[x] != NULL)
	{
		if(list[x][0] != '\0')
		{
			if(wcsncmp(start, list[x], start_len) == 0)
			{
				add_mapping_item(&m, list[x]);
				++m.len;
			}
		}
		else if(m.len != 0)
		{
			m.len = add_to_string_array(&m.items, m.len, 1, "");
		}

		free(list[x]);
		++x;
	}
	free(list);

	if(m.len > 0 && m.items[m.len - 1][0] == '\0')
	{
		free(m.items[m.len - 1]);
		--m.len;
	}

	return display_menu(&m, view);
}

/* Adds map_info to the menu after pre-formatting.  Map_info is assumed to be
 * non-empty. */
static void
add_mapping_item(menu_info *m, const wchar_t map_info[])
{
	enum { MAP_WIDTH = 10 };

	const wchar_t *rhs;
	char *mb_lhs, *mb_rhs;

	rhs = map_info + wcslen(map_info) + 1;
	if(rhs[0] == L'\0')
	{
		rhs = L"<nop>";
	}

	mb_lhs = wstr_to_spec(map_info);
	mb_rhs = wstr_to_spec(rhs);

	m->items = reallocarray(m->items, m->len + 1, sizeof(char *));
	m->items[m->len] = format_str("%-*s %s", MAP_WIDTH, mb_lhs, mb_rhs);

	free(mb_lhs);
	free(mb_rhs);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
