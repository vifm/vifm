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

#include "filelist.h"

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <lm.h>
#include <winioctl.h>
#endif

#include <curses.h>

#include <sys/stat.h> /* stat */

#include <assert.h> /* assert() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* intptr_t uint64_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* memcmp() memcpy() memset() strcat() strcmp() strcpy()
                       strdup() strlen() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "engine/autocmds.h"
#include "engine/mode.h"
#include "int/fuse.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "ui/cancellation.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/statusline.h"
#include "ui/ui.h"
#include "utils/dynarray.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/fsdata.h"
#include "utils/fswatch.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/trie.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "filtering.h"
#include "flist_pos.h"
#include "flist_sel.h"
#include "macros.h"
#include "opt_handlers.h"
#include "registers.h"
#include "running.h"
#include "sort.h"
#include "status.h"
#include "types.h"

static void init_view(FileView *view);
static void init_flist(FileView *view);
static void reset_view(FileView *view);
static void init_view_history(FileView *view);
static void navigate_to_history_pos(FileView *view, int pos);
static int navigate_to_file_in_custom_view(FileView *view, const char dir[],
		const char file[]);
static int fill_dir_entry_by_path(dir_entry_t *entry, const char path[]);
#ifndef _WIN32
static int fill_dir_entry(dir_entry_t *entry, const char path[],
		const struct dirent *d);
static int data_is_dir_entry(const struct dirent *d);
#else
static int fill_dir_entry(dir_entry_t *entry, const char path[],
		const WIN32_FIND_DATAW *ffd);
static int data_is_dir_entry(const WIN32_FIND_DATAW *ffd);
#endif
static int flist_custom_finish_internal(FileView *view, CVType type, int reload,
		const char dir[], int allow_empty);
static void on_location_change(FileView *view, int force);
static void disable_view_sorting(FileView *view);
static void enable_view_sorting(FileView *view);
static void exclude_in_compare(FileView *view, int selection_only);
static void mark_group(FileView *view, FileView *other, int idx);
static int exclude_temporary_entries(FileView *view);
static int is_temporary(FileView *view, const dir_entry_t *entry, void *arg);
static void uncompress_traverser(const char name[], int valid,
		const void *parent_data, void *data, void *arg);
static void load_dir_list_internal(FileView *view, int reload, int draw_only);
static int populate_dir_list_internal(FileView *view, int reload);
static int populate_custom_view(FileView *view, int reload);
static int entry_exists(FileView *view, const dir_entry_t *entry, void *arg);
static void zap_compare_view(FileView *view, FileView *other, zap_filter filter,
		void *arg);
static int find_separator(FileView *view, int idx);
static int update_dir_watcher(FileView *view);
static int custom_list_is_incomplete(const FileView *view);
static int is_dead_or_filtered(FileView *view, const dir_entry_t *entry,
		void *arg);
static void update_entries_data(FileView *view);
static int is_dir_big(const char path[]);
static void free_view_entries(FileView *view);
static int update_dir_list(FileView *view, int reload);
static void start_dir_list_change(FileView *view, dir_entry_t **entries,
		int *len, int reload);
static void finish_dir_list_change(FileView *view, dir_entry_t *entries,
		int len);
static int add_file_entry_to_view(const char name[], const void *data,
		void *param);
static void sort_dir_list(int msg, FileView *view);
static void merge_lists(FileView *view, dir_entry_t *entries, int len);
static void add_to_trie(trie_t *trie, FileView *view, dir_entry_t *entry);
static int is_in_trie(trie_t *trie, FileView *view, dir_entry_t *entry,
		void **data);
static void merge_entries(dir_entry_t *new, const dir_entry_t *prev);
static int correct_pos(FileView *view, int pos, int dist, int closest);
static int rescue_from_empty_filelist(FileView *view);
static void init_dir_entry(FileView *view, dir_entry_t *entry,
		const char name[]);
static dir_entry_t * alloc_dir_entry(dir_entry_t **list, int list_size);
static int tree_has_changed(const dir_entry_t *entries, size_t nchildren);
TSTATIC void pick_cd_path(FileView *view, const char base_dir[],
		const char path[], int *updir, char buf[], size_t buf_size);
static void find_dir_in_cdpath(const char base_dir[], const char dst[],
		char buf[], size_t buf_size);
static int iter_entries(FileView *view, dir_entry_t **entry,
		entry_predicate pred);
static void clear_marking(FileView *view);
static int flist_load_tree_internal(FileView *view, const char path[],
		int reload);
static int make_tree(FileView *view, const char path[], int reload,
		trie_t *excluded_paths);
static int add_files_recursively(FileView *view, const char path[],
		trie_t *excluded_paths, int parent_pos, int no_direct_parent);
static int file_is_visible(FileView *view, const char filename[], int is_dir,
		const void *data, int apply_local_filter);
static int add_directory_leaf(FileView *view, const char path[],
		int parent_pos);
static int init_parent_entry(FileView *view, dir_entry_t *entry,
		const char path[]);

void
init_filelists(void)
{
	fview_init();

	init_view(&rwin);
	init_view(&lwin);

	curr_view = &lwin;
	other_view = &rwin;
}

/* Loads initial display values into view structure. */
static void
init_view(FileView *view)
{
	init_flist(view);
	fview_view_init(view);

	reset_view(view);

	init_view_history(view);
	fview_sorting_updated(view);
}

/* Initializes file list part of the view. */
static void
init_flist(FileView *view)
{
	view->list_pos = 0;
	view->history_num = 0;
	view->history_pos = 0;
	view->on_slow_fs = 0;

	view->hide_dot = 1;
	view->matches = 0;

	view->custom.entries = NULL;
	view->custom.entry_count = 0;
	view->custom.orig_dir = NULL;
	view->custom.title = NULL;

	/* Load fake empty element to make dir_entry valid. */
	view->dir_entry = dynarray_extend(NULL, sizeof(dir_entry_t));
	memset(view->dir_entry, 0, sizeof(*view->dir_entry));
	view->dir_entry[0].name = strdup("");
	view->dir_entry[0].type = FT_DIR;
	view->dir_entry[0].hi_num = -1;
	view->dir_entry[0].name_dec_num = -1;
	view->dir_entry[0].origin = &view->curr_dir[0];
	view->list_rows = 1;
}

void
reset_views(void)
{
	reset_view(&lwin);
	reset_view(&rwin);
}

/* Loads some of view parameters that should be restored on configuration
 * reloading (e.g. on :restart command). */
static void
reset_view(FileView *view)
{
	fview_view_reset(view);
	filters_view_reset(view);

	memset(&view->sort[0], SK_NONE, sizeof(view->sort));
	ui_view_sort_list_ensure_well_formed(view, view->sort);
	memcpy(&view->sort_g[0], &view->sort[0], sizeof(view->sort_g));
}

/* Allocates memory for view history smartly (handles huge values). */
static void
init_view_history(FileView *view)
{
	if(cfg.history_len == 0)
		return;

	view->history = calloc(cfg.history_len, sizeof(history_t));
	while(view->history == NULL)
	{
		cfg.history_len /= 2;
		view->history = calloc(cfg.history_len, sizeof(history_t));
	}
}

void
load_initial_directory(FileView *view, const char dir[])
{
	/* Use current working directory as original location for custom views loaded
	 * from command-line via "-". */
	if(view->curr_dir[0] == '\0' || strcmp(view->curr_dir, "-") == 0)
	{
		copy_str(view->curr_dir, sizeof(view->curr_dir), dir);
	}
	else
	{
		dir = view->curr_dir;
	}

	if(!is_root_dir(view->curr_dir))
	{
		chosp(view->curr_dir);
	}
	if(change_directory(view, dir) < 0)
	{
		leave_invalid_dir(view);
		(void)change_directory(view, view->curr_dir);
	}
}

dir_entry_t *
get_current_entry(const FileView *view)
{
	if(view->list_pos < 0 || view->list_pos >= view->list_rows)
	{
		return NULL;
	}

	return &view->dir_entry[view->list_pos];
}

char *
get_current_file_name(FileView *view)
{
	if(view->list_pos == -1)
	{
		static char empty_string[1];
		return empty_string;
	}
	return get_current_entry(view)->name;
}

void
invert_sorting_order(FileView *view)
{
	if(memcmp(&view->sort_g[0], &view->sort[0], sizeof(view->sort_g)) == 0)
	{
		view->sort_g[0] = -view->sort_g[0];
	}
	view->sort[0] = -view->sort[0];
}

void
navigate_backward_in_history(FileView *view)
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
navigate_forward_in_history(FileView *view)
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
flist_hist_clear(FileView *view)
{
	int i;
	for(i = 0; i <= view->history_pos && i < view->history_num; ++i)
	{
		view->history[i].file[0] = '\0';
	}
}

void
save_view_history(FileView *view, const char path[], const char file[], int pos)
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
	if(pos < 0)
		pos = view->list_pos;

	if(view->history_num > 0 &&
			stroscmp(view->history[view->history_pos].dir, path) == 0)
	{
		if(curr_stats.load_stage < 2 || file[0] == '\0')
			return;
		x = view->history_pos;
		(void)replace_string(&view->history[x].file, file);
		view->history[x].rel_pos = pos - view->top_line;
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
			view->history[x--].dir[0] = '\0';
		view->history_num = view->history_pos + 1;
	}
	x = view->history_num;

	if(x == cfg.history_len)
	{
		cfg_free_history_items(view->history, 1);
		memmove(view->history, view->history + 1,
				sizeof(history_t)*(cfg.history_len - 1));

		x--;
		view->history_num = x;
	}
	view->history[x].dir = strdup(path);
	view->history[x].file = strdup(file);
	view->history[x].rel_pos = pos - view->top_line;
	view->history_num++;
	view->history_pos = view->history_num - 1;
}

