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
#include <stdlib.h> /* abs() calloc() free() malloc() realloc() */
#include <string.h> /* memcpy() memset() strcat() strcmp() strcpy() strdup()
                       strlen() */
#include <time.h> /* localtime() */

#include "cfg/config.h"
#include "compat/os.h"
#include "engine/mode.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "ui/statusbar.h"
#include "ui/statusline.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/filemon.h"
#include "utils/filter.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/tree.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "color_scheme.h"
#include "column_view.h"
#include "fileview.h"
#include "fuse.h"
#include "macros.h"
#include "opt_handlers.h"
#include "running.h"
#include "sort.h"
#include "status.h"
#include "types.h"

/* Structure to communicate data during filling view with list files. */
typedef struct
{
	FileView *const view; /* View being filled. */
	const int is_root;    /* Whether we're at file system root. */
	int with_parent_dir;  /* Whether parent direcotory was seen during filling. */
}
dir_fill_info_t;

/* Type of predicate functions to reason about entries.  Should return non-zero
 * if particular property holds and zero otherwise. */
typedef int (*predicate_func)(const dir_entry_t *entry);

/* Type of filter function for zapping list of entries.  Should return non-zero
 * if entry is to be keeped and zero otherwise. */
typedef int (*zap_filter)(FileView *view, const dir_entry_t *entry, void *arg);

static void init_view(FileView *view);
static void reset_view(FileView *view);
static void reset_filter(filter_t *filter);
static void init_view_history(FileView *view);
static void capture_selection(FileView *view);
static void capture_file_or_selection(FileView *view, int skip_if_no_selection);
static void correct_list_pos_down(FileView *view, size_t pos_delta);
static void correct_list_pos_up(FileView *view, size_t pos_delta);
static void move_cursor_out_of_scope(FileView *view, predicate_func pred);
static void navigate_to_history_pos(FileView *view, int pos);
static size_t get_effective_scroll_offset(const FileView *view);
static void save_selection(FileView *view);
static void free_saved_selection(FileView *view);
static int add_file_entry_to_view(const char name[], const void *data,
		void *param);
TSTATIC int file_is_visible(FileView *view, const char filename[], int is_dir);
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
static dir_entry_t * entry_from_path(dir_entry_t *entries, int count,
		const char path[]);
static void load_dir_list_internal(FileView *view, int reload, int draw_only);
static int populate_dir_list_internal(FileView *view, int reload);
static int is_dead_or_filtered(FileView *view, const dir_entry_t *entry,
		void *arg);
static void update_entries_data(FileView *view);
static int zap_entries(FileView *view, dir_entry_t *entries, int *count,
		zap_filter filter, void *arg, int allow_empty_list);
static int is_dir_big(const char path[]);
static void free_view_entries(FileView *view);
static void sort_dir_list(int msg, FileView *view);
static int rescue_from_empty_filelist(FileView *view);
static void add_parent_dir(FileView *view);
static void append_slash(const char name[], char buf[], size_t buf_size);
static void ensure_filtered_list_not_empty(FileView *view,
		dir_entry_t *parent_entry);
static void local_filter_finish(FileView *view);
static void update_filtering_lists(FileView *view, int add, int clear);
static void init_dir_entry(FileView *view, dir_entry_t *entry,
		const char name[]);
static int is_newly_filtered(FileView *view, const dir_entry_t *entry,
		void *arg);
static int load_unfiltered_list(FileView *const view);
static void replace_dir_entries(FileView *view, dir_entry_t **entries,
		int *count, const dir_entry_t *with_entries, int with_count);
static void free_dir_entries(FileView *view, dir_entry_t **entries, int *count);
static void free_dir_entry(const FileView *view, dir_entry_t *entry);
static int get_unfiltered_pos(const FileView *const view, int pos);
static void store_local_filter_position(FileView *const view, int pos);
static int extract_previously_selected_pos(FileView *const view);
static void clear_local_filter_hist_after(FileView *const view, int pos);
static int find_nearest_neighour(const FileView *const view);
static int add_dir_entry(dir_entry_t **list, size_t *list_size,
		const dir_entry_t *entry);
static dir_entry_t * alloc_dir_entry(dir_entry_t **list, int list_size);
static int file_can_be_displayed(const char directory[], const char filename[]);
static int parent_dir_is_visible(int in_root);
static void find_dir_in_cdpath(const char base_dir[], const char dst[],
		char buf[], size_t buf_size);
static int iter_selection_or_current(FileView *view, dir_entry_t **entry);
static int iter_entries(FileView *view, dir_entry_t **entry,
		predicate_func pred);
static int is_entry_selected(const dir_entry_t *entry);
static int is_entry_marked(const dir_entry_t *entry);
static void clear_marking(FileView *view);

void
init_filelists(void)
{
	init_view(&rwin);
	init_view(&lwin);

	curr_view = &lwin;
	other_view = &rwin;

	fview_init();
}

