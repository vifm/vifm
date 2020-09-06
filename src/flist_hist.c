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
#include "compat/reallocarray.h"
#include "ui/fileview.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "filelist.h"
#include "flist_pos.h"

static void navigate_to_history_pos(view_t *view, int pos);
static void free_view_history(view_t *view);
static void reduce_view_history(view_t *view, int new_size);
static void free_view_history_items(const history_t history[], size_t len);
static int find_in_hist(const view_t *view, const view_t *source, int *pos,
		int *rel_pos);
static history_t * find_hist_entry(const view_t *view, const char dir[]);

void
flist_hist_go_back(view_t *view)
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
flist_hist_go_forward(view_t *view)
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
navigate_to_history_pos(view_t *view, int pos)
{
	curr_stats.drop_new_dir_hist = 1;
	if(change_directory(view, view->history[pos].dir) < 0)
	{
		curr_stats.drop_new_dir_hist = 0;
		return;
	}
	curr_stats.drop_new_dir_hist = 0;

	load_dir_list(view, 0);
	fpos_set_pos(view, fpos_find_by_name(view, view->history[pos].file));

	view->history_pos = pos;
}

void
flist_hist_resize(view_t *view, int new_size)
{
	const int old_size = MAX(cfg.history_len, 0);
	const int delta = new_size - old_size;

	if(new_size <= 0)
	{
		free_view_history(view);
		return;
	}

	if(delta < 0)
	{
		reduce_view_history(view, new_size);
	}

	if(delta > 0 && view->history_num >= new_size)
	{
		/* We must be restarting, don't truncate history. */
		return;
	}

	view->history = reallocarray(view->history, new_size, sizeof(history_t));

	if(delta > 0)
	{
		int real_delta = new_size - MAX(view->history_num, old_size);
		if(real_delta > 0)
		{
			size_t hist_item_len = sizeof(history_t)*real_delta;
			memset(view->history + view->history_num, 0, hist_item_len);
		}
	}
}

/* Clears and frees directory history of the view. */
static void
free_view_history(view_t *view)
{
	free_view_history_items(view->history, view->history_num);
	free(view->history);
	view->history = NULL;

	view->history_num = 0;
	view->history_pos = 0;
}

/* Moves items of directory history when size of history becomes smaller. */
static void
reduce_view_history(view_t *view, int new_size)
{
	const int delta = MIN(view->history_num - new_size, view->history_pos);
	if(delta <= 0)
	{
		return;
	}

	free_view_history_items(view->history, delta);
	memmove(view->history, view->history + delta,
			sizeof(history_t)*(view->history_num - delta));

	if(view->history_num > new_size)
	{
		view->history_num = new_size;
	}
	view->history_pos -= delta;
}

void
flist_hist_save(view_t *view)
{
	flist_hist_setup(view, NULL, NULL, -1, -1);
}

void
flist_hist_setup(view_t *view, const char path[], const char file[],
		int rel_pos, time_t timestamp)
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
			free_view_history_items(&view->history[x--], 1);
		}
		view->history_num = view->history_pos + 1;
	}
	x = view->history_num;

	/* Directory history can exceed cfg.history_len during restart. */
	if(x >= cfg.history_len)
	{
		int surplus = x - cfg.history_len + 1;
		free_view_history_items(view->history, surplus);
		memmove(view->history, view->history + surplus,
				sizeof(history_t)*(cfg.history_len - 1));

		x = cfg.history_len - 1;
		view->history_num = x;
	}
	view->history[x].dir = strdup(path);
	view->history[x].file = strdup(file);
	view->history[x].timestamp = timestamp;
	view->history[x].rel_pos = rel_pos;
	++view->history_num;
	view->history_pos = view->history_num - 1;
}

/* Frees memory previously allocated for specified history items. */
static void
free_view_history_items(const history_t history[], size_t len)
{
	size_t i;
	for(i = 0; i < len; ++i)
	{
		free(history[i].dir);
		free(history[i].file);
	}
}

void
flist_hist_clear(view_t *view)
{
	int i;
	for(i = 0; i <= view->history_pos && i < view->history_num; ++i)
	{
		view->history[i].file[0] = '\0';
	}
}