int
is_in_view_history(FileView *view, const char *path)
{
	int i;
	if(view->history == NULL || view->history_num <= 0)
		return 0;
	for(i = view->history_pos; i >= 0; i--)
	{
		if(strlen(view->history[i].dir) < 1)
			break;
		if(stroscmp(view->history[i].dir, path) == 0)
			return 1;
	}
	return 0;
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

void
leave_invalid_dir(FileView *view)
{
	struct stat s;
	char *const path = view->curr_dir;

	if(fuse_try_updir_from_a_mount(path, view))
	{
		return;
	}

	/* Use stat() directly to skip all possible optimizations.  It might be a bit
	 * heavier, but this is quite rare error-recovery procedure, we can afford it
	 * here.  Even in the worst case of going to the root, this isn't that much
	 * calls. */
	while((os_stat(path, &s) != 0 || !(s.st_mode & S_IFDIR) ||
			!directory_accessible(path)) && is_path_well_formed(path))
	{
		if(fuse_try_updir_from_a_mount(path, view))
		{
			break;
		}

		remove_last_path_component(path);
	}

	ensure_path_well_formed(path);
}

void
navigate_to(FileView *view, const char path[])
{
	if(change_directory(view, path) >= 0)
	{
		load_dir_list(view, 0);
		fview_cursor_redraw(view);
	}
}

void
navigate_back(FileView *view)
{
	const char *const dest = flist_custom_active(view)
	                       ? view->custom.orig_dir
	                       : view->last_dir;
	navigate_to(view, dest);
}

void
navigate_to_file(FileView *view, const char dir[], const char file[],
		int preserve_cv)
{
	if(flist_custom_active(view) && preserve_cv)
	{
		/* Try to find requested element in custom list of files. */
		if(navigate_to_file_in_custom_view(view, dir, file) == 0)
		{
			return;
		}
	}

	/* Do not change directory if we already there. */
	if(!paths_are_equal(view->curr_dir, dir))
	{
		if(change_directory(view, dir) >= 0)
		{
			load_dir_list(view, 0);
		}
	}

	if(paths_are_equal(view->curr_dir, dir))
	{
		(void)ensure_file_is_selected(view, file);
	}
}

/* navigate_to_file() helper that tries to find requested file in custom view.
 * Returns non-zero on failure to find such file, otherwise zero is returned. */
static int
navigate_to_file_in_custom_view(FileView *view, const char dir[],
		const char file[])
{
	char full_path[PATH_MAX];
	dir_entry_t *entry;

	snprintf(full_path, sizeof(full_path), "%s/%s", dir, file);

	if(custom_list_is_incomplete(view))
	{
		entry = entry_from_path(view->local_filter.entries,
				view->local_filter.entry_count, full_path);
		if(entry == NULL)
		{
			/* No such entry in the view at all. */
			return 1;
		}

		if(!local_filter_matches(view, entry))
		{
			/* The item is filtered-out by current settings, undo this filtering. */
			local_filter_remove(view);
			load_dir_list(view, 1);
		}
	}

	entry = entry_from_path(view->dir_entry, view->list_rows, full_path);
	if(entry == NULL)
	{
		/* File might not exist anymore at that location. */
		return 1;
	}

	view->list_pos = entry_to_pos(view, entry);
	ui_view_schedule_redraw(view);
	return 0;
}

int
change_directory(FileView *view, const char directory[])
{
	/* TODO: refactor this big function change_directory(). */

	char dir_dup[PATH_MAX];
	const int was_in_custom_view = flist_custom_active(view);
	int location_changed;

	if(is_dir_list_loaded(view))
	{
		save_view_history(view, NULL, NULL, -1);
	}

	if(is_path_absolute(directory))
	{
		canonicalize_path(directory, dir_dup, sizeof(dir_dup));
	}
	else
	{
		char newdir[PATH_MAX];
		const char *const dir = flist_get_dir(view);
#ifdef _WIN32
		if(directory[0] == '/')
		{
			snprintf(newdir, sizeof(newdir), "%c:%s", dir[0], directory);
		}
		else
#endif
		{
			snprintf(newdir, sizeof(newdir), "%s/%s", dir, directory);
		}
		canonicalize_path(newdir, dir_dup, sizeof(dir_dup));
	}

#ifdef _WIN32
	to_forward_slash(dir_dup);
#endif

	if(cfg.chase_links)
	{
		char real_path[PATH_MAX];
		if(os_realpath(dir_dup, real_path) == real_path)
		{
			/* Do this on success only, if realpath() fails, just go with the original
			 * path. */
			canonicalize_path(real_path, dir_dup, sizeof(dir_dup));
#ifdef _WIN32
			to_forward_slash(real_path);
#endif
		}
	}

	if(!is_root_dir(dir_dup))
		chosp(dir_dup);

	if(!is_valid_dir(dir_dup))
	{
		show_error_msgf("Directory Access Error", "Cannot open %s", dir_dup);
		copy_str(view->curr_dir, sizeof(view->curr_dir), dir_dup);
		leave_invalid_dir(view);
		copy_str(dir_dup, sizeof(dir_dup), view->curr_dir);
	}

	location_changed = (stroscmp(dir_dup, flist_get_dir(view)) != 0);

	if(location_changed)
	{
		copy_str(view->last_dir, sizeof(view->last_dir), view->curr_dir);
		view->on_slow_fs = is_on_slow_fs(view->curr_dir, cfg.slow_fs_list);
	}

	/* Check if we're exiting from a FUSE mounted top level directory and the
	 * other pane isn't in it or any of it subdirectories.
	 * If so, unmount & let FUSE serialize */
	if(is_parent_dir(directory) && fuse_is_mount_point(view->curr_dir))
	{
		FileView *other = (view == curr_view) ? other_view : curr_view;
		if(!path_starts_with(other->curr_dir, view->curr_dir))
		{
			const int r = fuse_try_unmount(view);
			if(r != 0)
			{
				return r;
			}
		}
		else if(fuse_try_updir_from_a_mount(view->curr_dir, view))
		{
			/* On success fuse_try_updir_from_a_mount() calls change_directory()
			 * recursively to change directory, so we can just leave. */
			return 1;
		}
	}

	/* Clean up any excess separators */
	if(!is_root_dir(view->curr_dir))
		chosp(view->curr_dir);
	if(!is_root_dir(view->last_dir))
		chosp(view->last_dir);

#ifndef _WIN32
	if(!path_exists(dir_dup, DEREF))
#else
	if(!is_valid_dir(dir_dup))
#endif
	{
		LOG_SERROR_MSG(errno, "Can't access \"%s\"", dir_dup);
		log_cwd();

		show_error_msgf("Directory Access Error", "Cannot open %s", dir_dup);

		flist_sel_stash(view);
		return -1;
	}

	if(os_access(dir_dup, X_OK) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't access(, X_OK) \"%s\"", dir_dup);
		log_cwd();

		show_error_msgf("Directory Access Error",
				"You do not have execute access on %s", dir_dup);

		flist_sel_stash(view);
		return -1;
	}

	if(os_access(dir_dup, R_OK) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't access(, R_OK) \"%s\"", dir_dup);
		log_cwd();

		if(location_changed)
		{
			show_error_msgf("Directory Access Error",
					"You do not have read access on %s", dir_dup);
		}
	}

	if(vifm_chdir(dir_dup) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't chdir() \"%s\"", dir_dup);
		log_cwd();

		show_error_msgf("Change Directory Error", "Couldn't open %s", dir_dup);
		return -1;
	}

	if(!is_root_dir(dir_dup))
		chosp(dir_dup);

	flist_sel_view_reloaded(view, location_changed);

	/* Need to use setenv instead of getcwd for a symlink directory */
	env_set("PWD", dir_dup);

	copy_str(view->curr_dir, sizeof(view->curr_dir), dir_dup);

	if(is_dir_list_loaded(view))
	{
		save_view_history(view, NULL, "", -1);
	}

	/* Perform additional actions on leaving custom view. */
	if(was_in_custom_view)
	{
		if(ui_view_unsorted(view))
		{
			enable_view_sorting(view);
		}
		if(cv_compare(view->custom.type))
		{
			FileView *const other = (view == curr_view) ? other_view : curr_view;

			/* Indicate that this is not a compare view anymore. */
			view->custom.type = CV_REGULAR;

			/* Leave compare mode in both views at the same time. */
			if(other->custom.type == CV_DIFF)
			{
				cd_updir(other, 1);
			}
		}
	}

	if(location_changed || was_in_custom_view)
	{
		on_location_change(view, location_changed);
	}
	return 0;
}

int
is_dir_list_loaded(FileView *view)
{
	dir_entry_t *const entry = (view->list_rows < 1) ? NULL : &view->dir_entry[0];
	return entry != NULL && entry->name[0] != '\0';
}

#ifdef _WIN32
static void
fill_with_shared(FileView *view)
{
	NET_API_STATUS res;
	wchar_t *wserver;

	wserver = to_wide(view->curr_dir + 2);
	if(wserver == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	do
	{
		PSHARE_INFO_0 buf_ptr;
		DWORD er = 0, tr = 0, resume = 0;

		res = NetShareEnum(wserver, 0, (LPBYTE *)&buf_ptr, -1, &er, &tr, &resume);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			PSHARE_INFO_0 p;
			DWORD i;

			for(i = 0, p = buf_ptr; i < er; ++i, ++p)
			{
				dir_entry_t *dir_entry;
				char *utf8_name;

				dir_entry = alloc_dir_entry(&view->dir_entry, view->list_rows);
				if(dir_entry == NULL)
				{
					show_error_msg("Memory Error", "Unable to allocate enough memory");
					continue;
				}

				utf8_name = utf8_from_utf16((wchar_t *)p->shi0_netname);

				init_dir_entry(view, dir_entry, utf8_name);
				dir_entry->type = FT_DIR;

				free(utf8_name);

				++view->list_rows;
			}
			NetApiBufferFree(buf_ptr);
		}
	}
	while(res == ERROR_MORE_DATA);

	free(wserver);
}

static time_t
win_to_unix_time(FILETIME ft)
{
	const uint64_t WINDOWS_TICK = 10000000;
	const uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;
	uint64_t win_time;

	win_time = ft.dwHighDateTime;
	win_time = (win_time << 32) | ft.dwLowDateTime;

	return win_time/WINDOWS_TICK - SEC_TO_UNIX_EPOCH;
}
#endif

char *
get_typed_entry_fpath(const dir_entry_t *entry)
{
	const char *type_suffix;
	char full_path[PATH_MAX];

	type_suffix = is_directory_entry(entry) ? "/" : "";

	get_full_path_of(entry, sizeof(full_path), full_path);
	return format_str("%s%s", full_path, type_suffix);
}

int
flist_custom_active(const FileView *view)
{
	/* First check isn't enough on startup, which leads to false positives.  Yet
	 * this implicit condition seems to be preferable to omit introducing function
	 * that would terminate custom view mode. */
	return view->curr_dir[0] == '\0' && !is_null_or_empty(view->custom.orig_dir);
}

void
flist_custom_start(FileView *view, const char title[])
{
	free_dir_entries(view, &view->custom.entries, &view->custom.entry_count);
	(void)replace_string(&view->custom.next_title, title);

	trie_free(view->custom.paths_cache);
	view->custom.paths_cache = trie_create();
}

dir_entry_t *
flist_custom_add(FileView *view, const char path[])
{
	char canonic_path[PATH_MAX];
	to_canonic_path(path, flist_get_dir(view), canonic_path,
			sizeof(canonic_path));

	/* Don't add duplicates. */
	if(trie_put(view->custom.paths_cache, canonic_path) != 0)
	{
		return NULL;
	}

	return entry_list_add(view, &view->custom.entries, &view->custom.entry_count,
			canonic_path);
}

dir_entry_t *
flist_custom_put(FileView *view, dir_entry_t *entry)
{
	char full_path[PATH_MAX];
	dir_entry_t *dir_entry;
	size_t list_size = view->custom.entry_count;

	get_full_path_of(entry, sizeof(full_path), full_path);

	/* Don't add duplicates. */
	if(trie_put(view->custom.paths_cache, full_path) != 0)
	{
		return NULL;
	}

	dir_entry = add_dir_entry(&view->custom.entries, &list_size, entry);
	view->custom.entry_count = list_size;
	return dir_entry;
}

void
flist_custom_add_separator(FileView *view, int id)
{
	dir_entry_t *const dir_entry = alloc_dir_entry(&view->custom.entries,
			view->custom.entry_count);
	if(dir_entry != NULL)
	{
		init_dir_entry(view, dir_entry, "");
		dir_entry->origin = strdup(flist_get_dir(view));
		dir_entry->id = id;
		++view->custom.entry_count;
	}
}

#ifndef _WIN32

/* Fills directory entry with information about file specified by the path.
 * Returns non-zero on error, otherwise zero is returned. */
static int
fill_dir_entry_by_path(dir_entry_t *entry, const char path[])
{
	return fill_dir_entry(entry, path, NULL);
}

/* Fills fields of the entry from stat information of the file specified by its
 * path.  d is optional source of file type.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
fill_dir_entry(dir_entry_t *entry, const char path[], const struct dirent *d)
{
	struct stat s;

	/* Load the inode information or leave blank values in the entry. */
	if(os_lstat(path, &s) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't lstat() \"%s\"", path);
		return 1;
	}

	entry->type = get_type_from_mode(s.st_mode);
	if(entry->type == FT_UNK)
	{
		entry->type = (d == NULL) ? FT_UNK : type_from_dir_entry(d);
	}
	if(entry->type == FT_UNK)
	{
		LOG_ERROR_MSG("Can't determine type of \"%s\"", path);
		return 1;
	}

	entry->size = (uintmax_t)s.st_size;
	entry->mode = s.st_mode;
	entry->uid = s.st_uid;
	entry->gid = s.st_gid;
	entry->mtime = s.st_mtime;
	entry->atime = s.st_atime;
	entry->ctime = s.st_ctime;
	entry->nlinks = s.st_nlink;

	if(entry->type == FT_LINK)
	{
		/* Query mode of symbolic link target. */

		struct stat s;

		const SymLinkType symlink_type = get_symlink_type(entry->name);
		if(symlink_type != SLT_SLOW && os_stat(entry->name, &s) == 0)
		{
			entry->mode = s.st_mode;
		}
	}

	return 0;
}

