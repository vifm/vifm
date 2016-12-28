/* vifm
 * Copyright (C) 2016 xaizek.
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

#include "flist_hist.h"

#include <string.h> /* memmove() */

#include "cfg/config.h"
#include "ui/fileview.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"
#include "filelist.h"
#include "flist_pos.h"

static void navigate_to_history_pos(FileView *view, int pos);

void
flist_hist_go_back(FileView *view)
{
	/* When in custom view, we don't want to skip top history item. */
	int pos = flist_custom_active(view)
	        ? view->history_pos
	        : (view->history_pos - 1);

	while(pos >= 0)
	{
		const char *const dir = view->history[pos].dir;
		if(is_valid_dir(dir) && !paths_are_equal(view->curr_dir, dir))
		{
			break;
		}

		--pos;
	}

	if(pos >= 0)
	{
		navigate_to_history_pos(view, pos);
	}
}

void
flist_hist_go_forward(FileView *view)
{
	int pos = view->history_pos + 1;

	while(pos <= view->history_num - 1)
	{
		const char *const dir = view->history[pos].dir;
		if(is_valid_dir(dir) && !paths_are_equal(view->curr_dir, dir))
		{
			break;
		}

		++pos;
	}

	if(pos <= view->history_num - 1)
	{
		navigate_to_history_pos(view, pos);
	}
}

/* Changes current directory of the view to one of previously visited
 * locations. */
static void
navigate_to_history_pos(FileView *view, int pos)
{
	curr_stats.drop_new_dir_hist = 1;
	if(change_directory(view, view->history[pos].dir) < 0)
	{
		curr_stats.drop_new_dir_hist = 0;
		return;
	}
	curr_stats.drop_new_dir_hist = 0;

	load_dir_list(view, 0);
	flist_set_pos(view, find_file_pos_in_list(view, view->history[pos].file));

	view->history_pos = pos;
}

void
flist_hist_save(FileView *view, const char path[], const char file[],
		int rel_pos)
{
	int x;

	/* This could happen on FUSE error. */
	if(view->list_rows <= 0 && file == NULL)
		return;

	if(cfg.history_len <= 0)
		return;

	if(flist_custom_active(view))
		return;

	if(path == NULL)
		path = view->curr_dir;
	if(file == NULL)
		file = get_current_entry(view)->name;
	if(rel_pos < 0)
		rel_pos = view->list_pos - view->top_line;

	if(view->history_num > 0 &&
			stroscmp(view->history[view->history_pos].dir, path) == 0)
	{
		if(curr_stats.load_stage < 2 || file[0] == '\0')
			return;
		x = view->history_pos;
		(void)replace_string(&view->history[x].file, file);
		view->history[x].rel_pos = rel_pos;
		return;
	}

	if(curr_stats.drop_new_dir_hist)
	{
		return;
	}

	if(view->history_num > 0 && view->history_pos != view->history_num - 1)
	{
		x = view->history_num - 1;
		while(x > view->history_pos)
		{
			cfg_free_history_items(&view->history[x--], 1);
		}
		view->history_num = view->history_pos + 1;
	}
	x = view->history_num;

	if(x == cfg.history_len)
	{
		cfg_free_history_items(view->history, 1);
		memmove(view->history, view->history + 1,
				sizeof(history_t)*(cfg.history_len - 1));

		--x;
		view->history_num = x;
	}
	view->history[x].dir = strdup(path);
	view->history[x].file = strdup(file);
	view->history[x].rel_pos = rel_pos;
	++view->history_num;
	view->history_pos = view->history_num - 1;
}

int
flist_hist_contains(FileView *view, const char path[])
{
	int i;

	if(view->history == NULL || view->history_num <= 0)
	{
		return 0;
	}

	for(i = view->history_pos; i >= 0; --i)
	{
		if(strlen(view->history[i].dir) < 1)
		{
			break;
		}
		if(stroscmp(view->history[i].dir, path) == 0)
		{
			return 1;
		}
	}
	return 0;
}

void
flist_hist_clear(FileView *view)
{
	int i;
	for(i = 0; i <= view->history_pos && i < view->history_num; ++i)
	{
		view->history[i].file[0] = '\0';
	}
}

void
flist_hist_lookup(FileView *view, const FileView *source)
{
	int pos = 0;
	int rel_pos = -1;

	if(cfg.history_len > 0 && source->history_num > 0 && curr_stats.ch_pos)
	{
		int x;
		int found = 0;
		x = source->history_pos;
		if(stroscmp(source->history[x].dir, view->curr_dir) == 0 &&
				source->history[x].file[0] == '\0')
			x--;
		for(; x >= 0; x--)
		{
			if(source->history[x].dir[0] == '\0')
				break;
			if(stroscmp(source->history[x].dir, view->curr_dir) == 0)
			{
				found = 1;
				break;
			}
		}
		if(found)
		{
			pos = find_file_pos_in_list(view, source->history[x].file);
			rel_pos = source->history[x].rel_pos;
		}
		else if(path_starts_with(view->last_dir, view->curr_dir) &&
				stroscmp(view->last_dir, view->curr_dir) != 0 &&
				strchr(view->last_dir + strlen(view->curr_dir) + 1, '/') == NULL)
		{
			/* This handles positioning of cursor on directory we just left by doing
			 * `cd ..` or equivalent. */

			const char *const dir_name = view->last_dir + strlen(view->curr_dir) + 1U;
			pos = find_file_pos_in_list(view, dir_name);
			rel_pos = -1;
		}
		else
		{
			view->list_pos = 0;
			view->curr_line = 0;
			view->top_line = 0;
			return;
		}
	}

	if(pos < 0)
		pos = 0;
	view->list_pos = pos;
	if(rel_pos >= 0)
	{
		view->top_line = pos - MIN((int)view->window_cells - 1, rel_pos);
		if(view->top_line < 0)
			view->top_line = 0;
		view->curr_line = pos - view->top_line;
	}
	else
	{
		const int last = (int)get_last_visible_cell(view);
		if(view->list_pos < (int)view->window_cells)
		{
			scroll_up(view, view->top_line);
		}
		else if(view->list_pos > last)
		{
			scroll_down(view, view->list_pos - last);
		}
	}
	(void)consider_scroll_offset(view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