/* Loads initial display values into view structure. */
static void
init_view(FileView *view)
{
	view->curr_line = 0;
	view->top_line = 0;
	view->list_rows = 0;
	view->list_pos = 0;
	view->selected_filelist = NULL;
	view->history_num = 0;
	view->history_pos = 0;
	view->local_cs = 0;
	view->on_slow_fs = 0;

	view->hide_dot = 1;
	view->matches = 0;
	view->columns = columns_create();
	view->view_columns = strdup("");

	view->custom.entries = NULL;
	view->custom.entry_count = 0;
	view->custom.orig_dir = NULL;
	view->custom.title = NULL;

	reset_view(view);

	init_view_history(view);
	reset_view_sort(view);
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
	view->invert = cfg.filter_inverted_by_default ? 1 : 0;
	view->prev_invert = view->invert;
	view->ls_view = 0;
	view->max_filename_width = 0;
	view->column_count = 1;

	view->num_type = NT_NONE;
	view->num_width = 4;
	view->real_num_width = 0;

	view->postponed_redraw = 0;
	view->postponed_reload = 0;

	(void)replace_string(&view->prev_manual_filter, "");
	reset_filter(&view->manual_filter);
	(void)replace_string(&view->prev_auto_filter, "");
	reset_filter(&view->auto_filter);

	(void)replace_string(&view->local_filter.prev, "");
	reset_filter(&view->local_filter.filter);
	view->local_filter.in_progress = 0;
	view->local_filter.saved = NULL;
	view->local_filter.poshist = NULL;
	view->local_filter.poshist_len = 0U;

	memset(&view->sort[0], SK_NONE, sizeof(view->sort));
	ui_view_sort_list_ensure_well_formed(view->sort);
}

/* Resets filter to empty state (either initializes or clears it). */
static void
reset_filter(filter_t *filter)
{
	if(filter->raw == NULL)
	{
		(void)filter_init(filter, FILTER_DEF_CASE_SENSITIVITY);
	}
	else
	{
		filter_clear(filter);
	}
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

	view->dir_entry = calloc(1, sizeof(dir_entry_t));

	view->dir_entry[0].name = strdup("");
	view->dir_entry[0].type = FT_DIR;
	view->dir_entry[0].hi_num = -1;
	view->dir_entry[0].origin = &view->curr_dir[0];

	view->list_rows = 1;
	if(!is_root_dir(view->curr_dir))
	{
		chosp(view->curr_dir);
	}
	(void)change_directory(view, dir);
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

void
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
	capture_file_or_selection(view, 1);
}

/* Collects currently selected files (or current file only if no selection
 * present and skip_if_no_selection is zero) in view->selected_filelist array.
 * Use free_file_capture() to clean up memory allocated by this function. */