/* Checks whether file is a directory.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
data_is_dir_entry(const struct dirent *d)
{
	return is_dirent_targets_dir(d);
}

#else

/* Fills directory entry with information about file specified by the path.
 * Returns non-zero on error, otherwise zero is returned. */
static int
fill_dir_entry_by_path(dir_entry_t *entry, const char path[])
{
	wchar_t *utf16_path;
	HANDLE hfind;
	WIN32_FIND_DATAW ffd;

	utf16_path = utf8_to_utf16(path);
	hfind = FindFirstFileW(utf16_path, &ffd);
	free(utf16_path);

	if(hfind == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	fill_dir_entry(entry, path, &ffd);

	FindClose(hfind);

	return 0;
}

/* Fills fields of the entry from *ffd fields for the file specified by its
 * path.  type_hint is additional source of file type.  Returns zero on success,
 * Returns zero on success, otherwise non-zero is returned. */
static int
fill_dir_entry(dir_entry_t *entry, const char path[],
		const WIN32_FIND_DATAW *ffd)
{
	entry->size = ((uintmax_t)ffd->nFileSizeHigh*(MAXDWORD + 1))
	            + ffd->nFileSizeLow;
	entry->attrs = ffd->dwFileAttributes;
	entry->mtime = win_to_unix_time(ffd->ftLastWriteTime);
	entry->atime = win_to_unix_time(ffd->ftLastAccessTime);
	entry->ctime = win_to_unix_time(ffd->ftCreationTime);

	if(is_win_symlink(ffd->dwFileAttributes, ffd->dwReserved0))
	{
		entry->type = FT_LINK;
	}
	else if(ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		/* Windows doesn't like returning size of directories when it can. */
		entry->size = get_file_size(entry->name);
		entry->type = FT_DIR;
	}
	else if(is_win_executable(path))
	{
		entry->type = FT_EXEC;
	}
	else
	{
		entry->type = FT_REG;
	}

	return 0;
}

/* Checks whether file is a directory.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
data_is_dir_entry(const WIN32_FIND_DATAW *ffd)
{
	return (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

#endif

int
flist_custom_finish(FileView *view, CVType type, int allow_empty)
{
	return flist_custom_finish_internal(view, type, 0, flist_get_dir(view),
			allow_empty);
}

/* Finishes file list population, handles empty resulting list corner case.
 * reload flag suppresses actions taken on location change.  dir is current
 * directory of the view.  Returns zero on success, otherwise (on empty list)
 * non-zero is returned. */
static int
flist_custom_finish_internal(FileView *view, CVType type, int reload,
		const char dir[], int allow_empty)
{
	enum { NORMAL, CUSTOM, UNSORTED } previous;
	const int empty_view = (view->custom.entry_count == 0);

	trie_free(view->custom.paths_cache);
	view->custom.paths_cache = NULL;

	if(empty_view && !allow_empty)
	{
		free_dir_entries(view, &view->custom.entries, &view->custom.entry_count);
		update_string(&view->custom.next_title, NULL);
		return 1;
	}

	free(view->custom.title);
	view->custom.title = view->custom.next_title;
	view->custom.next_title = NULL;

	/* If there are no files and we are allowed to add ".." directory, do it. */
	if(empty_view || (!cv_unsorted(type) && cfg_parent_dir_is_visible(0)))
	{
		dir_entry_t *const dir_entry = alloc_dir_entry(&view->custom.entries,
				view->custom.entry_count);
		if(dir_entry != NULL)
		{
			init_dir_entry(view, dir_entry, "..");
			dir_entry->type = FT_DIR;
			dir_entry->origin = strdup(dir);
			++view->custom.entry_count;
		}
	}

	previous = (view->curr_dir[0] != '\0')
	         ? NORMAL
	         : (ui_view_unsorted(view) ? UNSORTED : CUSTOM);

	if(previous == NORMAL)
	{
		/* Save current location in the directory before replacing it with custom
		 * view. */
		if(is_dir_list_loaded(view))
		{
			save_view_history(view, NULL, NULL, -1);
		}

		(void)replace_string(&view->custom.orig_dir, dir);
		view->curr_dir[0] = '\0';
	}

	/* Replace view file list with custom list. */
	free_dir_entries(view, &view->dir_entry, &view->list_rows);
	view->dir_entry = view->custom.entries;
	view->list_rows = view->custom.entry_count;
	view->custom.entries = NULL;
	view->custom.entry_count = 0;
	view->dir_entry = dynarray_shrink(view->dir_entry);
	view->filtered = 0;

	/* Kind of custom view must be set to correct value before option loading and
	 * sorting. */
	view->custom.type = type;

	if(cv_unsorted(type))
	{
		/* Disabling sorting twice in a row erases sorting completely. */
		if(previous != UNSORTED)
		{
			disable_view_sorting(view);
		}
	}
	else if(previous == UNSORTED)
	{
		enable_view_sorting(view);
	}

	if(!reload)
	{
		on_location_change(view, 0);
	}

	sort_dir_list(0, view);

	flist_ensure_pos_is_valid(view);

	return 0;
}

/* Perform actions on view location change.  Force activates all actions
 * unconditionally otherwise they are checked against cvoptions. */
static void
on_location_change(FileView *view, int force)
{
	free_dir_entries(view, &view->local_filter.entries,
			&view->local_filter.entry_count);

	if(force || (cfg.cvoptions & CVO_LOCALFILTER))
	{
		filters_dir_updated(view);
	}
	/* Stage check is to skip body of the if in tests. */
	if(curr_stats.load_stage > 0 && (force || (cfg.cvoptions & CVO_LOCALOPTS)))
	{
		reset_local_options(view);
	}
	if(force || (cfg.cvoptions & CVO_AUTOCMDS))
	{
		vle_aucmd_execute("DirEnter", view->curr_dir, view);
	}
}

/* Disables view sorting saving its state for the future. */
static void
disable_view_sorting(FileView *view)
{
	memcpy(&view->custom.sort[0], &view->sort[0], sizeof(view->custom.sort));
	memset(&view->sort[0], SK_NONE, sizeof(view->sort));
	load_sort_option(view);
}

/* Undoes was was done by disable_view_sorting(). */
static void
enable_view_sorting(FileView *view)
{
	memcpy(&view->sort[0], &view->custom.sort[0], sizeof(view->sort));
	load_sort_option(view);
}

void
flist_custom_exclude(FileView *view, int selection_only)
{
	dir_entry_t *entry;

	if(!flist_custom_active(view))
	{
		return;
	}

	if(cv_compare(view->custom.type))
	{
		exclude_in_compare(view, selection_only);
		return;
	}

	entry = NULL;
	while(iter_selection_or_current(view, &entry))
	{
		entry->temporary = 1;

		if(view->custom.type == CV_TREE)
		{
			char full_path[PATH_MAX];
			get_full_path_of(entry, sizeof(full_path), full_path);
			(void)trie_put(view->custom.excluded_paths, full_path);
		}
	}

	(void)exclude_temporary_entries(view);
}

/* Removes selected files from compare view.  Zero selection_only enables
 * excluding files that share ids with selected items. */
static void
exclude_in_compare(FileView *view, int selection_only)
{
	FileView *const other = (view == curr_view) ? other_view : curr_view;
	const int double_compare = (view->custom.type == CV_DIFF);
	const int n = other->list_rows;
	dir_entry_t *entry = NULL;
	while(iter_selection_or_current(view, &entry))
	{
		if(selection_only)
		{
			entry->temporary = 1;
			if(double_compare)
			{
				other->dir_entry[entry - view->dir_entry].temporary = 1;
			}
		}
		else
		{
			mark_group(view, other, entry - view->dir_entry);
		}
	}

	(void)exclude_temporary_entries(view);
	if(double_compare && exclude_temporary_entries(other) == n)
	{
		/* Leave compare mode if we excluded all files. */
		cd_updir(view, 1);
	}
}

/* Selects all neighbours of the idx-th element that share its id. */
static void
mark_group(FileView *view, FileView *other, int idx)
{
	int i;
	int id;

	if(view->dir_entry[idx].temporary)
	{
		return;
	}

	id = view->dir_entry[idx].id;

	for(i = idx - 1; i >= 0 && view->dir_entry[i].id == id; --i)
	{
		view->dir_entry[i].temporary = 1;
		if(view->custom.type == CV_DIFF)
		{
			other->dir_entry[i].temporary = 1;
		}
	}
	for(i = idx; i < view->list_rows && view->dir_entry[i].id == id; ++i)
	{
		view->dir_entry[i].temporary = 1;
		if(view->custom.type == CV_DIFF)
		{
			other->dir_entry[i].temporary = 1;
		}
	}
}

/* Excludes view entries that are marked as "temporary".  Returns number of
 * items that were visible before. */
static int
exclude_temporary_entries(FileView *view)
{
	const int n = zap_entries(view, view->dir_entry, &view->list_rows,
			&is_temporary, NULL, 0, 1);
	(void)zap_entries(view, view->custom.entries, &view->custom.entry_count,
			&is_temporary, NULL, 1, 1);

	flist_ensure_pos_is_valid(view);
	ui_view_schedule_redraw(view);

	flist_sel_recount(view);

	return n;
}

/* zap_entries() filter to filter-out files, which were marked for removal.
 * Returns non-zero if entry is to be kept and zero otherwise.*/
static int
is_temporary(FileView *view, const dir_entry_t *entry, void *arg)
{
	return !entry->temporary;
}

void
flist_custom_clone(FileView *to, const FileView *from)
{
	dir_entry_t *dst, *src;
	int nentries;
	int i, j;
	const int from_tree = (from->custom.type == CV_TREE);

	assert(flist_custom_active(from) && to->custom.paths_cache == NULL &&
			"Wrong state of destination view.");

	replace_string(&to->custom.orig_dir, from->custom.orig_dir);
	to->curr_dir[0] = '\0';

	replace_string(&to->custom.title,
			from_tree ? "from tree" : from->custom.title);
	to->custom.type = (ui_view_unsorted(from) || from_tree)
	                ? CV_VERY
	                : CV_REGULAR;

	if(custom_list_is_incomplete(from))
	{
		src = from->local_filter.entries;
		nentries = from->local_filter.entry_count;
	}
	else
	{
		src = from->dir_entry;
		nentries = from->list_rows;
	}

	dst = dynarray_extend(NULL, nentries*sizeof(*dst));

	j = 0;
	for(i = 0; i < nentries; ++i)
	{
		if(to->custom.type != CV_TREE && src[i].child_pos != 0 &&
				is_parent_dir(src[i].name))
		{
			continue;
		}

		dst[j] = src[i];
		dst[j].name = strdup(dst[j].name);
		if(dst[j].origin == from->curr_dir)
		{
			dst[j].origin = to->curr_dir;
		}
		else
		{
			dst[j].origin = strdup(dst[j].origin);
		}

		/* As destination pane won't be a tree, erase tree-specific data, because
		 * some tree-specific code is driven directly by these fields. */
		dst[j].child_count = 0;
		dst[j].child_pos = 0;

		++j;
	}

	free_dir_entries(to, &to->custom.entries, &to->custom.entry_count);
	free_dir_entries(to, &to->dir_entry, &to->list_rows);
	to->dir_entry = dst;
	to->list_rows = j;

	to->filtered = 0;

	if(ui_view_unsorted(to))
	{
		disable_view_sorting(to);
	}
}

void
flist_custom_uncompress_tree(FileView *view)
{
	unsigned int i;

	assert(view->custom.type == CV_TREE &&
			"This function is for tree-view only!");

	dir_entry_t *entries = view->dir_entry;
	size_t nentries = view->list_rows;

	const size_t path_prefix_len = strlen(flist_get_dir(view));
	fsdata_t *const tree = fsdata_create(0, 0);

	view->dir_entry = NULL;
	view->list_rows = 0;

	for(i = 0U; i < nentries; ++i)
	{
		char full_path[PATH_MAX];
		void *data = &entries[i];
		get_full_path_of(&entries[i], sizeof(full_path), full_path);
		fsdata_set(tree, full_path + path_prefix_len, &data, sizeof(data));
	}

	fsdata_traverse(tree, &uncompress_traverser, view);

	fsdata_free(tree);
	dynarray_free(entries);
}

/* fsdata_traverse() callback that flattens the tree into array of entries. */
static void
uncompress_traverser(const char name[], int valid, const void *parent_data,
		void *data, void *arg)
{
	/* Initially data associated with existing entries point to original entries.
	 * Once that entry is copied, the data is replaced with its index in the new
	 * list. */

	FileView *view = arg;

	dir_entry_t *dir_entry = alloc_dir_entry(&view->dir_entry, view->list_rows);
	++view->list_rows;

	if(valid)
	{
		*dir_entry = **(dir_entry_t **)data;
		dir_entry->child_count = 0;
	}
	else
	{
		char full_path[PATH_MAX];

		init_dir_entry(view, dir_entry, name);
		if(parent_data == NULL)
		{
			dir_entry->origin = strdup(flist_get_dir(view));
		}
		else
		{
			char parent_path[PATH_MAX];
			const intptr_t *parent_idx = parent_data;
			get_full_path_at(view, *parent_idx, sizeof(parent_path), parent_path);
			dir_entry->origin = strdup(parent_path);
		}

		get_full_path_of(dir_entry, sizeof(full_path), full_path);
		fill_dir_entry_by_path(dir_entry, full_path);

		dir_entry->temporary = 1;
	}

	*(intptr_t *)data = dir_entry - view->dir_entry;

	if(parent_data != NULL)
	{
		const intptr_t *const parent_idx = parent_data;
		dir_entry->child_pos = (dir_entry - view->dir_entry) - *parent_idx;

		do
		{
			dir_entry -= dir_entry->child_pos;
			++dir_entry->child_count;
		}
		while(dir_entry->child_pos != 0);
	}
}

const char *
flist_get_dir(const FileView *view)
{
	if(flist_custom_active(view))
	{
		assert(view->custom.orig_dir != NULL && "Wrong view dir state.");
		return view->custom.orig_dir;
	}

	return view->curr_dir;
}

void
flist_goto_by_path(FileView *view, const char path[])
{
	char full_path[PATH_MAX];
	dir_entry_t *entry;
	const char *const name = get_last_path_component(path);

	get_current_full_path(view, sizeof(full_path), full_path);
	if(stroscmp(full_path, path) == 0)
	{
		return;
	}

	if(flist_custom_active(view) && view->custom.type == CV_TREE &&
			strcmp(name, "..") == 0)
	{
		int pos;
		char dir_only[PATH_MAX];

		snprintf(dir_only, sizeof(dir_only), "%.*s", (int)(name - path), path);
		pos = flist_find_entry(view, name, dir_only);
		if(pos != -1)
		{
			view->list_pos = pos;
		}
		return;
	}

	entry = entry_from_path(view->dir_entry, view->list_rows, path);
	if(entry != NULL)
	{
		view->list_pos = entry_to_pos(view, entry);
	}
}

dir_entry_t *
entry_from_path(dir_entry_t *entries, int count, const char path[])
{
	char canonic_path[PATH_MAX];
	const char *fname;
	int i;

	to_canonic_path(path, flist_get_dir(curr_view), canonic_path,
			sizeof(canonic_path));

	fname = get_last_path_component(canonic_path);
	for(i = 0; i < count; ++i)
	{
		char full_path[PATH_MAX];
		dir_entry_t *const entry = &entries[i];

		if(stroscmp(entry->name, fname) != 0)
		{
			continue;
		}

		get_full_path_of(entry, sizeof(full_path), full_path);
		if(stroscmp(full_path, canonic_path) == 0)
		{
			return entry;
		}
	}

	return NULL;
}

uint64_t
entry_get_nitems(const FileView *view, const dir_entry_t *entry)
{
	uint64_t nitems;
	dcache_get_of(entry, NULL, &nitems);

	if(nitems == DCACHE_UNKNOWN && !view->on_slow_fs)
	{
		nitems = entry_calc_nitems(entry);
	}

	return nitems;
}

uint64_t
entry_calc_nitems(const dir_entry_t *entry)
{
	uint64_t ret;
	char full_path[PATH_MAX];
	get_full_path_of(entry, sizeof(full_path), full_path);

	ret = count_dir_items(full_path);
	dcache_set_at(full_path, DCACHE_UNKNOWN, ret);

	return ret;
}

void
populate_dir_list(FileView *view, int reload)
{
	(void)populate_dir_list_internal(view, reload);
}

void
load_dir_list(FileView *view, int reload)
{
	load_dir_list_internal(view, reload, 0);
}

/* Loads file list for the view and redraws the view.  The reload parameter
 * should be set in case of view refresh operation.  The draw_only parameter
 * prevents extra actions that might be taken care of outside this function as
 * well. */
static void
load_dir_list_internal(FileView *view, int reload, int draw_only)
{
	if(populate_dir_list_internal(view, reload) != 0)
	{
		return;
	}

	if(draw_only)
	{
		draw_dir_list_only(view);
	}
	else
	{
		draw_dir_list(view);
	}

	if(view == curr_view)
	{
		if(strnoscmp(view->curr_dir, cfg.fuse_home, strlen(cfg.fuse_home)) == 0 &&
				stroscmp(other_view->curr_dir, view->curr_dir) == 0)
		{
			load_dir_list(other_view, 1);
		}
	}
}

/* Loads filelist for the view.  The reload parameter should be set in case of
 * view refresh operation.  Returns non-zero on error. */
static int
populate_dir_list_internal(FileView *view, int reload)
{
	view->filtered = 0;

	if(flist_custom_active(view))
	{
		return populate_custom_view(view, reload);
	}

	if(!reload && is_dir_big(view->curr_dir))
	{
		if(!vle_mode_is(CMDLINE_MODE))
		{
			ui_sb_quick_msgf("%s", "Reading directory...");
		}
	}

	if(curr_stats.load_stage < 2)
	{
		update_all_windows();
	}

	/* this is needed for lstat() below */
	if(vifm_chdir(view->curr_dir) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't chdir() into \"%s\"", view->curr_dir);
		return 1;
	}

	if(update_dir_list(view, reload) != 0)
	{
		/* We don't have read access, only execute, or there were other problems. */
		free_view_entries(view);
		add_parent_dir(view);
	}

	if(!reload && !vle_mode_is(CMDLINE_MODE))
	{
		ui_sb_clear();
	}

	view->column_count = calculate_columns_count(view);

	/* If reloading the same directory don't jump to history position.  Stay at
	 * the current line. */
	if(!reload)
	{
		flist_hist_lookup(view, view);
	}

	fview_dir_updated(view);

	if(view->list_rows < 1)
	{
		if(rescue_from_empty_filelist(view))
		{
			return 0;
		}

		add_parent_dir(view);
	}

	fview_list_updated(view);

	if(update_dir_watcher(view) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't get directory mtime \"%s\"", view->curr_dir);
		return 1;
	}

	return 0;
}

/* (Re)loads custom view file list.  Returns non-zero on error. */
static int
populate_custom_view(FileView *view, int reload)
{
	if(view->custom.type == CV_TREE)
	{
		dir_entry_t *prev_dir_entries;
		int prev_list_rows, result;

		start_dir_list_change(view, &prev_dir_entries, &prev_list_rows, reload);
		result = flist_load_tree_internal(view, flist_get_dir(view), 1);

		if(view->dir_entry == NULL)
		{
			/* Restore original list in case of failure. */
			view->dir_entry = prev_dir_entries;
			view->list_rows = result;
		}
		else
		{
			finish_dir_list_change(view, prev_dir_entries, prev_list_rows);
		}

		if(result != 0)
		{
			show_error_msg("Tree View", "Reload failed");
		}

		return result;
	}

	if(view->custom.type == CV_DIFF)
	{
		if(filter_in_compare(view, NULL, &entry_exists))
		{
			return 0;
		}
	}
	else
	{
		if(custom_list_is_incomplete(view))
		{
			/* Load initial list of custom entries if it's available. */
			replace_dir_entries(view, &view->dir_entry, &view->list_rows,
					view->local_filter.entries, view->local_filter.entry_count);
		}

		(void)zap_entries(view, view->dir_entry, &view->list_rows,
				&is_dead_or_filtered, NULL, 0, 0);
	}

	update_entries_data(view);
	sort_dir_list(!reload, view);
	fview_list_updated(view);
	return 0;
}

int
filter_in_compare(FileView *view, void *arg, zap_filter filter)
{
	FileView *const other = (view == curr_view) ? other_view : curr_view;

	zap_compare_view(view, other, filter, arg);
	if(view->list_rows == 0)
	{
		show_error_msg("Comparison", "No files left in the views, leaving them.");
		cd_updir(view, 1);
		return 1;
	}

	(void)zap_entries(other, other->dir_entry, &other->list_rows, &is_temporary,
			NULL, 0, 1);
	ui_view_schedule_redraw(other);
	flist_sel_recount(other);
	return 0;
}

/* Checks whether entry refers to non-existing file.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
entry_exists(FileView *view, const dir_entry_t *entry, void *arg)
{
	return path_exists_at(entry->origin, entry->name, NODEREF);
}

/* Removes entries that refer to non-existing files from compare view marking
 * corresponding entries of the other view as temporary.  This is a
 * zap_entries() tailored for needs of compare, because that function is already
 * too complex. */
static void
zap_compare_view(FileView *view, FileView *other, zap_filter filter, void *arg)
{
	int i, j = 0;

	for(i = 0; i < view->list_rows; ++i)
	{
		dir_entry_t *const entry = &view->dir_entry[i];

		if(!fentry_is_fake(entry) && !filter(view, entry, arg))
		{
			const int separator = find_separator(other, i);
			if(separator >= 0)
			{
				free_dir_entry(view, entry);
				other->dir_entry[separator].temporary = 1;

				if(view->list_pos == i)
				{
					view->list_pos = j;
				}
				continue;
			}
			replace_string(&entry->name, "");
			entry->type = FT_UNK;
			entry->id = other->dir_entry[i].id;
		}

		if(i != j)
		{
			view->dir_entry[j] = view->dir_entry[i];
		}

		++j;
	}

	view->list_rows = j;
}

/* Finds separator among the group of equivalent files of the view specified by
 * its position.  Returns index of the separator or -1. */
static int
find_separator(FileView *view, int idx)
{
	int i;
	const int id = view->dir_entry[idx].id;

	for(i = idx; i >= 0 && view->dir_entry[i].id == id; --i)
	{
		if(!view->dir_entry[i].temporary && fentry_is_fake(&view->dir_entry[i]))
		{
			return i;
		}
	}
	for(i = idx + 1; i < view->list_rows && view->dir_entry[i].id == id; ++i)
	{
		if(!view->dir_entry[i].temporary && fentry_is_fake(&view->dir_entry[i]))
		{
			return i;
		}
	}
	return -1;
}

/* Updates directory watcher of the view.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
update_dir_watcher(FileView *view)
{
	int error;
	const char *const curr_dir = flist_get_dir(view);

	if(view->watch == NULL || stroscmp(view->watched_dir, curr_dir) != 0)
	{
		fswatch_free(view->watch);

		view->watch = fswatch_create(curr_dir);
		if(view->watch == NULL)
		{
			/* This is bad, but there isn't much we can do here and this doesn't feel
			 * like a reason to block anything else. */
			return 0;
		}

		copy_str(view->watched_dir, sizeof(view->watched_dir), curr_dir);
	}

	(void)fswatch_changed(view->watch, &error);

	return error;
}

