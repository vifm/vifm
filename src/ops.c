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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "config.h"
#include "fileops.h"
#include "macros.h"
#include "menus.h"
#include "status.h"
#include "undo.h"
#include "utils.h"

#include "ops.h"

static int op_none(void *data, const char *src, const char *dst);
static int op_remove(void *data, const char *src, const char *dst);
static int op_delete(void *data, const char *src, const char *dst);
static int op_copy(void *data, const char *src, const char *dst);
static int op_move(void *data, const char *src, const char *dst);
static int op_chown(void *data, const char *src, const char *dst);
static int op_chgrp(void *data, const char *src, const char *dst);
static int op_chmod(void *data, const char *src, const char *dst);
static int op_asymlink(void *data, const char *src, const char *dst);
static int op_rsymlink(void *data, const char *src, const char *dst);

typedef int (*op_func)(void *data, const char *src, const char *dst);

static op_func op_funcs[] = {
	op_none,     /* OP_NONE */
	op_remove,   /* OP_REMOVE */
	op_delete,   /* OP_DELETE */
	op_copy,     /* OP_COPY */
	op_move,     /* OP_MOVE */
	op_move,     /* OP_MOVETMP0 */
	op_move,     /* OP_MOVETMP1 */
	op_move,     /* OP_MOVETMP2 */
	op_chown,    /* OP_CHOWN */
	op_chgrp,    /* OP_CHGRP */
	op_chmod,    /* OP_CHMOD */
	op_asymlink, /* OP_ASYMLINK */
	op_rsymlink, /* OP_RSYMLINK */
};

static int _gnuc_unused op_funcs_size_guard[
	(ARRAY_LEN(op_funcs) == OP_COUNT) ? 1 : -1
];

int
perform_operation(enum OPS op, void *data, const char *src, const char *dst)
{
	return op_funcs[op](data, src, dst);
}

static int
op_none(void *data, const char *src, const char *dst)
{
	return 0;
}

static int
op_remove(void *data, const char *src, const char *dst)
{
	char *escaped;
	char cmd[16 + PATH_MAX];
	int result;

	if(cfg.confirm && !curr_stats.confirmed)
	{
		curr_stats.confirmed = query_user_menu("Permanent deletion",
				"Are you sure? If you want to see file names use :undolist! command");
		if(!curr_stats.confirmed)
			return SKIP_UNDO_REDO_OPERATION;
	}

	escaped = escape_filename(src, 0);
	if(escaped == NULL)
		return -1;

	snprintf(cmd, sizeof(cmd), "rm -rf %s", escaped);
	result = system_and_wait_for_errors(cmd);

	free(escaped);
	return result;
}

static int
op_delete(void *data, const char *src, const char *dst)
{
	/* TODO: write code */
	return 0;
}

static int
op_copy(void *data, const char *src, const char *dst)
{
	char *escaped_src, *escaped_dst;
	char cmd[6 + PATH_MAX*2 + 1];
	int result;

	escaped_src = escape_filename(src, 0);
	escaped_dst = escape_filename(dst, 0);
	if(escaped_src == NULL || escaped_dst == NULL)
	{
		free(escaped_dst);
		free(escaped_src);
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "cp -nR --preserve=mode,timestamps %s %s",
			escaped_src, escaped_dst);
	result = system_and_wait_for_errors(cmd);

	free(escaped_dst);
	free(escaped_src);
	return result;
}

static int
op_move(void *data, const char *src, const char *dst)
{
	char *escaped_src, *escaped_dst;
	char cmd[6 + PATH_MAX*2 + 1];
	int result;

	escaped_src = escape_filename(src, 0);
	escaped_dst = escape_filename(dst, 0);
	if(escaped_src == NULL || escaped_dst == NULL)
	{
		free(escaped_dst);
		free(escaped_src);
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "mv -n %s %s", escaped_src, escaped_dst);
	result = system_and_wait_for_errors(cmd);

	free(escaped_dst);
	free(escaped_src);
	return result;
}

static int
op_chown(void *data, const char *src, const char *dst)
{
	/* TODO: write code */
	return 0;
}

static int
op_chgrp(void *data, const char *src, const char *dst)
{
	/* TODO: write code */
	return 0;
}

static int
op_chmod(void *data, const char *src, const char *dst)
{
	/* TODO: write code */
	return 0;
}

static int
op_asymlink(void *data, const char *src, const char *dst)
{
	/* TODO: write code */
	return 0;
}

static int
op_rsymlink(void *data, const char *src, const char *dst)
{
	/* TODO: write code */
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