static void
capture_file_or_selection(FileView *view, int skip_if_no_selection)
{
	int y;
	dir_entry_t *entry;

	recount_selected_files(view);

	if(view->selected_files == 0)
	{
		if(skip_if_no_selection)
		{
			free_file_capture(view);
			return;
		}

		/* No selected files so just use the current file. */
		view->dir_entry[view->list_pos].selected = 1;
		view->selected_files = 1;
		view->user_selection = 0;
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
reset_view_sort(FileView *view)
{
	if(view->view_columns[0] == '\0')
	{
		column_info_t column_info =
		{
			.column_id = SK_BY_NAME, .full_width = 0UL, .text_width = 0UL,
			.align = AT_LEFT,        .sizing = ST_AUTO, .cropping = CT_NONE,
		};

		columns_clear(view->columns);
		columns_add_column(view->columns, column_info);

		column_info.column_id = get_secondary_key(abs(view->sort[0]));
		column_info.align = AT_RIGHT;
		columns_add_column(view->columns, column_info);
	}
	else if(strstr(view->view_columns, "{}") != NULL)
	{
		load_view_columns_option(view, view->view_columns);
	}
}

void
invert_sorting_order(FileView *view)
{
	view->sort[0] = -view->sort[0];
}

void
update_scroll_bind_offset(void)
{
	int rwin_pos = rwin.top_line/rwin.column_count;
	int lwin_pos = lwin.top_line/lwin.column_count;
	curr_stats.scroll_bind_off = rwin_pos - lwin_pos;
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
get_corrected_list_pos_down(const FileView *view, size_t pos_delta)
{
	int scroll_offset = get_effective_scroll_offset(view);
	if(view->list_pos <=
			view->top_line + scroll_offset + (MAX((int)pos_delta, 1) - 1))
	{
		size_t column_correction = view->list_pos%view->column_count;
		size_t offset = scroll_offset + pos_delta + column_correction;
		return view->top_line + offset;
	}
	return view->list_pos;
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

int
get_corrected_list_pos_up(const FileView *view, size_t pos_delta)
{
	int scroll_offset = get_effective_scroll_offset(view);
	int last = get_last_visible_file(view);
	if(view->list_pos >= last - scroll_offset - (MAX((int)pos_delta, 1) - 1))
	{
		size_t column_correction = (view->column_count - 1) -
				view->list_pos%view->column_count;
		size_t offset = scroll_offset + pos_delta + column_correction;
		return last - offset;
	}
	return view->list_pos;
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
	curr_stats.skip_history = 1;
	if(change_directory(view, view->history[pos].dir) < 0)
	{
		curr_stats.skip_history = 0;
		return;
	}
	curr_stats.skip_history = 0;

	load_dir_list(view, 1);
	move_to_list_pos(view, find_file_pos_in_list(view, view->history[pos].file));

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

	if(curr_stats.skip_history)
		return;

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
		view->top_line = pos - MIN(view->window_cells - 1, rel_pos);
		if(view->top_line < 0)
			view->top_line = 0;
		view->curr_line = pos - view->top_line;
	}
	else
	{
		size_t last = get_last_visible_file(view);
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
consider_scroll_offset(FileView *view)
{
	int need_redraw = 0;
	int pos = view->list_pos;
	if(cfg.scroll_off > 0)
	{
		size_t s = get_effective_scroll_offset(view);
		/* Check scroll offset at the top. */
		if(can_scroll_up(view) && pos - view->top_line < s)
		{
			scroll_up(view, s - (pos - view->top_line));
			need_redraw = 1;
		}
		/* Check scroll offset at the bottom. */
		if(can_scroll_down(view))
		{
			size_t last = get_last_visible_file(view);
			if(pos > last - s)
			{
				scroll_down(view, s + (pos - last));
				need_redraw = 1;
			}
		}
	}
	return need_redraw;
}

/* Returns scroll offset value for the view taking view height into account. */
static size_t
get_effective_scroll_offset(const FileView *view)
{
	int val = MIN(DIV_ROUND_UP(view->window_rows, 2), MAX(cfg.scroll_off, 0));
	return val*view->column_count;
}

int
can_scroll_up(const FileView *view)
{
	return view->top_line > 0;
}

int
can_scroll_down(const FileView *view)
{
	return get_last_visible_file(view) < view->list_rows - 1;
}

void
scroll_by_files(FileView *view, ssize_t by)
{
	if(by > 0)
	{
		scroll_down(view, by);
	}
	else if(by < 0)
	{
		scroll_up(view, -by);
	}
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

size_t
get_window_top_pos(const FileView *view)
{
	if(view->top_line == 0)
	{
		return 0;
	}
	else
	{
		return view->top_line + get_effective_scroll_offset(view);
	}
}

size_t
get_window_middle_pos(const FileView *view)
{
	int list_middle = view->list_rows/(2*view->column_count);
	int window_middle = view->window_rows/2;
	return view->top_line + MIN(list_middle, window_middle)*view->column_count;
}

size_t
get_window_bottom_pos(const FileView *view)
{
	if(all_files_visible(view))
	{
		size_t last = view->list_rows - 1;
		return last - last%view->column_count;
	}
	else
	{
		size_t off = get_effective_scroll_offset(view);
		size_t column_correction = view->column_count - 1;
		return get_last_visible_file(view) - off - column_correction;
	}
}

size_t
get_last_visible_file(const FileView *view)
{
	return view->top_line + view->window_cells - 1;
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
		move_to_list_pos(view, view->list_pos);
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
#ifdef _WIN32
		if(directory[0] == '/')
			snprintf(newdir, sizeof(newdir), "%c:%s", view->curr_dir[0], directory);
		else
#endif
			snprintf(newdir, sizeof(newdir), "%s/%s", view->curr_dir, directory);
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
	if(is_parent_dir(directory) && fuse_is_in_mounted_dir(view->curr_dir))
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
		filter_clear(&view->local_filter.filter);
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
	return 0;
}

int
is_dir_list_loaded(FileView *view)
{
	dir_entry_t *entry = &view->dir_entry[0];
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

static void
reset_selected_files(FileView *view, int need_free)
{
	int i;
	for(i = 0; i < view->selected_files; ++i)
	{
		if(view->selected_filelist[i] != NULL)
		{
			const int pos = find_file_pos_in_list(view, view->selected_filelist[i]);
			if(pos >= 0 && pos < view->list_rows)
			{
				view->dir_entry[pos].selected = 1;
			}
		}
	}

	if(need_free)
	{
		i = view->selected_files;
		free_file_capture(view);
		view->selected_files = i;
	}
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

static int
refill_dir_list(FileView *view)
{
	dir_fill_info_t info = {
		.view = view,
		.is_root = is_root_dir(view->curr_dir),
		.with_parent_dir = 0,
	};

	view->matches = 0;
	free_view_entries(view);

#ifdef _WIN32
	if(is_unc_root(view->curr_dir))
	{
		fill_with_shared(view);
		return 0;
	}
#endif

	if(enum_dir_content(view->curr_dir, &add_file_entry_to_view, &info) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't opendir() \"%s\"", view->curr_dir);
		return 1;
	}

#ifdef _WIN32
	/* Not all Windows file systems provide standard dot directories. */
	if(!info.with_parent_dir && parent_dir_is_visible(info.is_root))
	{
		add_parent_dir(view);
		info.with_parent_dir = 1;
	}
#endif

	if(!info.with_parent_dir && !info.is_root)
	{
		if((cfg.dot_dirs & DD_NONROOT_PARENT) || view->list_rows == 0)
		{
			add_parent_dir(view);
		}
	}

	return 0;
}

/* enum_dir_content() callback that appends files to file list.  Returns zero on
 * success or non-zero to indicate failure and stop enumeration. */
static int
add_file_entry_to_view(const char name[], const void *data, void *param)
{
	dir_fill_info_t *const info = param;
	FileView *view = info->view;
	dir_entry_t *entry;

	/* Always ignore the "." directory. */
	if(strcmp(name, ".") == 0)
	{
		return 0;
	}

	/* Always include the ".." directory unless it is the root directory. */
	if(strcmp(name, "..") == 0)
	{
		if(!parent_dir_is_visible(info->is_root))
		{
			return 0;
		}

		info->with_parent_dir = 1;
	}
	else if(view->hide_dot && name[0] == '.')
	{
		++view->filtered;
		return 0;
	}
	else if(!file_is_visible(view, name, data_is_dir_entry(data)))
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

/* Checks whether file/directory passes filename filters of the view.  Returns
 * non-zero if given filename passes filter and should be visible, otherwise
 * zero is returned, in which case the file should be hidden. */
TSTATIC int
file_is_visible(FileView *view, const char filename[], int is_dir)
{
	/* FIXME: some very long file names won't be matched against some
	 * regexps. */
	char name_with_slash[NAME_MAX + 1 + 1];
	if(is_dir)
	{
		append_slash(filename, name_with_slash, sizeof(name_with_slash));
		filename = name_with_slash;
	}

	if(filter_matches(&view->auto_filter, filename) > 0)
	{
		return 0;
	}

	if(filter_matches(&view->local_filter.filter, filename) == 0)
	{
		return 0;
	}

	if(filter_is_empty(&view->manual_filter))
	{
		return 1;
	}

	if(filter_matches(&view->manual_filter, filename) > 0)
	{
		return !view->invert;
	}
	else
	{
		return view->invert;
	}
}

char *
get_typed_current_fname(const FileView *view)
{
	return get_typed_entry_fname(&view->dir_entry[view->list_pos]);
}

char *
get_typed_entry_fname(const dir_entry_t *entry)
{
	const char *const name = entry->name;
	return is_directory_entry(entry) ? format_str("%s/", name) : strdup(name);
}

char *
get_typed_fname(const char path[])
{
	const char *const last_part = get_last_path_component(path);
	return is_dir(path) ? format_str("%s/", last_part) : strdup(last_part);
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
}

void
flist_custom_add(FileView *view, const char path[])
{
	char canonic_path[PATH_MAX];
	dir_entry_t *dir_entry;

	/* Don't add duplicates. */
	if(entry_from_path(view->custom.entries, view->custom.entry_count,
				path) != NULL)
	{
		return;
	}

	dir_entry = alloc_dir_entry(&view->custom.entries, view->custom.entry_count);
	if(dir_entry == NULL)
	{
		return;
	}

	if(to_canonic_path(path, canonic_path, sizeof(canonic_path)) != 0)
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
	if(view->custom.entry_count == 0)
	{
		free_dir_entries(view, &view->custom.entries, &view->custom.entry_count);
		return 1;
	}

	if(parent_dir_is_visible(0))
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

	sort_dir_list(0, view);

	ui_view_schedule_redraw(view);

	if(view->list_pos >= view->list_rows)
	{
		view->list_pos = view->list_rows - 1;
	}

	free_dir_entries(view, &view->dir_entry, &view->list_rows);
	view->dir_entry = view->custom.entries;
	view->list_rows = view->custom.entry_count;
	view->custom.entries = NULL;
	view->custom.entry_count = 0;
	filter_clear(&view->local_filter.filter);

	return 0;
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
	dir_entry_t *entry = entry_from_path(view->dir_entry, view->list_rows, path);
	if(entry != NULL)
	{
		view->list_pos = entry_to_pos(view, entry);
	}
}

/* Finds directory entry in the list of entries by the path.  Returns pointer to
 * the found entry or NULL. */
static dir_entry_t *
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
	int need_free = (view->selected_filelist == NULL);

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

	if(reload && view->selected_files > 0 && view->selected_filelist == NULL)
	{
		capture_selection(view);
	}

	if(refill_dir_list(view) != 0)
	{
		/* We don't have read access, only execute, or there were other problems. */
		free_view_entries(view);
		add_parent_dir(view);
	}

	sort_dir_list(!reload, view);

	if(!reload && !vle_mode_is(CMDLINE_MODE))
	{
		clean_status_bar();
	}

	view->column_count = calculate_columns_count(view);

	/* If reloading the same directory don't jump to
	 * history position.  Stay at the current line
	 */
	if(!reload)
		check_view_dir_history(view);

	view->local_cs = check_directory_for_color_scheme(view == &lwin,
			view->curr_dir);

	if(view->list_rows < 1)
	{
		if(rescue_from_empty_filelist(view))
		{
			return 0;
		}

		add_parent_dir(view);
	}

	fview_list_updated(view);

	if(reload && view->selected_files != 0)
	{
		reset_selected_files(view, need_free);
	}
	else if(view->selected_files != 0)
	{
		view->selected_files = 0;
	}

	return 0;
}

/* zap_entries() filter to filter-out inexistent files or files which names
 * match local filter. */
static int
is_dead_or_filtered(FileView *view, const dir_entry_t *entry, void *arg)
{
	/* FIXME: some very long file names won't be matched against some
		* regexps. */
	char name_with_slash[NAME_MAX + 1 + 1];
	const char *filename = entry->name;
	if(is_directory_entry(entry))
	{
		append_slash(filename, name_with_slash, sizeof(name_with_slash));
		filename = name_with_slash;
	}

	return path_exists_at(entry->origin, entry->name, DEREF)
	    && filter_matches(&view->local_filter.filter, filename) != 0;
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

/* Removes dead entries (those that refer to non-existing files) or those that
 * do not match local filter from the view.  Returns number of erased
 * entries. */
static int
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

/* Performs actions needed to rescue from abnormal situation with empty
 * filelist.  Returns non-zero if file list was reloaded. */
static int
rescue_from_empty_filelist(FileView *view)
{
	/*
	 * It is possible to set the file name filter so that no files are showing
	 * in the / directory.  All other directories will always show at least the
	 * ../ file.  This resets the filter and reloads the directory.
	 */
	if(filter_is_empty(&view->manual_filter))
	{
		return 0;
	}

	show_error_msgf("Filter error",
			"The %s\"%s\" pattern did not match any files. It was reset.",
			view->invert ? "" : "inverted ", view->manual_filter.raw);
	filter_clear(&view->auto_filter);
	filter_clear(&view->manual_filter);
	view->invert = 1;

	load_dir_list(view, 1);
	return 1;
}

/* Adds parent directory entry (..) to filelist. */
static void
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
filter_selected_files(FileView *view)
{
	dir_entry_t *entry;
	filter_t filter;
	int filtered;

	(void)filter_init(&filter, FILTER_DEF_CASE_SENSITIVITY);

	/* Traverse items and update/create filter values. */
	entry = NULL;
	while(iter_selection_or_current(view, &entry))
	{
		const char *name = entry->name;
		char name_with_slash[NAME_MAX + 1 + 1];

		if(is_directory_entry(entry))
		{
			append_slash(entry->name, name_with_slash, sizeof(name_with_slash));
			name = name_with_slash;
		}

		(void)filter_append(&view->auto_filter, name);
		(void)filter_append(&filter, name);
	}

	/* Update entry lists to remove entries that must be filtered out now.  No
	 * view reload is needed. */
	filtered = zap_entries(view, view->dir_entry, &view->list_rows,
			&is_newly_filtered, &filter, 0);
	if(flist_custom_active(view))
	{
		(void)zap_entries(view, view->custom.entries, &view->custom.entry_count,
				&is_newly_filtered, &filter, 1);
	}
	else
	{
		view->filtered += filtered;
	}

	filter_dispose(&filter);

	if(view->list_pos >= view->list_rows)
	{
		view->list_pos = view->list_rows - 1;
	}

	ui_view_schedule_redraw(view);
}

/* zap_entries() filter to filter-out files that match filter passed in the
 * arg. */
static int
is_newly_filtered(FileView *view, const dir_entry_t *entry, void *arg)
{
	filter_t *const filter = arg;

	/* FIXME: some very long file names won't be matched against some
	 * regexps. */
	char name_with_slash[NAME_MAX + 1 + 1];
	const char *filename = entry->name;

	if(is_directory_entry(entry))
	{
		append_slash(filename, name_with_slash, sizeof(name_with_slash));
		filename = name_with_slash;
	}

	return filter_matches(filter, filename) == 0;
}

void
set_dot_files_visible(FileView *view, int visible)
{
	view->hide_dot = !visible;
	ui_view_schedule_reload(view);
}

void
toggle_dot_files(FileView *view)
{
	set_dot_files_visible(view, view->hide_dot);
}

void
remove_filename_filter(FileView *view)
{
	if(filter_is_empty(&view->manual_filter) &&
			filter_is_empty(&view->auto_filter))
	{
		return;
	}

	(void)replace_string(&view->prev_manual_filter, view->manual_filter.raw);
	filter_clear(&view->manual_filter);
	(void)replace_string(&view->prev_auto_filter, view->auto_filter.raw);
	filter_clear(&view->auto_filter);

	view->prev_invert = view->invert;
	view->invert = cfg.filter_inverted_by_default ? 1 : 0;
	ui_view_schedule_full_reload(view);
}

void
restore_filename_filter(FileView *view)
{
	(void)filter_set(&view->manual_filter, view->prev_manual_filter);
	(void)filter_set(&view->auto_filter, view->prev_auto_filter);
	view->invert = view->prev_invert;
	ui_view_schedule_full_reload(view);
}

void
toggle_filter_inversion(FileView *view)
{
	view->invert = !view->invert;
	load_dir_list(view, 1);
	move_to_list_pos(view, 0);
}

void
local_filter_set(FileView *view, const char filter[])
{
	const int current_file_pos = view->local_filter.in_progress
		? get_unfiltered_pos(view, view->list_pos)
		: load_unfiltered_list(view);

	if(current_file_pos >= 0)
	{
		store_local_filter_position(view, current_file_pos);
	}

	(void)filter_change(&view->local_filter.filter, filter,
			!regexp_should_ignore_case(filter));

	update_filtering_lists(view, 1, 0);
}

/* Loads full list of files into unfiltered list of the view.  Returns positon
 * of file under cursor in the unfiltered list. */
static int
load_unfiltered_list(FileView *const view)
{
	int current_file_pos = view->list_pos;

	view->local_filter.in_progress = 1;

	view->local_filter.saved = strdup(view->local_filter.filter.raw);

	if(view->filtered > 0)
	{
		char full_path[PATH_MAX];
		dir_entry_t *entry;

		get_current_full_path(view, sizeof(full_path), full_path);

		filter_clear(&view->local_filter.filter);
		populate_dir_list(view, 1);

		/* Resolve current file position in updated list. */
		entry = entry_from_path(view->dir_entry, view->list_rows, full_path);
		if(entry != NULL)
		{
			current_file_pos = entry_to_pos(view, entry);
		}

		if(current_file_pos >= view->list_rows)
		{
			current_file_pos = view->list_rows - 1;
		}
	}
	else
	{
		/* Save unfiltered (by local filter) list for further use. */
		replace_dir_entries(view, &view->custom.entries,
				&view->custom.entry_count, view->dir_entry, view->list_rows);
	}

	view->local_filter.unfiltered = view->dir_entry;
	view->local_filter.unfiltered_count = view->list_rows;
	view->local_filter.prefiltered_count = view->filtered;
	view->dir_entry = NULL;

	return current_file_pos;
}

/* Replaces all entries of the *entries with copy of with_entries elements. */
static void
replace_dir_entries(FileView *view, dir_entry_t **entries, int *count,
		const dir_entry_t *with_entries, int with_count)
{
	dir_entry_t *new;
	int i;

	new = malloc(sizeof(*new)*with_count);
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

/* Frees single directory entry. */
static void
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

/* Gets position of an item in dir_entry list at position pos in the unfiltered
 * list.  Returns index on success, otherwise -1 is returned. */
static int
get_unfiltered_pos(const FileView *const view, int pos)
{
	const size_t count = view->local_filter.unfiltered_count;
	const char *const filename = view->dir_entry[pos].name;
	while(pos < count)
	{
		/* Compare pointers, which are the same for same entry in two arrays. */
		if(view->local_filter.unfiltered[pos].name == filename)
		{
			return pos;
		}
		pos++;
	}
	return -1;
}

/* Adds local filter position (in unfiltered list) to position history. */
static void
store_local_filter_position(FileView *const view, int pos)
{
	size_t *const len = &view->local_filter.poshist_len;
	int *const arr = realloc(view->local_filter.poshist, sizeof(int)*(*len + 1));
	if(arr != NULL)
	{
		view->local_filter.poshist = arr;
		arr[*len] = pos;
		++*len;
	}
}

void
local_filter_update_view(FileView *view, int rel_pos)
{
	int pos = extract_previously_selected_pos(view);

	if(pos < 0)
	{
		pos = find_nearest_neighour(view);
	}

	if(pos >= 0)
	{
		if(pos == 0 && is_parent_dir(view->dir_entry[0].name) &&
				view->list_rows > 0)
		{
			pos++;
		}

		view->list_pos = pos;
		view->top_line = pos - rel_pos;
	}
}

/* Finds one of previously selected files and updates list of visited files
 * if needed.  Returns file position in current list of files or -1. */
static int
extract_previously_selected_pos(FileView *const view)
{
	size_t i;
	for(i = 0; i < view->local_filter.poshist_len; i++)
	{
		const int unfiltered_pos = view->local_filter.poshist[i];
		const char *const file = view->local_filter.unfiltered[unfiltered_pos].name;
		const int filtered_pos = find_file_pos_in_list(view, file);

		if(filtered_pos >= 0)
		{
			clear_local_filter_hist_after(view, i);
			return filtered_pos;
		}
	}
	return -1;
}

/* Clears all positions after the pos. */
static void
clear_local_filter_hist_after(FileView *const view, int pos)
{
	view->local_filter.poshist_len -= view->local_filter.poshist_len - (pos + 1);
}

/* Find nearest filtered neighbour.  Returns index of nearest unfiltered
 * neighbour of the entry initially pointed to by cursor. */
static int
find_nearest_neighour(const FileView *const view)
{
	const int count = view->local_filter.unfiltered_count;

	if(view->local_filter.poshist_len > 0U)
	{
		const int unfiltered_orig_pos = view->local_filter.poshist[0U];
		int i;
		for(i = unfiltered_orig_pos; i < count; i++)
		{
			const int filtered_pos = find_file_pos_in_list(view,
					view->local_filter.unfiltered[i].name);
			if(filtered_pos >= 0)
			{
				return filtered_pos;
			}
		}
	}

	assert(view->list_rows > 0 && "List of files is empty.");
	return view->list_rows - 1;
}

void
local_filter_accept(FileView *view)
{
	if(!view->local_filter.in_progress)
	{
		return;
	}

	update_filtering_lists(view, 0, 1);

	local_filter_finish(view);

	cfg_save_filter_history(view->local_filter.filter.raw);

	/* Some of previously selected files could be filtered out, update number of
	 * selected files. */
	recount_selected_files(view);
}

void
local_filter_apply(FileView *view, const char filter[])
{
	if(view->local_filter.in_progress)
	{
		assert(!view->local_filter.in_progress && "Wrong local filter applying.");
		return;
	}

	(void)filter_set(&view->local_filter.filter, filter);
	cfg_save_filter_history(view->local_filter.filter.raw);
}

void
local_filter_cancel(FileView *view)
{
	if(!view->local_filter.in_progress)
	{
		return;
	}

	(void)filter_set(&view->local_filter.filter, view->local_filter.saved);

	free(view->dir_entry);
	view->dir_entry = NULL;
	view->list_rows = 0;

	update_filtering_lists(view, 1, 1);
	local_filter_finish(view);
}

/* Copies/moves elements of the unfiltered list into dir_entry list.  add
 * parameter controls whether entries matching filter are copied into dir_entry
 * list.  clear parameter controls whether entries not matching filter are
 * cleared in unfiltered list. */
static void
update_filtering_lists(FileView *view, int add, int clear)
{
	size_t i;
	size_t list_size = 0U;
	dir_entry_t *parent_entry = NULL;

	for(i = 0; i < view->local_filter.unfiltered_count; i++)
	{
		/* FIXME: some very long file names won't be matched against some
		 * regexps. */
		char name_with_slash[NAME_MAX + 1 + 1];

		dir_entry_t *const entry = &view->local_filter.unfiltered[i];
		const char *name = entry->name;

		if(is_parent_dir(name))
		{
			parent_entry = entry;
			if(add && parent_dir_is_visible(is_root_dir(view->curr_dir)))
			{
				(void)add_dir_entry(&view->dir_entry, &list_size, entry);
			}
			continue;
		}

		if(is_directory_entry(entry))
		{
			append_slash(name, name_with_slash, sizeof(name_with_slash));
			name = name_with_slash;
		}

		if(filter_matches(&view->local_filter.filter, name) != 0)
		{
			if(add)
			{
				(void)add_dir_entry(&view->dir_entry, &list_size, entry);
			}
		}
		else
		{
			if(clear)
			{
				free_dir_entry(view, entry);
			}
		}
	}

	if(add)
	{
		view->list_rows = list_size;
		view->filtered = view->local_filter.prefiltered_count
		               + view->local_filter.unfiltered_count - list_size;
		ensure_filtered_list_not_empty(view, parent_entry);
	}
}

/* Appends slash to the name and stores result in the buffer. */
static void
append_slash(const char name[], char buf[], size_t buf_size)
{
	const size_t nchars = copy_str(buf, buf_size - 1, name);
	buf[nchars - 1] = '/';
	buf[nchars] = '\0';
}

/* Use parent_entry to make filtered list not empty, or create such entry (if
 * parent_entry is NULL) and put it to original list. */
static void
ensure_filtered_list_not_empty(FileView *view, dir_entry_t *parent_entry)
{
	if(view->list_rows != 0U)
	{
		return;
	}

	if(parent_entry == NULL)
	{
		add_parent_dir(view);
		if(view->list_rows > 0)
		{
			(void)add_dir_entry(&view->local_filter.unfiltered,
					&view->local_filter.unfiltered_count,
					&view->dir_entry[view->list_rows - 1]);
		}
	}
	else
	{
		size_t list_size = 0U;
		(void)add_dir_entry(&view->dir_entry, &list_size, parent_entry);
		view->list_rows = list_size;
	}
}

/* Finishes filtering process and frees associated resources. */
static void
local_filter_finish(FileView *view)
{
	free(view->local_filter.unfiltered);
	free(view->local_filter.saved);
	view->local_filter.in_progress = 0;

	free(view->local_filter.poshist);
	view->local_filter.poshist = NULL;
	view->local_filter.poshist_len = 0U;
}

void
local_filter_remove(FileView *view)
{
	(void)replace_string(&view->local_filter.prev, view->local_filter.filter.raw);
	filter_clear(&view->local_filter.filter);
}

void
local_filter_restore(FileView *view)
{
	(void)filter_set(&view->local_filter.filter, view->local_filter.prev);
	(void)replace_string(&view->local_filter.prev, "");
}

/* Adds new entry to the *list of length *list_size and updates them
 * appropriately.  Returns zero on success, otherwise non-zero is returned. */
static int
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
	dir_entry_t *const new_entry_list = realloc(*list,
			sizeof(dir_entry_t)*(list_size + 1));
	if(new_entry_list == NULL)
	{
		return NULL;
	}

	*list = new_entry_list;
	return &new_entry_list[list_size];
}

void
redraw_view(FileView *view)
{
	if(curr_stats.need_update == UT_NONE && !curr_stats.restart_in_progress)
	{
		redraw_view_imm(view);
	}
}

void
redraw_view_imm(FileView *view)
{
	if(window_shows_dirlist(view))
	{
		draw_dir_list(view);
		if(view == curr_view)
		{
			move_to_list_pos(view, view->list_pos);
		}
		else
		{
			put_inactive_mark(view);
		}
	}
}

void
redraw_current_view(void)
{
	redraw_view(curr_view);
}

static void
reload_window(FileView *view)
{
	if(!window_shows_dirlist(view))
		return;

	curr_stats.skip_history = 1;

	load_saving_pos(view, is_dir_list_loaded(view));

	if(view != curr_view)
	{
		put_inactive_mark(view);
		refresh_view_win(view);
	}

	curr_stats.skip_history = 0;
}

void
check_if_filelist_have_changed(FileView *view)
{
	if(view->on_slow_fs || flist_custom_active(view))
	{
		return;
	}

#ifndef _WIN32
	filemon_t mon;
	if(filemon_from_file(view->curr_dir, &mon) != 0)
#else
	int r;
	if(is_unc_root(view->curr_dir))
		return;
	r = win_check_dir_changed(view);
	if(r < 0)
#endif
	{
		LOG_SERROR_MSG(errno, "Can't stat() \"%s\"", view->curr_dir);
		log_cwd();

		show_error_msgf("Directory Change Check", "Cannot open %s", view->curr_dir);

		leave_invalid_dir(view);
		(void)change_directory(view, view->curr_dir);
		clean_selected_files(view);
		reload_window(view);
		return;
	}

#ifndef _WIN32
	if(!filemon_equal(&mon, &view->mon))
#else
	if(r > 0)
#endif
	{
		reload_window(view);
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

	if(view == curr_view)
	{
		move_to_list_pos(view, view->list_pos);
	}
	else
	{
		mvwaddstr(view->win, view->curr_line, 0, " ");
		if(move_curr_line(view))
			draw_dir_list(view);
		put_inactive_mark(view);
	}

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

	reset_view_sort(view);

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

	move_to_list_pos(view, (file_pos < 0) ? 0 : file_pos);
	return file_pos >= 0;
}

/* Checks if file specified can be displayed. Used to filter some files, that
 * are hidden intensionally.  Returns non-zero if file can be made visible. */
static int
file_can_be_displayed(const char directory[], const char filename[])
{
	if(is_parent_dir(filename))
	{
		return parent_dir_is_visible(is_root_dir(directory));
	}
	return path_exists_at(directory, filename, DEREF);
}

/* Returns non-zero if ../ directory can be displayed. */
static int
parent_dir_is_visible(int in_root)
{
	return ((in_root && (cfg.dot_dirs & DD_ROOT_PARENT)) ||
			(!in_root && (cfg.dot_dirs & DD_NONROOT_PARENT)));
}

int
cd(FileView *view, const char *base_dir, const char *path)
{
	char dir[PATH_MAX];
	int updir = 0;

	if(path != NULL)
	{
		char *arg = expand_tilde(path);
#ifndef _WIN32
		if(is_path_absolute(arg))
			snprintf(dir, sizeof(dir), "%s", arg);
#else
		strcpy(dir, base_dir);
		break_at(dir + 2, '/');
		if(is_path_absolute(arg) && *arg != '/')
			snprintf(dir, sizeof(dir), "%s", arg);
		else if(*arg == '/' && is_unc_root(arg))
			snprintf(dir, sizeof(dir), "%s", arg);
		else if(*arg == '/' && is_unc_path(arg))
			snprintf(dir, sizeof(dir), "%s", arg);
		else if(*arg == '/' && is_unc_path(base_dir))
			sprintf(dir + strlen(dir), "/%s", arg + 1);
		else if(stroscmp(arg, "/") == 0 && is_unc_path(base_dir))
			snprintf(dir, strchr(base_dir + 2, '/') - base_dir + 1, "%s", base_dir);
		else if(*arg == '/')
			snprintf(dir, sizeof(dir), "%c:%s", base_dir[0], arg);
#endif
		else if(strcmp(arg, "-") == 0)
			copy_str(dir, sizeof(dir), view->last_dir);
		else
			find_dir_in_cdpath(base_dir, arg, dir, sizeof(dir));
		updir = is_parent_dir(arg);
		free(arg);
	}
	else
	{
		snprintf(dir, sizeof(dir), "%s", cfg.home_dir);
	}

	if(!cd_is_possible(dir))
	{
		return 0;
	}
	if(updir)
	{
		cd_updir(view);
	}
	else if(change_directory(view, dir) < 0)
	{
		return 0;
	}

	load_dir_list(view, 0);
	if(view == curr_view)
	{
		move_to_list_pos(view, view->list_pos);
	}
	else
	{
		draw_dir_list(other_view);
		refresh_view_win(other_view);
	}
	return 0;
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

	if(entry->type == FT_DIR)
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

/* Same as iter_selected_entries() function, but when selection is absent
 * current file is processed. */
static int
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
		format_entry_name(view, entry - view->dir_entry, sizeof(name), name);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