/* Checks whether currently loaded custom list of files is missing some files
 * compared to the original custom list.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
custom_list_is_incomplete(const FileView *view)
{
	if(view->local_filter.entry_count == 0)
	{
		return 0;
	}

	if(view->list_rows == 1 && is_parent_dir(view->dir_entry[0].name) &&
			!(view->local_filter.entry_count == 1 &&
				is_parent_dir(view->local_filter.entries[0].name)))
	{
		return 1;
	}

	return view->list_rows != view->local_filter.entry_count;
}

/* zap_entries() filter to filter-out inexistent files or files which names
 * match local filter. */
static int
is_dead_or_filtered(FileView *view, const dir_entry_t *entry, void *arg)
{
	if(!path_exists_at(entry->origin, entry->name, NODEREF))
	{
		return 0;
	}

	if(local_filter_matches(view, entry))
	{
		return 1;
	}

	++view->filtered;
	return 0;
}

/* Re-read meta-data for each entry (does nothing for entries on which querying
 * fails). */
static void
update_entries_data(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		char full_path[PATH_MAX];
		dir_entry_t *const entry = &view->dir_entry[i];

		/* Fake entries do not map onto files in file system. */
		if(fentry_is_fake(entry))
		{
			continue;
		}

		get_full_path_of(entry, sizeof(full_path), full_path);

		/* Do not care about possible failure, just use previous meta-data. */
		(void)fill_dir_entry_by_path(entry, full_path);
	}
}

