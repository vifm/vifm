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

#include "search.h"

#include <sys/types.h>

#include <curses.h>
#include <regex.h>

#include <assert.h> /* assert() */
#include <string.h>

#include "cfg/config.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "filelist.h"
#include "ui.h"

enum
{
	PREVIOUS,
	NEXT
};

static int find_and_goto_match(FileView *view, int start, int direction);
static void print_result(const FileView *const view, int found, int backward);

/* returns non-zero if pattern was found */
int
find_previous_pattern(FileView *view, int wrap)
{
	if(find_and_goto_match(view, view->list_pos, PREVIOUS))
		move_to_list_pos(view, view->list_pos);
	else if(wrap && find_and_goto_match(view, view->list_rows, PREVIOUS))
		move_to_list_pos(view, view->list_pos);
	else
		return 0;
	return 1;
}

/* returns non-zero if pattern was found */
int
find_next_pattern(FileView *view, int wrap)
{
	if(find_and_goto_match(view, view->list_pos, NEXT))
		move_to_list_pos(view, view->list_pos);
	else if(wrap && find_and_goto_match(view, 0, NEXT))
		move_to_list_pos(view, view->list_pos);
	else
		return 0;
	return 1;
}

static int
find_and_goto_match(FileView *view, int start, int direction)
{
	int i;
	int begin, end, step;

	if(direction == PREVIOUS)
	{
		begin = start - 1;
		end = 0;
		step = -1;

		assert(begin >= end && "Wrong range.");
	}
	else
	{
		begin = start + 1;
		end = view->list_rows;
		step = 1;

		assert(begin <= end && "Wrong range.");
	}

	for(i = begin; i != end; i += step)
	{
		if(view->dir_entry[i].search_match)
		{
			view->list_pos = i;
			break;
		}
	}

	return i != end;
}

int
find_pattern(FileView *view, const char pattern[], int backward, int move,
		int *const found)
{
	int cflags;
	int nmatches = 0;
	regex_t re;
	int x;
	int err;

	if(move && cfg.hl_search)
		clean_selected_files(view);
	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].search_match = 0;

	*found = 0;

	if(pattern[0] == '\0')
	{
		*found = 1;
		return 0;
	}

	cflags = get_regexp_cflags(pattern);
	if((err = regcomp(&re, pattern, cflags)) == 0)
	{
		if(pattern != view->regexp)
			snprintf(view->regexp, sizeof(view->regexp), "%s", pattern);

		for(x = 0; x < view->list_rows; x++)
		{
			char buf[NAME_MAX];

			if(is_parent_dir(view->dir_entry[x].name))
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
			nmatches++;
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
	draw_dir_list(view);

	view->matches = nmatches;
	if(nmatches > 0)
	{
		int was_found = 1;
		if(move)
		{
			if(backward)
				was_found = find_previous_pattern(view, cfg.wrap_scan);
			else
				was_found = find_next_pattern(view, cfg.wrap_scan);
		}
		*found = was_found;

		if(cfg.hl_search && !was_found)
		{
			/* Update the view.  It look might have changed, because of selection. */
			move_to_list_pos(view, view->list_pos);
		}

		if(!cfg.hl_search)
		{
			print_result(view, was_found, backward);
			return 1;
		}
		return 0;
	}
	else
	{
		move_to_list_pos(view, view->list_pos);
		print_search_fail_msg(view, backward);
		return 1;
	}
}

/* Prints success or error message, determined by the found argument, about
 * search results to a user. */
static void
print_result(const FileView *const view, int found, int backward)
{
	if(found)
	{
		print_search_msg(view, backward);
	}
	else
	{
		print_search_fail_msg(view, backward);
	}
}

void
print_search_msg(const FileView *view, int backward)
{
	if(view->matches == 0)
	{
		print_search_fail_msg(view, backward);
	}
	else
	{
		status_bar_messagef("%d matching file%s for: %s", view->matches,
				(view->matches == 1) ? "" : "s", view->regexp);
	}
}

void
print_search_fail_msg(const FileView *view, int backward)
{
	if(!cfg.wrap_scan)
	{
		if(backward)
			status_bar_errorf("Search hit TOP without match for: %s", view->regexp);
		else
			status_bar_errorf("Search hit BOTTOM without match for: %s",
					view->regexp);
	}
	else
	{
		status_bar_errorf("No matching files for: %s", view->regexp);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
