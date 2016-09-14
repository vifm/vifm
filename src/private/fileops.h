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

extern line_prompt_func line_prompt;
extern options_prompt_func options_prompt;

#endif /* VIFM__PRIVATE__FILEOPS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