int
zap_entries(FileView *view, dir_entry_t *entries, int *count, zap_filter filter,
		void *arg, int allow_empty_list, int remove_subtrees)
{
	int i, j;

	j = 0;
	for(i = 0; i < *count; ++i)
	{
		int k, pos, parent;
		dir_entry_t *const entry = &entries[i];
		const int nremoved = remove_subtrees ? (entry->child_count + 1) : 1;

		if(filter(view, entry, arg))
		{
			if(i != j)
			{
				entries[j] = entries[i];
			}

			++j;
			continue;
		}

		/* Reassign children of node about to be deleted to its parent.  Child count
		 * don't need an update here because these nodes are already counted. */
		pos = i + 1;
		while(pos < i + 1 + entry->child_count)
		{
			if(entry->child_pos == 0)
			{
				entries[pos].child_pos = 0;
			}
			else
			{
				entries[pos].child_pos += entry->child_pos - 1;
			}
			pos += entries[pos].child_count + 1;
		}

		if(entry->child_pos != 0)
		{
			/* Visit all parent nodes to update number of their children and also
			 * all sibling nodes which require their parent links updated. */
			int pos = i + (entry->child_count + 1);
			int parent = j - entry->child_pos;
			while(1)
			{
				while(pos <= parent + (i - j) + entries[parent].child_count)
				{
					entries[pos].child_pos -= nremoved;
					pos += entries[pos].child_count + 1;
				}
				entries[parent].child_count -= nremoved;
				if(entries[parent].child_pos == 0)
				{
					break;
				}
				parent -= entries[parent].child_pos;
			}
		}

		for(k = 0; k < nremoved; ++k)
		{
			free_dir_entry(view, &entry[k]);
		}

		if(view->list_pos >= i && view->list_pos < i + nremoved)
		{
			view->list_pos = j;
		}

		/* Add directory leaf if we just removed last child of the last of nodes
		 * that wasn't filtered.  We can use one entry because if something was
		 * filtered out, there is space for at least one extra entry. */
		parent = i - entry->child_pos - (i - j);
		if(remove_subtrees && parent == j - 1 && entries[parent].child_count == 0)
		{
			char full_path[PATH_MAX];
			char *path;

			int pos = i + (entry->child_count + 1);

			get_full_path_of(&entries[j - 1], sizeof(full_path), full_path);
			path = format_str("%s/..", full_path);
			init_parent_entry(view, &entries[j], path);
			remove_last_path_component(path);
			entries[j].origin = path;
			entries[j].child_pos = 1;

			/* Since we now adding back one entry, correct increase parent counts and
			 * child positions back by one. */
			while(1)
			{
				while(pos <= parent + (i - j + nremoved) + entries[parent].child_count)
				{
					++entries[pos].child_pos;
					pos += entries[pos].child_count + 1;
				}
				++entries[parent].child_count;
				if(entries[parent].child_pos == 0)
				{
					break;
				}
				parent -= entries[parent].child_pos;
			}

			++j;
		}

		i += nremoved - 1;
	}

	*count = j;

	if(*count == 0 && !allow_empty_list)
	{
		add_parent_dir(view);
	}

	return i - j;
}

/* Checks for subjectively relative size of a directory specified by the path
 * parameter.  Returns non-zero if size of the directory in question is
 * considered to be big. */
static int
is_dir_big(const char path[])
{
#ifndef _WIN32
	struct stat s;
	if(os_stat(path, &s) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", path);
		return 1;
	}
	return s.st_size > s.st_blksize;
#else
	return 1;
#endif
}

/* Frees list of directory entries of the view. */
static void
free_view_entries(FileView *view)
{
	free_dir_entries(view, &view->dir_entry, &view->list_rows);
}

/* Updates file list with files from current directory.  Returns zero on
 * success, otherwise non-zero is returned. */
static int
update_dir_list(FileView *view, int reload)
{
	dir_entry_t *prev_dir_entries;
	int prev_list_rows;

	start_dir_list_change(view, &prev_dir_entries, &prev_list_rows, reload);

#ifdef _WIN32
	if(is_unc_root(view->curr_dir))
	{
		fill_with_shared(view);
		free_dir_entries(view, &prev_dir_entries, &prev_list_rows);
		return 0;
	}
#endif

	if(enum_dir_content(view->curr_dir, &add_file_entry_to_view, view) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't opendir() \"%s\"", view->curr_dir);
		free_dir_entries(view, &prev_dir_entries, &prev_list_rows);
		return 1;
	}

	if(cfg_parent_dir_is_visible(is_root_dir(view->curr_dir)) ||
			view->list_rows == 0)
	{
		add_parent_dir(view);
	}

	sort_dir_list(!reload, view);

	/* Merging must be performed after sorting so that list position remains fixed
	 * (sorting doesn't preserve it). */
	finish_dir_list_change(view, prev_dir_entries, prev_list_rows);

	return 0;
}

/* Starts file list update, saving previous list for future reference if
 * necessary. */
static void
start_dir_list_change(FileView *view, dir_entry_t **entries, int *len,
		int reload)
{
	if(reload)
	{
		*entries = view->dir_entry;
		*len = view->list_rows;
		view->dir_entry = NULL;
		view->list_rows = 0;
	}
	else
	{
		*entries = NULL;
		*len = 0;
		free_view_entries(view);
	}

	view->matches = 0;
	view->selected_files = 0;
}

/* Finishes file list update, possibly merging information from old entries into
 * new ones. */
static void
finish_dir_list_change(FileView *view, dir_entry_t *entries, int len)
{
	if(entries != NULL)
	{
		merge_lists(view, entries, len);
		free_dir_entries(view, &entries, &len);
	}

	view->dir_entry = dynarray_shrink(view->dir_entry);
}

/* enum_dir_content() callback that appends files to file list.  Returns zero on
 * success or non-zero to indicate failure and stop enumeration. */
