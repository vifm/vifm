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

#ifndef VIFM__FOPS_COMMON_H__
#define VIFM__FOPS_COMMON_H__

#include "compat/fs_limits.h"
#include "utils/test_helpers.h"
#include "background.h"
#include "ops.h"

struct dir_entry_t;
struct view_t;

/* Path roles for fops_is_dir_writable() function. */
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
	char **list;             /* User supplied list of new file names. */
	int nlines;              /* Number of user supplied file names (list size). */
	int move;                /* Whether this is a move operation. */
	int force;               /* Whether destination files should be removed. */
	int skip;                /* Skip files that already exist at destination. */
	char **sel_list;         /* Full paths of files to be processed. */
	size_t sel_list_len;     /* Number of files to process (sel_list size). */
	char path[PATH_MAX + 1]; /* Path at which processing should take place. */
	int from_file;           /* Whether list was read from a file. */
	int use_trash;           /* Whether either source or destination is trash
	                            directory. */
	char *is_in_trash;       /* Flags indicating whether i-th file is in trash.
	                            Can be NULL when unused. */
	ops_t *ops;              /* Pointer to pre-allocated operation description. */
}
bg_args_t;

struct dirent;
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

/* Function invoked to check whether edited list is OK.  Should return non-zero
 * if so and zero otherwise.  Should reallocate *error on error.  *data is the
 * one passed to fops_query_list(). */
typedef int (*fops_query_verify_func)(char *files[], int nfiles, char *names[],
		int nnames, char **error, void *data);

/* Filename editing function. */
extern line_prompt_func fops_line_prompt;
/* Function to choose from one of options. */
extern options_prompt_func fops_options_prompt;

/* Initializes file operations. */
void fops_init(line_prompt_func line_func, options_prompt_func options_func);

/* Whether set of view files can be altered (renamed, deleted, but not added).
 * Returns non-zero if so, otherwise zero is returned. */
int fops_view_can_be_changed(const struct view_t *view);

/* Checks if name list is consistent.  Reallocates *error to provide error
 * message.  Returns non-zero is so, otherwise zero is returned. */
int fops_is_name_list_ok(int count, int nlines, char *list[], char *files[],
		char **error);

/* Checks whether list of files doesn't mention any existing files.  Reallocates
 * *error to provide error message.  Returns non-zero if everything is fine,
 * otherwise zero is returned. */
int fops_is_copy_list_ok(const char dst[], int count, char *list[], int force,
		char **error);

/* Checks rename correctness and forms an array of duplication marks.
 * Reallocates *error to provide error message.  Directory names in files array
 * should be without trailing slash.  Returns non-zero if everything is fine,
 * otherwise zero is returned. */
int fops_is_rename_list_ok(char *files[], char is_dup[], int len, char *list[],
		char **error);

/* Checks single file rename for correctness.  Reallocates *error to provide
 * error message.  Returns value > 0 if rename is correct, < 0 if rename isn't
 * needed and 0 when rename operation should be aborted. */
int fops_check_file_rename(const char dir[], const char old[], const char new[],
		char **error);

/* Makes list of marked filenames.  *nmarked is always set (0 for empty list).
 * Returns pointer to the list, NULL for empty list. */
char ** fops_grab_marked_files(struct view_t *view, size_t *nmarked);

/* Uses dentry to check file type and falls back to lstat() if dentry contains
 * unknown type. */
int fops_is_dir_entry(const char full_path[], const struct dirent* dentry);

/* Adds marked files to the ops.  Considers UI cancellation.  dst_hint can be
 * NULL.  Returns number of files enqueued. */
int fops_enqueue_marked_files(ops_t *ops, struct view_t *view,
		const char dst_hint[], int to_trash);

/* Allocates opt_t structure and configures it as needed.  Returns pointer to
 * newly allocated structure, which should be freed by free_ops(). */
ops_t * fops_get_ops(OPS main_op, const char descr[], const char base_dir[],
		const char target_dir[]);

