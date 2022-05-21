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

#include "undo.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <stdio.h>
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strcpy() strdup() */

#include "compat/fs_limits.h"
#include "compat/reallocarray.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "ops.h"
#include "registers.h"
#include "trash.h"

/* XXX: the unit isn't clever enough to handle operations on the same file
 *      within group of changes, hence fake OP_MOVETMP* operations.  Hard to
 *      tell if this can be done without such workarounds, but worth a try. */

typedef struct
{
	char *msg;
	int error;
	int balance;
	int can_undone;
	int incomplete;
}
group_t;

typedef struct
{
	OPS op;
	const char *src;        /* NULL, buf1 or buf2 */
	const char *dst;        /* NULL, buf1 or buf2 */
	void *data;             /* for uid_t, gid_t and mode_t */
	const char *exists;     /* NULL, buf1 or buf2 */
	const char *dont_exist; /* NULL, buf1 or buf2 */
}
op_t;

typedef struct cmd_t
{
	char *buf1;
	char *buf2;
	op_t do_op;
	op_t undo_op;

	group_t *group;
	struct cmd_t *prev;
	struct cmd_t *next;
}
cmd_t;

static OPS undo_op[] = {
	OP_NONE,     /* OP_NONE */
	OP_NONE,     /* OP_USR */
	OP_NONE,     /* OP_REMOVE */
	OP_SYMLINK,  /* OP_REMOVESL */
	OP_REMOVE,   /* OP_COPY */
	OP_REMOVE,   /* OP_COPYF */
	OP_REMOVE,   /* OP_COPYA */
	OP_MOVE,     /* OP_MOVE */
	OP_MOVE,     /* OP_MOVEF */
	OP_MOVE,     /* OP_MOVEA */
	OP_MOVETMP1, /* OP_MOVETMP1 */
	OP_MOVETMP2, /* OP_MOVETMP2 */
	OP_MOVETMP3, /* OP_MOVETMP3 */
	OP_MOVETMP4, /* OP_MOVETMP4 */
	OP_CHOWN,    /* OP_CHOWN */
	OP_CHGRP,    /* OP_CHGRP */
#ifndef _WIN32
	OP_CHMOD,    /* OP_CHMOD */
	OP_CHMODR,   /* OP_CHMODR */
#else
	OP_SUBATTR,  /* OP_ADDATTR */
	OP_ADDATTR,  /* OP_SUBATTR */
#endif
	OP_REMOVE,   /* OP_SYMLINK */
	OP_REMOVESL, /* OP_SYMLINK2 */
	OP_RMDIR,    /* OP_MKDIR */
	OP_MKDIR,    /* OP_RMDIR */
	OP_REMOVE,   /* OP_MKFILE */
};
ARRAY_GUARD(undo_op, OP_COUNT);

