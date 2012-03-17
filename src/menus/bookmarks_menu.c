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

#include <string.h> /* strdup() strcpy() strlen() */

#include "../modes/menu.h"
#include "../utils/macros.h"
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../config.h"
#include "../bookmarks.h"

#include "bookmarks_menu.h"

static int bookmark_khandler(struct menu_info *m, wchar_t *keys);

int
show_bookmarks_menu(FileView *view, const char *marks)
{
	int j, x;
	int max_len;
	char buf[PATH_MAX];

	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = cfg.num_bookmarks;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = 0;
	m.type = BOOKMARK;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.items = NULL;
	m.data = NULL;
	m.key_handler = bookmark_khandler;

	getmaxyx(menu_win, m.win_rows, x);

	m.len = init_active_bookmarks(marks);
	if(m.len == 0)
	{
		(void)show_error_msg("No bookmarks set", "No bookmarks are set.");
		return 0;
	}

	m.title = strdup(" Mark -- Directory -- File ");

	max_len = 0;
	x = 0;
	while(x < m.len)
	{
		size_t len;

		len = get_utf8_string_length(bookmarks[active_bookmarks[x]].directory);
		if(len > max_len)
			max_len = len;
		x++;
	}
	max_len = MIN(max_len + 3, getmaxx(menu_win) - 5 - 2 - 10);

	x = 0;
	while(x < m.len)
	{
		char *with_tilde;
		int overhead;

		j = active_bookmarks[x];
		with_tilde = replace_home_part(bookmarks[j].directory);
		if(strlen(with_tilde) > max_len - 3)
			strcpy(with_tilde + max_len - 6, "...");
		overhead = get_utf8_overhead(with_tilde);
		if(!is_bookmark(j))
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, "[invalid]");
		}
		else if(!pathcmp(bookmarks[j].file, "..") ||
				!pathcmp(bookmarks[j].file, "../"))
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, "[none]");
		}
		else
		{
			snprintf(buf, sizeof(buf), "%c   %-*s%s", index2mark(j),
					max_len + overhead, with_tilde, bookmarks[j].file);
		}

		m.items = realloc(m.items, sizeof(char *) * (x + 1));
		m.items[x] = malloc(sizeof(buf) + 2);
		snprintf(m.items[x], sizeof(buf), "%s", buf);

		x++;
	}
	m.len = x;

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
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
