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

#ifndef _WIN32
#include <sys/types.h> /* gid_t uid_t */
#endif

#include <stdint.h> /* uint64_t */

#include "ui/ui.h"
#include "utils/test_helpers.h"

/* Type of reaction on an error. */
typedef enum
{
	ST_NONE,       /* Ignore message. */
	ST_STATUS_BAR, /* Show message in the status bar. */
	ST_DIALOG,     /* Shows error dialog. */
}
SignalType;

/* Type of copy/move-like operation. */
typedef enum
{
	CMLO_COPY,     /* Copy file. */
	CMLO_MOVE,     /* Move file. */
	CMLO_LINK_REL, /* Make relative symbolic link. */
	CMLO_LINK_ABS, /* Make absolute symbolic link. */
}
CopyMoveLikeOp;

struct response_variant;

/* Callback for returning edited filename. */
typedef void (*fo_prompt_cb)(const char new_filename[]);

/* Line completion function.  arg is user supplied value, which is passed
 * through.  Should return completion offset. */
typedef int (*fo_complete_cmd_func)(const char cmd[], void *arg);

/* Function to request filename editing. */
typedef void (*line_prompt_func)(const char prompt[], const char filename[],
		fo_prompt_cb cb, fo_complete_cmd_func complete, int allow_ee);

/* Function to choose an option.  Returns choice. */
typedef char (*options_prompt_func)(const char title[], const char message[],
		const struct response_variant *variants);

/* Initializes file operations. */
void init_fileops(line_prompt_func line_func, options_prompt_func options_func);

/* Removes marked files (optionally into trash directory) of the view to
 * specified register.  Returns new value for save_msg flag. */
int delete_files(FileView *view, int reg, int use_trash);

/* Removes marked files (optionally into trash directory) of the view to
 * specified register.  Returns new value for save_msg flag. */
int delete_files_bg(FileView *view, int use_trash);

/* Yanks marked files of the view into register specified by its name via reg
 * parameter.  Returns new value for save_msg. */
int yank_files(FileView *view, int reg);

/* Renames single file under the cursor. */
void rename_current_file(FileView *view, int name_only);

/* Renames marked files using names given in the list of length nlines (or
 * filled in by the user, when the list is empty).  Recursively traverses
 * directories in selection when recursive flag is not zero.  Recursive
 * traversal is incompatible with list of names.  Returns new value for
 * save_msg flag. */
int rename_files(FileView *view, char **list, int nlines, int recursive);

/* Increments/decrements first number in names of marked files of the view k
 * times.  Returns new value for save_msg flag. */
int incdec_names(FileView *view, int k);

#ifndef _WIN32
/* Sets uid and or gid for marked files.  Non-zero u enables setting of uid,
 * non-zero g of gid. */
void chown_files(int u, int g, uid_t uid, gid_t gid);
#endif

void change_owner(void);

void change_group(void);

int change_link(FileView *view);

/* Puts files from specified register into current directory.  at specifies
 * index of entry to be used to obtain destination path, -1 means current
 * position.  Returns new value for save_msg flag. */
int put_files(FileView *view, int at, int reg_name, int move);

/* Starts background task that puts files from specified register into current
 * directory.  at specifies index of entry to be used to obtain destination
 * path, -1 means current position.  Returns new value for save_msg flag. */
int put_files_bg(FileView *view, int at, int reg_name, int move);

/* Clones marked files in the view.  Returns new value for save_msg flag. */
int clone_files(FileView *view, char *list[], int nlines, int force,
		int copies);

/* Whether set of view files can be altered (renamed, deleted, but not added).
 * Returns non-zero if so, otherwise zero is returned. */
int can_change_view_files(const FileView *view);

/* Returns new value for save_msg flag. */
int put_links(FileView *view, int reg_name, int relative);

/* Replaces matches of regular expression in names of files of the view.
 * Returns new value for save_msg flag. */
int substitute_in_names(FileView *view, const char pattern[], const char sub[],
		int ic, int glob);

/* Replaces letters in names of marked files of the view according to the
 * mapping: from[i] -> to[i] (must have the same length).  Returns new value for
 * save_msg flag. */
int tr_in_names(FileView *view, const char from[], const char to[]);

/* Returns pointer to a statically allocated buffer. */
const char * substitute_in_name(const char name[], const char pattern[],
		const char sub[], int glob);

/* Changes case of all letters in names of marked files of the view.  Returns
 * new value for save_msg flag. */
int change_case(FileView *view, int to_upper);

/* Performs copy/moves-like operation on marked files.  Returns new value for
 * save_msg flag. */
int cpmv_files(FileView *view, char **list, int nlines, CopyMoveLikeOp op,
		int force);

/* Copies or moves marked files to the other view in background.  Returns new
 * value for save_msg flag. */
int cpmv_files_bg(FileView *view, char **list, int nlines, int move, int force);

/* Creates directories, possibly including intermediate ones.  Can modify
 * strings in the names array.  at specifies index of entry to be used to obtain
 * destination path, -1 means current position.  Returns new value for save_msg
 * flag. */
int make_dirs(FileView *view, int at, char *names[], int count,
		int create_parent);

/* Creates files.  at specifies index of entry to be used to obtain destination
 * path, -1 means current position.  Returns new value for save_msg flag. */
int make_files(FileView *view, int at, char *names[], int count);

/* Returns new value for save_msg flag. */
int restore_files(FileView *view);

/* Calculates size of a directory specified by path possibly using cache of
 * known sizes.  Forcing disables using previously cached values.  Returns size
 * of a directory or zero on error. */
uint64_t calculate_dir_size(const char path[], int force);

/* Initiates background calculation of directory sizes.  Forcing disables using
 * previously cached values. */
void calculate_size_bg(const FileView *view, int force);

#ifdef TEST
#include "ops.h"
#endif

TSTATIC_DEFS(
	int is_rename_list_ok(char *files[], char is_dup[], int len, char *list[]);
	int check_file_rename(const char dir[], const char old[], const char new[],
		SignalType signal_type);
	int merge_dirs(const char src[], const char dst[], ops_t *ops);
	const char * gen_clone_name(const char normal_name[]);
	int is_name_list_ok(int count, int nlines, char *list[], char *files[]);
	const char * incdec_name(const char fname[], int k);
)

#endif /* VIFM__FILEOPS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