static enum
{
	OPER_1ST,
	OPER_2ND,
	OPER_NON,
} opers[][8] = {
	/* 1st arg   2nd arg   exists    absent   */
	{ OPER_NON, OPER_NON, OPER_NON, OPER_NON,    /* redo OP_NONE */
		OPER_NON, OPER_NON, OPER_NON, OPER_NON, }, /* undo OP_NONE */
	{ OPER_NON, OPER_NON, OPER_NON, OPER_NON,    /* redo OP_USR  */
		OPER_NON, OPER_NON, OPER_NON, OPER_NON, }, /* undo OP_NONE */
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_REMOVE */
		OPER_NON, OPER_NON, OPER_NON, OPER_NON, }, /* undo OP_NONE   */
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_REMOVESL */
	  OPER_2ND, OPER_1ST, OPER_NON, OPER_NON, }, /* undo OP_SYMLINK2 */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_2ND,    /* redo OP_COPY   */
		OPER_2ND, OPER_NON, OPER_2ND, OPER_NON, }, /* undo OP_REMOVE */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_NON,    /* redo OP_COPYF  */
		OPER_2ND, OPER_NON, OPER_2ND, OPER_NON, }, /* undo OP_REMOVE */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_2ND,    /* redo OP_COPYA  */
		OPER_2ND, OPER_NON, OPER_2ND, OPER_NON, }, /* undo OP_REMOVE */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_2ND,    /* redo OP_MOVE */
		OPER_2ND, OPER_1ST, OPER_2ND, OPER_1ST, }, /* undo OP_MOVE */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_NON,    /* redo OP_MOVEF */
		OPER_2ND, OPER_1ST, OPER_2ND, OPER_1ST, }, /* undo OP_MOVE  */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_2ND,    /* redo OP_MOVEA */
		OPER_2ND, OPER_1ST, OPER_2ND, OPER_1ST, }, /* undo OP_MOVE  */
	{ OPER_1ST, OPER_2ND, OPER_2ND, OPER_NON,    /* redo OP_MOVETMP1 */
		OPER_2ND, OPER_1ST, OPER_2ND, OPER_NON, }, /* undo OP_MOVETMP1 */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_NON,    /* redo OP_MOVETMP2 */
		OPER_2ND, OPER_1ST, OPER_1ST, OPER_NON, }, /* undo OP_MOVETMP2 */
	{ OPER_1ST, OPER_2ND, OPER_NON, OPER_2ND,    /* redo OP_MOVETMP3 */
		OPER_2ND, OPER_1ST, OPER_2ND, OPER_1ST, }, /* undo OP_MOVETMP3 */
	{ OPER_1ST, OPER_2ND, OPER_1ST, OPER_2ND,    /* redo OP_MOVETMP4 */
		OPER_2ND, OPER_1ST, OPER_NON, OPER_NON, }, /* undo OP_MOVETMP4 */
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_CHOWN */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_CHOWN */
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_CHGRP */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_CHGRP */
#ifndef _WIN32
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_CHMOD */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_CHMOD */
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_CHMODR */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_CHMODR */
#else
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_ADDATTR */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_SUBATTR */
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_SUBATTR */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_ADDATTR */
#endif
	{ OPER_1ST, OPER_2ND, OPER_NON, OPER_2ND,    /* redo OP_SYMLINK */
		OPER_2ND, OPER_NON, OPER_2ND, OPER_NON, }, /* undo OP_REMOVE  */
	{ OPER_1ST, OPER_2ND, OPER_NON, OPER_NON,    /* redo OP_SYMLINK2 */
		OPER_2ND, OPER_NON, OPER_2ND, OPER_NON, }, /* undo OP_REMOVESL */
	{ OPER_1ST, OPER_NON, OPER_NON, OPER_1ST,    /* redo OP_MKDIR */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_RMDIR */
	{ OPER_1ST, OPER_NON, OPER_1ST, OPER_NON,    /* redo OP_RMDIR */
		OPER_1ST, OPER_NON, OPER_NON, OPER_1ST, }, /* undo OP_MKDIR */
	{ OPER_1ST, OPER_NON, OPER_NON, OPER_1ST,    /* redo OP_MKFILE */
		OPER_1ST, OPER_NON, OPER_1ST, OPER_NON, }, /* undo OP_REMOVE  */
};
ARRAY_GUARD(opers, OP_COUNT);

/* Flags that indicate which operations have data that must be free()'d. */
static const char data_is_ptr[] = {
	0, /* OP_NONE */
	1, /* OP_USR */
	0, /* OP_REMOVE */
	0, /* OP_REMOVESL */
	0, /* OP_COPY */
	0, /* OP_COPYF */
	0, /* OP_COPYA */
	0, /* OP_MOVE */
	0, /* OP_MOVEF */
	0, /* OP_MOVEA */
	0, /* OP_MOVETMP1 */
	0, /* OP_MOVETMP2 */
	0, /* OP_MOVETMP3 */
	0, /* OP_MOVETMP4 */
	0, /* OP_CHOWN */
	0, /* OP_CHGRP */
#ifndef _WIN32
	1, /* OP_CHMOD */
	1, /* OP_CHMODR */
#else
	0, /* OP_ADDATTR */
	0, /* OP_SUBATTR */
#endif
	0, /* OP_SYMLINK */
	0, /* OP_SYMLINK2 */
	0, /* OP_MKDIR */
	0, /* OP_RMDIR */
	0, /* OP_MKFILE */
};
ARRAY_GUARD(data_is_ptr, OP_COUNT);

