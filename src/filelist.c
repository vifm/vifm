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
#include <fcntl.h>
#include <lm.h>
#include <windows.h>
#include <winioctl.h>
#endif

#include <curses.h>

#include <sys/stat.h> /* stat */
#include <unistd.h> /* close() fork() pipe() */

#include <assert.h> /* assert() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint64_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* abs() calloc() free() */
#include <string.h> /* memcpy() memset() strcat() strcmp() strcpy() strdup()
                       strlen() */
#include <time.h> /* localtime() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "compat/reallocarray.h"
#include "engine/mode.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "ui/statusbar.h"
#include "ui/statusline.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/filemon.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/tree.h"
#include "utils/trie.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "fileview.h"
#include "filtering.h"
#include "fuse.h"
#include "macros.h"
#include "opt_handlers.h"
#include "running.h"
#include "sort.h"
#include "status.h"
#include "types.h"

/* Custom argument for is_in_list() function. */
typedef struct
{
	int nitems;   /* Number of items in the list. */
	char **items; /* The list itself. */
}
list_t;

/* Type of predicate functions to reason about entries.  Should return non-zero
 * if particular property holds and zero otherwise. */
typedef int (*predicate_func)(const dir_entry_t *entry);

static void init_view(FileView *view);
static void init_flist(FileView *view);
static void reset_view(FileView *view);
static void init_view_history(FileView *view);
static void free_file_capture(FileView *view);
static void capture_selection(FileView *view);
static void correct_list_pos_down(FileView *view, size_t pos_delta);
static void correct_list_pos_up(FileView *view, size_t pos_delta);
static void move_cursor_out_of_scope(FileView *view, predicate_func pred);
static void navigate_to_history_pos(FileView *view, int pos);
static void save_selection(FileView *view);
static void free_saved_selection(FileView *view);
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
static int is_in_list(FileView *view, const dir_entry_t *entry, void *arg);
static void load_dir_list_internal(FileView *view, int reload, int draw_only);
static int populate_dir_list_internal(FileView *view, int reload);
static int is_dead_or_filtered(FileView *view, const dir_entry_t *entry,
		void *arg);
static void update_entries_data(FileView *view);
static int is_dir_big(const char path[]);
static void free_view_entries(FileView *view);
static int update_dir_list(FileView *view, int reload);
static int add_file_entry_to_view(const char name[], const void *data,
		void *param);
static void sort_dir_list(int msg, FileView *view);
static void merge_lists(FileView *view, dir_entry_t *entries, int len);
static void merge_entries(dir_entry_t *new, const dir_entry_t *prev);
static int correct_pos(FileView *view, int pos, int dist, int closes);
static int rescue_from_empty_filelist(FileView *view);
static void init_dir_entry(FileView *view, dir_entry_t *entry,
		const char name[]);
static void free_dir_entries(FileView *view, dir_entry_t **entries, int *count);
static dir_entry_t * alloc_dir_entry(dir_entry_t **list, int list_size);
static int file_can_be_displayed(const char directory[], const char filename[]);
TSTATIC void pick_cd_path(FileView *view, const char base_dir[],
		const char path[], int *updir, char buf[], size_t buf_size);
static void find_dir_in_cdpath(const char base_dir[], const char dst[],
		char buf[], size_t buf_size);
static int iter_entries(FileView *view, dir_entry_t **entry,
		predicate_func pred);