void
flist_hist_lookup(view_t *view, const view_t *source)
{
	int pos = 0;
	int rel_pos = -1;

	if(cfg.history_len > 0 && source->history_num > 0 && curr_stats.ch_pos)
	{
		if(!find_in_hist(view, source, &pos, &rel_pos))
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
		view->top_line = pos - MIN(view->window_cells - 1, rel_pos);
		if(view->top_line < 0)
			view->top_line = 0;
		view->curr_line = pos - view->top_line;
	}
	else
	{
		const int last = fpos_get_last_visible_cell(view);
		if(view->list_pos < view->window_cells)
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

int
flist_hist_find(const view_t *view, entries_t entries, const char dir[],
		int *top)
{
	int pos;

	const history_t *const hist_entry = find_hist_entry(view, dir);
	if(hist_entry == NULL)
	{
		*top = 0;
		return 0;
	}

	for(pos = 0; pos < entries.nentries; ++pos)
	{
		if(stroscmp(entries.entries[pos].name, hist_entry->file) == 0)
		{
			break;
		}
	}
	if(pos >= entries.nentries)
	{
		pos = 0;
	}

	*top = pos - MIN(entries.nentries, hist_entry->rel_pos);
	if(*top < 0)
	{
		*top = 0;
	}

	return pos;
}

/* Searches for current directory of the view in the history of source.  Returns
 * non-zero if something was found, otherwise zero is returned.  On success,
 * *pos and *rel_pos are set, but might be negative if they aren't valid when
 * applied to existing list of files. */
static int
find_in_hist(const view_t *view, const view_t *source, int *pos, int *rel_pos)
{
	const history_t *const hist_entry = find_hist_entry(source, view->curr_dir);
	if(hist_entry != NULL)
	{
		*pos = fpos_find_by_name(view, hist_entry->file);
		*rel_pos = hist_entry->rel_pos;
		return 1;
	}

	if(view->last_dir != NULL &&
			path_starts_with(view->last_dir, view->curr_dir) &&
			stroscmp(view->last_dir, view->curr_dir) != 0 &&
			strchr(view->last_dir + strlen(view->curr_dir) + 1, '/') == NULL)
	{
		/* This handles positioning of cursor on directory we just left by doing
		 * `cd ..` or equivalent. */

		const char *const dir_name = view->last_dir + strlen(view->curr_dir) + 1U;
		*pos = fpos_find_by_name(view, dir_name);
		*rel_pos = -1;
		return 1;
	}

	return 0;
}

void
flist_hist_update(view_t *view, const char dir[], const char file[],
		int rel_pos)
{
	history_t *const hist_entry = find_hist_entry(view, dir);
	if(hist_entry != NULL)
	{
		(void)replace_string(&hist_entry->file, file);
		hist_entry->rel_pos = rel_pos;
	}
}

/* Finds entry in view history by directory path.  Returns pointer to the entry
 * or NULL. */
static history_t *
find_hist_entry(const view_t *view, const char dir[])
{
	history_t *const history = view->history;
	int i = view->history_pos;

	if(view->history_num <= 0)
	{
		return NULL;
	}

	if(stroscmp(history[i].dir, dir) == 0 && history[i].file[0] == '\0')
	{
		--i;
	}

	for(; i >= 0 && history[i].dir[0] != '\0'; --i)
	{
		if(stroscmp(history[i].dir, dir) == 0)
		{
			return &history[i];
		}
	}

	return NULL;
}

void
flist_hist_clone(view_t *dst, const view_t *src)
{
	int i;

	free_view_history_items(dst->history, dst->history_num);
	dst->history_pos = 0;
	dst->history_num = 0;

	for(i = 0; i < src->history_num; ++i)
	{
		const history_t *const hist_entry = &src->history[i];
		flist_hist_setup(dst, hist_entry->dir, hist_entry->file,
				hist_entry->rel_pos, hist_entry->timestamp);
	}

	dst->history_pos = MIN(src->history_pos, dst->history_num - 1);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