/* Operation handler function.  Performs all undo and redo operations. */
static un_perform_func do_func;
/* External function, which corrects operation availability and influence on
 * operation checks. */
static un_op_available_func op_avail_func;
/* External optional callback to abort execution of compound operations. */
static un_cancel_requested_func cancel_func;
/* Number of undo levels, which are not groups but operations. */
static const int *undo_levels;

/* List head thanks to which current is never NULL.  cmds.prev points to the
 * actual head of the list or back to cmds when the list is empty. */
static cmd_t cmds = {
	.prev = &cmds,
};
/* Points at the current item of the list of operations.  Matches cmds.prev
 * unless some operations were undone. */
static cmd_t *current = &cmds;

static int group_opened;
static long long next_group;
static group_t *last_group;
static char *group_msg;

static int command_count;

static int no_function(void);
static void init_cmd(cmd_t *cmd, OPS op, void *do_data, void *undo_data);
static void init_entry(cmd_t *cmd, const char **e, int type);
static void remove_cmd(cmd_t *cmd);
static int is_undo_group_possible(void);
static int is_redo_group_possible(void);
static int is_op_possible(const op_t *op);
static void change_filename_in_trash(cmd_t *cmd, const char filename[]);
static void update_entry(const char **e, const char old[], const char new[]);
static char ** fill_undolist_detail(char **list);
static const char * get_op_desc(op_t op);
static char ** fill_undolist_nondetail(char **list);

void
un_init(un_perform_func exec_func, un_op_available_func op_avail,
		un_cancel_requested_func cancel, const int *max_levels)
{
	assert(exec_func != NULL);

	do_func = exec_func;
	op_avail_func = op_avail;
	cancel_func = (cancel != NULL) ? cancel : &no_function;
	undo_levels = max_levels;
}

/* Always says no.  Returns zero. */
static int
no_function(void)
{
	return 0;
}

void
un_reset(void)
{
	assert(!group_opened);

	while(cmds.next != NULL)
		remove_cmd(cmds.next);
	cmds.prev = &cmds;

	current = &cmds;
	next_group = 0;
	last_group = NULL;
}

void
un_group_open(const char msg[])
{
	assert(!group_opened);

	group_opened = 1;

	(void)replace_string(&group_msg, msg);
	last_group = NULL;
}

void
un_group_reopen_last(void)
{
	assert(!group_opened);
	assert(next_group != 0);

	group_opened = 1;
	next_group--;
}

char *
un_replace_group_msg(const char msg[])
{
	char *result;

	result = group_msg;
	group_msg = (msg != NULL) ? strdup(msg) : NULL;
	if(last_group != NULL && group_msg != NULL)
	{
		(void)replace_string(&last_group->msg, group_msg);
	}
	return result;
}

