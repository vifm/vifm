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
#include <limits.h> /* INT_MAX INT_MIN */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* intptr_t uint64_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* memcmp() memcpy() memset() strcat() strcmp() strcpy()
                       strdup() strlen() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "compat/pthread.h"
#include "engine/autocmds.h"
#include "engine/mode.h"
#include "int/fuse.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "modes/view.h"
#include "ui/cancellation.h"
#include "ui/column_view.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/statusline.h"
#include "ui/tabs.h"
#include "ui/ui.h"
#include "utils/dynarray.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/fsdata.h"
#include "utils/fswatch.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/matcher.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/trie.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "filtering.h"
#include "flist_hist.h"
#include "flist_pos.h"
#include "flist_sel.h"
#include "fops_misc.h"
#include "macros.h"
#include "marks.h"
#include "opt_handlers.h"
#include "registers.h"
#include "running.h"
#include "sort.h"
#include "status.h"
#include "types.h"

/* State of a fold. */
typedef enum
{
	FOLD_UNDEFINED,   /* Indication of undefined state. */
	FOLD_USER_OPENED, /* User manually opened the fold. */
	FOLD_USER_CLOSED, /* User manually closed the fold. */
	FOLD_AUTO_OPENED, /* An auto-closed fold that was opened the first time. */
	FOLD_AUTO_CLOSED, /* Closed on reaching depth limit on building or after
	                     opening such a fold ("lazy unfolding"). */
}
FoldState;

static void init_flist(view_t *view);
static void reset_view(view_t *view);
static void init_view_history(view_t *view);
static int navigate_to_file_in_custom_view(view_t *view, const char dir[],
		const char file[]);
static int fill_dir_entry_by_path(dir_entry_t *entry, const char path[]);
static void on_custom_view_leave(view_t *view);
#ifndef _WIN32
static int fill_dir_entry(dir_entry_t *entry, const char path[],
		const struct dirent *d);
static int data_is_dir_entry(const struct dirent *d, const char path[]);
#else
static int fill_dir_entry(dir_entry_t *entry, const char path[],
		const WIN32_FIND_DATAW *ffd);
static int data_is_dir_entry(const WIN32_FIND_DATAW *ffd, const char path[]);
#endif
static int flist_custom_finish_internal(view_t *view, CVType type, int reload,
		const char dir[], int allow_empty);
static void on_location_change(view_t *view, int force);
static void disable_view_sorting(view_t *view);
static void enable_view_sorting(view_t *view);
static void exclude_in_compare(view_t *view, int selection_only);
static void mark_group(view_t *view, view_t *other, int idx);
static int got_excluded(view_t *view, const dir_entry_t *entry, void *arg);
static int exclude_temporary_entries(view_t *view);
static int is_temporary(view_t *view, const dir_entry_t *entry, void *arg);
static void flist_custom_drop_save(view_t *view);
static uint64_t recalc_entry_size(const dir_entry_t *entry, uint64_t old_size);
static uint64_t entry_calc_nitems(const dir_entry_t *entry);
static void load_dir_list_internal(view_t *view, int reload, int draw_only);
static int populate_dir_list_internal(view_t *view, int reload);
static int populate_custom_view(view_t *view, int reload);
static void re_apply_folds(view_t *view, trie_t *folded_paths);
static int entry_exists(view_t *view, const dir_entry_t *entry, void *arg);
static void zap_compare_view(view_t *view, view_t *other, zap_filter filter,
		void *arg);
static int find_separator(view_t *view, int idx);
static void update_dir_watcher(view_t *view);
static int custom_list_is_incomplete(const view_t *view);
static int is_dead_or_filtered(view_t *view, const dir_entry_t *entry,
		void *arg);
static void update_entries_data(view_t *view);
static void fix_tree_links(dir_entry_t *entries, dir_entry_t *entry,
		int old_idx, int new_idx, int displacement, int correction);
static int is_dir_big(const char path[]);
static void free_view_entries(view_t *view);
static int update_dir_list(view_t *view, int reload);
static void start_dir_list_change(view_t *view, dir_entry_t **entries, int *len,
		int reload);
static void finish_dir_list_change(view_t *view, dir_entry_t *entries, int len);
static int add_file_entry_to_view(const char name[], const void *data,
		void *param);
static void sort_dir_list(int msg, view_t *view);
static void merge_lists(view_t *view, dir_entry_t *entries, int len);
TSTATIC void check_file_uniqueness(view_t *view);
static void add_to_trie(trie_t *trie, view_t *view, dir_entry_t *entry);
static int is_in_trie(trie_t *trie, view_t *view, dir_entry_t *entry,
		void **data);
static void merge_entries(dir_entry_t *new, const dir_entry_t *prev);
static int correct_pos(view_t *view, int pos, int dist, int closest);
static int rescue_from_empty_filelist(view_t *view);
static void add_parent_entry(view_t *view, dir_entry_t **entries, int *count);
static void init_dir_entry(view_t *view, dir_entry_t *entry, const char name[]);
static dir_entry_t * alloc_dir_entry(dir_entry_t **list, int list_size);
static int tree_has_changed(const dir_entry_t *entries, size_t nchildren);
static FSWatchState poll_watcher(fswatch_t *watch, const char path[]);
static void remove_child_entries(view_t *view, dir_entry_t *entry);
static void find_dir_in_cdpath(const char base_dir[], const char dst[],
		char buf[], size_t buf_size);
static entries_t list_sibling_dirs(view_t *view);
static entries_t flist_list_in(view_t *view, const char path[], int only_dirs,
		int can_include_parent);
static dir_entry_t * pick_sibling(view_t *view, entries_t parent_dirs,
		int offset, int wrap, int *wrapped);
static int iter_entries(view_t *view, dir_entry_t **entry, entry_predicate pred,
		int valid_only);
static int mark_selected(view_t *view);
static int set_position_by_path(view_t *view, const char path[]);
static int flist_load_tree_internal(view_t *view, const char path[], int reload,
		int depth);
static int make_tree(view_t *view, const char path[], int reload,
		trie_t *excluded_paths, trie_t *folded_paths, int depth);
static void tree_from_cv(view_t *view);
static int complete_tree(const char name[], int valid, const void *parent_data,
		void *data, void *arg);
static void reset_entry_list(view_t *view, dir_entry_t **entries, int *count);
static void drop_tops(dir_entry_t *entries, int *nentries, int extra);
static int add_files_recursively(view_t *view, const char path[],
		trie_t *excluded_paths, trie_t *folded_paths, int parent_pos,
		int no_direct_parent, int depth);
static FoldState get_fold_state(trie_t *folded_paths, const char full_path[]);
static int set_fold_state(trie_t *folded_paths, const char full_path[],
		FoldState state);
static int entry_is_visible(view_t *view, const char name[], const void *data);
static int tree_candidate_is_visible(view_t *view, const char path[],
		const char name[], int is_dir, int apply_local_filter);
static int add_directory_leaf(view_t *view, const char path[], int parent_pos);
static int init_parent_entry(view_t *view, dir_entry_t *entry,
		const char path[]);

void
init_filelists(void)
{
	fview_setup();

	flist_init_view(&rwin);
	flist_init_view(&lwin);

	curr_view = &lwin;
	other_view = &rwin;
}

void
flist_init_view(view_t *view)
{
	init_flist(view);
	fview_init(view);

	reset_view(view);

	init_view_history(view);
	fview_sorting_updated(view);
}

/* Initializes file list part of the view. */
static void
init_flist(view_t *view)
{
	view->list_pos = 0;
	view->last_seen_pos = -1;
	view->history_num = 0;
	view->history_pos = 0;
	view->on_slow_fs = 0;
	view->has_dups = 0;

	view->watched_dir = NULL;
	view->last_dir = NULL;

	view->matches = 0;

	view->custom.entries = NULL;
	view->custom.entry_count = 0;
	view->custom.orig_dir = NULL;
	view->custom.title = NULL;

	/* Load fake empty element to make dir_entry valid. */
	view->dir_entry = dynarray_cextend(NULL, sizeof(dir_entry_t));
	view->dir_entry[0].name = strdup("");
	view->dir_entry[0].type = FT_DIR;
	view->dir_entry[0].hi_num = -1;
	view->dir_entry[0].name_dec_num = -1;
	view->dir_entry[0].origin = &view->curr_dir[0];
	view->list_rows = 1;
}

void
flist_free_view(view_t *view)
{
	/* For the application, we don't need to zero out fields after freeing them,
	 * but doing so allows reusing this function in tests. */

	free_dir_entries(&view->dir_entry, &view->list_rows);
	free_dir_entries(&view->custom.entries, &view->custom.entry_count);

	update_string(&view->custom.next_title, NULL);
	update_string(&view->custom.orig_dir, NULL);
	update_string(&view->custom.title, NULL);
	trie_free(view->custom.excluded_paths);
	trie_free(view->custom.folded_paths);
	trie_free(view->custom.paths_cache);
	view->custom.excluded_paths = NULL;
	view->custom.folded_paths = NULL;
	view->custom.paths_cache = NULL;

	free_dir_entries(&view->custom.full.entries, &view->custom.full.nentries);

	/* Two pointer fields below don't contain valid data that needs to be freed,
	 * zeroing them for tests and to at least mention them to signal that they
	 * weren't forgotten. */
	view->local_filter.unfiltered = NULL;
	view->local_filter.saved = NULL;
	view->local_filter.unfiltered_count = 0;

	update_string(&view->local_filter.prev, NULL);
	free(view->local_filter.poshist);
	view->local_filter.poshist = NULL;

	filter_dispose(&view->local_filter.filter);
	filter_dispose(&view->auto_filter);
	matcher_free(view->manual_filter);
	view->manual_filter = NULL;
	update_string(&view->prev_manual_filter, NULL);
	update_string(&view->prev_auto_filter, NULL);

	view->custom.type = CV_REGULAR;

	update_string(&view->file_preview_clear_cmd, NULL);

	fswatch_free(view->watch);
	view->watch = NULL;
	update_string(&view->watched_dir, NULL);

	update_string(&view->last_dir, NULL);

	flist_free_cache(&view->left_column);
	flist_free_cache(&view->right_column);

	update_string(&view->last_curr_file, NULL);

	free_string_array(view->saved_selection, view->nsaved_selection);
	view->nsaved_selection = 0;
	view->saved_selection = NULL;

	flist_hist_resize(view, 0);

	cs_reset(&view->cs);

	columns_free(view->columns);
	view->columns = NULL;

	update_string(&view->view_columns, NULL);
	update_string(&view->view_columns_g, NULL);
	update_string(&view->sort_groups, NULL);
	update_string(&view->sort_groups_g, NULL);
	update_string(&view->preview_prg, NULL);
	update_string(&view->preview_prg_g, NULL);

	modview_info_free(view->vi);
	view->vi = NULL;

	regfree(&view->primary_group);

	marks_clear_view(view);

	pthread_mutex_destroy(view->timestamps_mutex);
	free(view->timestamps_mutex);
	view->timestamps_mutex = NULL;
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
reset_view(view_t *view)
{
	fview_reset(view);
	filters_view_reset(view);

	view->hide_dot_g = view->hide_dot = 1;

	memset(&view->sort[0], SK_NONE, sizeof(view->sort));
	ui_view_sort_list_ensure_well_formed(view, view->sort);
	memcpy(&view->sort_g[0], &view->sort[0], sizeof(view->sort_g));
}

/* Allocates memory for view history smartly (handles huge values). */
static void
init_view_history(view_t *view)
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
load_initial_directory(view_t *view, const char dir[])
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
get_current_entry(const view_t *view)
{
	if(view->list_pos < 0 || view->list_pos >= view->list_rows)
	{
		return NULL;
	}

	return &view->dir_entry[view->list_pos];
}

char *
get_current_file_name(view_t *view)
{
	if(view->list_pos == -1)
	{
		static char empty_string[1];
		return empty_string;
	}
	return get_current_entry(view)->name;
}

void
invert_sorting_order(view_t *view)
{
	if(memcmp(&view->sort_g[0], &view->sort[0], sizeof(view->sort_g)) == 0)
	{
		view->sort_g[0] = -view->sort_g[0];
	}
	view->sort[0] = -view->sort[0];
}

void
leave_invalid_dir(view_t *view)
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

int
navigate_to(view_t *view, const char path[])
{
	if(change_directory(view, path) < 0)
	{
		return 1;
	}

	/* Changing directory can require updating tab line. */
	stats_redraw_later();

	load_dir_list(view, 0);
	fview_cursor_redraw(view);
	return 0;
}

void
navigate_back(view_t *view)
{
	const char *const dest = flist_custom_active(view)
	                       ? view->custom.orig_dir
	                       : view->last_dir;
	if(dest != NULL)
	{
		navigate_to(view, dest);
	}
}

void
navigate_to_file(view_t *view, const char dir[], const char file[],
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
		(void)fpos_ensure_selected(view, file);
	}
}

