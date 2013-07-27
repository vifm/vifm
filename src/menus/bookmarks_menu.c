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

#include "bookmarks_menu.h"

#include <stdio.h> /* snprintf() */
#include <string.h> /* strdup() strcpy() strlen() */

#include "../cfg/config.h"
#include "../modes/menu.h"
#include "../utils/fs_limits.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../bookmarks.h"

static int bookmark_khandler(struct menu_info *m, wchar_t *keys);

int
show_bookmarks_menu(FileView *view, const char marks[])
{
	int i;
	int max_len;

	static menu_info m;
	init_menu_info(&m, BOOKMARK, strdup("No bookmarks set"));
	m.key_handler = bookmark_khandler;

	m.len = init_active_bookmarks(marks);
	m.title = strdup(" Mark -- Directory -- File ");

	max_len = 0;
	i = 0;
	while(i < m.len)
	{
		size_t len;

		len = get_screen_string_length(bookmarks[active_bookmarks[i]].directory);
		if(len > max_len)
			max_len = len;
		i++;
	}
	max_len = MIN(max_len + 3, getmaxx(menu_win) - 5 - 2 - 10);

	i = 0;
	while(i < m.len)
	{
		char item_buf[PATH_MAX];
		char *with_tilde;
		int overhead;
		int j;

		j = active_bookmarks[i];
		with_tilde = replace_home_part(bookmarks[j].directory);
		if(get_screen_string_length(with_tilde) > max_len - 3)
		{
			size_t width = get_normal_utf8_string_widthn(with_tilde, max_len - 6);
			strcpy(with_tilde + width, "...");
		}
		overhead = get_screen_overhead(with_tilde);
		if(!is_bookmark(j))
		{
			snprintf(item_buf, sizeof(item_buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, "[invalid]");
		}
		else if(is_parent_dir(bookmarks[j].file))
		{
			snprintf(item_buf, sizeof(item_buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, "[none]");
		}
		else
		{
			snprintf(item_buf, sizeof(item_buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, bookmarks[j].file);
		}

		i = add_to_string_array(&m.items, i, 1, item_buf);
	}
	m.len = i;

	return display_menu(&m, view);
}

static int
bookmark_khandler(struct menu_info *m, wchar_t *keys)
{
	if(wcscmp(keys, L"dd") == 0)
	{
		clean_menu_position(m);
		remove_bookmark(active_bookmarks[m->pos]);
		memmove(active_bookmarks + m->pos, active_bookmarks + m->pos + 1,
				sizeof(int)*(m->len - 1 - m->pos));

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