int
un_group_add_op(OPS op, void *do_data, void *undo_data, const char buf1[],
		const char buf2[])
{
	int mem_error;
	cmd_t *cmd;

	assert(group_opened);
	assert(buf1 != NULL);
	assert(buf2 != NULL);

	/* free list tail */
	while(current->next != NULL)
		remove_cmd(current->next);

	while(command_count > 0 && command_count >= *undo_levels)
		remove_cmd(cmds.next);

	if(*undo_levels <= 0)
	{
		if(data_is_ptr[op])
		{
			free(do_data);
		}
		if(data_is_ptr[undo_op[op]])
		{
			free(undo_data);
		}
		return 0;
	}

	command_count++;

	/* add operation to the list */
	cmd = calloc(1, sizeof(*cmd));
	if(cmd == NULL)
		return -1;

	cmd->buf1 = strdup(buf1);
	cmd->buf2 = strdup(buf2);
	cmd->prev = current;
	init_cmd(cmd, op, do_data, undo_data);
	if(last_group != NULL)
	{
		cmd->group = last_group;
	}
	else if((cmd->group = malloc(sizeof(group_t))) != NULL)
	{
		cmd->group->msg = strdup(group_msg);
		cmd->group->error = 0;
		cmd->group->balance = 0;
		cmd->group->can_undone = 1;
		cmd->group->incomplete = 0;
	}
	mem_error = cmd->group == NULL || cmd->buf1 == NULL || cmd->buf2 == NULL;
	if(mem_error)
	{
		remove_cmd(cmd);
		return -1;
	}
	last_group = cmd->group;

	if(undo_op[op] == OP_NONE)
		cmd->group->can_undone = 0;

	current->next = cmd;
	current = cmd;
	cmds.prev = cmd;

	return 0;
}

static void
init_cmd(cmd_t *cmd, OPS op, void *do_data, void *undo_data)
{
	cmd->do_op.op = op;
	cmd->do_op.data = do_data;
	cmd->undo_op.op = undo_op[op];
	cmd->undo_op.data = undo_data;
	init_entry(cmd, &cmd->do_op.src, opers[op][0]);
	init_entry(cmd, &cmd->do_op.dst, opers[op][1]);
	init_entry(cmd, &cmd->do_op.exists, opers[op][2]);
	init_entry(cmd, &cmd->do_op.dont_exist, opers[op][3]);
	init_entry(cmd, &cmd->undo_op.src, opers[op][4]);
	init_entry(cmd, &cmd->undo_op.dst, opers[op][5]);
	init_entry(cmd, &cmd->undo_op.exists, opers[op][6]);
	init_entry(cmd, &cmd->undo_op.dont_exist, opers[op][7]);
}

static void
init_entry(cmd_t *cmd, const char **e, int type)
{
	if(type == OPER_NON)
		*e = NULL;
	else if(type == OPER_1ST)
		*e = cmd->buf1;
	else
		*e = cmd->buf2;
}

static void
remove_cmd(cmd_t *cmd)
{
	int last_cmd_in_group = 1;

	if(cmd == current)
		current = cmd->prev;

	if(cmd->prev != NULL)
	{
		cmd->prev->next = cmd->next;
		if(cmd->group == cmd->prev->group)
			last_cmd_in_group = 0;
	}
	if(cmd->next != NULL)
	{
		cmd->next->prev = cmd->prev;
		if(cmd->group == cmd->next->group)
			last_cmd_in_group = 0;
	}
	else /* this is the last command in the list */
	{
		cmds.prev = cmd->prev;
	}

	if(last_cmd_in_group)
	{
		free(cmd->group->msg);
		free(cmd->group);
		if(last_group == cmd->group)
			last_group = NULL;
	}
	else
	{
		cmd->group->incomplete = 1;
	}
	free(cmd->buf1);
	free(cmd->buf2);
	if(data_is_ptr[cmd->do_op.op])
		free(cmd->do_op.data);
	if(data_is_ptr[cmd->undo_op.op])
		free(cmd->undo_op.data);

	free(cmd);

	command_count--;
}

void
un_group_close(void)
{
	assert(group_opened);

	group_opened = 0;
	next_group++;

	while(cmds.next != NULL && cmds.next->group->incomplete)
		remove_cmd(cmds.next);
}

int
un_last_group_empty(void)
{
	return (last_group == NULL);
}

