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

	SKIP_UNDO_REDO_OPERATION = -8192,
};

/* Operation execution handler.  data is from add_operation() call.  Should
 * return zero on successful execution of operation, can return
 * SKIP_UNDO_REDO_OPERATION.
 */
typedef int (*perform_func)(OPS op, void *data, const char src[],
		const char dst[]);

/* Return value meaning:
 *   0 - available (but undo.c need to check file existence or absence)
 * < 0 - not available
 * > 0 - available always (no additional checks are performed) */
typedef int (*op_available_func)(OPS op);

/*
 * Won't call reset_undo_list, so this function could be called multiple
 * times.
 * exec_func can't be NULL and should return non-zero on error.
 * op_avail can be NULL.
 */
void init_undo_list(perform_func exec_func, op_available_func op_avail,
		const int* max_levels);

/*
 * Frees all allocated memory
 */
void reset_undo_list(void);

/*
 * Only stores msg pointer, so it should be valid until cmd_group_end is
 * called
 */
void cmd_group_begin(const char *msg);

/*
 * Reopens last command group
 */
void cmd_group_continue(void);

/* Replaces group message (makes its own copy of the msg).  If msg equals NULL
 * or no group is currently open, does nothing.  Returns previous value.
 */
char * replace_group_msg(const char msg[]);

/*
 * Returns 0 on success
 */
int add_operation(OPS op, void *do_data, void *undo_data, const char *buf1,
		const char *buf2);

/*
 * Returns non-zero is the last group isn't empty.
 */
int last_cmd_group_empty(void);

/*
 * Closes current group of commands
 */
void cmd_group_end(void);

/*
 * Return value:
 *   0 - on success
 *  -1 - no operation for undo is available
 *  -2 - there were errors
 *  -3 - undoing group is impossible
 *  -4 - skipped unbalanced operation
 *  -5 - operation cannot be undone
 *  -6 - operation skipped by user
 *   1 - operation was skipped due to previous errors (no command run)
 */
int undo_group(void);

/*
 * Return value:
 *   0 - on success
 *  -1 - no operation for undo is available
 *  -2 - there were errors
 *  -3 - redoing group is impossible
 *  -4 - skipped unbalanced operation
 *  -6 - operation skipped by user
 *   1 - operation was skipped due to previous errors (no command run)
 */
int redo_group(void);

/*
 * When detail is not 0 show detailed information for groups.
 * Last element of list returned is NULL.
 * Returns NULL on error.
 */
char ** undolist(int detail);

/*
 * Returns position in the list returned by undolist().
 */
int get_undolist_pos(int detail);

/*
 * Removes all commands which files are in trash dir.
 */
void clean_cmds_with_trash(void);

#endif /* VIFM__UNDO_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
