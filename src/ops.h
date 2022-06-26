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

#ifndef VIFM__OPS_H__
#define VIFM__OPS_H__

#include "io/ioeta.h"

/* Kinds of operations on files. */
typedef enum
{
	OP_NONE,
	OP_USR,
	OP_REMOVE,   /* rm -rf */
	OP_REMOVESL, /* cl */
	OP_COPY,     /* copy and clone */
	OP_COPYF,    /* copy with file overwrite */
	OP_COPYA,    /* copy with appending to existing contents of destination */
	OP_MOVE,     /* move, rename and substitute */
	OP_MOVEF,    /* move with file overwrite */
	OP_MOVEA,    /* move file appending to existing contents of destination */
	OP_MOVETMP1, /* multiple files rename */
	OP_MOVETMP2, /* multiple files rename */
	OP_MOVETMP3, /* multiple files rename */
	OP_MOVETMP4, /* multiple files rename */
	OP_CHOWN,
	OP_CHGRP,
#ifndef _WIN32
	OP_CHMOD,
	OP_CHMODR,
#else
	OP_ADDATTR,
	OP_SUBATTR,
#endif
	OP_SYMLINK,  /* symlink creation */
	OP_SYMLINK2, /* symlink replacement */
	OP_MKDIR,
	OP_RMDIR,
	OP_MKFILE,
	OP_COUNT
}
OPS;

/* Result of trying to execute an operation via perform_operation(). */
typedef enum
{
	OPS_SUCCEEDED, /* Operation has succeeded. */
	OPS_SKIPPED,   /* Operation was rejected by the user. */
	OPS_FAILED,    /* Operation has failed. */
}
OpsResult;

/* Policy on treating conflicts during operation processing. */
typedef enum
{
	CRP_ASK,           /* Prompt user for the decision. */
	CRP_SKIP_ALL,      /* Automatically skip file. */
	CRP_OVERWRITE_ALL, /* Automatically overwrite file. */
}
ConflictResolutionPolicy;

/* Policy on treating errors during operation processing. */
typedef enum
{
	ERP_ASK,        /* Prompt user for the decision. */
	ERP_IGNORE_ALL, /* Automatically ignore all future errors. */
}
ErrorResolutionPolicy;

struct response_variant;

/* Function to choose an option.  Returns choice. */
typedef char (*ops_choice_func)(const char title[], const char message[],
		const struct response_variant *variants);

/* Asks user to confirm some action by answering "Yes" or "No".  Returns
 * non-zero when user answers yes, otherwise zero is returned. */
typedef int (*ops_confirm_func)(const char title[], const char message[]);

/* Description of file operation on a set of files.  Collects information and
 * helps to keep track of progress. */
typedef struct
{
	OPS main_op;           /* Primary operation performed on items. */
	int total;             /* Total number of items to be processed. */
	int current;           /* Number of current item. */
	int succeeded;         /* Number of successfully processed items. */
	ioeta_estim_t *estim;  /* When non-NULL, populated with estimates for items
	                          and also frees it on ops_free(). */
	const char *descr;     /* Description of operations. */
	int shallow_eta;       /* Count only top level items, without recursion. */
	int bg;                /* Executed in background (no user interaction). */
	struct bg_op_t *bg_op; /* Information for background operation. */
	char *errors;          /* Multi-line string of errors. */

	/* It's unsafe to access global cfg object from threads performing background
	 * operations, so copy them and use the copies. */
	char *slow_fs_list;    /* Copy of 'slowfs' option value. */
	char *delete_prg;      /* Copy of 'deleteprg' option value. */
	int use_system_calls;  /* Copy of 'syscalls' option value. */
	int fast_file_cloning; /* Copy of part of 'iooptions' option value. */
	int data_sync;         /* Copy of part of 'iooptions' option value. */
	int shell_type;        /* Copy of curr_stats.shell_type */

	/* Pointers to user-interaction functions. */
	ops_choice_func choose;   /* Picking one of options. */
	ops_confirm_func confirm; /* Yes/No choice. */

	char *base_dir;   /* Base directory in which operation is taking place. */
	char *target_dir; /* Target directory of the operation (same as base_dir if
	                     none). */

	ConflictResolutionPolicy crp; /* What should be done on conflicts. */
	ErrorResolutionPolicy erp;    /* What should be done on unexpected errors. */

	/* TODO: count number of skipped files. */
}
ops_t;

/* Allocates and initializes new ops_t.  Returns just allocated structure. */
ops_t * ops_alloc(OPS main_op, int bg, const char descr[],
		const char base_dir[], const char target_dir[], ops_choice_func choose,
		ops_confirm_func confirm);

/* Describes main operation with one generic word.  Returns the description. */
const char * ops_describe(const ops_t *ops);

/* Puts new item to the ops.  Destination argument is a hint to optimize
 * estimating performance, it can be NULL. */
void ops_enqueue(ops_t *ops, const char src[], const char dst[]);

/* Advances ops to the next item. */
void ops_advance(ops_t *ops, int succeeded);

/* Frees ops_t.  The ops can be NULL. */
void ops_free(ops_t *ops);

/* Performs single operations, possibly part of the ops (which can be NULL).
 * Returns status. */
OpsResult perform_operation(OPS op, ops_t *ops, void *data, const char src[],
		const char dst[]);

#endif /* VIFM__OPS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
