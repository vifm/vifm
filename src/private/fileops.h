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

#ifndef VIFM__PRIVATE__FILEOPS_H__
#define VIFM__PRIVATE__FILEOPS_H__

#include "../ui/ui.h"
#include "../background.h"
#include "../fileops.h"
#include "../ops.h"

/* Path roles for check_if_dir_writable() function. */
typedef enum
{
	DR_CURRENT,     /* Current (source) path. */
	DR_DESTINATION, /* Destination path. */
}
DirRole;

/* Pack of arguments supplied to procedures implementing file operations in
 * background. */
typedef struct
{
	char **list;         /* User supplied list of new file names. */
	int nlines;          /* Number of user supplied file names (list size). */
	int move;            /* Whether this is a move operation. */
	int force;           /* Whether destination files should be removed. */
	char **sel_list;     /* Full paths of files to be processed. */
	size_t sel_list_len; /* Number of files to process (sel_list size). */
	char path[PATH_MAX]; /* Path at which processing should take place. */
	int from_file;       /* Whether list was read from a file. */
	int use_trash;       /* Whether either source or destination is trash
	                        directory. */
	char *is_in_trash;   /* Flags indicating whether i-th file is in trash.  Can
	                        be NULL when unused. */
	ops_t *ops;          /* Pointer to pre-allocated operation description. */
}
bg_args_t;

struct dirent;

const char * get_dst_dir(const FileView *view, int at);
int can_add_files_to_view(const FileView *view, int at);
void free_ops(ops_t *ops);
ops_t * get_ops(OPS main_op, const char descr[], const char base_dir[],
		const char target_dir[]);
const char * get_dst_name(const char src_path[], int from_trash);
const char * get_cancellation_suffix(void);
void progress_msg(const char text[], int ready, int total);
int is_dir_entry(const char full_path[], const struct dirent* dentry);
void append_fname(char buf[], size_t len, const char fname[]);
void bg_ops_init(ops_t *ops, bg_op_t *bg_op);
ops_t * get_bg_ops(OPS main_op, const char descr[], const char dir[]);
void free_bg_args(bg_args_t *args);
int enqueue_marked_files(ops_t *ops, FileView *view, const char dst_hint[],
		int to_trash);
int mv_file(const char src[], const char src_dir[], const char dst[],
		const char dst_dir[], OPS op, int cancellable, ops_t *ops);
void general_prepare_for_bg_task(FileView *view, bg_args_t *args);
int can_read_selected_files(FileView *view);
int check_dir_path(const FileView *view, const char path[], char buf[],
		size_t buf_len);
int check_if_dir_writable(DirRole dir_role, const char path[]);
char ** grab_marked_files(FileView *view, size_t *nmarked);
char ** edit_list(size_t count, char **orig, int *nlines, int ignore_change);
int is_name_list_ok(int count, int nlines, char *list[], char *files[]);
void append_marked_files(FileView *view, char buf[], char **fnames);
int mv_file_f(const char src[], const char dst[], OPS op, int bg,
		int cancellable, ops_t *ops);

extern line_prompt_func line_prompt;
extern options_prompt_func options_prompt;

#endif /* VIFM__PRIVATE__FILEOPS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