static int
add_file_entry_to_view(const char name[], const void *data, void *param)
{
	FileView *const view = param;
	dir_entry_t *entry;

	/* Always ignore the "." and ".." directories. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	{
		return 0;
	}

	if(!file_is_visible(view, name, 0, data, 1))
	{
		++view->filtered;
		return 0;
	}

	entry = alloc_dir_entry(&view->dir_entry, view->list_rows);
	if(entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 1;
	}

	init_dir_entry(view, entry, name);

	if(fill_dir_entry(entry, entry->name, data) == 0)
	{
		++view->list_rows;
	}
	else
	{
		free_dir_entry(view, entry);
	}

	return 0;
}

void
resort_dir_list(int msg, FileView *view)
{
	char full_path[PATH_MAX];
	const int top_delta = view->list_pos - view->top_line;
	if(view->list_pos < view->list_rows)
	{
		get_current_full_path(view, sizeof(full_path), full_path);
	}

	sort_dir_list(msg, view);

	if(view->list_pos < view->list_rows)
	{
		flist_goto_by_path(view, full_path);
		view->top_line = view->list_pos - top_delta;
	}
}

/* Resorts view without reloading it.  msg parameter controls whether to show
 * "Sorting..." statusbar message. */
static void
sort_dir_list(int msg, FileView *view)
{
	if(msg && view->list_rows > 2048 && !vle_mode_is(CMDLINE_MODE))
	{
		ui_sb_quick_msgf("%s", "Sorting directory...");
	}

	sort_view(view);

	if(msg && !vle_mode_is(CMDLINE_MODE))
	{
		ui_sb_clear();
	}
}

/* Merges elements from previous list into the new one. */
static void
merge_lists(FileView *view, dir_entry_t *entries, int len)
{
	int i;
	int closest_dist;
	const int prev_pos = view->list_pos;
	trie_t *prev_names = trie_create();

	for(i = 0; i < len; ++i)
	{
		add_to_trie(prev_names, view, &entries[i]);

		/* We won't use the name later, so free some memory. */
		update_string(&entries[i].name, NULL);
	}

	closest_dist = INT_MIN;
	for(i = 0; i < view->list_rows; ++i)
	{
		int dist;
		void *data;
		dir_entry_t *const entry = &view->dir_entry[i];
		if(!is_in_trie(prev_names, view, entry, &data))
		{
			continue;
		}

		/* Transfer information from previous entry to the new one. */
		merge_entries(entry, data);

		/* Update number of selected files (should have been zeroed beforehand). */
		view->selected_files += (entry->selected != 0);

		/* Update cursor position in a smart way. */
		dist = (dir_entry_t*)data - entries - prev_pos;
		closest_dist = correct_pos(view, i, dist, closest_dist);
	}

	trie_free(prev_names);
}

/* Adds view entry into the trie mapping its name to entry structure. */
static void
add_to_trie(trie_t *trie, FileView *view, dir_entry_t *entry)
{
	int error;

	if(flist_custom_active(view))
	{
		char full_path[PATH_MAX];
		get_full_path_of(entry, sizeof(full_path), full_path);
		error = trie_set(trie, full_path, entry);
	}
	else
	{
		error = trie_set(trie, entry->name, entry);
	}

	assert(error == 0 && "Duplicated file names in the list?");
	(void)error;
}

/* Looks up entry in the trie by its name.  Retrieves directory entry stored by
 * add_to_trie() into *data (unchanged on lookup failure).  Returns non-zero if
 * item was successfully retrieved and zero otherwise. */
static int
is_in_trie(trie_t *trie, FileView *view, dir_entry_t *entry, void **data)
{
	int error;

	if(flist_custom_active(view))
	{
		char full_path[PATH_MAX];
		get_full_path_of(entry, sizeof(full_path), full_path);
		error = trie_get(trie, full_path, data);
	}
	else
	{
		error = trie_get(trie, entry->name, data);
	}

	return (error == 0);
}

/* Merges data from previous entry into the new one.  Both entries should
 * correspond to the same file (their names must match). */
static void
merge_entries(dir_entry_t *new, const dir_entry_t *prev)
{
	new->id = prev->id;

	new->selected = prev->selected;
	new->was_selected = prev->was_selected;

	/* No need to check for name here, because only entries with exactly the same
	 * names are merged. */
	if(new->type == prev->type)
	{
		new->hi_num = prev->hi_num;
		new->name_dec_num = prev->name_dec_num;
	}
}

/* Corrects selected item position in the list.  Returns updated value of the
 * closest variable, which initially should be INT_MIN. */
static int
correct_pos(FileView *view, int pos, int dist, int closest)
{
	if(dist == 0)
	{
		closest = 0;
		view->list_pos = pos;
	}
	else if((closest < 0 && dist > closest) || (closest > 0 && dist < closest))
	{
		closest = dist;
		view->list_pos = pos;
	}
	return closest;
}

/* Performs actions needed to rescue from abnormal situation with empty
 * filelist.  Returns non-zero if file list was reloaded. */
static int
rescue_from_empty_filelist(FileView *view)
{
	/* It is possible to set the file name filter so that no files are showing
	 * in the / directory.  All other directories will always show at least the
	 * ../ file.  This resets the filter and reloads the directory. */
	if(filename_filter_is_empty(view))
	{
		return 0;
	}

	show_error_msgf("Filter error",
			"The %s\"%s\" pattern did not match any files. It was reset.",
			view->invert ? "" : "inverted ", view->manual_filter.raw);

	filename_filter_clear(view);

	load_dir_list(view, 1);
	if(view->list_rows < 1)
	{
		leave_invalid_dir(view);
		(void)change_directory(view, view->curr_dir);
		load_dir_list(view, 0);
	}

	return 1;
}

void
add_parent_dir(FileView *view)
{
	dir_entry_t *const dir_entry = alloc_dir_entry(&view->dir_entry,
			view->list_rows);
	if(dir_entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if(init_parent_entry(view, dir_entry, "..") == 0)
	{
		++view->list_rows;
	}
}

/* Initializes dir_entry_t with name and all other fields with default
 * values. */
static void
init_dir_entry(FileView *view, dir_entry_t *entry, const char name[])
{
	entry->name = strdup(name);
	entry->origin = &view->curr_dir[0];

	entry->size = 0ULL;
#ifndef _WIN32
	entry->uid = (uid_t)-1;
	entry->gid = (gid_t)-1;
	entry->mode = (mode_t)0;
#else
	entry->attrs = 0;
#endif

	entry->mtime = (time_t)0;
	entry->atime = (time_t)0;
	entry->ctime = (time_t)0;

	entry->type = FT_UNK;
	entry->hi_num = -1;
	entry->name_dec_num = -1;

	entry->child_count = 0;
	entry->child_pos = 0;

	/* All files start as unselected, unmatched and unmarked. */
	entry->selected = 0;
	entry->was_selected = 0;
	entry->search_match = 0;
	entry->marked = 0;
	entry->temporary = 0;

	entry->tag = -1;
	entry->id = -1;
}

void
replace_dir_entries(FileView *view, dir_entry_t **entries, int *count,
		const dir_entry_t *with_entries, int with_count)
{
	dir_entry_t *new;
	int i;

	new = dynarray_extend(NULL, with_count*sizeof(*new));
	if(new == NULL)
	{
		return;
	}

	memcpy(new, with_entries, sizeof(*new)*with_count);

	for(i = 0; i < with_count; ++i)
	{
		dir_entry_t *const entry = &new[i];

		entry->name = strdup(entry->name);
		entry->origin = strdup(entry->origin);

		if(entry->name == NULL || entry->origin == NULL)
		{
			int count_so_far = i + 1;
			free_dir_entries(view, &new, &count_so_far);
			return;
		}
	}

	free_dir_entries(view, entries, count);
	*entries = new;
	*count = with_count;
}

void
free_dir_entries(FileView *view, dir_entry_t **entries, int *count)
{
	int i;
	for(i = 0; i < *count; ++i)
	{
		free_dir_entry(view, &(*entries)[i]);
	}

	dynarray_free(*entries);
	*entries = NULL;
	*count = 0;
}

void
free_dir_entry(const FileView *view, dir_entry_t *entry)
{
	free(entry->name);
	entry->name = NULL;

	if(entry->origin != &view->curr_dir[0])
	{
		free(entry->origin);
		entry->origin = NULL;
	}
}

dir_entry_t *
add_dir_entry(dir_entry_t **list, size_t *list_size, const dir_entry_t *entry)
{
	dir_entry_t *const new_entry = alloc_dir_entry(list, *list_size);
	if(new_entry == NULL)
	{
		return NULL;
	}

	*new_entry = *entry;
	++*list_size;
	return new_entry;
}

dir_entry_t *
entry_list_add(FileView *view, dir_entry_t **list, int *list_size,
		const char path[])
{
	dir_entry_t *const dir_entry = alloc_dir_entry(list, *list_size);
	if(dir_entry == NULL)
	{
		return NULL;
	}

	init_dir_entry(view, dir_entry, get_last_path_component(path));

	dir_entry->origin = strdup(path);
	remove_last_path_component(dir_entry->origin);

	if(fill_dir_entry_by_path(dir_entry, path) != 0)
	{
		free_dir_entry(view, dir_entry);
		return NULL;
	}

	++*list_size;
	return dir_entry;
}

/* Allocates one more directory entry for the *list of size list_size by
 * extending it.  Returns pointer to new entry or NULL on failure. */
static dir_entry_t *
alloc_dir_entry(dir_entry_t **list, int list_size)
{
	dir_entry_t *new_entry_list = dynarray_extend(*list, sizeof(dir_entry_t));
	if(new_entry_list == NULL)
	{
		return NULL;
	}

	*list = new_entry_list;
	return &new_entry_list[list_size];
}

void
check_if_filelist_have_changed(FileView *view)
{
	int failed, changed;
	const char *const curr_dir = flist_get_dir(view);

	if(view->on_slow_fs ||
			(flist_custom_active(view) && view->custom.type != CV_TREE) ||
			is_unc_root(curr_dir))
	{
		return;
	}

	if(view->watch == NULL)
	{
		/* If watch is not initialized, try to do this, but don't fail on error. */

		(void)update_dir_watcher(view);
		failed = 0;
		changed = (view->watch != NULL);
	}
	else
	{
		changed = fswatch_changed(view->watch, &failed);
	}

	/* Check if we still have permission to visit this directory. */
	failed |= (os_access(curr_dir, X_OK) != 0);

	if(failed)
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", curr_dir);
		log_cwd();

		show_error_msgf("Directory Change Check", "Cannot open %s", curr_dir);

		leave_invalid_dir(view);
		(void)change_directory(view, curr_dir);
		flist_sel_stash(view);
		ui_view_schedule_reload(view);
		return;
	}

	if(changed)
	{
		ui_view_schedule_reload(view);
	}
	else if(flist_custom_active(view) && view->custom.type == CV_TREE)
	{
		if(tree_has_changed(view->dir_entry, view->list_rows))
		{
			ui_view_schedule_reload(view);
		}
	}
}

/* Checks whether tree-view needs a reload (any of subdirectories were changed).
 * Returns non-zero if so, otherwise zero is returned. */
static int
tree_has_changed(const dir_entry_t *entries, size_t nchildren)
{
	size_t pos = 0U;
	while(pos < nchildren)
	{
		const dir_entry_t *const entry = &entries[pos];
		if(entry->type == FT_DIR && !is_parent_dir(entry->name))
		{
			char full_path[PATH_MAX];
			struct stat s;

			get_full_path_of(entry, sizeof(full_path), full_path);
			if(os_stat(full_path, &s) != 0 || entry->mtime != s.st_mtime)
			{
				return 1;
			}

			if(tree_has_changed(entry + 1, entry->child_count))
			{
				return 1;
			}
		}

		pos += entry->child_count + 1;
	}
	return 0;
}

int
cd_is_possible(const char *path)
{
	if(!is_valid_dir(path))
	{
		LOG_SERROR_MSG(errno, "Can't access \"%s\"", path);

		show_error_msgf("Destination doesn't exist", "\"%s\"", path);
		return 0;
	}
	else if(!directory_accessible(path))
	{
		LOG_SERROR_MSG(errno, "Can't access(, X_OK) \"%s\"", path);

		show_error_msgf("Permission denied", "\"%s\"", path);
		return 0;
	}
	else
	{
		return 1;
	}
}

void
load_saving_pos(FileView *view, int reload)
{
	char full_path[PATH_MAX];

	if(curr_stats.load_stage < 2)
		return;

	if(!window_shows_dirlist(view))
		return;

	if(view->local_filter.in_progress)
	{
		return;
	}

	get_current_full_path(view, sizeof(full_path), full_path);

	load_dir_list_internal(view, reload, 1);

	flist_goto_by_path(view, full_path);

	fview_cursor_redraw(view);

	if(curr_stats.number_of_windows != 1 || view == curr_view)
	{
		refresh_view_win(view);
	}
}

int
window_shows_dirlist(const FileView *const view)
{
	if(curr_stats.number_of_windows == 1 && view == other_view)
	{
		return 0;
	}

	if(view->explore_mode)
	{
		return 0;
	}

	if(view == other_view && vle_mode_is(VIEW_MODE))
	{
		return 0;
	}

	if(view == other_view && curr_stats.view)
	{
		return 0;
	}

	if(NONE(vle_primary_mode_is, NORMAL_MODE, VISUAL_MODE, VIEW_MODE,
				CMDLINE_MODE))
	{
		return 0;
	}

	return 1;
}

void
change_sort_type(FileView *view, char type, char descending)
{
	if(cv_compare(view->custom.type))
	{
		return;
	}

	view->sort[0] = descending ? -type : type;
	memset(&view->sort[1], SK_NONE, sizeof(view->sort) - 1);
	memcpy(&view->sort_g[0], &view->sort[0], sizeof(view->sort_g));

	fview_sorting_updated(view);

	load_sort_option(view);

	ui_view_schedule_reload(view);
}

int
pane_in_dir(const FileView *view, const char path[])
{
	return paths_are_same(view->curr_dir, path);
}

int
cd(FileView *view, const char base_dir[], const char path[])
{
	char dir[PATH_MAX];
	char canonic_dir[PATH_MAX];
	int updir;

	pick_cd_path(view, base_dir, path, &updir, dir, sizeof(dir));
	to_canonic_path(dir, base_dir, canonic_dir, sizeof(canonic_dir));

	if(updir)
	{
		cd_updir(view, 1);
	}
	else if(!cd_is_possible(canonic_dir) ||
			change_directory(view, canonic_dir) < 0)
	{
		return 0;
	}

	load_dir_list(view, 0);
	if(view == curr_view)
	{
		fview_cursor_redraw(view);
	}
	else
	{
		draw_dir_list(other_view);
		refresh_view_win(other_view);
	}
	return 0;
}

/* Picks new directory or requested going up one level judging from supplied
 * base directory, desired location and current location of the view. */
TSTATIC void
pick_cd_path(FileView *view, const char base_dir[], const char path[],
		int *updir, char buf[], size_t buf_size)
{
	char *arg;

	*updir = 0;

	if(is_null_or_empty(path))
	{
		copy_str(buf, buf_size, cfg.home_dir);
		return;
	}

	arg = expand_tilde(path);

#ifndef _WIN32
	if(is_path_absolute(arg))
		copy_str(buf, buf_size, arg);
#else
	if(is_path_absolute(arg) && *arg != '/')
		copy_str(buf, buf_size, arg);
	else if(*arg == '/' && is_unc_root(arg))
		copy_str(buf, buf_size, arg);
	else if(*arg == '/' && is_unc_path(arg))
		copy_str(buf, buf_size, arg);
	else if(*arg == '/' && is_unc_path(base_dir))
		sprintf(buf + strlen(buf), "/%s", arg + 1);
	else if(strcmp(arg, "/") == 0 && is_unc_path(base_dir))
		copy_str(buf, strchr(base_dir + 2, '/') - base_dir + 1, base_dir);
	else if(*arg == '/')
		snprintf(buf, buf_size, "%c:%s", base_dir[0], arg);
#endif
	else if(strcmp(arg, "-") == 0)
		copy_str(buf, buf_size, view->last_dir);
	else if(is_parent_dir(arg) && stroscmp(base_dir, flist_get_dir(view)) == 0)
		*updir = 1;
	else
		find_dir_in_cdpath(base_dir, arg, buf, buf_size);
	free(arg);
}

/* Searches for an existing directory in cdpath and fills the buf with final
 * absolute path. */
static void
find_dir_in_cdpath(const char base_dir[], const char dst[], char buf[],
		size_t buf_size)
{
	char *free_this;
	char *part, *state;

	if(is_builtin_dir(dst) || starts_with_lit(dst, "./") ||
			starts_with_lit(dst, "../"))
	{
		snprintf(buf, buf_size, "%s/%s", base_dir, dst);
		return;
	}

	part = strdup(cfg.cd_path);
	free_this = part;

	state = NULL;
	while((part = split_and_get(part, ',', &state)) != NULL)
	{
		snprintf(buf, buf_size, "%s/%s", expand_tilde(part), dst);

		if(is_dir(buf))
		{
			free(free_this);
			return;
		}
	}
	free(free_this);

	snprintf(buf, buf_size, "%s/%s", base_dir, dst);
}

int
view_needs_cd(const FileView *view, const char path[])
{
	if(path[0] == '\0' || stroscmp(view->curr_dir, path) == 0)
	{
		return 0;
	}
	return 1;
}

void
set_view_path(FileView *view, const char path[])
{
	if(view_needs_cd(view, path))
	{
		copy_str(view->curr_dir, sizeof(view->curr_dir), path);
		exclude_file_name(view->curr_dir);
	}
}

uint64_t
get_file_size_by_entry(const FileView *view, size_t pos)
{
	uint64_t size = 0;
	const dir_entry_t *const entry = &view->dir_entry[pos];

	size = DCACHE_UNKNOWN;
	if(is_directory_entry(entry))
	{
		dcache_get_of(entry, &size, NULL);
	}

	return (size == DCACHE_UNKNOWN) ? entry->size : size;
}

int
is_directory_entry(const dir_entry_t *entry)
{
	if(entry->type == FT_DIR)
	{
		return 1;
	}

	if(entry->type == FT_LINK)
	{
		char full_path[PATH_MAX];
		get_full_path_of(entry, sizeof(full_path), full_path);
		return (get_symlink_type(full_path) != SLT_UNKNOWN);
	}

	return 0;
}

int
iter_selected_entries(FileView *view, dir_entry_t **entry)
{
	return iter_entries(view, entry, &is_entry_selected);
}

int
iter_active_area(FileView *view, dir_entry_t **entry)
{
	dir_entry_t *const curr = get_current_entry(view);
	if(!curr->selected)
	{
		*entry = (*entry == NULL && fentry_is_valid(curr)) ? curr : NULL;
		return *entry != NULL;
	}
	return iter_selected_entries(view, entry);
}

int
iter_marked_entries(FileView *view, dir_entry_t **entry)
{
	return iter_entries(view, entry, &is_entry_marked);
}

/* Implements iteration over the entries which match specific predicate.
 * Returns non-zero if matching entry is found and is loaded to *entry,
 * otherwise it's set to NULL and zero is returned. */
static int
iter_entries(FileView *view, dir_entry_t **entry, entry_predicate pred)
{
	int next = (*entry == NULL) ? 0 : (*entry - view->dir_entry + 1);

	while(next < view->list_rows)
	{
		dir_entry_t *const e = &view->dir_entry[next];
		if(fentry_is_valid(e) && pred(e))
		{
			*entry = e;
			return 1;
		}
		++next;
	}

	*entry = NULL;
	return 0;
}

int
is_entry_selected(const dir_entry_t *entry)
{
	return entry->selected;
}

int
is_entry_marked(const dir_entry_t *entry)
{
	return entry->marked;
}

/* Same as iter_selected_entries() function, but when selection is absent
 * current file is processed. */
int
iter_selection_or_current(FileView *view, dir_entry_t **entry)
{
	if(view->selected_files == 0)
	{
		dir_entry_t *const curr = get_current_entry(view);
		*entry = (*entry == NULL && fentry_is_valid(curr)) ? curr : NULL;
		return *entry != NULL;
	}
	return iter_selected_entries(view, entry);
}

int
entry_to_pos(const FileView *view, const dir_entry_t *entry)
{
	const int pos = entry - view->dir_entry;
	return (pos >= 0 && pos < view->list_rows) ? pos : -1;
}

void
get_current_full_path(const FileView *view, size_t buf_len, char buf[])
{
	get_full_path_at(view, view->list_pos, buf_len, buf);
}

void
get_full_path_at(const FileView *view, int pos, size_t buf_len, char buf[])
{
	get_full_path_of(&view->dir_entry[pos], buf_len, buf);
}

void
get_full_path_of(const dir_entry_t *entry, size_t buf_len, char buf[])
{
	snprintf(buf, buf_len, "%s%s%s", entry->origin,
			ends_with_slash(entry->origin) ? "" : "/", entry->name);
}

void
get_short_path_of(const FileView *view, const dir_entry_t *entry, int format,
		size_t buf_len, char buf[])
{
	char name[NAME_MAX];
	const char *path = entry->origin;

	char *free_this = NULL;
	const char *root_path = flist_get_dir(view);

	if(fentry_is_fake(entry))
	{
		copy_str(buf, buf_len, "");
		return;
	}

	if(format && view->custom.type == CV_TREE && ui_view_displays_columns(view) &&
			entry->child_pos != 0)
	{
		const dir_entry_t *const parent = entry - entry->child_pos;
		free_this = format_str("%s/%s", parent->origin, parent->name);
		root_path = free_this;
	}

	if(format)
	{
		/* XXX: decorations should apply to whole shortened paths? */
		format_entry_name(entry, sizeof(name), name);
	}
	else
	{
		copy_str(name, sizeof(name), entry->name);
	}

	if(is_parent_dir(entry->name))
	{
		copy_str(buf, buf_len, name);
		free(free_this);
		return;
	}

	if(!path_starts_with(path, root_path))
	{
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
		copy_str(buf, buf_len, full_path);
		free(free_this);
		return;
	}

	assert(strlen(path) >= strlen(root_path) && "Path is too short.");

	path += strlen(root_path);
	path = skip_char(path, '/');
	if(path[0] == '\0')
	{
		copy_str(buf, buf_len, name);
	}
	else
	{
		snprintf(buf, buf_len, "%s/%s", path, name);
	}

	free(free_this);
}

void
check_marking(FileView *view, int count, const int indexes[])
{
	if(count != 0)
	{
		mark_files_at(view, count, indexes);
	}
	else if(view->selected_files != 0)
	{
		mark_selected(view);
	}
	else
	{
		clear_marking(view);
		get_current_entry(view)->marked = 1;
	}
}

void
mark_files_at(FileView *view, int count, const int indexes[])
{
	int i;

	clear_marking(view);

	for(i = 0; i < count; ++i)
	{
		view->dir_entry[indexes[i]].marked = 1;
	}
}

static void
clear_marking(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].marked = 0;
	}
}

