/* vifm
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

#ifndef VIFM__UNDO_H__
#define VIFM__UNDO_H__

#include "ops.h"

enum
{
	COMMAND_GROUP_INFO_LEN = 320,
};

/* Error statuses for un_group_undo() and un_group_redo(). */
typedef enum
{
	UN_ERR_SUCCESS,   /* Successful undo/redo. */
	UN_ERR_NONE,      /* No more groups to undo/redo. */
	UN_ERR_FAIL,      /* Try has failed with an error from perform function. */
	UN_ERR_BROKEN,    /* FS changes made undoing/redoing group impossible. */
	UN_ERR_BALANCE,   /* Skipped unbalanced group. */
	UN_ERR_NOUNDO,    /* Cannot undone (e.g., permanent file deletion). */
	UN_ERR_SKIPPED,   /* Operation skipped by the user. */
	UN_ERR_CANCELLED, /* Operation was cancelled by the user. */
	UN_ERR_ERRORS,    /* Skipped entirely due to errors on previous undo/redo. */
}
UnErrCode;

/* Operation execution handler.  data is from un_group_add_op() call.  Should
 * return status of the operation. */
typedef OpsResult (*un_perform_func)(OPS op, void *data, const char src[],
		const char dst[]);

/* Return value meaning:
 *   0 - available (but undo.c need to check file existence or absence);
 * < 0 - not available;
 * > 0 - available always (no additional checks are performed). */
typedef int (*un_op_available_func)(OPS op);

/* Callback to control execution of sets of operations.  Should return non-zero
 * in case processing should be aborted, otherwise zero is expected. */
typedef int (*un_cancel_requested_func)(void);

/* Won't call un_reset(), so this function could be called multiple times.
 * exec_func can't be NULL and should return non-zero on error.  op_avail and
 * cancel can be NULL. */
void un_init(un_perform_func exec_func, un_op_available_func op_avail,
		un_cancel_requested_func cancel, const int *max_levels);

/* Frees all allocated memory. */
void un_reset(void);

/* Only stores msg pointer, so it should be valid until un_group_close() is
 * called. */
void un_group_open(const char msg[]);

/* Reopens last command group. */
void un_group_reopen_last(void);

/* Replaces group message (makes its own copy of the msg).  If msg equals NULL
 * or no group is currently open, does nothing.  Returns previous value. */
char * un_replace_group_msg(const char msg[]);

/* Returns 0 on success. */
int un_group_add_op(OPS op, void *do_data, void *undo_data, const char buf1[],
		const char buf2[]);

/* Closes current group of commands. */
void un_group_close(void);

/* Checks if the last opened group isn't empty (could still be opened).  Returns
 * non-zero if so, otherwise zero is returned. */
int un_last_group_empty(void);

/* Returns UN_ERR_* codes. */
UnErrCode un_group_undo(void);

/* Returns UN_ERR_* codes, except for UN_ERR_NOUNDO, it's matched by
 * UN_ERR_BALANCE on redo. */
UnErrCode un_group_redo(void);

/* When detail is not 0 show detailed information for groups.  Last element of
 * list returned is NULL.  Returns NULL on error. */
char ** un_get_list(int detail);

/* Returns position in the list returned by un_get_list(). */
int un_get_list_pos(int detail);

/* Changes position in the undo list according to position in the list returned
 * by un_get_list(). */
void un_set_pos(int pos, int detail);

/* Removes all commands which files are in specified trash directory.  The
 * special value NULL means "all trash directories". */
void un_clear_cmds_with_trash(const char trash_dir[]);

#endif /* VIFM__UNDO_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