static int is_entry_selected(const dir_entry_t *entry);
static int is_entry_marked(const dir_entry_t *entry);
static void clear_marking(FileView *view);

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
	view->selected_filelist = NULL;
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
	view->dir_entry = calloc(1, sizeof(dir_entry_t));
	view->dir_entry[0].name = strdup("");
	view->dir_entry[0].type = FT_DIR;
	view->dir_entry[0].hi_num = -1;
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
	ui_view_sort_list_ensure_well_formed(view);
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
load_initial_directory(FileView *view, const char *dir)
{
	if(view->curr_dir[0] == '\0')
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
get_current_entry(FileView *view)
{
	if(view->list_pos < 0 || view->list_pos > view->list_rows)
	{
		return NULL;
	}

	return &view->dir_entry[view->list_pos];
}

char *
get_current_file_name(FileView *view)
{
	if(view->list_pos == -1)
		return "";
	return view->dir_entry[view->list_pos].name;
}

/* Frees memory from list of captured files. */
static void
free_file_capture(FileView *view)
{
	if(view->selected_filelist != NULL)
	{
		free_string_array(view->selected_filelist, view->selected_files);
		view->selected_filelist = NULL;
	}
}

/* Collects currently selected files in view->selected_filelist array.  Use
 * free_file_capture() to clean up memory allocated by this function. */
static void
capture_selection(FileView *view)
{
	int y;
	dir_entry_t *entry;

	recount_selected_files(view);

	if(view->selected_files == 0)
	{
		free_file_capture(view);
		return;
	}

	if(view->selected_filelist != NULL)
	{
		free_file_capture(view);
		/* Setting this because free_file_capture() doesn't do it. */
		view->selected_files = 0;
	}
	recount_selected_files(view);
	view->selected_filelist = calloc(view->selected_files, sizeof(char *));
	if(view->selected_filelist == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	y = 0;
	entry = NULL;
	while(iter_selected_entries(view, &entry))
	{
		if(is_parent_dir(entry->name))
		{
			entry->selected = 0;
			continue;
		}

		view->selected_filelist[y] = strdup(entry->name);
		if(view->selected_filelist[y] == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			break;
		}
		++y;
	}
	view->selected_files = y;
}

void
recount_selected_files(FileView *view)
{
	int i;

	view->selected_files = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->selected_files += (view->dir_entry[i].selected != 0);
	}
}

int
find_file_pos_in_list(const FileView *const view, const char file[])
{
	int i;
	for(i = 0; i < view->list_rows; i++)
	{
		if(stroscmp(view->dir_entry[i].name, file) == 0)
		{
			return i;
		}
	}
	return -1;
}

void
invert_sorting_order(FileView *view)
{
	view->sort[0] = -view->sort[0];
}

void
correct_list_pos(FileView *view, ssize_t pos_delta)
{
	if(pos_delta > 0)
	{
		correct_list_pos_down(view, pos_delta);
	}
	else if(pos_delta < 0)
	{
		correct_list_pos_up(view, -pos_delta);
	}
}

int
correct_list_pos_on_scroll_down(FileView *view, size_t lines_count)
{
	if(!all_files_visible(view))
	{
		correct_list_pos_down(view, lines_count*view->column_count);
		return 1;
	}
	return 0;
}

/* Tries to move cursor forwards by pos_delta positions. */
static void
correct_list_pos_down(FileView *view, size_t pos_delta)
{
	view->list_pos = get_corrected_list_pos_down(view, pos_delta);
}

int
correct_list_pos_on_scroll_up(FileView *view, size_t lines_count)
{
	if(!all_files_visible(view))
	{
		correct_list_pos_up(view, lines_count*view->column_count);
		return 1;
	}
	return 0;
}

/* Tries to move cursor backwards by pos_delta positions. */
static void
correct_list_pos_up(FileView *view, size_t pos_delta)
{
	view->list_pos = get_corrected_list_pos_up(view, pos_delta);
}

void
flist_set_pos(FileView *view, int pos)
{
	if(pos < 1)
	{
		pos = 0;
	}

	if(pos > view->list_rows - 1)
	{
		pos = view->list_rows - 1;
	}

	if(pos != -1)
	{
		view->list_pos = pos;
		fview_position_updated(view);
	}
}

void
flist_ensure_pos_is_valid(FileView *view)
{
	if(view->list_pos >= view->list_rows)
	{
		view->list_pos = view->list_rows - 1;
	}
}

void
move_cursor_out_of(FileView *view, FileListScope scope)
{
	switch(scope)
	{
		case FLS_SELECTION:
			move_cursor_out_of_scope(view, &is_entry_selected);
			break;
		case FLS_MARKING:
			move_cursor_out_of_scope(view, &is_entry_marked);
			break;

		default:
			assert(0 && "Unhandled file list scope type");
			break;
	}
}

/* Ensures that cursor is moved outside of entries that satisfy the predicate if
 * that's possible. */
static void
move_cursor_out_of_scope(FileView *view, predicate_func pred)
{
	/* TODO: if we reach bottom of the list and predicate holds try scanning to
	 * the top. */
	int i = view->list_pos;
	while(i < view->list_rows - 1 && pred(&view->dir_entry[i]))
	{
		++i;
	}
	view->list_pos = i;
}

void
navigate_backward_in_history(FileView *view)
{
	int pos = view->history_pos - 1;

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

	load_dir_list(view, 1);
	flist_set_pos(view, find_file_pos_in_list(view, view->history[pos].file));

	view->history_pos = pos;
}

void
clean_positions_in_history(FileView *view)
{
	int i;
	for(i = 0; i <= view->history_pos && i < view->history_num; ++i)
	{
		view->history[i].file[0] = '\0';
	}
}

void
save_view_history(FileView *view, const char *path, const char *file, int pos)
{
	int x;

	/* This could happen on FUSE error. */
	if(view->list_rows <= 0)
		return;

	if(cfg.history_len <= 0)
		return;

	if(flist_custom_active(view))
		return;

	if(path == NULL)
		path = view->curr_dir;
	if(file == NULL)
		file = view->dir_entry[view->list_pos].name;
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

static void
check_view_dir_history(FileView *view)
{
	int pos = 0;
	int rel_pos = -1;

	if(cfg.history_len > 0 && view->history_num > 0 && curr_stats.ch_pos)
	{
		int x;
		int found = 0;
		x = view->history_pos;
		if(stroscmp(view->history[x].dir, view->curr_dir) == 0 &&
				view->history[x].file[0] == '\0')
			x--;
		for(; x >= 0; x--)
		{
			if(strlen(view->history[x].dir) < 1)
				break;
			if(stroscmp(view->history[x].dir, view->curr_dir) == 0)
			{
				found = 1;
				break;
			}
		}
		if(found)
		{
			pos = find_file_pos_in_list(view, view->history[x].file);
			rel_pos = view->history[x].rel_pos;
		}
		else if(path_starts_with(view->last_dir, view->curr_dir) &&
				stroscmp(view->last_dir, view->curr_dir) != 0 &&
				strchr(view->last_dir + strlen(view->curr_dir) + 1, '/') == NULL)
		{
			char buf[NAME_MAX];
			snprintf(buf, sizeof(buf), "%s/",
					view->last_dir + strlen(view->curr_dir));

			pos = find_file_pos_in_list(view, buf);
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

int
at_first_line(const FileView *view)
{
	return view->list_pos/view->column_count == 0;
}

int
at_last_line(const FileView *view)
{
	size_t col_count = view->column_count;
	return view->list_pos/col_count == (view->list_rows - 1)/col_count;
}

int
at_first_column(const FileView *view)
{
	return view->list_pos%view->column_count == 0;
}

int
at_last_column(const FileView *view)
{
	return view->list_pos%view->column_count == view->column_count - 1;
}

void
go_to_start_of_line(FileView *view)
{
	view->list_pos = get_start_of_line(view);
}

int
get_start_of_line(const FileView *view)
{
	int pos = MAX(MIN(view->list_pos, view->list_rows - 1), 0);
	return ROUND_DOWN(pos, view->column_count);
}

int
get_end_of_line(const FileView *view)
{
	int pos = MAX(MIN(view->list_pos, view->list_rows - 1), 0);
	pos += (view->column_count - 1) - pos%view->column_count;
	return MIN(pos, view->list_rows - 1);
}

void
clean_selected_files(FileView *view)
{
	save_selection(view);
	erase_selection(view);
}

/* Saves list of selected files if any. */
static void
save_selection(FileView *view)
{
	if(view->selected_files != 0)
	{
		char **save_selected_filelist;

		free_string_array(view->saved_selection, view->nsaved_selection);

		save_selected_filelist = view->selected_filelist;
		view->selected_filelist = NULL;

		capture_selection(view);
		view->nsaved_selection = view->selected_files;
		view->saved_selection = view->selected_filelist;

		view->selected_filelist = save_selected_filelist;
	}
}

void
erase_selection(FileView *view)
{
	int i;

	/* This is needed, since otherwise we loose number of items in the array,
	 * which can cause access violation of memory leaks. */
	free_file_capture(view);

	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].selected = 0;
	}
	view->selected_files = 0;
}

void
invert_selection(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; i++)
	{
		dir_entry_t *const e = &view->dir_entry[i];
		if(!is_parent_dir(e->name))
		{
			e->selected = !e->selected;
		}
	}
	view->selected_files = view->list_rows - view->selected_files;
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
navigate_to_file(FileView *view, const char dir[], const char file[])
{
	/* Do not change directory if we already there. */
	if(!paths_are_equal(view->curr_dir, dir))
	{
		if(change_directory(view, dir) >= 0)
		{
			load_dir_list(view, 1);
		}
	}

	if(paths_are_equal(view->curr_dir, dir))
	{
		(void)ensure_file_is_selected(view, file);
	}
}

int
change_directory(FileView *view, const char directory[])
{
	char dir_dup[PATH_MAX];
	char real_path[PATH_MAX];
	const int was_in_custom_view = flist_custom_active(view);
	int location_changed;

	if(is_dir_list_loaded(view))
	{
		save_view_history(view, NULL, NULL, -1);
	}

	if(cfg.chase_links)
	{
		if(realpath(directory, real_path) == real_path)
		{
			/* Do this on success only, if realpath() fails, just go with original
			 * path. */
			directory = real_path;
		}
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

	if(!is_root_dir(dir_dup))
		chosp(dir_dup);

	if(!is_valid_dir(dir_dup))
	{
		show_error_msgf("Directory Access Error", "Cannot open %s", dir_dup);
		copy_str(view->curr_dir, sizeof(view->curr_dir), dir_dup);
		leave_invalid_dir(view);
		copy_str(dir_dup, sizeof(dir_dup), view->curr_dir);
	}

	location_changed = stroscmp(dir_dup, view->curr_dir) != 0;

	if(location_changed)
	{
		copy_str(view->last_dir, sizeof(view->last_dir), view->curr_dir);
		view->on_slow_fs = is_on_slow_fs(view->curr_dir);
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

		clean_selected_files(view);
		return -1;
	}

	if(os_access(dir_dup, X_OK) != 0 && !is_unc_root(dir_dup))
	{
		LOG_SERROR_MSG(errno, "Can't access(, X_OK) \"%s\"", dir_dup);
		log_cwd();

		show_error_msgf("Directory Access Error",
				"You do not have execute access on %s", dir_dup);

		clean_selected_files(view);
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

	if(location_changed)
	{
		filters_dir_updated(view);
		free_saved_selection(view);
	}
	else
	{
		save_selection(view);
	}
	erase_selection(view);

	/* Need to use setenv instead of getcwd for a symlink directory */
	env_set("PWD", dir_dup);

	copy_str(view->curr_dir, sizeof(view->curr_dir), dir_dup);

	if(is_dir_list_loaded(view))
	{
		save_view_history(view, NULL, "", -1);
	}

	/* Perform additional actions on leaving custom view. */
	if(was_in_custom_view && view->custom.unsorted)
	{
		memcpy(&view->sort[0], &view->custom.sort[0], sizeof(view->sort));
		load_sort_option(view);
	}

	return 0;
}

int
is_dir_list_loaded(FileView *view)
{
	dir_entry_t *const entry = (view->list_rows < 1) ? NULL : &view->dir_entry[0];
	return entry != NULL && entry->name[0] != '\0';
}

/* Frees list of previously selected files. */
static void
free_saved_selection(FileView *view)
{
	free_string_array(view->saved_selection, view->nsaved_selection);
	view->nsaved_selection = 0;
	view->saved_selection = NULL;
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
get_typed_current_fpath(const FileView *view)
{
	dir_entry_t *entry;
	const char *type_suffix;
	char full_path[PATH_MAX];

	entry = get_current_entry((FileView *)view);
	type_suffix = is_directory_entry(entry) ? "/" : "";

	get_full_path_of(entry, sizeof(full_path), full_path);
	return format_str("%s%s", full_path, type_suffix);
}

char *
get_typed_entry_fname(const dir_entry_t *entry)
{
	const char *const name = entry->name;
	return is_directory_entry(entry) ? format_str("%s/", name) : strdup(name);
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
	(void)replace_string(&view->custom.title, title);

	view->custom.paths_cache = trie_create();
}

void
flist_custom_add(FileView *view, const char path[])
{
	char canonic_path[PATH_MAX];
	dir_entry_t *dir_entry;

	if(to_canonic_path(path, canonic_path, sizeof(canonic_path)) != 0)
	{
		return;
	}

	/* Don't add duplicates. */
	if(trie_put(view->custom.paths_cache, canonic_path) != 0)
	{
		return;
	}

	dir_entry = alloc_dir_entry(&view->custom.entries, view->custom.entry_count);
	if(dir_entry == NULL)
	{
		return;
	}

	init_dir_entry(view, dir_entry, get_last_path_component(canonic_path));

	dir_entry->origin = strdup(canonic_path);
	remove_last_path_component(dir_entry->origin);

	if(fill_dir_entry_by_path(dir_entry, canonic_path) != 0)
	{
		free_dir_entry(view, dir_entry);
		return;
	}

	++view->custom.entry_count;
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
	entry->size = ((uintmax_t)ffd->nFileSizeHigh << 32) + ffd->nFileSizeLow;
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
flist_custom_finish(FileView *view)
{
	trie_free(view->custom.paths_cache);
	view->custom.paths_cache = NULL_TRIE;

	if(view->custom.entry_count == 0)
	{
		free_dir_entries(view, &view->custom.entries, &view->custom.entry_count);
		free(view->custom.title);
		view->custom.title = NULL;
		return 1;
	}

	if(cfg_parent_dir_is_visible(0))
	{
		dir_entry_t *const dir_entry = alloc_dir_entry(&view->custom.entries,
				view->custom.entry_count);
		if(dir_entry != NULL)
		{
			init_dir_entry(view, dir_entry, "..");
			dir_entry->type = FT_DIR;
			++view->custom.entry_count;
		}
	}

	if(view->curr_dir[0] != '\0')
	{
		(void)replace_string(&view->custom.orig_dir, view->curr_dir);
		view->curr_dir[0] = '\0';
	}

	ui_view_schedule_redraw(view);

	/* Replace view file list with custom list. */
	free_dir_entries(view, &view->dir_entry, &view->list_rows);
	view->dir_entry = view->custom.entries;
	view->list_rows = view->custom.entry_count;
	view->custom.entries = NULL;
	view->custom.entry_count = 0;

	sort_dir_list(0, view);

	flist_ensure_pos_is_valid(view);

	filters_dir_updated(view);

	return 0;
}

void
flist_custom_exclude(FileView *view)
{
	dir_entry_t *entry;
	int nfiles = 0;
	char **files = NULL;
	list_t list;

	if(!flist_custom_active(view))
	{
		return;
	}

	entry = NULL;
	while(iter_selection_or_current(view, &entry))
	{
		char full_path[PATH_MAX];
		get_full_path_of(entry, sizeof(full_path), full_path);

		nfiles = add_to_string_array(&files, nfiles, 1, full_path);
	}

	list.nitems = nfiles;
	list.items = files;

	(void)zap_entries(view, view->dir_entry, &view->list_rows, &is_in_list, &list,
			0);
	(void)zap_entries(view, view->custom.entries, &view->custom.entry_count,
			&is_in_list, &list, 1);

	free_string_array(files, nfiles);

	flist_ensure_pos_is_valid(view);
	ui_view_schedule_redraw(view);
}

/* zap_entries() filter to filter-out files from array of strings.  Returns
 * non-zero if entry is to be keeped and zero otherwise.*/
static int
is_in_list(FileView *view, const dir_entry_t *entry, void *arg)
{
	const list_t *list = arg;
	char full_path[PATH_MAX];
	get_full_path_of(entry, sizeof(full_path), full_path);
	return !is_in_string_array(list->items, list->nitems, full_path);
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

	get_current_full_path(view, sizeof(full_path), full_path);
	if(stroscmp(full_path, path) == 0)
	{
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

	if(to_canonic_path(path, canonic_path, sizeof(canonic_path)) != 0)
	{
		return NULL;
	}

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
		if(view->custom.entry_count != 0)
		{
			/* Load initial list of custom entries if it's available. */
			replace_dir_entries(view, &view->dir_entry, &view->list_rows,
					view->custom.entries, view->custom.entry_count);
		}

		(void)zap_entries(view, view->dir_entry, &view->list_rows,
				&is_dead_or_filtered, NULL, 0);
		update_entries_data(view);
		sort_dir_list(!reload, view);
		fview_list_updated(view);
		return 0;
	}

	if(update_dir_mtime(view) != 0 && !is_unc_root(view->curr_dir))
	{
		LOG_SERROR_MSG(errno, "Can't get directory mtime \"%s\"", view->curr_dir);
		return 1;
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
		clean_status_bar();
	}

	view->column_count = calculate_columns_count(view);

	/* If reloading the same directory don't jump to history position.  Stay at
	 * the current line. */
	if(!reload)
	{
		check_view_dir_history(view);
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

	return 0;
}

/* zap_entries() filter to filter-out inexistent files or files which names
 * match local filter. */
static int
is_dead_or_filtered(FileView *view, const dir_entry_t *entry, void *arg)
{
	if(!path_exists_at(entry->origin, entry->name, DEREF))
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
		dir_entry_t *const entry = &view->dir_entry[i];

		char full_path[PATH_MAX];
		get_full_path_of(entry, sizeof(full_path), full_path);

		/* Do not care about possible failure, just use previous meta-data. */
		(void)fill_dir_entry_by_path(entry, full_path);
	}
}

int
zap_entries(FileView *view, dir_entry_t *entries, int *count, zap_filter filter,
		void *arg, int allow_empty_list)
{
	int i, j;

	j = 0;
	for(i = 0; i < *count; ++i)
	{
		dir_entry_t *const entry = &entries[i];
		if(!filter(view, entry, arg))
		{
			free_dir_entry(view, entry);
			continue;
		}

		if(i != j)
		{
			entries[j] = entries[i];
		}

		++j;
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
	dir_entry_t *prev_dir_entries = NULL;
	int prev_list_rows = 0;

	if(reload)
	{
		prev_dir_entries = view->dir_entry;
		prev_list_rows = view->list_rows;
		view->dir_entry = NULL;
		view->list_rows = 0;
	}
	else
	{
		free_view_entries(view);
	}

	view->matches = 0;
	view->selected_files = 0;

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

	if(reload)
	{
		/* Merging must be performed after sorting so that list position remains
		 * fixed (sorting doesn't preserve it). */
		merge_lists(view, prev_dir_entries, prev_list_rows);
		free_dir_entries(view, &prev_dir_entries, &prev_list_rows);
	}

	return 0;
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

	if(view->hide_dot && name[0] == '.')
	{
		++view->filtered;
		return 0;
	}

	if(!file_is_visible(view, name, data_is_dir_entry(data)))
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
		clean_status_bar();
	}
}

/* Merges elements from previous list into the new one. */
static void
merge_lists(FileView *view, dir_entry_t *entries, int len)
{
	int i;
	int closes_dist;
	const int prev_pos = view->list_pos;
	trie_t prev_names = trie_create();

	for(i = 0; i < len; ++i)
	{
		const int code = trie_set(prev_names, entries[i].name, &entries[i]);
		assert(code == 0 && "Duplicated file names in the list?");
		(void)code;

		/* We won't use the name later, so free some memory. */
		update_string(&entries[i].name, NULL);
	}

	closes_dist = INT_MIN;
	for(i = 0; i < view->list_rows; ++i)
	{
		int dist;
		void *data;
		dir_entry_t *const entry = &view->dir_entry[i];
		if(trie_get(prev_names, entry->name, &data) != 0)
		{
			continue;
		}

		/* Transfer information from previous entry to the new one. */
		merge_entries(entry, data);

		/* Update number of selected files (should have been zeroed beforehand). */
		view->selected_files += (entry->selected != 0);

		/* Update cursor position in a smart way. */
		dist = (dir_entry_t*)data - entries - prev_pos;
		closes_dist = correct_pos(view, i, dist, closes_dist);
	}

	trie_free(prev_names);
}

/* Merges data from previous entry into the new one.  Both entries should
 * correspond to the same file (their names must match). */
static void
merge_entries(dir_entry_t *new, const dir_entry_t *prev)
{
	new->selected = prev->selected;
	new->was_selected = prev->was_selected;

	if(new->type == prev->type)
	{
		new->hi_num = prev->hi_num;
	}
}

/* Corrects selected item position in the list.  Returns updated value of the
 * closes variable, which initially should be INT_MIN. */
static int
correct_pos(FileView *view, int pos, int dist, int closes)
{
	if(dist == 0)
	{
		closes = 0;
		view->list_pos = pos;
	}
	else if((closes < 0 && dist > closes) || (closes > 0 && dist < closes))
	{
		closes = dist;
		view->list_pos = pos;
	}
	return closes;
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
	dir_entry_t *dir_entry;
	struct stat s;

	dir_entry = alloc_dir_entry(&view->dir_entry, view->list_rows);
	if(dir_entry == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	init_dir_entry(view, dir_entry, "..");
	dir_entry->type = FT_DIR;

	/* Load the inode info or leave blank values in dir_entry. */
	if(os_lstat(dir_entry->name, &s) != 0)
	{
		free_dir_entry(view, dir_entry);
		LOG_SERROR_MSG(errno, "Can't lstat() \"%s/%s\"", view->curr_dir,
				dir_entry->name);
		log_cwd();
		return;
	}

	dir_entry->size = (uintmax_t)s.st_size;
#ifndef _WIN32
	dir_entry->mode = s.st_mode;
	dir_entry->uid = s.st_uid;
	dir_entry->gid = s.st_gid;
#endif
	dir_entry->mtime = s.st_mtime;
	dir_entry->atime = s.st_atime;
	dir_entry->ctime = s.st_ctime;

	++view->list_rows;
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

	/* All files start as unselected, unmatched and unmarked. */
	entry->selected = 0;
	entry->was_selected = 0;
	entry->search_match = 0;
	entry->marked = 0;

	entry->list_num = -1;
}

void
replace_dir_entries(FileView *view, dir_entry_t **entries, int *count,
		const dir_entry_t *with_entries, int with_count)
{
	dir_entry_t *new;
	int i;

	new = reallocarray(NULL, with_count, sizeof(*new));
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

/* Frees list of directory entries related to the view.  Sets *entries and
 * *count to safe values. */
static void
free_dir_entries(FileView *view, dir_entry_t **entries, int *count)
{
	int i;
	for(i = 0; i < *count; ++i)
	{
		free_dir_entry(view, &(*entries)[i]);
	}

	free(*entries);
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

int
add_dir_entry(dir_entry_t **list, size_t *list_size, const dir_entry_t *entry)
{
	dir_entry_t *const new_entry = alloc_dir_entry(list, *list_size);
	if(new_entry == NULL)
	{
		return 1;
	}

	*new_entry = *entry;
	++*list_size;
	return 0;
}

/* Allocates one more directory entry for the *list of size list_size by
 * extending it.  Returns pointer to new entry or NULL on failure. */
static dir_entry_t *
alloc_dir_entry(dir_entry_t **list, int list_size)
{
	dir_entry_t *new_entry_list;
	new_entry_list = reallocarray(*list, list_size + 1, sizeof(dir_entry_t));
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

	if(view->on_slow_fs || flist_custom_active(view) ||
			is_unc_root(view->curr_dir))
	{
		return;
	}

#ifndef _WIN32
	{
		filemon_t mon;
		failed = filemon_from_file(view->curr_dir, &mon) != 0;
		changed = !failed && !filemon_equal(&mon, &view->mon);
	}
#else
	{
		const int r = win_check_dir_changed(view);
		failed = r < 0;
		changed = r > 0;
	}
#endif

	failed |= os_access(view->curr_dir, X_OK) != 0;

	if(failed)
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", view->curr_dir);
		log_cwd();

		show_error_msgf("Directory Change Check", "Cannot open %s", view->curr_dir);

		leave_invalid_dir(view);
		(void)change_directory(view, view->curr_dir);
		clean_selected_files(view);
		ui_view_schedule_reload(view);
		return;
	}

	if(changed)
	{
		ui_view_schedule_reload(view);
	}
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
	view->sort[0] = descending ? -type : type;
	memset(&view->sort[1], SK_NONE, sizeof(view->sort) - 1);

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
ensure_file_is_selected(FileView *view, const char name[])
{
	int file_pos;
	char nm[NAME_MAX];

	/* This is for compatibility with paths loaded from vifminfo that have
	 * trailing slash. */
	copy_str(nm, sizeof(nm), name);
	chosp(nm);

	file_pos = find_file_pos_in_list(view, nm);
	if(file_pos < 0 && file_can_be_displayed(view->curr_dir, nm))
	{
		if(nm[0] == '.')
		{
			set_dot_files_visible(view, 1);
			file_pos = find_file_pos_in_list(view, nm);
		}

		if(file_pos < 0)
		{
			remove_filename_filter(view);

			/* remove_filename_filter() postpones list of files reloading. */
			(void)populate_dir_list_internal(view, 1);

			file_pos = find_file_pos_in_list(view, nm);
		}
	}

	flist_set_pos(view, (file_pos < 0) ? 0 : file_pos);
	return file_pos >= 0;
}

/* Checks if file specified can be displayed. Used to filter some files, that
 * are hidden intentionally.  Returns non-zero if file can be made visible. */
static int
file_can_be_displayed(const char directory[], const char filename[])
{
	if(is_parent_dir(filename))
	{
		return cfg_parent_dir_is_visible(is_root_dir(directory));
	}
	return path_exists_at(directory, filename, DEREF);
}

int
cd(FileView *view, const char *base_dir, const char *path)
{
	char dir[PATH_MAX];
	int updir;

	pick_cd_path(view, base_dir, path, &updir, dir, sizeof(dir));

	if(updir)
	{
		cd_updir(view, 1);
	}
	else if(!cd_is_possible(dir) || change_directory(view, dir) < 0)
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
		return 0;
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

	if(is_directory_entry(entry))
	{
		char full_path[PATH_MAX];
		get_full_path_of(entry, sizeof(full_path), full_path);
		tree_get_data(curr_stats.dirsize_cache, full_path, &size);
	}

	return (size == 0) ? entry->size : size;
}

int
is_directory_entry(const dir_entry_t *entry)
{
	return (entry->type == FT_DIR)
	    || (entry->type == FT_LINK &&
	        get_symlink_type(entry->name) != SLT_UNKNOWN);
}

int
iter_selected_entries(FileView *view, dir_entry_t **entry)
{
	return iter_entries(view, entry, &is_entry_selected);
}

int
iter_active_area(FileView *view, dir_entry_t **entry)
{
	dir_entry_t *const current = &view->dir_entry[view->list_pos];
	if(current->selected)
	{
		return iter_selected_entries(view, entry);
	}
	else
	{
		*entry = (*entry == NULL) ? current : NULL;
		return *entry != NULL;
	}
}

int
iter_marked_entries(FileView *view, dir_entry_t **entry)
{
	return iter_entries(view, entry, &is_entry_marked);
}

static int
iter_entries(FileView *view, dir_entry_t **entry, predicate_func pred)
{
	int next = (*entry == NULL) ? 0 : (*entry - view->dir_entry + 1);

	while(next < view->list_rows)
	{
		dir_entry_t *const e = &view->dir_entry[next];
		if(pred(e) && !is_parent_dir(e->name))
		{
			*entry = e;
			return 1;
		}
		++next;
	}

	*entry = NULL;
	return 0;
}

static int
is_entry_selected(const dir_entry_t *entry)
{
	return entry->selected;
}

static int
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
		dir_entry_t *const current = &view->dir_entry[view->list_pos];
		*entry = (*entry == NULL) ? current : NULL;
		return *entry != NULL;
	}
	else
	{
		return iter_selected_entries(view, entry);
	}
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
		return;
	}

	if(!path_starts_with(path, flist_get_dir(view)))
	{
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
		copy_str(buf, buf_len, full_path);
		return;
	}

	assert(strlen(path) >= strlen(flist_get_dir(view)) && "Path is too short.");

	path += strlen(flist_get_dir(view));
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
		view->dir_entry[view->list_pos].marked = 1;
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

void
mark_selected(FileView *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].marked = view->dir_entry[i].selected;
	}
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
	if(very)
	{
		memcpy(&view->custom.sort[0], &view->sort[0], sizeof(view->custom.sort));
		memset(&view->sort[0], SK_NONE, sizeof(view->sort));
	}
	view->custom.unsorted = very;

	if(flist_custom_finish(view) != 0)
	{
		/* Restore sorting of the view. */
		if(very)
		{
			memcpy(&view->sort[0], &view->custom.sort[0], sizeof(view->sort[0]));
		}

		show_error_msg("Custom view", "Ignoring empty list of files");
		return;
	}

	if(very)
	{
		/* As custom view isn't activated until flist_custom_finish() is called,
		 * need to update option separately from view sort array. */
		load_sort_option(view);
	}

	flist_set_pos(view, 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