int
mark_selected(FileView *view)
{
	int i;
	int nmarked = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].marked = view->dir_entry[i].selected;
		if(view->dir_entry[i].marked)
		{
			++nmarked;
		}
	}
	return nmarked;
}

int
mark_selection_or_current(FileView *view)
{
	dir_entry_t *const curr = get_current_entry(view);
	if(view->selected_files == 0 && fentry_is_valid(curr))
	{
		curr->selected = 1;
		view->selected_files = 1;
	}
	return mark_selected(view);
}

int
flist_count_marked(FileView *const view)
{
	int i;
	int count = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		const dir_entry_t *const entry = &view->dir_entry[i];
		count += (entry->marked && fentry_is_valid(entry));
	}
	return count;
}

void
flist_set(FileView *view, const char title[], const char path[], char *lines[],
		int nlines)
{
	int i;

	if(vifm_chdir(path) != 0)
	{
		show_error_msgf("Custom view", "Can't change directory: %s", path);
		return;
	}

	flist_custom_start(view, "-");

	for(i = 0; i < nlines; ++i)
	{
		flist_add_custom_line(view, lines[i]);
	}

	flist_end_custom(view, 1);
}

void
flist_add_custom_line(FileView *view, const char line[])
{
	int line_num;
	/* Skip empty lines. */
	char *const path = (skip_whitespace(line)[0] == '\0')
	                 ? NULL
	                 : parse_file_spec(line, &line_num);
	if(path != NULL)
	{
		flist_custom_add(view, path);
		free(path);
	}
}

