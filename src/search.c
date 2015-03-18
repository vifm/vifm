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

#include <regex.h>

#include <assert.h> /* assert() */
#include <stdio.h> /* snprintf() */
#include <string.h>

#include "cfg/config.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "filelist.h"
#include "fileview.h"

static int find_and_goto_pattern(FileView *view, int wrap_start, int backward);
static int find_and_goto_match(FileView *view, int start, int backward);
static void print_result(const FileView *const view, int found, int backward);

int
goto_search_match(FileView *view, int backward)
{
	const int wrap_start = backward ? view->list_rows : -1;
	return find_and_goto_pattern(view, wrap_start, backward);
}

/* Looks for a search match in specified direction navigates to it if match is
 * found.  Wraps search around specified wrap start position, when requested.
 * Returns non-zero if something was found, otherwise zero is returned. */
static int
find_and_goto_pattern(FileView *view, int wrap_start, int backward)
{
	if(!find_and_goto_match(view, view->list_pos, backward))
	{
		if(!cfg.wrap_scan || !find_and_goto_match(view, wrap_start, backward))
		{
			return 0;
		}
	}
	move_to_list_pos(view, view->list_pos);
	return 1;
}

/* Looks for a search match in specified direction from given start position and
 * navigates to it if match is found.  Starting position is not included in
 * searched range.  Returns non-zero if something was found, otherwise zero is
 * returned. */
static int
find_and_goto_match(FileView *view, int start, int backward)
{
	int i;
	int begin, end, step;

	if(backward)
	{
		begin = start - 1;
		end = -1;
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
		int *const found, int interactive)
{
	int cflags;
	int nmatches = 0;
	regex_t re;
	int err;

	if(move && cfg.hl_search)
	{
		clean_selected_files(view);
	}

	reset_search_results(view);

	if(pattern[0] == '\0')
	{
		*found = 1;
		return 0;
	}

	*found = 0;

	cflags = get_regexp_cflags(pattern);
	if((err = regcomp(&re, pattern, cflags)) == 0)
	{
		int i;
		for(i = 0; i < view->list_rows; ++i)
		{
			dir_entry_t *const entry = &view->dir_entry[i];

			if(is_parent_dir(entry->name))
			{
				continue;
			}

			if(regexec(&re, entry->name, 0, NULL, 0) != 0)
			{
				continue;
			}

			entry->search_match = 1;
			if(cfg.hl_search)
			{
				entry->selected = 1;
				++view->selected_files;
			}
			++nmatches;
		}
		regfree(&re);
	}
	else
	{
		if(interactive)
		{
			status_bar_errorf("Regexp error: %s", get_regexp_error(err, &re));
		}
		regfree(&re);
		return 1;
	}

	/* Need to redraw the list so that the matching files are highlighted */
	draw_dir_list(view);

	view->matches = nmatches;
	if(nmatches > 0)
	{
		const int was_found = move ? goto_search_match(view, backward) : 1;
		*found = was_found;

		if(cfg.hl_search && !was_found)
		{
			/* Update the view.  It look might have changed, because of selection. */
			move_to_list_pos(view, view->list_pos);
		}

		if(!cfg.hl_search)
		{
			if(interactive)
			{
				print_result(view, was_found, backward);
			}
			return 1;
		}
		return 0;
	}
	else
	{
		move_to_list_pos(view, view->list_pos);
		if(interactive)
		{
			print_search_fail_msg(view, backward);
		}
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
				(view->matches == 1) ? "" : "s", cfg_get_last_search_pattern());
	}
}

void
print_search_fail_msg(const FileView *view, int backward)
{
	const char *const regexp = cfg_get_last_search_pattern();

	int cflags;
	regex_t re;
	int err;

	if(regexp[0] == '\0')
	{
		status_bar_message("");
		return;
	}

	cflags = get_regexp_cflags(regexp);
	err = regcomp(&re, regexp, cflags);

	if(err != 0)
	{
		status_bar_errorf("Regexp (%s) error: %s", regexp,
				get_regexp_error(err, &re));
		regfree(&re);
		return;
	}

	regfree(&re);

	if(cfg.wrap_scan)
	{
		status_bar_errorf("No matching files for: %s", regexp);
	}
	else if(backward)
	{
		status_bar_errorf("Search hit TOP without match for: %s", regexp);
	}
	else
	{
		status_bar_errorf("Search hit BOTTOM without match for: %s", regexp);
	}
}

void
reset_search_results(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].search_match = 0;
	}
	view->matches = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
