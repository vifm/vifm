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

void quick_view_file(FileView * view);
void toggle_quick_view(void);
FILE * use_info_prog(const char *viewer);
void clean_selected_files(FileView *view);
void goto_history_pos(FileView *view, int pos);
int change_directory(FileView *view, const char *directory);
void leave_invalid_dir(FileView *view, char *path);
void load_dir_list(FileView *view, int reload);
void draw_dir_list(FileView *view, int top);
int correct_list_pos_on_scroll_up(FileView *view, int pos_delta);
int correct_list_pos_on_scroll_down(FileView *view, int pos_delta);
char * get_current_file_name(FileView *view);
void move_to_list_pos(FileView *view, int pos);
int find_file_pos_in_list(FileView *view, const char *file);
void get_all_selected_files(FileView *view);
void get_selected_files(FileView *view, int count, const int *indexes);
void count_selected(FileView *view);
void free_selected_file_array(FileView *view);
void erase_current_line_bar(FileView *view);
void filter_selected_files(FileView *view);
void set_dot_files_visible(FileView *view, int visible);
void toggle_dot_files(FileView *view);
void remove_filename_filter(FileView *view);
void restore_filename_filter(FileView *view);
void scroll_view(FileView *view);
void check_if_filelists_have_changed(FileView *view);
void change_sort_type(FileView *view, char type, char descending);
void load_saving_pos(FileView *view, int reload);
void save_view_history(FileView *view, const char *path, const char *file,
		int pos);
int is_in_view_history(FileView *view, const char *path);
int pane_in_dir(FileView *view, const char *path);
void update_view_title(FileView *view);
void clean_positions_in_history(FileView *view);
int ensure_file_is_selected(FileView *view, const char *name);
int cd(FileView *view, const char *path);

#ifdef TEST
int regexp_filter_match(FileView *view, char *filename);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
