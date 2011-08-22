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

#include "macros.h"
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
void handle_file(FileView *view, int dont_execute, int force_follow);
void _gnuc_noreturn use_vim_plugin(FileView *view, int argc, char **argv);
int delete_file(FileView *view, int reg, int count, int *indexes,
		int use_trash);
int my_system(char *command);
void unmount_fuse(void);
int system_and_wait_for_errors(char *cmd);
void run_using_prog(FileView *view, const char *program, int dont_execute,
		int force_background);
int yank_files(FileView *view, int reg, int count, int *indexes);
void yank_selected_files(FileView *view, int reg);
int file_exec(char *command);
void view_file(const char *filename, int line);
void show_change_window(FileView *view, int type);
void rename_file(FileView *view, int name_only);
void rename_files(FileView *view, char **list, int nlines);
void change_owner(void);
void change_group(void);
int change_link(FileView *view);
/* Returns new value for save_msg flag. */
int put_files_from_register(FileView *view, int name, int force_move);
int clone_files(FileView *view, char **list, int nlines);
unsigned long long calc_dirsize(const char *path, int force_update);
int is_dir_writable(const char *path);
/* Returns new value for save_msg flag. */
int put_links(FileView *view, int reg_name, int relative);
/* Returns new value for save_msg flag. */
int substitute_in_names(FileView *view, const char *pattern, const char *sub,
		int ic, int glob);
/* Returns new value for save_msg flag. */
int tr_in_names(FileView *view, const char *pattern, const char *sub);
const char * substitute_in_name(const char *name, const char *pattern,
		const char *sub, int glob);
int change_case(FileView *view, int toupper, int count, int *indexes);
int cpmv_files(FileView *view, char **list, int nlines, int move);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