/* navigate_to_file() helper that tries to find requested file in custom view.
 * Returns non-zero on failure to find such file, otherwise zero is returned. */
static int
navigate_to_file_in_custom_view(view_t *view, const char dir[],
		const char file[])
{
	char full_path[PATH_MAX + 1];

	snprintf(full_path, sizeof(full_path), "%s/%s", dir, file);

	if(custom_list_is_incomplete(view))
	{
		const dir_entry_t *entry = entry_from_path(view, view->custom.full.entries,
				view->custom.full.nentries, full_path);
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

	if(set_position_by_path(view, full_path) != 0)
	{
		/* File might not exist anymore at that location. */
		return 1;
	}

	ui_view_schedule_redraw(view);
	return 0;
}

int
change_directory(view_t *view, const char directory[])
{
	/* TODO: refactor this big function change_directory(). */

	char dir_dup[PATH_MAX + 1];
	const int was_in_custom_view = flist_custom_active(view);
	int location_changed;

	if(is_dir_list_loaded(view))
	{
		flist_hist_save(view);
	}

	if(is_path_absolute(directory))
	{
		canonicalize_path(directory, dir_dup, sizeof(dir_dup));
	}
	else
	{
		char newdir[PATH_MAX + 1];
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

	system_to_internal_slashes(dir_dup);

	if(cfg.chase_links)
	{
		char real_path[PATH_MAX + 1];
		if(os_realpath(dir_dup, real_path) == real_path)
		{
			/* Do this on success only, if realpath() fails, just go with the original
			 * path. */
			canonicalize_path(real_path, dir_dup, sizeof(dir_dup));
			system_to_internal_slashes(dir_dup);
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

	/* Check if we're exiting from a FUSE mounted top level directory. */
	if(is_parent_dir(directory) && fuse_is_mount_point(view->curr_dir))
	{
		/* No other pane in any tab should be inside subtree that might be
		 * unmounted. */
		if(tabs_visitor_count(view->curr_dir) == 1)
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
	if(view->last_dir != NULL && !is_root_dir(view->last_dir))
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
	{
		chosp(dir_dup);
	}

	flist_sel_view_to_reload(view, location_changed ? dir_dup : NULL);

	/* Need to use dir_dup instead of calling get_cwd() to avoid resolving
	 * symbolic links in path. */
	env_set("PWD", dir_dup);

	if(location_changed)
	{
		replace_string(&view->last_dir, flist_get_dir(view));
		view->on_slow_fs = is_on_slow_fs(dir_dup, cfg.slow_fs_list);
	}

	copy_str(view->curr_dir, sizeof(view->curr_dir), dir_dup);

	if(is_dir_list_loaded(view))
	{
		flist_hist_setup(view, NULL, "", -1, -1);
	}

	if(was_in_custom_view)
	{
		on_custom_view_leave(view);
	}

	if(location_changed || was_in_custom_view)
	{
		on_location_change(view, location_changed);
	}
	if(location_changed)
	{
		view->location_changed = 1;
	}
	return 0;
}

/* Performs additional actions on leaving custom view. */
static void
on_custom_view_leave(view_t *view)
{
	if(ui_view_unsorted(view))
	{
		enable_view_sorting(view);
	}

	if(cv_compare(view->custom.type))
	{
		view_t *const other = (view == curr_view) ? other_view : curr_view;

		/* Indicate that this is not a compare view anymore. */
		view->custom.type = CV_REGULAR;

		/* Leave compare mode in both views at the same time. */
		if(other->custom.type == CV_DIFF)
		{
			rn_leave(other, 1);
		}
	}

	trie_free(view->custom.excluded_paths);
	view->custom.excluded_paths = NULL;

	trie_free(view->custom.folded_paths);
	view->custom.folded_paths = NULL;
}

int
is_dir_list_loaded(view_t *view)
{
	dir_entry_t *const entry = (view->list_rows < 1) ? NULL : &view->dir_entry[0];
	return entry != NULL && entry->name[0] != '\0';
}

#ifdef _WIN32
/* Appends list of shares to filelist of the view.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
fill_with_shared(view_t *view)
{
	NET_API_STATUS res;
	wchar_t *wserver;

	wserver = to_wide(view->curr_dir + 2);
	if(wserver == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 1;
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

	return (res != ERROR_SUCCESS);
}
#endif

char *
get_typed_entry_fpath(const dir_entry_t *entry)
{
	const char *type_suffix;
	char full_path[PATH_MAX + 1];

	type_suffix = fentry_is_dir(entry) ? "/" : "";

	get_full_path_of(entry, sizeof(full_path), full_path);
	return format_str("%s%s", full_path, type_suffix);
}

int
flist_custom_active(const view_t *view)
{
	/* First check isn't enough on startup, which leads to false positives.  Yet
	 * this implicit condition seems to be preferable to omit introducing function
	 * that would terminate custom view mode. */
	return view->curr_dir[0] == '\0' && !is_null_or_empty(view->custom.orig_dir);
}

void
flist_custom_start(view_t *view, const char title[])
{
	free_dir_entries(&view->custom.entries, &view->custom.entry_count);
	(void)replace_string(&view->custom.next_title, title);

	trie_free(view->custom.paths_cache);
	view->custom.paths_cache = trie_create(/*free_func=*/NULL);
}

dir_entry_t *
flist_custom_add(view_t *view, const char path[])
{
	char canonic_path[PATH_MAX + 1];
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
flist_custom_put(view_t *view, dir_entry_t *entry)
{
	char full_path[PATH_MAX + 1];
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
flist_custom_add_separator(view_t *view, int id)
{
	dir_entry_t *const dir_entry = alloc_dir_entry(&view->custom.entries,
			view->custom.entry_count);
	if(dir_entry != NULL)
	{
		init_dir_entry(view, dir_entry, "");
		dir_entry->origin = strdup(flist_get_dir(view));
		dir_entry->owns_origin = 1;
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
		entry->type = (d == NULL) ? FT_UNK : type_from_dir_entry(d, path);
	}
	if(entry->type == FT_UNK)
	{
		LOG_ERROR_MSG("Can't determine type of \"%s\"", path);
		return 1;
	}

	entry->size = (uintmax_t)s.st_size;
	entry->uid = s.st_uid;
	entry->gid = s.st_gid;
	entry->mode = s.st_mode;
	entry->inode = s.st_ino;
	entry->mtime = s.st_mtime;
	entry->atime = s.st_atime;
	entry->ctime = s.st_ctime;
	entry->nlinks = s.st_nlink;

	if(entry->type == FT_LINK)
	{
		struct stat s;

		const SymLinkType symlink_type = get_symlink_type(path);
		entry->dir_link = (symlink_type != SLT_UNKNOWN);

		/* Query mode of symbolic link target. */
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
data_is_dir_entry(const struct dirent *d, const char path[])
{
	return is_dirent_targets_dir(path, d);
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
	entry->size = (ffd->nFileSizeHigh*((uint64_t)MAXDWORD + 1))
	            + ffd->nFileSizeLow;
	entry->attrs = ffd->dwFileAttributes;
	entry->mtime = win_to_unix_time(ffd->ftLastWriteTime);
	entry->atime = win_to_unix_time(ffd->ftLastAccessTime);
	entry->ctime = win_to_unix_time(ffd->ftCreationTime);

	if(is_win_symlink(ffd->dwFileAttributes, ffd->dwReserved0))
	{
		const SymLinkType symlink_type = get_symlink_type(path);
		entry->dir_link = (symlink_type != SLT_UNKNOWN);

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
data_is_dir_entry(const WIN32_FIND_DATAW *ffd, const char path[])
{
	return (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

#endif

int
flist_custom_finish(view_t *view, CVType type, int allow_empty)
{
	return flist_custom_finish_internal(view, type, 0, flist_get_dir(view),
			allow_empty);
}

/* Finishes file list population, handles empty resulting list corner case.
 * reload flag suppresses actions taken on location change.  dir is current
 * directory of the view.  Returns zero on success, otherwise (on empty list)
 * non-zero is returned. */
static int
flist_custom_finish_internal(view_t *view, CVType type, int reload,
		const char dir[], int allow_empty)
{
	enum { NORMAL, CUSTOM, UNSORTED } previous;
	const int empty_view = (view->custom.entry_count == 0);

	trie_free(view->custom.paths_cache);
	view->custom.paths_cache = NULL;

	if(empty_view && !allow_empty)
	{
		free_dir_entries(&view->custom.entries, &view->custom.entry_count);
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
			dir_entry->owns_origin = 1;
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
			flist_hist_save(view);
		}

		(void)replace_string(&view->custom.orig_dir, dir);
		view->curr_dir[0] = '\0';
	}

	/* Replace view file list with custom list. */
	free_dir_entries(&view->dir_entry, &view->list_rows);
	view->dir_entry = view->custom.entries;
	view->list_rows = view->custom.entry_count;
	view->custom.entries = NULL;
	view->custom.entry_count = 0;
	view->dir_entry = dynarray_shrink(view->dir_entry);
	view->filtered = 0;
	view->matches = 0;

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

	flist_custom_drop_save(view);

	if(!reload)
	{
		on_location_change(view, 0);
		if(cfg.cvoptions & CVO_AUTOCMDS)
		{
			vle_aucmd_execute("DirEnter", flist_get_dir(view), view);
		}
	}

	sort_dir_list(0, view);

	ui_view_schedule_redraw(view);
	fpos_ensure_valid_pos(view);

	return 0;
}

/* Perform actions on view location change.  Force activates all actions
 * unconditionally otherwise they are checked against cvoptions. */
static void
on_location_change(view_t *view, int force)
{
	view->has_dups = 0;

	flist_custom_drop_save(view);

	if(force || (cfg.cvoptions & CVO_LOCALFILTER))
	{
		filters_dir_updated(view);
	}
	/* Stage check is to skip body of the if in tests. */
	if(curr_stats.load_stage > 0 && (force || (cfg.cvoptions & CVO_LOCALOPTS)))
	{
		reset_local_options(view);
	}
}

/* Disables view sorting saving its state for the future. */
static void
disable_view_sorting(view_t *view)
{
	memcpy(&view->custom.sort[0], &view->sort[0], sizeof(view->custom.sort));
	memset(&view->sort[0], SK_NONE, sizeof(view->sort));
	load_sort_option(view);
}

/* Undoes was was done by disable_view_sorting(). */
static void
enable_view_sorting(view_t *view)
{
	memcpy(&view->sort[0], &view->custom.sort[0], sizeof(view->sort));
	load_sort_option(view);
}

void
flist_custom_exclude(view_t *view, int selection_only)
{
	flist_set_marking(view, 0);

	if(!flist_custom_active(view))
	{
		return;
	}

	if(cv_compare(view->custom.type))
	{
		exclude_in_compare(view, selection_only);
		return;
	}

	dir_entry_t *entry = NULL;
	trie_t *excluded = trie_create(/*free_func=*/NULL);
	while(iter_marked_entries(view, &entry))
	{
		char full_path[PATH_MAX + 1];

		entry->temporary = 1;

		get_full_path_of(entry, sizeof(full_path), full_path);
		(void)trie_put(excluded, full_path);

		if(cv_tree(view->custom.type))
		{
			(void)trie_put(view->custom.excluded_paths, full_path);
		}
	}

	/* Update copy of full list of entries (it might be empty, which is OK),
	 * because exclusion is not reversible. */
	(void)zap_entries(view, view->custom.full.entries,
			&view->custom.full.nentries, &got_excluded, excluded, 1, 1);
	trie_free(excluded);

	(void)exclude_temporary_entries(view);
}

/* Removes marked files from compare view.  Zero selection_only enables
 * excluding files that share ids with selected items. */
static void
exclude_in_compare(view_t *view, int selection_only)
{
	view_t *const other = (view == curr_view) ? other_view : curr_view;
	const int double_compare = (view->custom.type == CV_DIFF);
	const int n = other->list_rows;
	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
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
		rn_leave(view, 1);
	}
}

/* Selects all neighbours of the idx-th element that share its id. */
static void
mark_group(view_t *view, view_t *other, int idx)
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

/* zap_entries() filter to filter-out files, which were just excluded from the
 * view.  Returns non-zero if entry is to be kept and zero otherwise. */
static int
got_excluded(view_t *view, const dir_entry_t *entry, void *arg)
{
	void *data;
	trie_t *const excluded = arg;
	int excluded_entry;

	char full_path[PATH_MAX + 1];
	get_full_path_of(entry, sizeof(full_path), full_path);

	excluded_entry = (trie_get(excluded, full_path, &data) == 0);
	return !excluded_entry;
}

/* Excludes view entries that are marked as "temporary".  Returns number of
 * items that were visible before. */
static int
exclude_temporary_entries(view_t *view)
{
	const int n = zap_entries(view, view->dir_entry, &view->list_rows,
			&is_temporary, NULL, 0, 1);
	(void)zap_entries(view, view->custom.entries, &view->custom.entry_count,
			&is_temporary, NULL, 1, 1);

	fpos_ensure_valid_pos(view);
	ui_view_schedule_redraw(view);

	return n;
}

/* zap_entries() filter to filter-out files, which were marked for removal.
 * Returns non-zero if entry is to be kept and zero otherwise.*/
static int
is_temporary(view_t *view, const dir_entry_t *entry, void *arg)
{
	return !entry->temporary;
}

void
flist_custom_clone(view_t *to, const view_t *from, int as_tree)
{
	dir_entry_t *dst, *src;
	int nentries;
	int i, j;
	const int from_tree = cv_tree(from->custom.type);

	assert(flist_custom_active(from) && to->custom.paths_cache == NULL &&
			"Wrong state of destination view.");

	replace_string(&to->custom.orig_dir, from->custom.orig_dir);
	to->curr_dir[0] = '\0';

	replace_string(&to->custom.title,
			(from_tree && !as_tree) ? "from tree" : from->custom.title);
	to->custom.type = (ui_view_unsorted(from) || from_tree)
	                ? (as_tree ? CV_CUSTOM_TREE : CV_VERY)
	                : CV_REGULAR;
	const int dst_is_tree = cv_tree(to->custom.type);

	if(custom_list_is_incomplete(from))
	{
		src = from->custom.full.entries;
		nentries = from->custom.full.nentries;
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
		if(!dst_is_tree && src[i].child_pos != 0 && is_parent_dir(src[i].name))
		{
			continue;
		}

		dst[j] = src[i];
		dst[j].name = strdup(dst[j].name);
		dst[j].origin = (dst[j].owns_origin ? strdup(dst[j].origin) : to->curr_dir);

		if(!dst_is_tree)
		{
			/* As destination pane won't be a tree, erase tree-specific data, because
			 * some tree-specific code is driven directly by these fields. */
			dst[j].child_count = 0;
			dst[j].child_pos = 0;
			dst[j].folded = 0;
		}

		++j;
	}

	free_dir_entries(&to->custom.entries, &to->custom.entry_count);
	free_dir_entries(&to->dir_entry, &to->list_rows);
	to->dir_entry = dst;
	to->list_rows = j;

	to->filtered = 0;

	if(ui_view_unsorted(to))
	{
		disable_view_sorting(to);
	}
}

void
flist_custom_uncompress_tree(view_t *view)
{
	unsigned int i;

	assert(cv_tree(view->custom.type) && "This function is for tree-view only!");

	dir_entry_t *entries = view->dir_entry;
	size_t nentries = view->list_rows;
	int restore_parent = 0;

	fsdata_t *const tree = fsdata_create(0, 0);

	view->dir_entry = NULL;
	view->list_rows = 0;

	for(i = 0U; i < nentries; ++i)
	{
		char full_path[PATH_MAX + 1];
		void *data = &entries[i];

		if(entries[i].child_pos == 0 && is_parent_dir(entries[i].name))
		{
			fentry_free(&entries[i]);
			restore_parent = 1;
			continue;
		}

		get_full_path_of(&entries[i], sizeof(full_path), full_path);
		fsdata_set(tree, full_path, &data, sizeof(data));
	}

	if(fsdata_traverse(tree, &complete_tree, view) != 0)
	{
		reset_entry_list(view, &view->dir_entry, &view->list_rows);
		restore_parent = 0;
	}

	fsdata_free(tree);
	dynarray_free(entries);

	drop_tops(view->dir_entry, &view->list_rows, 0);

	if(restore_parent)
	{
		add_parent_dir(view);
	}
}

void
flist_custom_save(view_t *view)
{
	if(flist_is_fs_backed(view))
	{
		flist_custom_drop_save(view);
		return;
	}

	if(view->custom.full.nentries == 0)
	{
		replace_dir_entries(view, &view->custom.full.entries,
				&view->custom.full.nentries, view->dir_entry, view->list_rows);

		/* Some flags shouldn't be copied to the full list. */
		int i;
		for(i = 0; i < view->list_rows; ++i)
		{
			view->custom.full.entries[i].selected = 0;
			view->custom.full.entries[i].folded = 0;
		}
	}
}

/* Erases list previously saved by flist_custom_save(). */
static void
flist_custom_drop_save(view_t *view)
{
	free_dir_entries(&view->custom.full.entries, &view->custom.full.nentries);
}

const char *
flist_get_dir(const view_t *view)
{
	if(flist_custom_active(view))
	{
		assert(view->custom.orig_dir != NULL && "Wrong view dir state.");
		return view->custom.orig_dir;
	}

	return view->curr_dir;
}

void
flist_goto_by_path(view_t *view, const char path[])
{
	char full_path[PATH_MAX + 1];
	const char *const name = get_last_path_component(path);

	get_current_full_path(view, sizeof(full_path), full_path);
	if(stroscmp(full_path, path) == 0)
	{
		return;
	}

	if(flist_custom_active(view) && cv_tree(view->custom.type) &&
			strcmp(name, "..") == 0)
	{
		int pos;
		char dir_only[PATH_MAX + 1];

		snprintf(dir_only, sizeof(dir_only), "%.*s", (int)(name - path), path);
		pos = fpos_find_entry(view, name, dir_only);
		if(pos != -1)
		{
			view->list_pos = pos;
		}
		return;
	}

	(void)set_position_by_path(view, path);
}

dir_entry_t *
entry_from_path(view_t *view, dir_entry_t *entries, int count,
		const char path[])
{
	char canonic_path[PATH_MAX + 1];
	const char *fname;
	int i;

	to_canonic_path(path, flist_get_dir(view), canonic_path,
			sizeof(canonic_path));

	fname = get_last_path_component(canonic_path);
	for(i = 0; i < count; ++i)
	{
		char full_path[PATH_MAX + 1];
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
fentry_get_nitems(const view_t *view, const dir_entry_t *entry)
{
	uint64_t nitems;
	fentry_get_dir_info(view, entry, NULL, &nitems);
	return nitems;
}

void
fentry_get_dir_info(const view_t *view, const dir_entry_t *entry,
		uint64_t *size, uint64_t *nitems)
{
	const int is_slow_fs = view->on_slow_fs;
	dcache_result_t size_res, nitems_res;

	assert((size != NULL || nitems != NULL) &&
			"At least one of out parameters has to be non-NULL.");

	dcache_get_of(entry, (size == NULL ? NULL : &size_res),
			(nitems == NULL ? NULL : &nitems_res));

	if(size != NULL)
	{
		*size = size_res.value;
		if(size_res.value != DCACHE_UNKNOWN && !size_res.is_valid && !is_slow_fs)
		{
			*size = recalc_entry_size(entry, size_res.value);
		}
	}

	if(nitems != NULL)
	{
		if(!nitems_res.is_valid && !is_slow_fs)
		{
			nitems_res.value = entry_calc_nitems(entry);
		}

		*nitems = (nitems_res.value == DCACHE_UNKNOWN ? 0 : nitems_res.value);
	}
}

/* Updates cached size of a directory also updating its relevant parents.
 * Returns current size of the directory entry. */
static uint64_t
recalc_entry_size(const dir_entry_t *entry, uint64_t old_size)
{
	uint64_t size;

	char full_path[PATH_MAX + 1];
	get_full_path_of(entry, sizeof(full_path), full_path);

	size = fops_dir_size(full_path, 0, &ui_cancellation_info);
	dcache_update_parent_sizes(full_path, size - old_size);

	return size;
}

/* Calculates number of items at path specified by the entry.  No check for file
 * type is performed.  Returns the number, which is zero for files. */
static uint64_t
entry_calc_nitems(const dir_entry_t *entry)
{
	char full_path[PATH_MAX + 1];
	get_full_path_of(entry, sizeof(full_path), full_path);

	uint64_t ret = count_dir_items(full_path);

	uint64_t inode = get_true_inode(entry);
	dcache_set_at(full_path, inode, DCACHE_UNKNOWN, ret);

	return ret;
}

int
populate_dir_list(view_t *view, int reload)
{
	const int result = populate_dir_list_internal(view, reload);
	if(view->list_pos > view->list_rows - 1)
	{
		view->list_pos = view->list_rows - 1;
	}
	return result;
}

void
load_dir_list(view_t *view, int reload)
{
	load_dir_list_internal(view, reload, 0);
}

/* Loads file list for the view and redraws the view.  The reload parameter
 * should be set in case of view refresh operation.  The draw_only parameter
 * prevents extra actions that might be taken care of outside this function as
 * well. */
static void
load_dir_list_internal(view_t *view, int reload, int draw_only)
{
	if(populate_dir_list(view, reload) != 0)
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
populate_dir_list_internal(view_t *view, int reload)
{
	char *saved_cwd;

	view->filtered = 0;

	/* List reload usually implies that something related to file list has
	 * changed, like an option.  Reset cached lists to make sure they are up to
	 * date with main column. */
	if(reload)
	{
		flist_free_cache(&view->left_column);
		flist_free_cache(&view->right_column);
	}

	if(flist_custom_active(view))
	{
		return populate_custom_view(view, reload);
	}

	if(!reload && is_dir_big(view->curr_dir) && !modes_is_cmdline_like())
	{
		ui_sb_quick_msgf("%s", "Reading directory...");
	}

	if(curr_stats.load_stage < 2)
	{
		update_all_windows();
	}

	saved_cwd = save_cwd();
	/* this is needed for lstat() below */
	if(vifm_chdir(view->curr_dir) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't chdir() into \"%s\"", view->curr_dir);
		restore_cwd(saved_cwd);
		return 1;
	}

	/* If directory didn't change. */
	if(view->watch != NULL && view->watched_dir != NULL &&
			stroscmp(view->watched_dir, view->curr_dir) == 0)
	{
		/* Drain all events that happened before this point. */
		(void)poll_watcher(view->watch, view->curr_dir);
	}

	if(is_unc_root(view->curr_dir))
	{
#ifdef _WIN32
		free_view_entries(view);
		if(fill_with_shared(view) == 0)
		{
			if(view->list_rows == 0)
			{
				add_parent_dir(view);
			}
		}
		else
		{
			show_error_msgf("Share enumeration error",
					"Can't load list of shares of %s", view->curr_dir);

			leave_invalid_dir(view);
			if(update_dir_list(view, reload) != 0)
			{
				/* We don't have read access, only execute, or there were other
				 * problems. */
				free_view_entries(view);
				add_parent_dir(view);
			}
		}
#endif
	}
	else if(update_dir_list(view, reload) != 0)
	{
		/* We don't have read access, only execute, or there were other problems. */
		free_view_entries(view);
		add_parent_dir(view);
	}

	if(!reload && !modes_is_cmdline_like())
	{
		ui_sb_clear();
	}

	fview_update_geometry(view);

	/* If reloading the same directory don't jump to history position.  Stay at
	 * the current line. */
	if(!reload)
	{
		/* XXX: why cursor is positioned in code that loads the list? */
		flist_hist_lookup(view, view);
	}

	if(view->location_changed)
	{
		fview_dir_updated(view);
	}

	if(view->list_rows < 1)
	{
		if(rescue_from_empty_filelist(view))
		{
			restore_cwd(saved_cwd);
			return 0;
		}

		add_parent_dir(view);
	}

	fview_list_updated(view);

	/* Because we reset directory watcher if directory didn't change before
	 * loading file list, it's possible that we did load everything, but one more
	 * update will happen anyway afterwards.  We shouldn't drain change event
	 * here, because it makes it possible to skip an update (when directory was
	 * changed while we were reading from it). */
	update_dir_watcher(view);

	if(view->location_changed)
	{
		view->location_changed = 0;
		vle_aucmd_execute("DirEnter", view->curr_dir, view);
	}

	restore_cwd(saved_cwd);
	return 0;
}

/* (Re)loads custom view file list.  Returns non-zero on error. */
static int
populate_custom_view(view_t *view, int reload)
{
	if(view->custom.type == CV_TREE)
	{
		dir_entry_t *prev_dir_entries;
		int prev_list_rows, result;

		start_dir_list_change(view, &prev_dir_entries, &prev_list_rows, reload);
		result = flist_load_tree_internal(view, flist_get_dir(view), 1, INT_MAX);

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
			dir_entry_t *prev_dir_entries;
			int prev_list_rows;

			start_dir_list_change(view, &prev_dir_entries, &prev_list_rows, reload);

			replace_dir_entries(view, &view->dir_entry, &view->list_rows,
					view->custom.full.entries, view->custom.full.nentries);

			/* We're merging instead of simply replacing entries to account for
			 * selection and possibly other attributes of entries.  Merging also takes
			 * care of cursor position. */
			finish_dir_list_change(view, prev_dir_entries, prev_list_rows);
		}

		flist_custom_save(view);

		re_apply_folds(view, view->custom.folded_paths);

		(void)zap_entries(view, view->dir_entry, &view->list_rows,
				&is_dead_or_filtered, NULL, 0, 0);
	}

	update_entries_data(view);
	sort_dir_list(!reload, view);
	fview_list_updated(view);
	return 0;
}

/* Applies folds after reloading of a custom view list. */
static void
re_apply_folds(view_t *view, trie_t *folded_paths)
{
	if(view->custom.type != CV_CUSTOM_TREE)
	{
		return;
	}

	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		dir_entry_t *entry = &view->dir_entry[i];
		if(entry->type != FT_DIR)
		{
			continue;
		}

		/* Just folded entry or an old one whose visibility hasn't changed. */
		if(entry->folded)
		{
			remove_child_entries(view, entry);
			continue;
		}

		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		FoldState state = get_fold_state(folded_paths, full_path);
		if(state == FOLD_USER_CLOSED || state == FOLD_AUTO_CLOSED)
		{
			/* Previously folded entry that has just become visible. */
			remove_child_entries(view, entry);
			entry->folded = 1;
		}
	}
}

int
filter_in_compare(view_t *view, void *arg, zap_filter filter)
{
	view_t *const other = (view == curr_view) ? other_view : curr_view;

	zap_compare_view(view, other, filter, arg);
	if(view->list_rows == 0)
	{
		/* Load views before showing the message as event loop in message dialog can
		 * try to reload views. */
		rn_leave(view, 1);
		show_error_msg("Comparison", "No files left in the views, left the mode.");
		return 1;
	}

	exclude_temporary_entries(other);
	return 0;
}

/* Checks whether entry refers to non-existing file.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
entry_exists(view_t *view, const dir_entry_t *entry, void *arg)
{
	return path_exists_at(entry->origin, entry->name, NODEREF);
}

/* Removes entries that refer to non-existing files from compare view marking
 * corresponding entries of the other view as temporary.  This is a
 * zap_entries() tailored for needs of compare, because that function is already
 * too complex. */
static void
zap_compare_view(view_t *view, view_t *other, zap_filter filter, void *arg)
{
	int i, j = 0;

	for(i = 0; i < view->list_rows; ++i)
	{
		dir_entry_t *const entry = &view->dir_entry[i];

		if(!fentry_is_fake(entry) && !filter(view, entry, arg))
		{
			if(entry->selected)
			{
				--view->selected_files;
			}

			const int separator = find_separator(other, i);
			if(separator >= 0)
			{
				fentry_free(entry);
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
find_separator(view_t *view, int idx)
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

/* Updates directory watcher of the view. */
static void
update_dir_watcher(view_t *view)
{
	const char *const curr_dir = flist_get_dir(view);

	if(view->watch == NULL || view->watched_dir == NULL ||
			stroscmp(view->watched_dir, curr_dir) != 0)
	{
		fswatch_free(view->watch);
		view->watch = fswatch_create(curr_dir);

		/* Failure to create a watch is bad, but there isn't much we can do here and
		 * this doesn't feel like a reason to block anything else. */
		if(view->watch != NULL)
		{
			replace_string(&view->watched_dir, curr_dir);
		}
	}
}

/* Checks whether currently loaded custom list of files is missing some files
 * compared to the original custom list.  Returns non-zero if so, otherwise zero
 * is returned. */
static int
custom_list_is_incomplete(const view_t *view)
{
	if(view->custom.full.nentries == 0)
	{
		return 0;
	}

	if(view->list_rows == 1 && is_parent_dir(view->dir_entry[0].name) &&
			!(view->custom.full.nentries == 1 &&
				is_parent_dir(view->custom.full.entries[0].name)))
	{
		return 1;
	}

	return (view->list_rows != view->custom.full.nentries);
}

/* zap_entries() filter to filter-out inexistent files or files which names
 * match local filter. */
static int
is_dead_or_filtered(view_t *view, const dir_entry_t *entry, void *arg)
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
update_entries_data(view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		char full_path[PATH_MAX + 1];
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
zap_entries(view_t *view, dir_entry_t *entries, int *count, zap_filter filter,
		void *arg, int allow_empty_list, int remove_subtrees)
{
	int i, j;

	j = 0;
	for(i = 0; i < *count; ++i)
	{
		dir_entry_t *const entry = &entries[i];
		const int nremoved = remove_subtrees ? (entry->child_count + 1) : 1;

		if(filter(view, entry, arg))
		{
			/* We're keeping this entry. */
			if(i != j)
			{
				entries[j] = entries[i];
			}

			++j;
			continue;
		}

		if(entry->selected && view->dir_entry == entries)
		{
			--view->selected_files;
		}

		/* Reassign children of node about to be deleted to its parent.  Child count
		 * doesn't need an update here because these nodes are already counted. */
		int pos = i + 1;
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

		fix_tree_links(entries, entry, i, j, 0, -nremoved);

		int k;
		for(k = 0; k < nremoved; ++k)
		{
			fentry_free(&entry[k]);
		}

		/* If we're removing file from main list of entries and cursor is right on
		 * this file, move cursor at position this file would take in resulting
		 * list. */
		if(entries == view->dir_entry && view->list_pos >= i &&
				view->list_pos < i + nremoved)
		{
			view->list_pos = j;
		}

		/* Add directory leaf if we just removed last child of the last of nodes
		 * that wasn't filtered.  We can use one entry because if something was
		 * filtered out, there is space for at least one extra entry. */
		const int parent = j - entry->child_pos;
		const int show_empty_dir_leafs = (cfg.dot_dirs & DD_TREE_LEAFS_PARENT);
		if(show_empty_dir_leafs && remove_subtrees && parent == j - 1 &&
				entries[parent].child_count == 0)
		{
			char full_path[PATH_MAX + 1];
			get_full_path_of(&entries[j - 1], sizeof(full_path), full_path);

			char *path = format_str("%s/..", full_path);
			init_parent_entry(view, &entries[j], path);
			remove_last_path_component(path);
			entries[j].origin = path;
			entries[j].owns_origin = 1;
			entries[j].child_pos = 1;

			/* Since we are now adding back one entry, increase parent counts and
			 * child positions back by one. */
			fix_tree_links(entries, entry, i, j, nremoved, 1);

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

/* Visits all parent nodes to update number of their children and also all
 * following sibling nodes which require their parent links updated.  Can be
 * used while moving entries upward. */
static void
fix_tree_links(dir_entry_t *entries, dir_entry_t *entry, int old_idx,
		int new_idx, int displacement, int correction)
{
	if(entry->child_pos == 0 || correction == 0)
	{
		return;
	}

	displacement += old_idx - new_idx;

	int pos = old_idx + (entry->child_count + 1);
	int parent = new_idx - entry->child_pos;
	while(1)
	{
		while(pos <= parent + displacement + entries[parent].child_count)
		{
			entries[pos].child_pos += correction;
			pos += entries[pos].child_count + 1;
		}
		entries[parent].child_count += correction;
		if(entries[parent].child_pos == 0)
		{
			break;
		}
		parent -= entries[parent].child_pos;
	}
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
free_view_entries(view_t *view)
{
	free_dir_entries(&view->dir_entry, &view->list_rows);
}

/* Updates file list with files from current directory.  Returns zero on
 * success, otherwise non-zero is returned. */
static int
update_dir_list(view_t *view, int reload)
{
	dir_entry_t *prev_dir_entries;
	int prev_list_rows;

	start_dir_list_change(view, &prev_dir_entries, &prev_list_rows, reload);

	if(enum_dir_content(view->curr_dir, &add_file_entry_to_view, view) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't opendir() \"%s\"", view->curr_dir);
		free_dir_entries(&prev_dir_entries, &prev_list_rows);
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
start_dir_list_change(view_t *view, dir_entry_t **entries, int *len, int reload)
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
finish_dir_list_change(view_t *view, dir_entry_t *entries, int len)
{
	check_file_uniqueness(view);

	if(entries != NULL)
	{
		merge_lists(view, entries, len);
		free_dir_entries(&entries, &len);
	}

	view->dir_entry = dynarray_shrink(view->dir_entry);
}

/* enum_dir_content() callback that appends files to file list.  Returns zero on
 * success or non-zero to indicate failure and stop enumeration. */
static int
add_file_entry_to_view(const char name[], const void *data, void *param)
{
	view_t *const view = param;
	dir_entry_t *entry;

	/* Always ignore the "." and ".." directories. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	{
		return 0;
	}

	if(!entry_is_visible(view, name, data))
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
		fentry_free(entry);
	}

	return 0;
}

void
resort_dir_list(int msg, view_t *view)
{
	char full_path[PATH_MAX + 1];
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
 * "Sorting..." status bar message. */
static void
sort_dir_list(int msg, view_t *view)
{
	if(msg && view->list_rows > 2048 && !modes_is_cmdline_like())
	{
		ui_sb_quick_msgf("%s", "Sorting directory...");
	}

	sort_view(view);

	if(msg && !modes_is_cmdline_like())
	{
		ui_sb_clear();
	}
}

/* Merges elements from previous list into the new one. */
static void
merge_lists(view_t *view, dir_entry_t *entries, int len)
{
	int i;
	int closest_dist;
	const int prev_pos = view->list_pos;
	trie_t *prev_names = trie_create(/*free_func=*/NULL);

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
		dist = (dir_entry_t *)data - entries - prev_pos;
		closest_dist = correct_pos(view, i, dist, closest_dist);
	}

	trie_free(prev_names);
}

/* Checks that entries don't have the same name (for non-cv).  And if there are
 * duplicates shows a warning to the user (shown each time broken directory is
 * entered) and drops duplicates. */
TSTATIC void
check_file_uniqueness(view_t *view)
{
	trie_t *file_names = trie_create(/*free_func=*/NULL);

	int had_dups = view->has_dups;

	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		add_to_trie(file_names, view, &view->dir_entry[i]);
	}

	trie_free(file_names);

	if(view->has_dups)
	{
		(void)exclude_temporary_entries(view);
		if(!had_dups)
		{
			show_error_msg("Broken File System", "Underlying file system seems to "
					"report duplicated file names.  Only one entry will be shown.");
		}
	}
}

/* Adds view entry into the trie mapping its name to entry structure.
 * Duplicated entries of a non-custom view are marked as temporary. */
static void
add_to_trie(trie_t *trie, view_t *view, dir_entry_t *entry)
{
	if(flist_custom_active(view))
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		int error = trie_set(trie, full_path, entry);
		assert(error == 0 && "Duplicated file names in the list?");
		(void)error;
		return;
	}

	if(trie_set(trie, entry->name, entry) != 0)
	{
		LOG_INFO_MSG("Duplicated entry is `%s` in `%s`", entry->name,
				entry->origin);
		entry->temporary = 1;
		view->has_dups = 1;
	}
}

/* Looks up entry in the trie by its name.  Retrieves directory entry stored by
 * add_to_trie() into *data (unchanged on lookup failure).  Returns non-zero if
 * item was successfully retrieved and zero otherwise. */
static int
is_in_trie(trie_t *trie, view_t *view, dir_entry_t *entry, void **data)
{
	int error;

	if(flist_custom_active(view))
	{
		char full_path[PATH_MAX + 1];
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
	new->folded = prev->folded;

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
correct_pos(view_t *view, int pos, int dist, int closest)
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
rescue_from_empty_filelist(view_t *view)
{
	/* It is possible to set the file name filter so that no files are showing
	 * in the / directory.  All other directories will always show at least the
	 * ../ file.  This resets the filter and reloads the directory. */
	if(name_filters_empty(view))
	{
		return 0;
	}

	show_error_msgf("Filter error",
			"The %s\"%s\" pattern did not match any files. It was reset.",
			view->invert ? "" : "inverted ", matcher_get_expr(view->manual_filter));

	name_filters_drop(view);

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
add_parent_dir(view_t *view)
{
	add_parent_entry(view, &view->dir_entry, &view->list_rows);
}

/* Adds parent directory entry (..) to specified list of entries. */
static void
add_parent_entry(view_t *view, dir_entry_t **entries, int *count)
{
	dir_entry_t *const dir_entry = alloc_dir_entry(entries, *count);
	if(dir_entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if(init_parent_entry(view, dir_entry, "..") == 0)
	{
		++*count;
	}
}

/* Initializes dir_entry_t with name and all other fields with default
 * values. */
static void
init_dir_entry(view_t *view, dir_entry_t *entry, const char name[])
{
	entry->name = strdup(name);
	entry->origin = &view->curr_dir[0];

	entry->size = 0ULL;
#ifndef _WIN32
	entry->uid = (uid_t)-1;
	entry->gid = (gid_t)-1;
	entry->mode = (mode_t)0;
	entry->inode = (ino_t)0;
#else
	entry->attrs = 0;
#endif

	entry->mtime = (time_t)0;
	entry->atime = (time_t)0;
	entry->ctime = (time_t)0;

	entry->type = FT_UNK;
	entry->nlinks = 0;
	entry->dir_link = 0;
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
	entry->owns_origin = 0;
	entry->folded = 0;

	entry->tag = -1;
	entry->id = -1;
}

void
replace_dir_entries(view_t *view, dir_entry_t **entries, int *count,
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
		entry->owns_origin = 1;

		if(entry->name == NULL || entry->origin == NULL)
		{
			int count_so_far = i + 1;
			free_dir_entries(&new, &count_so_far);
			return;
		}
	}

	free_dir_entries(entries, count);
	*entries = new;
	*count = with_count;
}

void
free_dir_entries(dir_entry_t **entries, int *count)
{
	int i;
	for(i = 0; i < *count; ++i)
	{
		fentry_free(&(*entries)[i]);
	}

	dynarray_free(*entries);
	*entries = NULL;
	*count = 0;
}

void
fentry_free(dir_entry_t *entry)
{
	free(entry->name);
	entry->name = NULL;

	if(entry->owns_origin)
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
entry_list_add(view_t *view, dir_entry_t **list, int *list_size,
		const char path[])
{
	dir_entry_t *const dir_entry = alloc_dir_entry(list, *list_size);
	if(dir_entry == NULL)
	{
		return NULL;
	}

	init_dir_entry(view, dir_entry, get_last_path_component(path));

	dir_entry->origin = strdup(path);
	dir_entry->owns_origin = 1;
	remove_last_path_component(dir_entry->origin);

	if(fill_dir_entry_by_path(dir_entry, path) != 0)
	{
		fentry_free(dir_entry);
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
check_if_filelist_has_changed(view_t *view)
{
	int failed, changed;
	const char *const curr_dir = flist_get_dir(view);

	if(view->on_slow_fs ||
			(flist_custom_active(view) && !cv_tree(view->custom.type)) ||
			is_unc_root(curr_dir))
	{
		return;
	}

	if(view->watch == NULL)
	{
		/* If watch is not initialized, try to do this, but don't fail on error. */

		update_dir_watcher(view);
		failed = 0;
		changed = (view->watch != NULL);
	}
	else
	{
		FSWatchState state = poll_watcher(view->watch, curr_dir);
		changed = (state != FSWS_UNCHANGED);
		failed = (state == FSWS_ERRORED);
	}

	/* Check if we still have permission to visit this directory. */
	if(os_access(curr_dir, X_OK) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't access(X_OK) \"%s\"", curr_dir);
		log_cwd();
		failed = 1;
	}

	if(failed)
	{
		show_error_msgf("Directory Check", "Cannot open %s", curr_dir);

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
	else if(flist_custom_active(view) && cv_tree(view->custom.type))
	{
		if(flist_is_fs_backed(view) &&
				tree_has_changed(view->dir_entry, view->list_rows))
		{
			ui_view_schedule_reload(view);
		}
	}
	else
	{
		if(flist_update_cache(view, &view->left_column, view->left_column.dir) ||
				flist_update_cache(view, &view->right_column, view->right_column.dir))
		{
			ui_view_schedule_redraw(view);
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
			char full_path[PATH_MAX + 1];
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
flist_update_cache(view_t *view, cached_entries_t *cache, const char path[])
{
	int update = 0;

	if(path == NULL)
	{
		return 0;
	}

	if(cache->watch == NULL || stroscmp(cache->dir, path) != 0)
	{
		fswatch_free(cache->watch);

		cache->watch = fswatch_create(path);
		if(cache->watch == NULL)
		{
			/* Reset the cache on failure to create a watcher to do not accidentally
			 * provide incorrect data. */
			flist_free_cache(cache);
			return 0;
		}

		replace_string(&cache->dir, path);

		update = 1;
	}

	if(poll_watcher(cache->watch, path) != FSWS_UNCHANGED || update)
	{
		free_dir_entries(&cache->entries.entries, &cache->entries.nentries);
		cache->entries = flist_list_in(view, path, 0, 1);
		return 1;
	}

	return 0;
}

/* Polls file-system watcher and re-enters current working directory of the
 * process if necessary.  Returns watcher's state. */
static FSWatchState
poll_watcher(fswatch_t *watch, const char path[])
{
	FSWatchState state = fswatch_poll(watch);

	if(state == FSWS_ERRORED || state == FSWS_REPLACED)
	{
		char curr_path[PATH_MAX + 1];
		if(get_cwd(curr_path, sizeof(curr_path)) == curr_path)
		{
			if(stroscmp(curr_path, path) == 0)
			{
				/* Re-enter current directory (vifm_chdir() can skip system call thus
				 * not synchronizing location with updated file-system data). */
				(void)os_chdir(path);
			}
		}
	}

	return state;
}

void
flist_free_cache(cached_entries_t *cache)
{
	free_dir_entries(&cache->entries.entries, &cache->entries.nentries);
	update_string(&cache->dir, NULL);
	fswatch_free(cache->watch);
	cache->watch = NULL;
}

void
flist_update_origins(view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		dir_entry_t *const entry = &view->dir_entry[i];
		if(!entry->owns_origin)
		{
			entry->origin = &view->curr_dir[0];
		}
	}
}

void
flist_toggle_fold(view_t *view)
{
	if(!flist_custom_active(view) || !cv_tree(view->custom.type))
	{
		return;
	}

	dir_entry_t *curr = get_current_entry(view);
	if(curr->type != FT_DIR)
	{
		return;
	}

	char full_path[PATH_MAX + 1];
	get_full_path_of(curr, sizeof(full_path), full_path);

	FoldState state = get_fold_state(view->custom.folded_paths, full_path);

	assert(curr->folded == (state == FOLD_USER_CLOSED ||
				state == FOLD_AUTO_CLOSED) && "Metadata is not in sync.");

	switch(state)
	{
		case FOLD_UNDEFINED:   state = FOLD_USER_CLOSED; break;
		case FOLD_USER_OPENED: state = FOLD_USER_CLOSED; break;
		case FOLD_USER_CLOSED: state = FOLD_USER_OPENED; break;
		case FOLD_AUTO_OPENED: state = FOLD_USER_CLOSED; break;
		case FOLD_AUTO_CLOSED: state = FOLD_AUTO_OPENED; break;
	}

	if(set_fold_state(view->custom.folded_paths, full_path, state))
	{
		curr->folded = !curr->folded;
		/* We reload even on folding to update number of filtered entries
		 * properly. */
		ui_view_schedule_reload(view);
	}
}

/* Folds a single entry by removing all of its children and updating tree
 * metadata accordingly. */
static void
remove_child_entries(view_t *view, dir_entry_t *entry)
{
	const int pos = (entry - view->dir_entry);
	const int child_count = entry->child_count;

	flist_custom_save(view);

	int i;
	for(i = 0; i < child_count; ++i)
	{
		fentry_free(&entry[1 + i]);
	}

	fix_tree_links(view->dir_entry, entry, pos, pos, 0, -child_count);

	memmove(entry + 1, entry + 1 + child_count,
			sizeof(*entry)*(view->list_rows - (pos + 1 + child_count)));
	view->list_rows -= child_count;
	entry->child_count = 0;
}

int
cd_is_possible(const char path[])
{
	if(!is_valid_dir(path))
	{
		LOG_SERROR_MSG(errno, "Can't access \"%s\"", path);

		show_error_msgf("Destination doesn't exist or isn't a directory", "\"%s\"",
				path);
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
load_saving_pos(view_t *view)
{
	char full_path[PATH_MAX + 1];

	if(curr_stats.load_stage < 2)
		return;

	if(!window_shows_dirlist(view))
		return;

	if(view->local_filter.in_progress)
	{
		return;
	}

	get_current_full_path(view, sizeof(full_path), full_path);

	load_dir_list_internal(view, 1, 1);

	flist_goto_by_path(view, full_path);

	fview_cursor_redraw(view);

	if(ui_view_is_visible(view))
	{
		refresh_view_win(view);
	}
}

int
window_shows_dirlist(const view_t *view)
{
	if(!ui_view_is_visible(view))
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

	if(view == other_view && curr_stats.preview.on)
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
change_sort_type(view_t *view, char type, char descending)
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
pane_in_dir(const view_t *view, const char path[])
{
	return paths_are_same(view->curr_dir, path);
}

int
cd(view_t *view, const char base_dir[], const char path[])
{
	char dir[PATH_MAX + 1];
	int updir;

	flist_pick_cd_path(view, base_dir, path, &updir, dir, sizeof(dir));

	if(updir)
	{
		rn_leave(view, 1);
	}
	else
	{
		char canonic_dir[PATH_MAX + 1];
		to_canonic_path(dir, base_dir, canonic_dir, sizeof(canonic_dir));
		if(!cd_is_possible(canonic_dir) || change_directory(view, canonic_dir) < 0)
		{
			return 0;
		}
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

void
flist_pick_cd_path(view_t *view, const char base_dir[], const char path[],
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
		copy_str(buf, buf_size, view->last_dir == NULL ? "." : view->last_dir);
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
		char *expanded = expand_tilde(part);

		build_path(buf, buf_size, expanded, dst);
		free(expanded);

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
go_to_sibling_dir(view_t *view, int offset, int wrap)
{
	entries_t parent_dirs;
	dir_entry_t *entry;
	int save_msg = 0;

	if(is_root_dir(flist_get_dir(view)) || flist_custom_active(view))
	{
		return 0;
	}

	parent_dirs = list_sibling_dirs(view);
	if(parent_dirs.nentries < 0)
	{
		show_error_msg("Error", "Can't list parent directory");
		return 0;
	}

	sort_entries(view, parent_dirs);

	entry = pick_sibling(view, parent_dirs, offset, wrap, &save_msg);
	if(entry != NULL)
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);
		if(change_directory(view, full_path) >= 0)
		{
			load_dir_list(view, 0);
			fview_position_updated(view);
		}
	}

	free_dir_entries(&parent_dirs.entries, &parent_dirs.nentries);
	return save_msg;
}

/* Lists siblings of current directory of the view (includes current directory
 * as well).  Returns the list, which is of length -1 on error. */
static entries_t
list_sibling_dirs(view_t *view)
{
	entries_t parent_dirs = {};
	char *path;

	if(is_root_dir(flist_get_dir(view)))
	{
		parent_dirs.nentries = -1;
		return parent_dirs;
	}

	path = strdup(flist_get_dir(view));
	remove_last_path_component(path);

	parent_dirs = flist_list_in(view, path, 1, 0);

	free(path);

	if(parent_dirs.nentries < 0)
	{
		return parent_dirs;
	}

	if(entry_from_path(view, parent_dirs.entries, parent_dirs.nentries,
				flist_get_dir(view)) == NULL)
	{
		/* If we couldn't find our current directory in the list (because it got
		 * filtered-out), add it to be able to determine where it would go if it
		 * were visible. */
		entry_list_add(view, &parent_dirs.entries, &parent_dirs.nentries,
				flist_get_dir(view));
	}

	return parent_dirs;
}

/* Lists files of specified directory.  Returns the list, which is of length -1
 * on error. */
static entries_t
flist_list_in(view_t *view, const char path[], int only_dirs,
		int can_include_parent)
{
	entries_t siblings = {};
	int len, i;
	char **list;

	list = list_all_files(path, &len);
	if(len < 0)
	{
		siblings.nentries = -1;
		return siblings;
	}

	for(i = 0; i < len; ++i)
	{
		dir_entry_t *entry;
		int is_dir;

		if(view->hide_dot && list[i][0] == '.')
		{
			continue;
		}

		char *full_path = format_str("%s/%s", path, list[i]);
		entry = entry_list_add(view, &siblings.entries, &siblings.nentries,
				full_path);
		free(full_path);

		if(entry == NULL)
		{
			continue;
		}

		is_dir = fentry_is_dir(entry);
		if((only_dirs && !is_dir) ||
				!filters_file_is_visible(view, path, list[i], is_dir, 0))
		{
			fentry_free(entry);
			--siblings.nentries;
		}
	}
	free_string_array(list, len);

	if(can_include_parent && cfg_parent_dir_is_visible(is_root_dir(path)))
	{
		char *const full_path = format_str("%s/..", path);
		entry_list_add(view, &siblings.entries, &siblings.nentries, full_path);
		free(full_path);
	}

	return siblings;
}

/* Picks next or previous sibling from the list with optional wrapping.
 * *wrapped is set to non-zero if wrapping happened.  Returns pointer to picked
 * sibling or NULL on error or if can't pick. */
static dir_entry_t *
pick_sibling(view_t *view, entries_t parent_dirs, int offset, int wrap,
		int *wrapped)
{
	dir_entry_t *entry;
	int i, initial;

	/* Find parent entry. */
	entry = entry_from_path(view, parent_dirs.entries, parent_dirs.nentries,
			flist_get_dir(view));
	if(entry == NULL)
	{
		show_error_msg("Error", "Can't find current directory position");
		return NULL;
	}

	*wrapped = 0;
	initial = entry - parent_dirs.entries;
	i = initial;

	while(offset != 0)
	{
		if(offset > 0 && i < parent_dirs.nentries - 1)
		{
			++i;
		}
		else if(offset < 0 && i > 0)
		{
			--i;
		}
		else if(wrap)
		{
			if(offset > 0)
			{
				i = 0;
				ui_sb_err("Hit BOTTOM, continuing at TOP");
			}
			else
			{
				i = parent_dirs.nentries - 1;
				ui_sb_err("Hit TOP, continuing at BOTTOM");
			}
			*wrapped = 1;
		}
		else
		{
			return (i == initial ? NULL : entry);
		}

		entry = &parent_dirs.entries[i];
		offset = (offset > 0 ? offset - 1 : offset + 1);
	}

	return entry;
}

int
view_needs_cd(const view_t *view, const char path[])
{
	if(path[0] == '\0' || stroscmp(view->curr_dir, path) == 0)
	{
		return 0;
	}
	return 1;
}

void
set_view_path(view_t *view, const char path[])
{
	if(view_needs_cd(view, path))
	{
		copy_str(view->curr_dir, sizeof(view->curr_dir), path);
		exclude_file_name(view->curr_dir);
	}
}

uint64_t
fentry_get_size(const view_t *view, const dir_entry_t *entry)
{
	uint64_t size = DCACHE_UNKNOWN;

	if(fentry_is_dir(entry))
	{
		fentry_get_dir_info(view, entry, &size, NULL);
	}

	return (size == DCACHE_UNKNOWN ? entry->size : size);
}

int
iter_selected_entries(view_t *view, dir_entry_t **entry)
{
	return iter_entries(view, entry, &is_entry_selected, /*valid_only=*/1);
}

int
iter_marked_entries(view_t *view, dir_entry_t **entry)
{
	return iter_entries(view, entry, &is_entry_marked, /*valid_only=*/1);
}

/* Implements iteration over the entries which match specific predicate.
 * Returns non-zero if matching entry is found and is loaded to *entry,
 * otherwise it's set to NULL and zero is returned. */
static int
iter_entries(view_t *view, dir_entry_t **entry, entry_predicate pred,
		int valid_only)
{
	int next = (*entry == NULL) ? 0 : (*entry - view->dir_entry + 1);

	while(next < view->list_rows)
	{
		dir_entry_t *const e = &view->dir_entry[next];
		if((!valid_only || fentry_is_valid(e)) && pred(e))
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

int
iter_selection_or_current(view_t *view, dir_entry_t **entry)
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
iter_selection_or_current_any(view_t *view, dir_entry_t **entry)
{
	if(view->selected_files == 0)
	{
		dir_entry_t *const curr = get_current_entry(view);
		*entry = (*entry == NULL ? curr : NULL);
		return *entry != NULL;
	}
	return iter_entries(view, entry, &is_entry_selected, /*valid_only=*/0);
}

int
entry_to_pos(const view_t *view, const dir_entry_t *entry)
{
	const int pos = entry - view->dir_entry;
	return (pos >= 0 && pos < view->list_rows) ? pos : -1;
}

void
get_current_full_path(const view_t *view, size_t buf_len, char buf[])
{
	get_full_path_at(view, view->list_pos, buf_len, buf);
}

void
get_full_path_at(const view_t *view, int pos, size_t buf_len, char buf[])
{
	if(pos < 0 || pos >= view->list_rows)
	{
		copy_str(buf, buf_len, "");
		return;
	}
	get_full_path_of(&view->dir_entry[pos], buf_len, buf);
}

void
get_full_path_of(const dir_entry_t *entry, size_t buf_len, char buf[])
{
	build_path(buf, buf_len, entry->origin, entry->name);
}

void
get_short_path_of(const view_t *view, const dir_entry_t *entry, NameFormat fmt,
		int drop_prefix, size_t buf_len, char buf[])
{
	char name[NAME_MAX + 1];
	const char *path = entry->origin;

	char parent_path[PATH_MAX + 1];
	const char *root_path = flist_get_dir(view);

	if(fentry_is_fake(entry))
	{
		copy_str(buf, buf_len, "");
		return;
	}

	if(drop_prefix && entry->child_pos != 0)
	{
		/* Replace root to force obtaining of file name only. */
		const dir_entry_t *const parent_entry = entry - entry->child_pos;
		get_full_path_of(parent_entry, sizeof(parent_path), parent_path);
		root_path = parent_path;
	}

	format_entry_name(entry, fmt, sizeof(name), name);

	if(is_parent_dir(entry->name) || is_root_dir(entry->name))
	{
		copy_str(buf, buf_len, name);
		return;
	}

	if(!path_starts_with(path, root_path))
	{
		build_path(buf, buf_len, path, name);
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
}

void
check_marking(view_t *view, int count, const int indexes[])
{
	if(count != 0)
	{
		mark_files_at(view, count, indexes);
	}
	else
	{
		flist_set_marking(view, 0);
	}
}

void
flist_set_marking(view_t *view, int prefer_current)
{
	if(view->pending_marking)
	{
		view->pending_marking = 0;
		return;
	}

	dir_entry_t *curr = get_current_entry(view);
	if(view->selected_files != 0 &&
			(!prefer_current || (curr != NULL && curr->selected)))
	{
		mark_selected(view);
	}
	else
	{
		clear_marking(view);

		if(curr != NULL && fentry_is_valid(curr))
		{
			curr->marked = 1;
		}
	}
}

void
mark_files_at(view_t *view, int count, const int indexes[])
{
	clear_marking(view);

	int i;
	for(i = 0; i < count; ++i)
	{
		view->dir_entry[indexes[i]].marked = 1;
	}

	view->pending_marking = 1;
}

/* Marks selected files of the view.  Returns number of marked files. */
static int
mark_selected(view_t *view)
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
mark_selection_or_current(view_t *view)
{
	dir_entry_t *const curr = get_current_entry(view);
	if(view->selected_files == 0 && fentry_is_valid(curr))
	{
		clear_marking(view);
		curr->marked = 1;
		return 1;
	}
	return mark_selected(view);
}

int
flist_count_marked(const view_t *view)
{
	int i, n = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		n += (view->dir_entry[i].marked != 0);
	}
	return n;
}

void
clear_marking(view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].marked = 0;
	}
}

void
flist_custom_set(view_t *view, const char title[], const char path[],
		char *lines[], int nlines)
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
		flist_custom_add_spec(view, lines[i]);
	}

	flist_custom_end(view, 1);
}

void
flist_custom_add_spec(view_t *view, const char line[])
{
	char *const path = parse_line_for_path(line, flist_get_dir(view));
	if(path != NULL)
	{
		flist_custom_add(view, path);
		free(path);
	}
}

void
flist_custom_end(view_t *view, int very)
{
	if(flist_custom_finish(view, very ? CV_VERY : CV_REGULAR, 0) != 0)
	{
		show_error_msg("Custom view", "Ignoring empty list of files");
		return;
	}

	fpos_set_pos(view, 0);
}

void
fentry_rename(view_t *view, dir_entry_t *entry, const char to[])
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

	/* Update origins of entries which include the one we're renaming. */
	if(flist_custom_active(view) && fentry_is_dir(entry))
	{
		int i;
		char *const root = format_str("%s/%s", entry->origin, old_name);
		const size_t root_len = strlen(root);

		/* TODO: this doesn't handle circular renames like a/ -> b/, needs more
		 *       bookkeeping. */
		for(i = 0; i < view->list_rows; ++i)
		{
			dir_entry_t *const e = &view->dir_entry[i];
			if(path_starts_with(e->origin, root))
			{
				char full_path[PATH_MAX + 1];
				get_full_path_of(e, sizeof(full_path), full_path);
				FoldState state = get_fold_state(view->custom.folded_paths, full_path);

				char *const new_origin = format_str("%s/%s%s", entry->origin, to,
						e->origin + root_len);
				chosp(new_origin);
				if(e->owns_origin)
				{
					free(e->origin);
				}
				e->origin = new_origin;
				e->owns_origin = 1;

				/* Clone visible child folds. */
				e->folded = 0;
				get_full_path_of(e, sizeof(full_path), full_path);
				if(set_fold_state(view->custom.folded_paths, full_path, state))
				{
					e->folded = (state == FOLD_USER_CLOSED || state == FOLD_AUTO_CLOSED);
				}
			}
		}

		free(root);
	}

	/* Cloning of a folded directory should produce a folded clone. */
	if(entry->folded)
	{
		char full_path[PATH_MAX + 1];
		build_path(full_path, sizeof(full_path), entry->origin, old_name);

		FoldState state = get_fold_state(view->custom.folded_paths, full_path);

		get_full_path_of(entry, sizeof(full_path), full_path);
		if(!set_fold_state(view->custom.folded_paths, full_path, state))
		{
			entry->folded = 0;
		}
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
fentry_is_dir(const dir_entry_t *entry)
{
	return (entry->type == FT_LINK) ? entry->dir_link : (entry->type == FT_DIR);
}

int
fentry_points_to(const dir_entry_t *entry, const char path[])
{
	char previewed[PATH_MAX + 1];
	get_full_path_of(entry, sizeof(previewed), previewed);

	if(paths_are_equal(path, previewed))
	{
		return 1;
	}

	/* Can also check resolved paths for symbolic links. */
	if(entry->type != FT_LINK)
	{
		/* Nothing more to do for a non-link. */
		return 0;
	}

	if(get_link_target_abs(previewed, entry->origin, previewed,
				sizeof(previewed)) != 0)
	{
		/* We don't know. */
		return 0;
	}
	return paths_are_equal(path, previewed);
}

int
flist_load_tree(view_t *view, const char path[], int depth)
{
	char full_path[PATH_MAX + 1];
	get_current_full_path(view, sizeof(full_path), full_path);

	if(flist_load_tree_internal(view, path, 0, depth) != 0)
	{
		return 1;
	}

	if(full_path[0] != '\0')
	{
		(void)set_position_by_path(view, full_path);
	}

	ui_view_schedule_redraw(view);
	return 0;
}

/* Looks up entry by its path in main entry list of the view and updates cursor
 * position when such entry is found.  Returns zero if position was updated,
 * otherwise non-zero is returned. */
static int
set_position_by_path(view_t *view, const char path[])
{
	const dir_entry_t *entry;
	entry = entry_from_path(view, view->dir_entry, view->list_rows, path);
	if(entry != NULL)
	{
		view->list_pos = entry_to_pos(view, entry);
		return 0;
	}
	return 1;
}

int
flist_clone_tree(view_t *to, const view_t *from)
{
	if(from->custom.type == CV_CUSTOM_TREE)
	{
		const int as_tree = (from->custom.type == CV_CUSTOM_TREE);
		flist_custom_clone(to, from, as_tree);
		re_apply_folds(to, from->custom.folded_paths);
	}
	else
	{
		if(make_tree(to, flist_get_dir(from), 0, from->custom.excluded_paths,
					from->custom.folded_paths, INT_MAX) != 0)
		{
			return 1;
		}
	}

	trie_free(to->custom.excluded_paths);
	to->custom.excluded_paths = trie_clone(from->custom.excluded_paths);

	trie_free(to->custom.folded_paths);
	to->custom.folded_paths = trie_clone(from->custom.folded_paths);

	return 0;
}

/* Implements tree view (re)loading.  The depth parameter can be used to limit
 * nesting level (>= 0).  Returns zero on success, otherwise
 * non-zero is returned. */
static int
flist_load_tree_internal(view_t *view, const char path[], int reload, int depth)
{
	trie_t *excluded_paths = reload ? view->custom.excluded_paths : NULL;
	trie_t *folded_paths = reload ? view->custom.folded_paths
	                              : trie_create(/*free_func=*/NULL);
	if(make_tree(view, path, reload, excluded_paths, folded_paths, depth) != 0)
	{
		if(!reload)
		{
			trie_free(folded_paths);
		}
		return 1;
	}

	if(!reload)
	{
		trie_free(view->custom.excluded_paths);
		view->custom.excluded_paths = trie_create(/*free_func=*/NULL);

		trie_free(view->custom.folded_paths);
		view->custom.folded_paths = folded_paths;
	}

	return 0;
}

/* (Re)loads tree at path into the view using specified list of excluded files.
 * The depth parameter can be used to limit nesting level (>= 0).  Returns zero
 * on success, otherwise non-zero is returned. */
static int
make_tree(view_t *view, const char path[], int reload, trie_t *excluded_paths,
		trie_t *folded_paths, int depth)
{
	char canonic_path[PATH_MAX + 1];
	int nfiltered;
	CVType type;
	const int from_custom = flist_custom_active(view)
	                     && ONE_OF(view->custom.type, CV_REGULAR, CV_VERY);

	flist_custom_start(view, from_custom ? view->custom.title : "");

	show_progress("Building tree...", 0);

	ui_cancellation_push_on();
	if(from_custom)
	{
		nfiltered = 0;
		tree_from_cv(view);
		type = CV_CUSTOM_TREE;
	}
	else
	{
		nfiltered = add_files_recursively(view, path, excluded_paths, folded_paths,
				-1, 0, depth);
		type = CV_TREE;
	}
	ui_cancellation_pop();

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

	if(flist_custom_finish_internal(view, type, reload, canonic_path, 1) != 0)
	{
		return 1;
	}
	view->filtered = nfiltered;

	replace_string(&view->custom.orig_dir, canonic_path);

	return 0;
}

/* Turns custom list into custom tree. */
static void
tree_from_cv(view_t *view)
{
	int i;

	dir_entry_t *entries = view->dir_entry;
	int nentries = view->list_rows;

	fsdata_t *const tree = fsdata_create(0, 0);

	view->dir_entry = NULL;
	view->list_rows = 0;

	for(i = 0U; i < nentries; ++i)
	{
		if(!is_parent_dir(entries[i].name))
		{
			char full_path[PATH_MAX + 1];
			void *data = &entries[i];
			get_full_path_of(&entries[i], sizeof(full_path), full_path);
			fsdata_set(tree, full_path, &data, sizeof(data));

			/* Mark entries that came from original list. */
			entries[i].tag = 1;
		}
		else
		{
			fentry_free(&entries[i]);
		}
	}

	if(fsdata_traverse(tree, &complete_tree, view) != 0)
	{
		reset_entry_list(view, &view->custom.entries, &view->custom.entry_count);
	}

	fsdata_free(tree);
	dynarray_free(entries);

	drop_tops(view->custom.entries, &view->custom.entry_count, 1);
}

/* fsdata_traverse() callback that flattens the tree into array of entries.
 * Should return non-zero to stop traverser. */
static int
complete_tree(const char name[], int valid, const void *parent_data, void *data,
		void *arg)
{
	/* Initially data associated with existing entries points to original entries.
	 * Once that entry is copied, the data is replaced with its index in the new
	 * list. */

	view_t *const view = arg;
	const int in_place = cv_tree(view->custom.type);

	dir_entry_t **entries = (in_place ? &view->dir_entry : &view->custom.entries);
	int *nentries = (in_place ? &view->list_rows : &view->custom.entry_count);

	dir_entry_t *dir_entry = alloc_dir_entry(entries, *nentries);
	if(dir_entry == NULL)
	{
		return 1;
	}

	++*nentries;

	if(valid)
	{
		*dir_entry = **(dir_entry_t **)data;
		dir_entry->child_count = 0;
		(*(dir_entry_t **)data)->name = NULL;
		(*(dir_entry_t **)data)->origin = NULL;
	}
	else
	{
		char full_path[PATH_MAX + 1];

		if(parent_data == NULL)
		{
			char *const typed_path = format_str("%s/", name);
			if(is_root_dir(typed_path))
			{
				/* Entry for a root is added temporarily (this happens only on Windows)
				 * as a storage of path prefix and is removed afterwards in
				 * drop_tops(). */
				init_dir_entry(view, dir_entry, "");
				dir_entry->origin = strdup(name);
			}
			else
			{
				init_dir_entry(view, dir_entry, name);
				dir_entry->origin = strdup("/");
			}
			free(typed_path);
			dir_entry->owns_origin = 1;
		}
		else
		{
			char parent_path[PATH_MAX + 1];
			const intptr_t *parent_idx = parent_data;
			init_dir_entry(view, dir_entry, name);
			get_full_path_of(&(*entries)[*parent_idx], sizeof(parent_path),
					parent_path);
			dir_entry->origin = strdup(parent_path);
			dir_entry->owns_origin = 1;
		}

		get_full_path_of(dir_entry, sizeof(full_path), full_path);
		fill_dir_entry_by_path(dir_entry, full_path);

		dir_entry->temporary = in_place;
	}

	*(intptr_t *)data = dir_entry - *entries;

	if(parent_data != NULL)
	{
		const intptr_t *const parent_idx = parent_data;
		dir_entry->child_pos = (dir_entry - *entries) - *parent_idx;

		do
		{
			dir_entry -= dir_entry->child_pos;
			++dir_entry->child_count;
		}
		while(dir_entry->child_pos != 0);
	}
	return 0;
}

/* Replaces list of entries with a single parent directory entry.  Might leave
 * the list empty on memory error. */
static void
reset_entry_list(view_t *view, dir_entry_t **entries, int *count)
{
	free_dir_entries(entries, count);
	add_parent_entry(view, entries, count);
}

/* Traverses root children and drops fake root nodes and optionally extra tops
 * of subtrees. */
static void
drop_tops(dir_entry_t *entries, int *nentries, int extra)
{
	int i;
	for(i = 0; i < *nentries - 1; i += entries[i].child_count + 1)
	{
		int j = i;
		while(j < *nentries - 1 && entries[j].child_count > 0 &&
				((extra && entries[j].child_count == entries[j + 1].child_count + 1) ||
				 entries[j].name[0] == '\0') && entries[j].tag != 1)
		{
			fentry_free(&entries[j++]);
		}

		memmove(&entries[i], &entries[j], sizeof(*entries)*(*nentries - j));
		*nentries -= j - i;
		entries[i].child_pos = 0;
	}
}

/* Adds custom view entries corresponding to file system tree.  parent_pos is
 * expected to be negative for the outermost invocation.  The depth parameter
 * is used to limit nesting level, when it's negative, parent node is just
 * marked as folded.  Returns number of filtered out files on success or partial
 * success and negative value on serious error. */
static int
add_files_recursively(view_t *view, const char path[], trie_t *excluded_paths,
		trie_t *folded_paths, int parent_pos, int no_direct_parent, int depth)
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

	FoldState parent_fold = get_fold_state(folded_paths, path);

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
		if(!tree_candidate_is_visible(view, path, lst[i], dir, 1))
		{
			const int real_dir = (dir && !is_symlink(full_path));

			FoldState state;
			if(real_dir)
			{
				state = get_fold_state(folded_paths, full_path);
			}

			if(real_dir &&
					(depth == 0 ||
					 (state == FOLD_UNDEFINED && parent_fold == FOLD_AUTO_OPENED)))
			{
				if(set_fold_state(folded_paths, full_path, FOLD_AUTO_CLOSED))
				{
					state = FOLD_AUTO_CLOSED;
				}
			}

			/* Traverse directory (but not symlink to it) even if we're skipping it,
			 * because we might need files that are inside of it. */
			if(real_dir && depth > 0 &&
					tree_candidate_is_visible(view, path, lst[i], dir, 0))
			{
				if(state != FOLD_AUTO_CLOSED && state != FOLD_USER_CLOSED)
				{
					nfiltered += add_files_recursively(view, full_path, excluded_paths,
							folded_paths, parent_pos, 1, depth - 1);
				}
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
		FoldState state = get_fold_state(folded_paths, full_path);
		entry->folded = (entry->type == FT_DIR)
		             && (state == FOLD_USER_CLOSED || state == FOLD_AUTO_CLOSED);

		if(entry->type == FT_DIR && !entry->folded)
		{
			if(depth == 0 ||
					(state == FOLD_UNDEFINED && parent_fold == FOLD_AUTO_OPENED))
			{
				if(set_fold_state(folded_paths, full_path, FOLD_AUTO_CLOSED))
				{
					entry->folded = 1;
				}
			}
			else
			{
				const int idx = view->custom.entry_count - 1;
				const int filtered = add_files_recursively(view, full_path,
						excluded_paths, folded_paths, idx, 0, depth - 1);
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
		}

		free(full_path);

		show_progress("Building tree...", 1000);
	}

	free_string_array(lst, len);

	/* The prev_count != 0 check is to make sure that we won't create leaf instead
	 * of the whole tree (this is handled in flist_custom_finish()). */
	int show_empty_dir_leafs = (cfg.dot_dirs & DD_TREE_LEAFS_PARENT);
	if(show_empty_dir_leafs && !no_direct_parent && prev_count != 0 &&
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

/* Retrieves state of the fold if present.  Returns the state or
 * FOLD_UNDEFINED. */
static FoldState
get_fold_state(trie_t *folded_paths, const char full_path[])
{
	void *data;
	if(trie_get(folded_paths, full_path, &data) != 0)
	{
		return FOLD_UNDEFINED;
	}

	return (FoldState)(uintptr_t)data;
}

/* Sets state of a fold. */
static int
set_fold_state(trie_t *folded_paths, const char full_path[], FoldState state)
{
	return (trie_set(folded_paths, full_path, (void *)state) >= 0);
}

/* Checks whether file is visible according to all filters.  Returns non-zero if
 * so, otherwise zero is returned. */
static int
entry_is_visible(view_t *view, const char name[], const void *data)
{
	if(view->hide_dot && name[0] == '.')
	{
		return 0;
	}

	char full_path[PATH_MAX + 1];
	snprintf(full_path, sizeof(full_path), "%s/%s", flist_get_dir(view), name);

	const int is_dir = data_is_dir_entry(data, full_path);
	return filters_file_is_visible(view, flist_get_dir(view), name, is_dir,
			/*apply_local_filter=*/1);
}

/* Checks whether a candidate for adding to a tree is visible according to
 * filters.  Returns non-zero if so, otherwise zero is returned. */
static int
tree_candidate_is_visible(view_t *view, const char path[], const char name[],
		int is_dir, int apply_local_filter)
{
	if(view->hide_dot && name[0] == '.')
	{
		return 0;
	}

	return filters_file_is_visible(view, path, name, is_dir, apply_local_filter);
}

/* Adds ".." directory leaf of an empty directory to the tree which is being
 * built.  Returns non-zero on error, otherwise zero is returned. */
static int
add_directory_leaf(view_t *view, const char path[], int parent_pos)
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
	entry->owns_origin = 1;

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
init_parent_entry(view_t *view, dir_entry_t *entry, const char path[])
{
	struct stat s;

	init_dir_entry(view, entry, get_last_path_component(path));
	entry->type = FT_DIR;

	/* Load the inode info or leave blank values in entry. */
	if(os_lstat(path, &s) != 0)
	{
		fentry_free(entry);
		LOG_SERROR_MSG(errno, "Can't lstat() \"%s\"", path);
		log_cwd();
		return 1;
	}

#ifndef _WIN32
	entry->size = (uintmax_t)s.st_size;
	entry->uid = s.st_uid;
	entry->gid = s.st_gid;
	entry->mode = s.st_mode;
	entry->inode = s.st_ino;
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

int
flist_is_fs_backed(const view_t *view)
{
	/* Custom trees don't track file-system changes. */
	return (!flist_custom_active(view) || view->custom.type == CV_TREE);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
