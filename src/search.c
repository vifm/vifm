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

#include <sys/types.h>

#include <curses.h>
#include <regex.h>

#include <string.h>

#include "config.h"
#include "filelist.h"
#include "ui.h"
#include "utils.h"

#include "search.h"

enum
{
	PREVIOUS,
	NEXT
};

static int
find_next_pattern_match(FileView *view, int start, int direction)
{
	int found = 0;
	int x;

	if(direction == PREVIOUS)
	{
		for(x = start - 1; x > 0; x--)
		{
			if(view->dir_entry[x].search_match)
			{
				found = 1;
				view->list_pos = x;
				break;
			}
		}
	}
	else if(direction == NEXT)
	{
		for(x = start + 1; x < view->list_rows; x++)
		{
			if(view->dir_entry[x].search_match)
			{
				found = 1;
				view->list_pos = x;
				break;
			}
		}
	}
	return found;
}

/* returns non-zero if pattern was found */
int
find_previous_pattern(FileView *view, int wrap)
{
	if(find_next_pattern_match(view, view->list_pos, PREVIOUS))
		move_to_list_pos(view, view->list_pos);
	else if(wrap && find_next_pattern_match(view, view->list_rows, PREVIOUS))
		move_to_list_pos(view, view->list_pos);
	else
		return 0;
	return 1;
}

/* returns non-zero if pattern was found */
int
find_next_pattern(FileView *view, int wrap)
{
	if(find_next_pattern_match(view, view->list_pos, NEXT))
		move_to_list_pos(view, view->list_pos);
	else if(wrap && find_next_pattern_match(view, 0, NEXT))
		move_to_list_pos(view, view->list_pos);
	else
		return 0;
	return 1;
}

static void
top_bottom_msg(FileView *view, int backward)
{
	if(backward)
		status_bar_errorf("Search hit TOP without match for: %s", view->regexp);
	else
		status_bar_errorf("Search hit BOTTOM without match for: %s", view->regexp);
}

int
find_pattern(FileView *view, const char *pattern, int backward, int move)
{
	int cflags;
	int found = 0;
	regex_t re;
	int x;
	int err;

	if(move)
		clean_selected_files(view);
	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].search_match = 0;

	if(pattern[0] == '\0')
		return 0;

	cflags = get_regexp_cflags(pattern);
	if((err = regcomp(&re, pattern, cflags)) == 0)
	{
		if(pattern != view->regexp)
			snprintf(view->regexp, sizeof(view->regexp), "%s", pattern);

		for(x = 0; x < view->list_rows; x++)
		{
			char buf[NAME_MAX];

			if(pathcmp(view->dir_entry[x].name, "../") == 0)
				continue;

			strncpy(buf, view->dir_entry[x].name, sizeof(buf));
			chosp(buf);
			if(regexec(&re, buf, 0, NULL, 0) != 0)
				continue;

			view->dir_entry[x].search_match = 1;
			if(cfg.hl_search)
			{
				view->dir_entry[x].selected = 1;
				view->selected_files++;
			}
			found++;
		}
		regfree(&re);
	}
	else
	{
		status_bar_errorf("Regexp error: %s", get_regexp_error(err, &re));
		regfree(&re);
		return 1;
	}

	/* Need to redraw the list so that the matching files are highlighted */
	draw_dir_list(view, view->top_line);

	if(found > 0)
	{
		int was_found = 1;
		if(move)
		{
			if(backward)
				was_found = find_previous_pattern(view, cfg.wrap_scan);
			else
				was_found = find_next_pattern(view, cfg.wrap_scan);
		}
		if(!cfg.hl_search)
		{
			view->matches = found;

			if(!was_found)
			{
				top_bottom_msg(view, backward);
				return 1;
			}

			status_bar_messagef("%d matching file%s for: %s", found,
					(found == 1) ? "" : "s", view->regexp);
			return 1;
		}
		return 0;
	}
	else
	{
		move_to_list_pos(view, view->list_pos);
		if(!cfg.wrap_scan)
			top_bottom_msg(view, backward);
		else
			status_bar_errorf("No matching files for: %s", view->regexp);
		return 1;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
