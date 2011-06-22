/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef __FILELIST_H__
#define __FILELIST_H__

#include "ui.h"

enum {
	LINK,
	DIRECTORY,
	DEVICE,
	SOCKET,
	EXECUTABLE,
	REGULAR,
	UNKNOWN
};

void friendly_size_notation(unsigned long long num, int str_size, char *str);
void quick_view_file(FileView * view);
void clean_selected_files(FileView *view);
void canonicalize_path(const char *directory, char *buf, size_t buf_size);
void goto_history_pos(FileView *view, int pos);
int change_directory(FileView *view, const char *directory);
void leave_invalid_dir(FileView *view, char *path);
void load_dir_list(FileView *view, int reload);
void draw_dir_list(FileView *view, int top);
char * get_current_file_name(FileView *view);
void moveto_list_pos(FileView *view, int pos);
int find_file_pos_in_list(FileView *view, const char *file);
void get_all_selected_files(FileView *view);
void get_selected_files(FileView *view, int count, int *indexes);
void free_selected_file_array(FileView *view);
void erase_current_line_bar(FileView *view);
void filter_selected_files(FileView *view);
void set_dot_files_visible(FileView *view, int visible);
void toggle_dot_files(FileView *view);
void remove_filename_filter(FileView *view);
void restore_filename_filter(FileView *view);
void scroll_view(FileView *view);
void check_if_filelists_have_changed(FileView *view);
int S_ISEXE(mode_t mode);
void change_sort_type(FileView *view, char type, char descending);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