UnErrCode
un_group_undo(void)
{
	assert(!group_opened);

	if(current == &cmds)
		return UN_ERR_NONE;

	int errors = (current->group->error != 0);
	const int disbalance = (current->group->balance != 0);
	const int cant_undo = !current->group->can_undone;
	if(errors || disbalance || cant_undo || !is_undo_group_possible())
	{
		do
			current = current->prev;
		while(current != &cmds && current->group == current->next->group);

		if(errors)     return UN_ERR_ERRORS;
		if(disbalance) return UN_ERR_BALANCE;
		if(cant_undo)  return UN_ERR_NOUNDO;
		return UN_ERR_BROKEN;
	}

	regs_sync_from_shared_memory();
	current->group->balance--;

	int skip = 0;
	int cancelled;
	do
	{
		if(!skip)
		{
			OpsResult result = do_func(current->undo_op.op, current->undo_op.data,
					current->undo_op.src, current->undo_op.dst);
			switch(result)
			{
				case OPS_SUCCEEDED:
					/* Nothing to do here. */
					break;
				case OPS_SKIPPED:
					skip = 1;
					current->group->balance++;
					break;
				case OPS_FAILED:
					current->group->error = 1;
					errors = 1;
					break;
			}
		}
		current = current->prev;
	}
	while(!(cancelled = cancel_func()) && current != &cmds &&
			current->group == current->next->group);

	regs_sync_to_shared_memory();

	if(cancelled) return UN_ERR_CANCELLED;
	if(skip)      return UN_ERR_SKIPPED;
	if(errors)    return UN_ERR_FAIL;
	return UN_ERR_SUCCESS;
}

static int
is_undo_group_possible(void)
{
	cmd_t *cmd = current;
	do
	{
		int ret;
		ret = is_op_possible(&cmd->undo_op);
		if(ret == 0)
			return 0;
		else if(ret < 0)
			change_filename_in_trash(cmd, cmd->undo_op.dst);
		cmd = cmd->prev;
	}
	while(cmd != &cmds && cmd->group == cmd->next->group);
	return 1;
}

UnErrCode
un_group_redo(void)
{
	assert(!group_opened);

	if(current->next == NULL)
		return UN_ERR_NONE;

	int errors = (current->next->group->error != 0);
	const int disbalance = (current->next->group->balance == 0);
	if(errors || disbalance || !is_redo_group_possible())
	{
		do
			current = current->next;
		while(current->next != NULL && current->group == current->next->group);

		if(errors)     return UN_ERR_ERRORS;
		if(disbalance) return UN_ERR_BALANCE;
		return UN_ERR_BROKEN;
	}

	regs_sync_from_shared_memory();
	current->next->group->balance++;

	int skip = 0;
	int cancelled;
	do
	{
		current = current->next;
		if(!skip)
		{
			OpsResult result = do_func(current->do_op.op, current->do_op.data,
					current->do_op.src, current->do_op.dst);
			switch(result)
			{
				case OPS_SUCCEEDED:
					/* Nothing to do here. */
					break;
				case OPS_SKIPPED:
					current->next->group->balance--;
					skip = 1;
					break;
				case OPS_FAILED:
					current->group->error = 1;
					errors = 1;
					break;
			}
		}
	}
	while(!(cancelled = cancel_func()) && current->next != NULL &&
			current->group == current->next->group);

	regs_sync_to_shared_memory();

	if(cancelled) return UN_ERR_CANCELLED;
	if(skip)      return UN_ERR_SKIPPED;
	if(errors)    return UN_ERR_FAIL;
	return UN_ERR_SUCCESS;
}

static int
is_redo_group_possible(void)
{
	cmd_t *cmd = current;
	do
	{
		int ret;
		cmd = cmd->next;
		ret = is_op_possible(&cmd->do_op);
		if(ret == 0)
			return 0;
		else if(ret < 0)
			change_filename_in_trash(cmd, cmd->do_op.dst);
	}
	while(cmd->next != NULL && cmd->group == cmd->next->group);
	return 1;
}

/*
 * Return value:
 *   0 - impossible
 * < 0 - possible with renaming file in trash
 * > 0 - possible
 */
