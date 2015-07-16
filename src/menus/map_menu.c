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
#include <stdlib.h> /* malloc() realloc() free() */
#include <string.h> /* strdup() strlen() strcat() */
#include <wchar.h> /* wcsncmp() wcslen() */

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
	size_t len;
	int i, str_len, buf_len;
	const wchar_t *rhs;
	char *item;

	str_len = wcslen(map_info);
	rhs = map_info + str_len + 1;

	buf_len = 0;
	for(i = 0; i < str_len; i += len)
	{
		buf_len += strlen(wchar_to_spec(map_info + i, &len));
	}

	if(rhs[0] == L'\0')
	{
		rhs = L"<nop>";
	}

	buf_len += 1 + wcslen(rhs)*4 + 1;

	m->items = realloc(m->items, sizeof(char *)*(m->len + 1));
	item = malloc(buf_len + MAP_WIDTH);
	item[0] = '\0';
	m->items[m->len] = item;

	for(i = 0; i < str_len; i += len)
	{
		strcat(item, wchar_to_spec(map_info + i, &len));
	}

	for(i = strlen(item); i < MAP_WIDTH; i++)
	{
		strcat(item, " ");
	}

	strcat(item, " ");

	for(i = 0; rhs[i] != L'\0'; i += len)
	{
		if(rhs[i] == L' ')
		{
			strcat(item, " ");
			len = 1;
		}
		else
		{
			strcat(item, wchar_to_spec(rhs + i, &len));
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
