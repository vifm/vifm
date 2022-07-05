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

#include <regex.h> /* regmatch_t regexec() regfree() */

#include <assert.h> /* assert() */
#include <stdio.h> /* snprintf() */
#include <string.h>

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "filelist.h"
#include "flist_sel.h"

static int find_and_goto_match(view_t *view, int start, int backward);
static void print_result(const view_t *view, int found, int backward);

int
goto_search_match(view_t *view, int backward)
{
	const int wrap_start = backward ? view->list_rows : -1;
	if(!find_and_goto_match(view, view->list_pos, backward))
	{
		if(!cfg.wrap_scan || !find_and_goto_match(view, wrap_start, backward))
		{
			return 0;
		}
	}

	/* Redraw the cursor which also might synchronize cursors of two views. */
	fview_cursor_redraw(view);
	/* Schedule redraw of the view to highlight search matches. */
	ui_view_schedule_redraw(view);

	return 1;
}

/* Looks for a search match in specified direction from given start position and
 * navigates to it if match is found.  Starting position is not included in
 * searched range.  Returns non-zero if something was found, otherwise zero is
 * returned. */
static int
find_and_goto_match(view_t *view, int start, int backward)
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
find_pattern(view_t *view, const char pattern[], int backward, int move,
		int *found, int print_errors)
{
	int cflags;
	int nmatches = 0;
	regex_t re;
	int err;
	view_t *other;

	if(move && cfg.hl_search)
	{
		flist_sel_stash(view);
	}

	reset_search_results(view);

	/* We at least could wipe out previous search results, so schedule a
	 * redraw. */
	ui_view_schedule_redraw(view);

	if(pattern[0] == '\0')
	{
		*found = 1;
		return 0;
	}

	*found = 0;

	cflags = get_regexp_cflags(pattern);
	if((err = regexp_compile(&re, pattern, cflags)) == 0)
	{
		int i;
		for(i = 0; i < view->list_rows; ++i)
		{
			regmatch_t matches[1];
			dir_entry_t *const entry = &view->dir_entry[i];
			const char *name = entry->name;
			char *free_this = NULL;

			if(is_parent_dir(name))
			{
				continue;
			}

			if(fentry_is_dir(entry))
			{
				free_this = format_str("%s/", name);
				name = free_this;
			}

			if(regexec(&re, name, 1, matches, 0) != 0)
			{
				free(free_this);
				continue;
			}

			entry->search_match = nmatches + 1;
			entry->match_left = matches[0].rm_so;
			entry->match_left += escape_unreadableo(name, matches[0].rm_so);
			entry->match_right = matches[0].rm_eo;
			entry->match_right += escape_unreadableo(name, matches[0].rm_eo);
			if(cfg.hl_search)
			{
				entry->selected = 1;
				++view->selected_files;
			}
			++nmatches;

			free(free_this);
		}
		regfree(&re);
	}
	else
	{
		if(print_errors)
		{
			ui_sb_errf("Regexp error: %s", get_regexp_error(err, &re));
		}
		regfree(&re);
		return -1;
	}

	other = (view == &lwin) ? &rwin : &lwin;
	if(other->matches != 0 && strcmp(other->last_search, pattern) != 0)
	{
		other->last_search[0] = '\0';
		ui_view_reset_search_highlight(other);
	}
	view->matches = nmatches;
	copy_str(view->last_search, sizeof(view->last_search), pattern);

	view->matches = nmatches;
	if(nmatches > 0)
	{
		const int was_found = move ? goto_search_match(view, backward) : 1;
		*found = was_found;

		if(!cfg.hl_search)
		{
			if(print_errors)
			{
				print_result(view, was_found, backward);
			}
			return 1;
		}
		return 0;
	}
	else
	{
		if(print_errors)
		{
			print_search_fail_msg(view, backward);
		}
		return 1;
	}
}

/* Prints success or error message, determined by the found argument, about
 * search results to a user. */
static void
print_result(const view_t *view, int found, int backward)
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
print_search_msg(const view_t *view, int backward)
{
	if(view->matches == 0)
	{
		print_search_fail_msg(view, backward);
	}
	else
	{
		ui_sb_msgf("%d of %d matching file%s for: %s",
				get_current_entry(view)->search_match, view->matches,
				(view->matches == 1) ? "" : "s", hists_search_last());
	}
}

void
print_search_next_msg(const view_t *view, int backward)
{
	const int match_number = get_current_entry(view)->search_match;
	const char search_type = backward ? '?' : '/';
	ui_sb_msgf("(%d of %d) %c%s", match_number, view->matches, search_type,
			hists_search_last());
}

void
print_search_fail_msg(const view_t *view, int backward)
{
	const char *const regexp = hists_search_last();

	int cflags;
	regex_t re;
	int err;

	if(regexp[0] == '\0')
	{
		ui_sb_clear();
		return;
	}

	cflags = get_regexp_cflags(regexp);
	err = regexp_compile(&re, regexp, cflags);

	if(err != 0)
	{
		ui_sb_errf("Regexp (%s) error: %s", regexp, get_regexp_error(err, &re));
		regfree(&re);
		return;
	}

	regfree(&re);

	if(cfg.wrap_scan)
	{
		ui_sb_errf("No matching files for: %s", regexp);
	}
	else if(backward)
	{
		ui_sb_errf("Search hit TOP without match for: %s", regexp);
	}
	else
	{
		ui_sb_errf("Search hit BOTTOM without match for: %s", regexp);
	}
}

void
reset_search_results(view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].search_match = 0;
	}
	view->matches = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
