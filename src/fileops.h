/* vifm
 * Copyright (C) 2001 Ken Steen.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by * the Free Software Foundation; either version 2 of the License, or
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

#ifndef __FILEOPS_H__
#define __FILEOPS_H__

#include "ui.h"

#define FILE_CHANGE 1
#define FILE_NAME 2
#define FILE_OWNER 4
#define FILE_GROUP 6
#define FILE_PERMISSIONS 8

#define DEFAULT_REG_NAME '"'

typedef struct
{
	char *dir;
	char *file;
}yank_t;

yank_t *yanked_file;

void cd_updir(FileView *view);
void handle_file(FileView *view, int dont_execute);
int delete_file(FileView *view, int reg, int count, int *indexes,
		int use_trash);
int my_system(char *command);
int system_and_wait_for_errors(char *cmd);
int yank_files(FileView *view, int reg, int count, int *indexes);
void yank_selected_files(FileView *view, int reg);
int pipe_and_capture_errors(char *command);
int file_exec(char *command);
void show_change_window(FileView *view, int type);
int check_link_is_dir(FileView *view, int pos);
void rename_file(FileView *view, int name_only);
void change_owner(FileView *view);
void change_group(FileView *view);
/* Returns new value for save_msg flag. */
int put_files_from_register(FileView *view, int name, int force_move);
void clone_file(FileView* view);
unsigned long long calc_dirsize(const char *path);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