static int
is_op_possible(const op_t *op)
{
	if(op_avail_func != NULL)
	{
		const int avail = op_avail_func(op->op);
		if(avail != 0)
		{
			return (avail > 0);
		}
	}

	if(op->exists != NULL && !path_exists(op->exists, NODEREF))
	{
		return 0;
	}
	if(op->dont_exist != NULL && path_exists(op->dont_exist, NODEREF) &&
			!is_case_change(op->src, op->dst))
	{
		return (trash_has_path(op->dst) ? -1 : 0);
	}
	return 1;
}

static void
change_filename_in_trash(cmd_t *cmd, const char filename[])
{
	const char *name_tail;
	char *new;
	char *old;
	char *const base_dir = strdup(filename);

	remove_last_path_component(base_dir);

	name_tail = trash_get_real_name_of(filename);
	new = trash_gen_path(base_dir, name_tail);
	assert(new != NULL && "Should always get trash name here.");

	free(base_dir);

	old = cmd->buf2;
	cmd->buf2 = new;

	regs_rename_contents(filename, new);

	update_entry(&cmd->do_op.src, old, cmd->buf2);
	update_entry(&cmd->do_op.dst, old, cmd->buf2);
	update_entry(&cmd->do_op.exists, old, cmd->buf2);
	update_entry(&cmd->do_op.dont_exist, old, cmd->buf2);
	update_entry(&cmd->undo_op.src, old, cmd->buf2);
	update_entry(&cmd->undo_op.dst, old, cmd->buf2);
	update_entry(&cmd->undo_op.exists, old, cmd->buf2);
	update_entry(&cmd->undo_op.dont_exist, old, cmd->buf2);

	free(old);
}

/* Checks whether *e equals old and updates it to new if so. */
static void
update_entry(const char **e, const char old[], const char new[])
{
	if(*e == old)
	{
		*e = new;
	}
}

char **
un_get_list(int detail)
{
	char **list, **p;
	int group_count;
	cmd_t *cmd;

	assert(!group_opened);

	group_count = 1;
	cmd = cmds.prev;
	while(cmd != &cmds)
	{
		if(cmd->group != cmd->prev->group)
			group_count++;
		cmd = cmd->prev;
	}

	if(detail)
		list = reallocarray(NULL, group_count + command_count*2 + 1,
				sizeof(char *));
	else
		list = reallocarray(NULL, group_count, sizeof(char *));

	if(list == NULL)
		return NULL;

	if(detail)
		p = fill_undolist_detail(list);
	else
		p = fill_undolist_nondetail(list);
	*p = NULL;

	return list;
}

static char **
fill_undolist_detail(char **list)
{
	int left;
	cmd_t *cmd;

	left = *undo_levels;
	cmd = cmds.prev;
	while(cmd != &cmds && left > 0)
	{
		if((*list = format_str(" %s", cmd->group->msg)) == NULL)
			break;

		list++;
		do
		{
			const char *p;

			p = get_op_desc(cmd->do_op);
			if((*list = format_str("  do: %s", p)) == NULL)
			{
				return list;
			}
			++list;

			p = get_op_desc(cmd->undo_op);
			if((*list = format_str("  undo: %s", p)) == NULL)
			{
				return list;
			}
			++list;

			cmd = cmd->prev;
			--left;
		}
		while(cmd != &cmds && cmd->group == cmd->next->group && left > 0);
	}

	return list;
}

