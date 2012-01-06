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

#ifndef __FILEOPS_H__
#define __FILEOPS_H__

#include "macros.h"
#include "ui.h"

#define DEFAULT_REG_NAME '"'

void cd_updir(FileView *view);
void handle_file(FileView *view, int dont_execute, int force_follow);
void _gnuc_noreturn use_vim_plugin(FileView *view, int argc, char **argv);
int delete_file(FileView *view, int reg, int count, int *indexes,
		int use_trash);
int delete_file_bg(FileView *view, int use_trash);
void unmount_fuse(void);
void run_using_prog(FileView *view, const char *program, int dont_execute,
		int force_background);
int yank_files(FileView *view, int reg, int count, int *indexes);
void yank_selected_files(FileView *view, int reg);
void handle_dir(FileView *view);
int file_exec(char *command);
void view_file(const char *filename, int line, int do_fork);
void show_change_window(FileView *view, int type);
void rename_file(FileView *view, int name_only);
int rename_files(FileView *view, char **list, int nlines, int recursive);
int incdec_names(FileView *view, int k);
#ifndef _WIN32
void chown_files(int u, int g, uid_t uid, gid_t gid);
#endif
void change_owner(void);
void change_group(void);
int change_link(FileView *view);
/* Returns new value for save_msg flag. */
int put_files_from_register(FileView *view, int name, int force_move);
int clone_files(FileView *view, char **list, int nlines, int force, int copies);
unsigned long long calc_dirsize(const char *path, int force_update);
int is_dir_writable(int dest, const char *path);
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
int cpmv_files(FileView *view, char **list, int nlines, int move, int type,
		int force);
int cpmv_files_bg(FileView *view, char **list, int nlines, int move, int force);
void make_dirs(FileView *view, char **names, int count, int create_parent);
int make_files(FileView *view, char **names, int count);

#ifdef TEST
const char * gen_clone_name(const char *normal_name);
int is_name_list_ok(int count, int nlines, char **list, char **files);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
