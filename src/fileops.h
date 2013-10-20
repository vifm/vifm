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

#ifndef VIFM__FILEOPS_H__
#define VIFM__FILEOPS_H__

#include <stdint.h> /* uint64_t */

#include "utils/test_helpers.h"
#include "ui.h"

typedef enum
{
	DR_CURRENT,
	DR_DESTINATION,
}DirRole;

/* Type of reaction on an error. */
typedef enum
{
	ST_NONE,       /* Show message in the status bar. */
	ST_STATUS_BAR, /* Show message in the status bar. */
	ST_DIALOG,     /* Shows error dialog. */
}
SignalType;

int delete_file(FileView *view, int reg, int count, int *indexes,
		int use_trash);
int delete_file_bg(FileView *view, int use_trash);
int yank_files(FileView *view, int reg, int count, int *indexes);
void yank_selected_files(FileView *view, int reg);
int file_exec(char *command);
void show_change_window(FileView *view, int type);
void rename_current_file(FileView *view, int name_only);
/* Renames selection to names given in the list of length nlines (or filled in
 * by the user, when the list is empty).  Recursively traverses directories in
 * selection when recursive flag is not zero.  Recursive traversal is
 * incompatible with list of names.  Returns new value for save_msg flag. */
int rename_files(FileView *view, char **list, int nlines, int recursive);
/* Returns new value for save_msg flag. */
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
uint64_t calc_dirsize(const char *path, int force_update);
/* This is a wrapper for is_dir_writable() function, which adds message
 * dialogs. */
int check_if_dir_writable(DirRole dir_role, const char *path);
/* Returns new value for save_msg flag. */
int put_links(FileView *view, int reg_name, int relative);
/* Returns new value for save_msg flag. */
int substitute_in_names(FileView *view, const char *pattern, const char *sub,
		int ic, int glob);
/* Returns new value for save_msg flag. */
int tr_in_names(FileView *view, const char *pattern, const char *sub);
/* Returns pointer to a statically allocated buffer. */
const char * substitute_in_name(const char name[], const char pattern[],
		const char sub[], int glob);
int change_case(FileView *view, int toupper, int count, int indexes[]);
int cpmv_files(FileView *view, char **list, int nlines, int move, int type,
		int force);
int cpmv_files_bg(FileView *view, char **list, int nlines, int move, int force);
/* Can modify strings in the names array. */
void make_dirs(FileView *view, char **names, int count, int create_parent);
int make_files(FileView *view, char **names, int count);

TSTATIC_DEFS(
	int is_rename_list_ok(char *files[], int *is_dup, int len, char *list[]);
	int check_file_rename(const char dir[], const char old[], const char new[],
		SignalType signal_type);
	const char * gen_clone_name(const char normal_name[]);
	int is_name_list_ok(int count, int nlines, char *list[], char *files[]);
	const char * add_to_name(const char filename[], int k);
)

#endif /* VIFM__FILEOPS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
