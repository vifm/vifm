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

#ifndef __FILELIST_H__
#define __FILELIST_H__

#include "ui.h"

enum
{
	LINK,
	DIRECTORY,
	DEVICE,
#ifndef _WIN32
	SOCKET,
#endif
	EXECUTABLE,
	REGULAR,
	FIFO,
	UNKNOWN
};

/* Initialization/termination functions. */

/* Prepares views for the first time. */
void init_filelists(void);
/* Reinitializes views. */
void prepare_views(void);
/* Loads view file list for the first time. */
void load_initial_directory(FileView *view, const char *dir);

/* Position related functions. */

int find_file_pos_in_list(FileView *view, const char *file);
int correct_list_pos_on_scroll_up(FileView *view, int pos_delta);
int correct_list_pos_on_scroll_down(FileView *view, int pos_delta);
void move_to_list_pos(FileView *view, int pos);

/* Appearance related functions. */

/* Reinitializes view columns. */
void reset_view_sort(FileView *view);
void draw_dir_list(FileView *view, int top);
void erase_current_line_bar(FileView *view);
/* Updates view (maybe postponed) on the screen (redraws file list and
 * cursor) */
void redraw_view(FileView *view);
/* Updates current view (maybe postponed) on the screen (redraws file list and
 * cursor) */
void redraw_current_view(void);
void change_sort_type(FileView *view, char type, char descending);
void update_view_title(FileView *view);

/* Directory traversing functions. */

int change_directory(FileView *view, const char *path);
int cd(FileView *view, const char *path);
/* Modifies path. */
void leave_invalid_dir(FileView *view, char *path);
int pane_in_dir(FileView *view, const char *path);

/* Selection related functions. */

/* Cleans selection possibly saving it for later use. */
void clean_selected_files(FileView *view);
/* Erases selection not saving anything. */
void erase_selection(FileView *view);
void get_all_selected_files(FileView *view);
void get_selected_files(FileView *view, int count, const int *indexes);
void count_selected(FileView *view);
void free_selected_file_array(FileView *view);
int ensure_file_is_selected(FileView *view, const char *name);

/* Filters related functions. */

void set_dot_files_visible(FileView *view, int visible);
void toggle_dot_files(FileView *view);

void filter_selected_files(FileView *view);
void remove_filename_filter(FileView *view);
void restore_filename_filter(FileView *view);
/* Sets filter regexp for the view. */
void set_filename_filter(FileView *view, const char *filter);

/* Directory history related functions. */

void goto_history_pos(FileView *view, int pos);
void save_view_history(FileView *view, const char *path, const char *file,
		int pos);
int is_in_view_history(FileView *view, const char *path);
void clean_positions_in_history(FileView *view);

/* Other functions. */

FILE * use_info_prog(const char *viewer);
void load_dir_list(FileView *view, int reload);
/* Resorts view without reloading it.  msg parameter controls whether to show
 * "Sorting..." statusbar message. */
void resort_dir_list(int msg, FileView *view);
void load_saving_pos(FileView *view, int reload);
char * get_current_file_name(FileView *view);
void check_if_filelists_have_changed(FileView *view);
/* Checks whether cd'ing into path is possible. Shows cd errors to a user.
 * Returns non-zero if it's possible, zero otherwise. */
int cd_is_possible(const char *path);
/* Checks whether directory list was loaded at least once since startup. */
int is_dir_list_loaded(FileView *view);
/* Returns non-zero if path should be changed. */
int view_is_at_path(FileView *view, const char *path);
/* Sets view's current directory from path value.
 * Returns non-zero if view's directory was changed. */
int set_view_path(FileView *view, const char *path);

#ifdef TEST
int regexp_filter_match(FileView *view, const char *filename);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
