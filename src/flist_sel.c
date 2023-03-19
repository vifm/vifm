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

#include "flist_sel.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* strdup() */

#include "cfg/config.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/matchers.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/trie.h"
#include "utils/utils.h"
#include "filelist.h"
#include "flist_pos.h"
#include "registers.h"
#include "running.h"

static void save_selection(view_t *view);
static void free_saved_selection(view_t *view);
static void select_unselect_entry(view_t *view, dir_entry_t *entry, int select);

void
flist_sel_stash(view_t *view)
{
	save_selection(view);
	flist_sel_drop(view);
}

void
flist_sel_drop(view_t *view)
{
	int i;
	int need_redraw = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		need_redraw |= view->dir_entry[i].selected;
		view->dir_entry[i].selected = 0;
	}
	view->selected_files = 0;

	if(need_redraw)
	{
		ui_view_schedule_redraw(view);
	}
}

void
flist_sel_view_to_reload(view_t *view, const char new_dir[])
{
	save_selection(view);

	if(new_dir != NULL)
	{
		selhist_put(view->curr_dir, view->saved_selection, view->nsaved_selection);
		view->nsaved_selection = 0;
		view->saved_selection = NULL;

		selhist_get(new_dir, &view->saved_selection, &view->nsaved_selection);
	}

	flist_sel_drop(view);
}

/* Collects currently selected files in view->saved_selection array.  Use
 * free_saved_selection() to clean up memory allocated by this function. */
static void
save_selection(view_t *view)
{
	int i;
	dir_entry_t *entry;

	flist_sel_recount(view);
	if(view->selected_files == 0)
	{
		return;
	}

	free_saved_selection(view);

	view->saved_selection = calloc(view->selected_files, sizeof(char *));
	if(view->saved_selection == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	i = 0;
	entry = NULL;
	while(iter_selected_entries(view, &entry))
	{
		char full_path[PATH_MAX + 1];

		get_full_path_of(entry, sizeof(full_path), full_path);
		view->saved_selection[i] = strdup(full_path);
		if(view->saved_selection[i] == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			break;
		}

		++i;
	}
	view->nsaved_selection = i;
}

/* Frees list of previously selected files. */
static void
free_saved_selection(view_t *view)
{
	free_string_array(view->saved_selection, view->nsaved_selection);
	view->nsaved_selection = 0;
	view->saved_selection = NULL;
}

void
flist_sel_invert(view_t *view)
{
	int i;
	view->selected_files = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		dir_entry_t *const e = &view->dir_entry[i];
		if(fentry_is_valid(e))
		{
			e->selected = !e->selected;
		}
		view->selected_files += (e->selected != 0);
	}
}

void
flist_sel_stash_if_nonempty(view_t *view)
{
	if(view->selected_files != 0)
	{
		flist_sel_stash(view);
	}
}

void
flist_sel_restore(view_t *view, const reg_t *reg)
{
	int i;
	trie_t *const selection_trie = trie_create(/*free_func=*/NULL);

	flist_sel_drop(view);

	if(reg == NULL)
	{
		int i;
		for(i = 0; i < view->nsaved_selection; ++i)
		{
			(void)trie_put(selection_trie, view->saved_selection[i]);
		}
	}
	else
	{
		int i;
		for(i = 0; i < reg->nfiles; ++i)
		{
			(void)trie_put(selection_trie, reg->files[i]);
		}
	}

	for(i = 0; i < view->list_rows; ++i)
	{
		char full_path[PATH_MAX + 1];
		void *ignored_data;
		dir_entry_t *const entry = &view->dir_entry[i];

		get_full_path_of(entry, sizeof(full_path), full_path);
		if(trie_get(selection_trie, full_path, &ignored_data) == 0)
		{
			entry->selected = 1;
			++view->selected_files;

			/* Assuming that selection is usually contiguous it makes sense to quit
			 * when we found all elements to optimize this operation. */
			if(view->selected_files == view->nsaved_selection)
			{
				break;
			}
		}
	}

	trie_free(selection_trie);

	redraw_current_view();
}

void
flist_sel_recount(view_t *view)
{
	int i;

	view->selected_files = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->selected_files += (view->dir_entry[i].selected != 0);
	}
}

void
flist_sel_by_range(view_t *view, int begin, int end, int select)
{
	select = (select != 0);

	if(begin < 0)
	{
		select_unselect_entry(view, get_current_entry(view), select);
	}
	else
	{
		int i;
		for(i = begin; i <= end; ++i)
		{
			select_unselect_entry(view, &view->dir_entry[i], select);
		}
	}

	ui_view_schedule_redraw(view);
}

void
flist_sel_by_indexes(view_t *view, int count, const int indexes[], int select)
{
	int i;
	for(i = 0; i < count; ++i)
	{
		int idx = indexes[i];
		dir_entry_t *const entry = &view->dir_entry[idx];
		select_unselect_entry(view, entry, select);
	}

	ui_view_schedule_redraw(view);
}