static const char *
get_op_desc(op_t op)
{
	static char buf[64 + 2*PATH_MAX] = "";
	switch(op.op)
	{
		case OP_NONE:
			strcpy(buf, "<no operation>");
			break;
		case OP_USR:
			copy_str(buf, sizeof(buf), (const char *)op.data);
			break;
		case OP_REMOVE:
		case OP_REMOVESL:
			snprintf(buf, sizeof(buf), "rm %s", op.src);
			break;
		case OP_COPY:
		case OP_COPYA:
			snprintf(buf, sizeof(buf), "cp %s to %s", op.src, op.dst);
			break;
		case OP_COPYF:
			snprintf(buf, sizeof(buf), "cp -f %s to %s", op.src, op.dst);
			break;
		case OP_MOVE:
		case OP_MOVEA:
		case OP_MOVETMP1:
		case OP_MOVETMP2:
		case OP_MOVETMP3:
		case OP_MOVETMP4:
			snprintf(buf, sizeof(buf), "mv %s to %s", op.src, op.dst);
			break;
		case OP_MOVEF:
			snprintf(buf, sizeof(buf), "mv -f %s to %s", op.src, op.dst);
			break;
		case OP_CHOWN:
			snprintf(buf, sizeof(buf), "chown %" PRINTF_ULL " %s",
					(unsigned long long)(size_t)op.data, op.src);
			break;
		case OP_CHGRP:
			snprintf(buf, sizeof(buf), "chown :%" PRINTF_ULL " %s",
					(unsigned long long)(size_t)op.data, op.src);
			break;
#ifndef _WIN32
		case OP_CHMOD:
		case OP_CHMODR:
			snprintf(buf, sizeof(buf), "chmod %s %s", (char *)op.data, op.src);
			break;
#else
		case OP_ADDATTR:
			snprintf(buf, sizeof(buf), "attrib +%s", attr_str((size_t)op.data));
			break;
		case OP_SUBATTR:
			snprintf(buf, sizeof(buf), "attrib -%s", attr_str((size_t)op.data));
			break;
#endif
		case OP_SYMLINK:
		case OP_SYMLINK2:
			snprintf(buf, sizeof(buf), "ln -s %s to %s", op.src, op.dst);
			break;
		case OP_MKDIR:
			snprintf(buf, sizeof(buf), "mkdir %s%s", (op.data == NULL) ? "" : "-p ",
					op.src);
			break;
		case OP_RMDIR:
			snprintf(buf, sizeof(buf), "rmdir %s", op.src);
			break;
		case OP_MKFILE:
			snprintf(buf, sizeof(buf), "touch %s", op.src);
			break;

		case OP_COUNT:
			strcpy(buf, "ERROR, not a valid operation kind");
			break;
	}

	return buf;
}

static char **
fill_undolist_nondetail(char **list)
{
	int left;
	cmd_t *cmd;

	left = *undo_levels;
	cmd = cmds.prev;
	while(cmd != &cmds && left-- > 0)
	{
		if((*list = format_str(" %s", cmd->group->msg)) == NULL)
			break;

		do
			cmd = cmd->prev;
		while(cmd != &cmds && cmd->group == cmd->next->group);
		list++;
	}

	return list;
}

int
un_get_list_pos(int detail)
{
	cmd_t *cur = cmds.prev;
	int result_group = 0;
	int result_cmd = 0;

	assert(!group_opened);

	if(cur == &cmds)
		result_group++;
	while(cur != current)
	{
		if(cur->group != cur->prev->group)
			result_group++;
		result_cmd += 2;
		cur = cur->prev;
	}
	return detail ? (result_group + result_cmd) : result_group;
}

void
un_set_pos(int pos, int detail)
{
	assert(!group_opened);

	cmd_t *cur = cmds.prev;
	cmd_t *last = cur;

	--pos;
	while(pos >= 0 && cur != &cmds)
	{
		if(cur->group != last->group)
		{
			last = cur;
			--pos;
		}
		if(detail)
		{
			pos -= 2;
		}
		cur = cur->prev;
	}

	current = (pos == 0 ? cur : last);
}

void
un_clear_cmds_with_trash(const char trash_dir[])
{
	cmd_t *cur = cmds.prev;

	assert(!group_opened);

	while(cur != &cmds)
	{
		cmd_t *prev = cur->prev;

		if(cur->group->balance < 0)
		{
			if(cur->do_op.exists != NULL &&
					trash_has_path_at(trash_dir, cur->do_op.exists))
			{
				remove_cmd(cur);
			}
		}
		else
		{
			if(cur->undo_op.exists != NULL &&
					trash_has_path_at(trash_dir, cur->undo_op.exists))
			{
				remove_cmd(cur);
			}
		}
		cur = prev;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
