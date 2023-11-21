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
#include "engine/mode.h"
#include "modes/modes.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "filelist.h"
#include "flist_sel.h"
#include "status.h"

static int find_match(view_t *view, int start, int backward);

int
search_find(view_t *view, const char pattern[], int backward,
		int stash_selection, int select_matches, int count,
		move_cursor_and_redraw_cb cb, int print_msg, int *found)
{
	int save_msg = 0;

	if(search_pattern(view, pattern, stash_selection, select_matches) != 0)
	{
		*found = 0;
		if(print_msg)
		{
			print_search_fail_msg(view, backward);
			save_msg = 1;
			return save_msg;
		}
		/* If we're not printing messages, we might be interested in broken
		 * pattern. */
		return -1;
	}

	*found = goto_search_match(view, backward, count, cb);

	if(print_msg)
	{
		save_msg = print_search_result(view, *found, backward, &print_search_msg);
	}

	return save_msg;
}

int
search_next(view_t *view, int backward, int stash_selection, int select_matches,
		int count, move_cursor_and_redraw_cb cb)
{
	int save_msg = 0;
	int found;

	if(hist_is_empty(&curr_stats.search_hist))
	{
		return save_msg;
	}

	if(view->matches == 0)
	{
		const char *const pattern = hists_search_last();
		if(search_pattern(view, pattern, stash_selection, select_matches) != 0)
		{
			print_search_fail_msg(view, backward);
			save_msg = 1;
			return save_msg;
		}
	}

	found = goto_search_match(view, backward, count, cb);

	save_msg = print_search_result(view, found, backward, &print_search_next_msg);

	return save_msg;
}

int
goto_search_match(view_t *view, int backward, int count,
		move_cursor_and_redraw_cb cb)
{
	const int i = find_search_match(view, backward, count);
	if(i == -1)
	{
		return 0;
	}

	cb(i);

	return 1;
}

int
find_search_match(view_t *view, int backward, int count)
{
	assert(count > 0 && "Zero searches.");

	int c, i = view->list_pos;
	for(c = 0; c < count; ++c)
 	{
		i = find_match(view, i, backward);
		if(i == -1)
		{
			if(cfg.wrap_scan)
			{
				const int wrap_start = backward ? view->list_rows : -1;
				i = find_match(view, wrap_start, backward);
				if(i == -1)
				{
					return -1;
				}
			}
			else
			{
				return -1;
			}
		}
 	}
	return i;
}

/* Looks for a search match in specified direction from given start position.
 * Starting position is not included in searched range.  Returns index of a
 * match, or -1 if no matches were found. */
static int
find_match(view_t *view, int start, int backward)
{
	int i = -1;
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
		if(view->list_rows == 0)
		{
			return -1;
		}

		begin = start + 1;
		end = view->list_rows;
		step = 1;

		assert(begin <= end && "Wrong range.");
	}

	for(i = begin; i != end; i += step)
	{
		if(view->dir_entry[i].search_match)
		{
			return i;
		}
	}

	return -1;
}

int
search_pattern(view_t *view, const char pattern[], int stash_selection,
		int select_matches)
{
	int cflags;
	int nmatches = 0;
	regex_t re;
	int err = 0;
	view_t *other;

	if(stash_selection)
	{
		flist_sel_stash(view);
	}

	reset_search_results(view);

	/* We at least could wipe out previous search results, so schedule a
	 * redraw. */
	ui_view_schedule_redraw(view);

	if(pattern[0] == '\0')
	{
		return err;
	}

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
			if(select_matches)
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
		regfree(&re);
		return err;
	}

	other = (view == &lwin) ? &rwin : &lwin;
	if(other->matches != 0 && strcmp(other->last_search, pattern) != 0)
	{
		other->last_search[0] = '\0';
		ui_view_reset_search_highlight(other);
	}
	view->matches = nmatches;
	copy_str(view->last_search, sizeof(view->last_search), pattern);

	return err;
}

int
print_search_result(const view_t *view, int found, int backward,
		print_search_msg_cb cb)
{
	if(view->matches > 0)
	{
		/* Print a message in all cases except for 'hlsearch nowrapscan' with no
		 * matches in non-visual mode to not supersede the "n files selected"
		 * message for possibly hidden selected files (the message is printed
		 * automatically). */
		if(found)
		{
			cb(view, backward);
			return 1;
		}
		else if(!cfg.hl_search || cfg.wrap_scan || vle_mode_is(VISUAL_MODE) ||
				vle_primary_mode_is(VISUAL_MODE))
		{
			print_search_fail_msg(view, backward);
			return 1;
		}
		return 0;
	}
	else
	{
		print_search_fail_msg(view, backward);
		return 1;
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