/* Displays simple operation progress message.  The ready is zero based. */
void fops_progress_msg(const char text[], int ready, int total);

/* Makes name of destination file from name of the source file.  Returns the
 * name. */
const char * fops_get_dst_name(const char src_path[], int from_trash);

/* Checks that all marked files can be read.  Returns non-zero if so, otherwise
 * zero is returned. */
int fops_can_read_marked_files(struct view_t *view);

/* Checks path argument and resolves target directory either to the argument or
 * current directory of the view.  Returns non-zero if value of the path was
 * used, otherwise zero is returned. */
int fops_check_dir_path(const struct view_t *view, const char path[],
		char buf[], size_t buf_len);

/* Prompts user with a file containing lines from orig array of length orig_len
 * and returns modified list of strings of length *edited_len or NULL on error
 * or unchanged list unless load_always is non-zero.  Can ask user to re-edit
 * file list. */
char ** fops_query_list(size_t orig_len, char *orig[], int *edited_len,
		int load_always, fops_query_verify_func verify, void *verify_data);

/* Finishes initialization of ops for background processes. */
void fops_bg_ops_init(ops_t *ops, bg_op_t *bg_op);

/* Allocates opt_t structure and configures it as needed.  Returns pointer to
 * newly allocated structure, which should be freed by fops_free_ops(). */
ops_t * fops_get_bg_ops(OPS main_op, const char descr[], const char dir[]);

/* Frees ops structure previously obtained by call to get_ops().  ops can be
 * NULL. */
void fops_free_ops(ops_t *ops);

/* Adapter for fops_mv_file_f() that accepts paths broken into directory/file
 * parts. */
int fops_mv_file(const char src[], const char src_dir[], const char dst[],
		const char dst_dir[], OPS op, int cancellable, ops_t *ops);

/* Moves file from one location to another.  Returns zero on success, otherwise
 * non-zero is returned. */
int fops_mv_file_f(const char src[], const char dst[], OPS op, int bg,
		int cancellable, ops_t *ops);

/* Frees background arguments structure with all its data. */
void fops_free_bg_args(bg_args_t *args);

/* Fills basic fields of the args structure. */
void fops_prepare_for_bg_task(struct view_t *view, bg_args_t *args);

/* Fills undo message buffer with names of marked files.  buf should be at least
 * COMMAND_GROUP_INFO_LEN characters length.  fnames can be NULL. */
void fops_append_marked_files(struct view_t *view, char buf[], char **fnames);

/* Appends file name to undo message buffer.  buf should be at least
 * COMMAND_GROUP_INFO_LEN characters length. */
void fops_append_fname(char buf[], size_t len, const char fname[]);

/* Provides different suffixes depending on whether cancellation was requested
 * or not.  Returns pointer to a string literal. */
const char * fops_get_cancellation_suffix(void);

/* Whether set of view files can be extended via addition of new elements.  at
 * parameter is the same as for fops_get_dst_dir().  Returns non-zero if so,
 * otherwise zero is returned. */
int fops_view_can_be_extended(const struct view_t *view, int at);

/* Retrieves current target directory of file system sub-tree.  Root for regular
 * and regular custom views and origin of either active (when at < 0) or
 * specified by its index entry for tree views.  Returns the path. */
const char * fops_get_dst_dir(const struct view_t *view, int at);

/* This is a wrapper for is_dir_writable() function, which adds message
 * dialogs.  Returns non-zero if directory can be changed, otherwise zero is
 * returned. */
int fops_is_dir_writable(DirRole dir_role, const char path[]);

TSTATIC_DEFS(
	struct ext_edit_t;
	struct progress_data_t;

	char ** edit_list(struct ext_edit_t *ext_edit, size_t orig_len, char *orig[],
		int *edited_len, int load_always);
	struct progress_data_t * alloc_progress_data(int bg, void *info);
)

#endif /* VIFM__FOPS_COMMON_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
