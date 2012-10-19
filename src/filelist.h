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

#include <stddef.h> /* size_t ssize_t */


#include "utils/test_helpers.h"
#include "ui.h"

/* Initialization/termination functions. */

/* Prepares views for the first time. */
void init_filelists(void);
/* Reinitializes views. */
void prepare_views(void);
/* Loads view file list for the first time. */
void load_initial_directory(FileView *view, const char *dir);

/* Position related functions. */

int find_file_pos_in_list(FileView *view, const char *file);
/* Recalculates difference of two panes scroll positions. */
void update_scroll_bind_offset(void);
/* Tries to move cursor by pos_delta positions.  A wrapper for
 * correct_list_pos_on_scroll_up() and correct_list_pos_on_scroll_down()
 * functions. */
void correct_list_pos(FileView *view, ssize_t pos_delta);
/* Returns non-zero if doing something makes sense. */
int correct_list_pos_on_scroll_down(FileView *view, size_t lines_count);
/* Returns new list position after making correction for scrolling down. */
int get_corrected_list_pos_down(const FileView *view, size_t pos_delta);
/* Returns non-zero if doing something makes sense. */
int correct_list_pos_on_scroll_up(FileView *view, size_t lines_count);
/* Returns new list position after making correction for scrolling up. */
int get_corrected_list_pos_up(const FileView *view, size_t pos_delta);
/* Returns non-zero if all files are visible, so no scrolling is needed. */
int all_files_visible(const FileView *view);
void move_to_list_pos(FileView *view, int pos);
/* Adds inactive cursor mark to the view. */
void put_inactive_mark(FileView *view);
/* Returns non-zero in case view can be scrolled up (there are more files). */
int can_scroll_up(const FileView *view);
/* Returns non-zero in case view can be scrolled down (there are more files). */
int can_scroll_down(const FileView *view);
/* Scrolls view down or up at least by specified number of files.  Updates both
 * top and cursor positions.  A wrapper for scroll_up() and scroll_down()
 * functions. */
void scroll_by_files(FileView *view, ssize_t by);
/* Scrolls view up at least by specified number of files.  Updates both top and
 * cursor positions. */
void scroll_up(FileView *view, size_t by);
/* Scrolls view down at least by specified number of files.  Updates both top
 * and cursor positions. */
void scroll_down(FileView *view, size_t by);
/* Returns non-zero if cursor is on the first line. */
int at_first_line(const FileView *view);
/* Returns non-zero if cursor is on the last line. */
int at_last_line(const FileView *view);
/* Returns non-zero if cursor is on the first column. */
int at_first_column(const FileView *view);
/* Returns non-zero if cursor is on the last column. */
int at_last_column(const FileView *view);
/* Returns window top position adjusted for 'scrolloff' option. */
size_t get_window_top_pos(const FileView *view);
/* Returns window middle position adjusted for 'scrolloff' option. */
size_t get_window_middle_pos(const FileView *view);
/* Returns window bottom position adjusted for 'scrolloff' option. */
size_t get_window_bottom_pos(const FileView *view);
/* Moves cursor to the first file in a row. */
void go_to_start_of_line(FileView *view);
/* Returns position of the first file in current line. */
int get_start_of_line(const FileView *view);
/* Returns position of the last file in current line. */
int get_end_of_line(const FileView *view);
/* Returns index of last visible file in the view.  Value returned may be
 * greater than or equal to number of files in the view, which should be
 * threated correctly. */
size_t get_last_visible_file(const FileView *view);

/* Appearance related functions. */

/* Reinitializes view columns. */
void reset_view_sort(FileView *view);
void draw_dir_list(FileView *view);
void erase_current_line_bar(FileView *view);
/* Updates view (maybe postponed) on the screen (redraws file list and
 * cursor) */
void redraw_view(FileView *view);
/* Updates current view (maybe postponed) on the screen (redraws file list and
 * cursor) */
void redraw_current_view(void);
/* Returns non-zero in case view shows list of files at the moment. */
int window_shows_dirlist(const FileView *const view);
void change_sort_type(FileView *view, char type, char descending);
void update_view_title(FileView *view);
/* Returns non-zero if redraw is needed */
int move_curr_line(FileView *view);
/* Returns number of columns in the view. */
size_t calculate_columns_count(FileView *view);

/* Directory traversing functions. */

int change_directory(FileView *view, const char *path);
/* Changes pane directory handling path just like cd command does. */
int cd(FileView *view, const char *base_dir, const char *path);
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
int ensure_file_is_selected(FileView *view, const char name[]);

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
int view_is_at_path(const FileView *view, const char path[]);
/* Sets view's current directory from path value.
 * Returns non-zero if view's directory was changed. */
int set_view_path(FileView *view, const char *path);
/* Returns possible cached or calculated value of file size. */
uint64_t get_file_size_by_entry(const FileView *view, size_t pos);

TSTATIC_DEFS(
	int regexp_filter_match(FileView *view, const char filename[]);
)

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