void
flist_end_custom(FileView *view, int very)
{
	if(flist_custom_finish(view, very ? CV_VERY : CV_REGULAR, 0) != 0)
	{
		show_error_msg("Custom view", "Ignoring empty list of files");
		return;
	}

	flist_set_pos(view, 0);
}

void
fentry_rename(FileView *view, dir_entry_t *entry, const char to[])
{
	char *const old_name = entry->name;

	/* Rename file in internal structures for correct positioning of cursor
	 * after reloading, as cursor will be positioned on the file with the same
	 * name. */
	entry->name = strdup(to);
	if(entry->name == NULL)
	{
		entry->name = old_name;
		return;
	}

	/* Name change can affect name specific highlight and decorations, so reset
	 * the caches. */
	entry->hi_num = -1;
	entry->name_dec_num = -1;

	if(flist_custom_active(view) && is_directory_entry(entry))
	{
		int i;
		char *const root = format_str("%s/%s", entry->origin, old_name);
		const size_t root_len = strlen(root);

		for(i = 0; i < view->list_rows; ++i)
		{
			dir_entry_t *const e = &view->dir_entry[i];
			if(path_starts_with(e->origin, root))
			{
				char *const new_origin = format_str("%s/%s%s", entry->origin, to,
						e->origin + root_len);
				chosp(new_origin);
				if(e->origin != view->curr_dir)
				{
					free(e->origin);
				}
				e->origin = new_origin;
			}
		}

		free(root);
	}

	free(old_name);
}

int
fentry_is_fake(const dir_entry_t *entry)
{
	return entry->name[0] == '\0';
}

int
fentry_is_valid(const dir_entry_t *entry)
{
	return !fentry_is_fake(entry) && !is_parent_dir(entry->name);
}

int
flist_load_tree(FileView *view, const char path[])
{
	if(flist_load_tree_internal(view, path, 0) != 0)
	{
		return 1;
	}

	ui_view_schedule_redraw(view);
	return 0;
}

int
flist_clone_tree(FileView *to, const FileView *from)
{
	if(make_tree(to, flist_get_dir(from), 0, from->custom.excluded_paths) != 0)
	{
		return 1;
	}

	trie_free(to->custom.excluded_paths);
	to->custom.excluded_paths = trie_clone(from->custom.excluded_paths);
	return 0;
}

/* Implements tree view (re)loading.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
flist_load_tree_internal(FileView *view, const char path[], int reload)
{
	trie_t *excluded_paths = reload ? view->custom.excluded_paths : NULL;

	if(make_tree(view, path, reload, excluded_paths) != 0)
	{
		return 1;
	}

	if(!reload)
	{
		trie_free(view->custom.excluded_paths);
		view->custom.excluded_paths = trie_create();
	}
	return 0;
}

/* (Re)loads tree at path into the view using specified list of excluded files.
 * Returns zero on success, otherwise non-zero is returned. */
static int
make_tree(FileView *view, const char path[], int reload, trie_t *excluded_paths)
{
	char canonic_path[PATH_MAX];
	int nfiltered;

	flist_custom_start(view, "tree");

	show_progress("Building tree...", 0);

	ui_cancellation_reset();
	ui_cancellation_enable();
	nfiltered = add_files_recursively(view, path, excluded_paths, -1, 0);
	ui_cancellation_disable();

	ui_sb_quick_msg_clear();

	if(ui_cancellation_requested())
	{
		return 1;
	}

	if(nfiltered < 0)
	{
		show_error_msg("Tree View", "Failed to list directory");
		return 1;
	}

	to_canonic_path(path, flist_get_dir(view), canonic_path,
			sizeof(canonic_path));

	if(flist_custom_finish_internal(view, CV_TREE, reload, canonic_path, 1) != 0)
	{
		return 1;
	}
	view->filtered = nfiltered;

	replace_string(&view->custom.orig_dir, canonic_path);

	return 0;
}

/* Adds custom view entries corresponding to file system tree.  parent_pos is
 * expected to be negative for the outermost invocation.  Returns number of
 * filtered out files on success or partial success and negative value on
 * serious error. */
static int
add_files_recursively(FileView *view, const char path[], trie_t *excluded_paths,
		int parent_pos, int no_direct_parent)
{
	int i;
	const int prev_count = view->custom.entry_count;
	int nfiltered = 0;

	int len;
	char **lst = list_all_files(path, &len);
	if(len < 0)
	{
		return -1;
	}

	for(i = 0; i < len && !ui_cancellation_requested(); ++i)
	{
		int dir;
		void *dummy;
		dir_entry_t *entry;
		char *const full_path = format_str("%s/%s", path, lst[i]);

		if(trie_get(excluded_paths, full_path, &dummy) == 0)
		{
			free(full_path);
			continue;
		}

		dir = is_dir(full_path);
		if(!file_is_visible(view, lst[i], dir, NULL, 1))
		{
			/* Traverse directory (but not symlink to it) even if we're skipping it,
			 * because we might need files that are inside of it. */
			if(dir && !is_symlink(full_path) &&
					file_is_visible(view, lst[i], dir, NULL, 0))
			{
				nfiltered += add_files_recursively(view, full_path, excluded_paths,
						parent_pos, 1);
			}

			free(full_path);
			++nfiltered;
			continue;
		}

		entry = flist_custom_add(view, full_path);
		if(entry == NULL)
		{
			free(full_path);
			free_string_array(lst, len);
			return -1;
		}

		if(parent_pos >= 0)
		{
			entry->child_pos = (view->custom.entry_count - 1) - parent_pos;
		}

		/* Not using dir variable here, because it is set for symlinks to
		 * directories as well. */
		if(entry->type == FT_DIR)
		{
			const int idx = view->custom.entry_count - 1;
			const int filtered = add_files_recursively(view, full_path,
					excluded_paths, idx, 0);
			/* Keep going in case of error and load partial list. */
			if(filtered >= 0)
			{
				/* If one of recursive calls returned error, keep going and build
				 * partial tree. */
				view->custom.entries[idx].child_count = (view->custom.entry_count - 1)
				                                      - idx;
				nfiltered += filtered;
			}
		}

		free(full_path);

		show_progress("Building tree...", 10000);
	}

	free_string_array(lst, len);

	/* The prev_count != 0 check is to make sure that we won't create leaf instead
	 * of the whole tree (this is handled in flist_custom_finish()). */
	if(!no_direct_parent && prev_count != 0 &&
			view->custom.entry_count == prev_count)
	{
		/* To be able to perform operations inside directory (e.g., create files),
		 * we need at least one element there. */
		if(add_directory_leaf(view, path, parent_pos) != 0)
		{
			return -1;
		}
	}

	return nfiltered;
}

/* Checks whether file is visible according to dot and filename filters.  is_dir
 * is used when data is NULL, otherwise data_is_dir_entry() called (this is an
 * optimization).  Returns non-zero if so, otherwise zero is returned. */
static int
file_is_visible(FileView *view, const char filename[], int is_dir,
		const void *data, int apply_local_filter)
{
	if(view->hide_dot && filename[0] == '.')
	{
		return 0;
	}

	if(data != NULL)
	{
		is_dir = data_is_dir_entry(data);
	}

	return apply_local_filter
	     ? filters_file_is_visible(view, filename, is_dir)
	     : filters_file_is_filtered(view, filename, is_dir);
}

/* Adds ".." directory leaf of an empty directory to the tree which is being
 * built.  Returns non-zero on error, otherwise zero is returned. */
static int
add_directory_leaf(FileView *view, const char path[], int parent_pos)
{
	char *full_path;
	dir_entry_t *entry;

	/* If local filter isn't empty, assume that user is looking for something and
	 * leafs will get in his way. */
	if(!filter_is_empty(&view->local_filter.filter))
	{
		return 0;
	}

	entry = alloc_dir_entry(&view->custom.entries, view->custom.entry_count);
	if(entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 1;
	}

	full_path = format_str("%s/..", path);
	if(init_parent_entry(view, entry, full_path) != 0)
	{
		free(full_path);
		/* Keep going in case of error and load partial list. */
		return 0;
	}

	remove_last_path_component(full_path);
	entry->origin = full_path;

	if(parent_pos >= 0)
	{
		entry->child_pos = (view->custom.entry_count - 1) - parent_pos;
	}

	++view->custom.entry_count;
	return 0;
}

/* Fills given entry of the view at specified path (can be relative, i.e. "..").
 * Returns zero on success, otherwise non-zero is returned. */
static int
init_parent_entry(FileView *view, dir_entry_t *entry, const char path[])
{
	struct stat s;

	init_dir_entry(view, entry, get_last_path_component(path));
	entry->type = FT_DIR;

	/* Load the inode info or leave blank values in entry. */
	if(os_lstat(path, &s) != 0)
	{
		free_dir_entry(view, entry);
		LOG_SERROR_MSG(errno, "Can't lstat() \"%s\"", path);
		log_cwd();
		return 1;
	}

#ifndef _WIN32
	entry->size = (uintmax_t)s.st_size;
	entry->mode = s.st_mode;
	entry->uid = s.st_uid;
	entry->gid = s.st_gid;
#else
	/* Windows doesn't like returning size of directories even if it can. */
	entry->size = get_file_size(entry->name);
#endif
	entry->mtime = s.st_mtime;
	entry->atime = s.st_atime;
	entry->ctime = s.st_ctime;
	entry->nlinks = s.st_nlink;

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