/* Selects or unselects the entry. */
static void
select_unselect_entry(view_t *view, dir_entry_t *entry, int select)
{
	if(fentry_is_valid(entry) && (entry->selected != 0) != select)
	{
		entry->selected = select;
		view->selected_files += (select ? 1 : -1);
	}
}

int
flist_sel_by_filter(view_t *view, const char cmd[], int erase_old, int select)
{
	trie_t *selection_trie;
	char **files;
	int nfiles;
	int i;

	MacroFlags flags;
	char *const expanded_cmd = ma_expand(cmd, NULL, &flags, MER_SHELL_OP);

	if(rn_for_lines(view, expanded_cmd, &files, &nfiles, flags) != 0)
	{
		free(expanded_cmd);
		ui_sb_err("Failed to start/read output of external command");
		return 1;
	}
	free(expanded_cmd);

	/* Append to previous selection unless ! is specified. */
	if(select && erase_old)
	{
		flist_sel_drop(view);
	}

	if(nfiles == 0)
	{
		free_string_array(files, nfiles);
		return 0;
	}

	/* Compose trie out of absolute paths of files to [un]select. */
	selection_trie = trie_create(/*free_func=*/NULL);
	for(i = 0; i < nfiles; ++i)
	{
		char *const path = parse_line_for_path(files[i], flist_get_dir(view));
		if(path != NULL)
		{
			(void)trie_put(selection_trie, path);
			free(path);
		}
	}
	free_string_array(files, nfiles);

	/* [un]select files that match the list. */
	select = (select != 0);
	for(i = 0; i < view->list_rows; ++i)
	{
		char full_path[PATH_MAX + 1];
		void *ignored_data;
		dir_entry_t *const entry = &view->dir_entry[i];

		if((entry->selected != 0) == select)
		{
			continue;
		}

		get_full_path_of(entry, sizeof(full_path), full_path);
		if(trie_get(selection_trie, full_path, &ignored_data) != 0)
		{
			if(!fentry_is_dir(entry))
			{
				continue;
			}

			/* Allow trailing slash for directory names. */
			strcat(full_path, "/");
			if(trie_get(selection_trie, full_path, &ignored_data) != 0)
			{
				continue;
			}
		}

		entry->selected = select;
		view->selected_files += (select ? 1 : -1);
	}

	trie_free(selection_trie);

	ui_view_schedule_redraw(view);
	return 0;
}

int
flist_sel_by_pattern(view_t *view, const char pattern[], int erase_old,
		int select)
{
	int i;
	char *error;
	matchers_t *const ms = matchers_alloc(pattern, 0, 1, hists_search_last(),
			&error);
	if(ms == NULL)
	{
		ui_sb_errf("Pattern error: %s", error);
		free(error);
		return 1;
	}

	select = (select != 0);

	/* Append to previous selection unless ! is specified. */
	if(select && erase_old)
	{
		flist_sel_drop(view);
	}

	for(i = 0; i < view->list_rows; ++i)
	{
		char file_path[PATH_MAX + 1];
		dir_entry_t *const entry = &view->dir_entry[i];

		if((entry->selected != 0) == select)
		{
			continue;
		}

		get_full_path_of(entry, sizeof(file_path) - 1U, file_path);

		if(matchers_match(ms, file_path) ||
				(fentry_is_dir(entry) && matchers_match_dir(ms, file_path)))
		{
			entry->selected = select;
			view->selected_files += (select ? 1 : -1);
		}
	}

	matchers_free(ms);

	ui_view_schedule_redraw(view);
	return 0;
}

void
flist_sel_count(view_t *view, int at, int count)
{
	/* Use current position if none given. */
	if(at < 0)
	{
		at = view->list_pos;
	}

	flist_sel_stash(view);

	while(count-- > 0 && at < view->list_rows)
	{
		if(fentry_is_valid(&view->dir_entry[at]))
		{
			view->dir_entry[at].selected = 1;
			++view->selected_files;
		}
		++at;
	}
}

int
flist_sel_range(view_t *view, int begin, int end, int mark_current)
{
	/* Range was specified for a command. */
	if(begin > -1)
	{
		int i;
		clear_marking(view);
		for(i = begin; i <= end; ++i)
		{
			if(fentry_is_valid(&view->dir_entry[i]))
			{
				view->dir_entry[i].marked = 1;
			}
		}
		return 1;
	}

	/* Use user's selection. */
	if(view->selected_files != 0)
	{
		return 0;
	}

	clear_marking(view);

	/* XXX: is it possible that begin <= -1 and end > -1? */
	if(end > -1)
	{
		if(fentry_is_valid(&view->dir_entry[end]))
		{
			view->dir_entry[end].marked = 1;
		}
	}
	else if(mark_current)
	{
		/* The front check is for tests. */
		if(view->list_pos < view->list_rows &&
				fentry_is_valid(&view->dir_entry[view->list_pos]))
		{
			view->dir_entry[view->list_pos].marked = 1;
		}
	}

	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
