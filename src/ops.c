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

#include "ops.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

#include "utils/utf8.h"
#endif

#include <sys/stat.h> /* gid_t uid_t */

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* strdup() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "compat/reallocarray.h"
#include "io/ioeta.h"
#include "io/iop.h"
#include "io/ior.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/cancellation.h"
#include "utils/cancellation.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "background.h"
#include "bmarks.h"
#include "status.h"
#include "trash.h"

#ifdef SUPPORT_NO_CLOBBER
#define NO_CLOBBER "-n"
#else
#define NO_CLOBBER ""
#endif

/* Enable O(1) file cloning if it's available in installed version of
 * coreutils. */
#ifdef SUPPORT_REFLINK_AUTO
#define REFLINK_AUTO "--reflink=auto"
#else
#define REFLINK_AUTO ""
#endif

#define PRESERVE_FLAGS "-p"

/* Types of conflict resolution actions to perform. */
typedef enum
{
	CA_FAIL,      /* Fail with an error. */
	CA_OVERWRITE, /* Overwrite existing files. */
	CA_APPEND,    /* Append the rest of source file to destination file. */
}
ConflictAction;

/* Type of function that implements single operation. */
typedef OpsResult (*op_func)(ops_t *ops, void *data, const char src[],
		const char dst[]);

static OpsResult op_none(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_remove(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_removesl(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_copy(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_copyf(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_copya(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_cp(ops_t *ops, void *data, const char src[],
		const char dst[], ConflictAction conflict_action);
static OpsResult op_move(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_movef(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_movea(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_mv(ops_t *ops, void *data, const char src[],
		const char dst[], ConflictAction conflict_action);
static IoCrs ca_to_crs(ConflictAction conflict_action);
static OpsResult op_chown(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_chgrp(ops_t *ops, void *data, const char src[],
		const char dst[]);
#ifndef _WIN32
static OpsResult op_chmod(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_chmodr(ops_t *ops, void *data, const char src[],
		const char dst[]);
#else
static OpsResult op_addattr(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_subattr(ops_t *ops, void *data, const char src[],
		const char dst[]);
#endif
static OpsResult op_symlink(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_mkdir(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_rmdir(ops_t *ops, void *data, const char src[],
		const char dst[]);
static OpsResult op_mkfile(ops_t *ops, void *data, const char src[],
		const char dst[]);
static int ops_uses_syscalls(const ops_t *ops);
static OpsResult exec_io_op(ops_t *ops, IoRes (*func)(io_args_t *),
		io_args_t *args, int cancellable);
static int confirm_overwrite(io_args_t *args, const char src[],
		const char dst[]);
static char * pretty_dir_path(const char path[]);
static IoErrCbResult dispatch_error(io_args_t *args, const ioe_err_t *err);
static char prompt_user(const io_args_t *args, const char title[],
		const char msg[], const response_variant variants[]);
static int ui_cancellation_hook(void *arg);
#ifndef _WIN32
static OpsResult run_operation_command(ops_t *ops, char cmd[], int cancellable);
#endif
static int bg_cancellation_hook(void *arg);
static OpsResult result_from_code(int exit_code);

/* List of functions that implement operations. */
static op_func op_funcs[] = {
	[OP_NONE]     = &op_none,
	[OP_USR]      = &op_none,
	[OP_REMOVE]   = &op_remove,
	[OP_REMOVESL] = &op_removesl,
	[OP_COPY]     = &op_copy,
	[OP_COPYF]    = &op_copyf,
	[OP_COPYA]    = &op_copya,
	[OP_MOVE]     = &op_move,
	[OP_MOVEF]    = &op_movef,
	[OP_MOVEA]    = &op_movea,
	[OP_MOVETMP1] = &op_move,
	[OP_MOVETMP2] = &op_move,
	[OP_MOVETMP3] = &op_move,
	[OP_MOVETMP4] = &op_move,
	[OP_CHOWN]    = &op_chown,
	[OP_CHGRP]    = &op_chgrp,
#ifndef _WIN32
	[OP_CHMOD]    = &op_chmod,
	[OP_CHMODR]   = &op_chmodr,
#else
	[OP_ADDATTR]  = &op_addattr,
	[OP_SUBATTR]  = &op_subattr,
#endif
	[OP_SYMLINK]  = &op_symlink,
	[OP_SYMLINK2] = &op_symlink,
	[OP_MKDIR]    = &op_mkdir,
	[OP_RMDIR]    = &op_rmdir,
	[OP_MKFILE]   = &op_mkfile,
};
ARRAY_GUARD(op_funcs, OP_COUNT);

/* Operation that is processed at the moment. */
static ops_t *curr_ops;

ops_t *
ops_alloc(OPS main_op, int bg, const char descr[], const char base_dir[],
		const char target_dir[], ops_choice_func choose, ops_confirm_func confirm)
{
	ops_t *const ops = calloc(1, sizeof(*ops));

	ops->main_op = main_op;
	ops->descr = descr;
	ops->bg = bg;

	update_string(&ops->slow_fs_list, cfg.slow_fs_list);
	update_string(&ops->delete_prg, cfg.delete_prg);
	ops->use_system_calls = cfg.use_system_calls;
	ops->fast_file_cloning = cfg.fast_file_cloning;
	ops->data_sync = cfg.data_sync;
	ops->shell_type = curr_stats.shell_type;

	ops->choose = choose;
	ops->confirm = confirm;

	ops->base_dir = strdup(base_dir);
	ops->target_dir = strdup(target_dir);

	return ops;
}

const char *
ops_describe(const ops_t *ops)
{
	return ops->descr;
}

void
ops_enqueue(ops_t *ops, const char src[], const char dst[])
{
	++ops->total;

	if(ops->estim == NULL)
	{
		return;
	}

	/* Check once and cache result, it should be the same for each invocation. */
	if(ops->estim->total_items == 0)
	{
		switch(ops->main_op)
		{
			case OP_MOVE:
			case OP_MOVEF:
			case OP_MOVETMP1:
			case OP_MOVETMP2:
			case OP_MOVETMP3:
			case OP_MOVETMP4:
				if(dst != NULL && are_on_the_same_fs(src, dst))
				{
					/* Moving files/directories inside file system is cheap operation on
					 * top level items, no need to recur below. */
					ops->shallow_eta = 1;
				}
				break;

			case OP_SYMLINK:
			case OP_SYMLINK2:
				/* No need for recursive traversal if we're going to create symbolic
				 * links. */
				ops->shallow_eta = 1;
				break;

			default:
				/* No optimizations for other operations. */
				break;
		}

		if(is_on_slow_fs(src, ops->slow_fs_list))
		{
			ops->shallow_eta = 1;
		}
	}

	ioeta_calculate(ops->estim, src, ops->shallow_eta);
}

void
ops_advance(ops_t *ops, int succeeded)
{
	++ops->current;
	assert(ops->current <= ops->total && "Current and total are out of sync.");

	if(succeeded)
	{
		++ops->succeeded;
	}
}

void
ops_free(ops_t *ops)
{
	if(ops == NULL)
	{
		return;
	}

	ioeta_free(ops->estim);
	free(ops->errors);
	free(ops->slow_fs_list);
	free(ops->delete_prg);
	free(ops->base_dir);
	free(ops->target_dir);
	free(ops);
}

OpsResult
perform_operation(OPS op, ops_t *ops, void *data, const char src[],
		const char dst[])
{
	return op_funcs[op](ops, data, src, dst);
}

static OpsResult
op_none(ops_t *ops, void *data, const char src[], const char dst[])
{
	return OPS_SUCCEEDED;
}

static OpsResult
op_remove(ops_t *ops, void *data, const char src[], const char dst[])
{
	if((ops == NULL || !ops->bg) && cfg_confirm_delete(0) &&
			!curr_stats.confirmed)
	{
		ops_confirm_func confirm = (ops != NULL ? ops->confirm : &prompt_msg);

		char *msg = format_str("Are you sure?  "
				"At least the following file is about to be deleted:\n \n%s\n \n"
				"If you're undoing a command and want to see file names, use "
				":undolist! command.",
				replace_home_part(src));
		curr_stats.confirmed = confirm("Permanent deletion", msg);
		free(msg);
		if(!curr_stats.confirmed)
			return OPS_SKIPPED;
	}

	return op_removesl(ops, data, src, dst);
}

static OpsResult
op_removesl(ops_t *ops, void *data, const char src[], const char dst[])
{
	const char *const delete_prg = (ops == NULL)
	                             ? cfg.delete_prg
	                             : ops->delete_prg;
	if(delete_prg[0] != '\0')
	{
#ifndef _WIN32
		char *escaped;
		char cmd[2*PATH_MAX + 1];
		const int cancellable = (data == NULL);

		escaped = shell_like_escape(src, 0);
		if(escaped == NULL)
		{
			return OPS_FAILED;
		}

		snprintf(cmd, sizeof(cmd), "%s %s", delete_prg, escaped);
		free(escaped);

		LOG_INFO_MSG("Running trash command: \"%s\"", cmd);
		return run_operation_command(ops, cmd, cancellable);
#else
		char cmd[PATH_MAX*2 + 1];
		snprintf(cmd, sizeof(cmd), "%s \"%s\"", delete_prg, src);
		internal_to_system_slashes(cmd);

		return result_from_code(os_system(cmd));
#endif
	}

	if(!ops_uses_syscalls(ops))
	{
#ifndef _WIN32
		char *escaped;
		char cmd[16 + PATH_MAX];
		const int cancellable = data == NULL;

		escaped = shell_like_escape(src, 0);
		if(escaped == NULL)
			return OPS_FAILED;

		snprintf(cmd, sizeof(cmd), "rm -rf %s", escaped);
		LOG_INFO_MSG("Running rm command: \"%s\"", cmd);
		OpsResult result = run_operation_command(ops, cmd, cancellable);

		free(escaped);
		return result;
#else
		if(is_dir(src))
		{
			char path[PATH_MAX + 1];
			int err;

			copy_str(path, sizeof(path), src);
			internal_to_system_slashes(path);

			wchar_t *utf16_path = utf8_to_utf16(path);

			/* SHFileOperationW requires pFrom to be double-nul terminated. */
			const size_t len = wcslen(utf16_path);
			wchar_t *new_utf16_path = reallocarray(utf16_path, len + 1U + 1U,
					sizeof(*utf16_path));
			if(new_utf16_path == NULL)
			{
				free(utf16_path);
				return OPS_FAILED;
			}

			utf16_path = new_utf16_path;
			utf16_path[len + 1U] = L'\0';

			SHFILEOPSTRUCTW fo = {
				.hwnd = NULL,
				.wFunc = FO_DELETE,
				.pFrom = utf16_path,
				.pTo = NULL,
				.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI,
			};
			err = SHFileOperationW(&fo);

			log_msg("Error: %d", err);
			free(utf16_path);

			return result_from_code(err);
		}
		else
		{
			wchar_t *const utf16_path = utf8_to_utf16(src);
			DWORD attributes = GetFileAttributesW(utf16_path);
			if(attributes & FILE_ATTRIBUTE_READONLY)
			{
				SetFileAttributesW(utf16_path, attributes & ~FILE_ATTRIBUTE_READONLY);
			}

			BOOL success = DeleteFileW(utf16_path);
			if(!success)
			{
				LOG_WERROR(GetLastError());
			}

			free(utf16_path);
			return result_from_code(!success);
		}
#endif
	}

	io_args_t args = {
		.arg1.path = src,
	};
	return exec_io_op(ops, &ior_rm, &args, data == NULL);
}

/* OP_COPY operation handler.  Copies file/directory without overwriting
 * destination files (when it's supported by the system).  Returns status. */
static OpsResult
op_copy(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_cp(ops, data, src, dst, CA_FAIL);
}

/* OP_COPYF operation handler.  Copies file/directory overwriting destination
 * files.  Returns status. */
static OpsResult
op_copyf(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_cp(ops, data, src, dst, CA_OVERWRITE);
}

/* OP_COPYA operation handler.  Copies file appending rest of the source file to
 * the destination.  Returns status. */
static OpsResult
op_copya(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_cp(ops, data, src, dst, CA_APPEND);
}

/* Copies file/directory overwriting/appending destination files if requested.
 * Returns status. */
static OpsResult
op_cp(ops_t *ops, void *data, const char src[], const char dst[],
		ConflictAction conflict_action)
{
	const int fast_file_cloning = (ops == NULL)
	                             ? cfg.fast_file_cloning
	                             : ops->fast_file_cloning;
	const int data_sync = (ops == NULL ? cfg.data_sync : ops->data_sync);

	if(!ops_uses_syscalls(ops))
	{
#ifndef _WIN32
		char *escaped_src, *escaped_dst;
		char cmd[6 + PATH_MAX*2 + 1];
		const int cancellable = (data == NULL);

		escaped_src = shell_like_escape(src, 0);
		escaped_dst = shell_like_escape(dst, 0);
		if(escaped_src == NULL || escaped_dst == NULL)
		{
			free(escaped_dst);
			free(escaped_src);
			return OPS_FAILED;
		}

		snprintf(cmd, sizeof(cmd),
				"cp %s %s -R " PRESERVE_FLAGS " %s %s",
				(conflict_action == CA_FAIL) ? NO_CLOBBER : "",
				fast_file_cloning ? REFLINK_AUTO : "",
				escaped_src, escaped_dst);
		LOG_INFO_MSG("Running cp command: \"%s\"", cmd);
		OpsResult result = run_operation_command(ops, cmd, cancellable);

		free(escaped_dst);
		free(escaped_src);
		return result;
#else
		int ret;

		if(is_dir(src))
		{
			char cmd[6 + PATH_MAX*2 + 1];
			snprintf(cmd, sizeof(cmd), "xcopy \"%s\" \"%s\" ", src, dst);
			internal_to_system_slashes(cmd);

			if(is_vista_and_above())
				strcat(cmd, "/B ");
			if(conflict_action != CA_FAIL)
			{
				strcat(cmd, "/Y ");
			}
			strcat(cmd, "/E /I /H /R > NUL");
			ret = os_system(cmd);
		}
		else
		{
			wchar_t *const utf16_src = utf8_to_utf16(src);
			wchar_t *const utf16_dst = utf8_to_utf16(dst);
			ret = (CopyFileW(utf16_src, utf16_dst, 0) == 0);
			free(utf16_dst);
			free(utf16_src);
		}

		return result_from_code(ret);
#endif
	}

	if(ops != NULL && conflict_action == CA_OVERWRITE)
	{
		ops->crp = CRP_OVERWRITE_ALL;
	}

	io_args_t args = {
		.arg1.src = src,
		.arg2.dst = dst,
		.arg3.crs = ca_to_crs(conflict_action),
		.arg4 = {
			.fast_file_cloning = fast_file_cloning,
			.data_sync = data_sync,
		},
	};
	return exec_io_op(ops, &ior_cp, &args, data == NULL);
}

/* OP_MOVE operation handler.  Moves file/directory without overwriting
 * destination files (when it's supported by the system).  Returns status. */
static OpsResult
op_move(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_mv(ops, data, src, dst, CA_FAIL);
}

/* OP_MOVEF operation handler.  Moves file/directory overwriting destination
 * files.  Returns status. */
static OpsResult
op_movef(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_mv(ops, data, src, dst, CA_OVERWRITE);
}

/* OP_MOVEA operation handler.  Moves file appending rest of the source file to
 * the destination.  Returns status. */
static OpsResult
op_movea(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_mv(ops, data, src, dst, CA_APPEND);
}

/* Moves file/directory overwriting/appending destination files if requested.
 * Returns status. */
static OpsResult
op_mv(ops_t *ops, void *data, const char src[], const char dst[],
		ConflictAction conflict_action)
{
	OpsResult result;

	if(!ops_uses_syscalls(ops))
	{
#ifndef _WIN32
		struct stat st;
		char *escaped_src, *escaped_dst;
		char cmd[6 + PATH_MAX*2 + 1];
		const int cancellable = data == NULL;

		if(conflict_action == CA_FAIL && os_lstat(dst, &st) == 0 &&
				!is_case_change(src, dst))
		{
			return OPS_FAILED;
		}

		escaped_src = shell_like_escape(src, 0);
		escaped_dst = shell_like_escape(dst, 0);
		if(escaped_src == NULL || escaped_dst == NULL)
		{
			free(escaped_dst);
			free(escaped_src);
			return OPS_FAILED;
		}

		snprintf(cmd, sizeof(cmd), "mv %s %s %s",
				(conflict_action == CA_FAIL) ? NO_CLOBBER : "",
				escaped_src, escaped_dst);
		free(escaped_dst);
		free(escaped_src);

		LOG_INFO_MSG("Running mv command: \"%s\"", cmd);
		result = run_operation_command(ops, cmd, cancellable);
#else
		wchar_t *const utf16_src = utf8_to_utf16(src);
		wchar_t *const utf16_dst = utf8_to_utf16(dst);

		BOOL success = MoveFileW(utf16_src, utf16_dst);

		free(utf16_src);
		free(utf16_dst);

		if(!success && GetLastError() == 5)
		{
			const OpsResult r = op_cp(ops, data, src, dst, conflict_action);
			if(r != OPS_SUCCEEDED)
			{
				return r;
			}
			return op_removesl(ops, data, src, NULL);
		}
		result = result_from_code(!success);
#endif
	}
	else
	{
		io_args_t args = {
			.arg1.src = src,
			.arg2.dst = dst,
			.arg3.crs = ca_to_crs(conflict_action),
			.arg4 = {
				/* It's safe to always use fast file cloning on moving files. */
				.fast_file_cloning = 1,
				.data_sync = (ops == NULL ? cfg.data_sync : ops->data_sync),
			},
		};
		result = exec_io_op(ops, &ior_mv, &args, data == NULL);
	}

	if(result == OPS_SUCCEEDED)
	{
		trash_file_moved(src, dst);
		bmarks_file_moved(src, dst);
	}

	return result;
}

/* Maps conflict action to conflict resolution strategy of i/o modules.  Returns
 * conflict resolution strategy type. */
static IoCrs
ca_to_crs(ConflictAction conflict_action)
{
	switch(conflict_action)
	{
		case CA_FAIL:      return IO_CRS_FAIL;
		case CA_OVERWRITE: return IO_CRS_REPLACE_FILES;
		case CA_APPEND:    return IO_CRS_APPEND_TO_FILES;
	}
	assert(0 && "Unhandled conflict action.");
	return IO_CRS_FAIL;
}

static OpsResult
op_chown(ops_t *ops, void *data, const char src[], const char dst[])
{
#ifndef _WIN32
	char cmd[10 + 32 + PATH_MAX];
	char *escaped;
	uid_t uid = (uid_t)(long)data;

	escaped = shell_like_escape(src, 0);
	snprintf(cmd, sizeof(cmd), "chown -fR %u %s", uid, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chown command: \"%s\"", cmd);
	return run_operation_command(ops, cmd, 1);
#else
	return OPS_FAILED;
#endif
}

static OpsResult
op_chgrp(ops_t *ops, void *data, const char src[], const char dst[])
{
#ifndef _WIN32
	char cmd[10 + 32 + PATH_MAX];
	char *escaped;
	gid_t gid = (gid_t)(long)data;

	escaped = shell_like_escape(src, 0);
	snprintf(cmd, sizeof(cmd), "chown -fR :%u %s", gid, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chgrp command: \"%s\"", cmd);
	return run_operation_command(ops, cmd, 1);
#else
	return OPS_FAILED;
#endif
}

#ifndef _WIN32
static OpsResult
op_chmod(ops_t *ops, void *data, const char src[], const char dst[])
{
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = shell_like_escape(src, 0);
	snprintf(cmd, sizeof(cmd), "chmod %s %s", (char *)data, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chmod command: \"%s\"", cmd);
	return run_operation_command(ops, cmd, 1);
}

static OpsResult
op_chmodr(ops_t *ops, void *data, const char src[], const char dst[])
{
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = shell_like_escape(src, 0);
	snprintf(cmd, sizeof(cmd), "chmod -R %s %s", (char *)data, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chmodr command: \"%s\"", cmd);
	return run_operation_command(ops, cmd, 1);
}
#else
static OpsResult
op_addattr(ops_t *ops, void *data, const char src[], const char dst[])
{
	const DWORD add_mask = (size_t)data;
	wchar_t *const utf16_path = utf8_to_utf16(src);
	const DWORD attrs = GetFileAttributesW(utf16_path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return OPS_FAILED;
	}
	if(!SetFileAttributesW(utf16_path, attrs | add_mask))
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return OPS_FAILED;
	}
	free(utf16_path);
	return OPS_SUCCEEDED;
}

static OpsResult
op_subattr(ops_t *ops, void *data, const char src[], const char dst[])
{
	const DWORD sub_mask = (size_t)data;
	wchar_t *const utf16_path = utf8_to_utf16(src);
	const DWORD attrs = GetFileAttributesW(utf16_path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return OPS_FAILED;
	}
	if(!SetFileAttributesW(utf16_path, attrs & ~sub_mask))
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return OPS_FAILED;
	}
	free(utf16_path);
	return OPS_SUCCEEDED;
}
#endif

static OpsResult
op_symlink(ops_t *ops, void *data, const char src[], const char dst[])
{
	if(!ops_uses_syscalls(ops))
	{
		char *escaped_src, *escaped_dst;
		char cmd[6 + PATH_MAX*2 + 1];
		OpsResult result;

#ifndef _WIN32
		escaped_src = shell_like_escape(src, 0);
		escaped_dst = shell_like_escape(dst, 0);
#else
		escaped_src = strdup(enclose_in_dquotes(src, ops->shell_type));
		escaped_dst = strdup(enclose_in_dquotes(dst, ops->shell_type));
#endif

		if(escaped_src == NULL || escaped_dst == NULL)
		{
			free(escaped_dst);
			free(escaped_src);
			return OPS_FAILED;
		}

#ifndef _WIN32
		snprintf(cmd, sizeof(cmd), "ln -s %s %s", escaped_src, escaped_dst);
		LOG_INFO_MSG("Running ln command: \"%s\"", cmd);
		result = run_operation_command(ops, cmd, 1);
#else
		char exe_dir[PATH_MAX + 2];
		if(get_exe_dir(exe_dir, ARRAY_LEN(exe_dir)) != 0)
		{
			free(escaped_dst);
			free(escaped_src);
			return OPS_FAILED;
		}

		snprintf(cmd, sizeof(cmd), "%s\\win_helper -s %s %s", exe_dir, escaped_src,
				escaped_dst);
		result = result_from_code(os_system(cmd));
#endif

		free(escaped_dst);
		free(escaped_src);
		return result;
	}

	io_args_t args = {
		.arg1.path = src,
		.arg2.target = dst,
		.arg3.crs = IO_CRS_REPLACE_FILES,
	};
	return exec_io_op(ops, &iop_ln, &args, 0);
}

static OpsResult
op_mkdir(ops_t *ops, void *data, const char src[], const char dst[])
{
	if(!ops_uses_syscalls(ops))
	{
#ifndef _WIN32
		char cmd[128 + PATH_MAX];
		char *escaped;

		escaped = shell_like_escape(src, 0);
		snprintf(cmd, sizeof(cmd), "mkdir %s %s", (data == NULL) ? "" : "-p",
				escaped);
		free(escaped);
		LOG_INFO_MSG("Running mkdir command: \"%s\"", cmd);
		return run_operation_command(ops, cmd, 1);
#else
		if(data == NULL)
		{
			wchar_t *const utf16_path = utf8_to_utf16(src);
			BOOL success = CreateDirectoryW(utf16_path, NULL);
			free(utf16_path);
			return result_from_code(!success);
		}
		else
		{
			char *const partial_path = strdup(src);
			char *part = partial_path + (is_path_absolute(src) ? 2 : 0);
			char *state = NULL;

			while((part = split_and_get(part, '/', &state)) != NULL)
			{
				if(!is_dir(partial_path))
				{
					wchar_t *const utf16_path = utf8_to_utf16(partial_path);
					if(!CreateDirectoryW(utf16_path, NULL))
					{
						free(utf16_path);
						free(partial_path);
						return OPS_FAILED;
					}
					free(utf16_path);
				}
			}
			free(partial_path);
			return OPS_SUCCEEDED;
		}
#endif
	}

	io_args_t args = {
		.arg1.path = src,
		.arg2.process_parents = data != NULL,
		.arg3.mode = 0755,
	};
	return exec_io_op(ops, &iop_mkdir, &args, 0);
}

static OpsResult
op_rmdir(ops_t *ops, void *data, const char src[], const char dst[])
{
	if(!ops_uses_syscalls(ops))
	{
#ifndef _WIN32
		char cmd[128 + PATH_MAX];
		char *escaped;

		escaped = shell_like_escape(src, 0);
		snprintf(cmd, sizeof(cmd), "rmdir %s", escaped);
		free(escaped);
		LOG_INFO_MSG("Running rmdir command: \"%s\"", cmd);
		return run_operation_command(ops, cmd, 1);
#else
		wchar_t *const utf16_path = utf8_to_utf16(src);
		const BOOL success = RemoveDirectoryW(utf16_path);
		free(utf16_path);
		return result_from_code(!success);
#endif
	}

	io_args_t args = {
		.arg1.path = src,
	};
	return exec_io_op(ops, &iop_rmdir, &args, 0);
}

static OpsResult
op_mkfile(ops_t *ops, void *data, const char src[], const char dst[])
{
	if(!ops_uses_syscalls(ops))
	{
#ifndef _WIN32
		char cmd[128 + PATH_MAX];
		char *escaped;

		escaped = shell_like_escape(src, 0);
		snprintf(cmd, sizeof(cmd), "touch %s", escaped);
		free(escaped);
		LOG_INFO_MSG("Running touch command: \"%s\"", cmd);
		return run_operation_command(ops, cmd, 1);
#else
		HANDLE hfile;

		wchar_t *const utf16_path = utf8_to_utf16(src);
		hfile = CreateFileW(utf16_path, 0, 0, NULL, CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL, NULL);
		free(utf16_path);
		if(hfile == INVALID_HANDLE_VALUE)
		{
			return OPS_FAILED;
		}

		CloseHandle(hfile);
		return OPS_SUCCEEDED;
#endif
	}

	io_args_t args = {
		.arg1.path = src,
	};
	return exec_io_op(ops, &iop_mkfile, &args, 0);
}

/* Checks whether specific operation should use system calls.  Returns non-zero
 * if so, otherwise zero is returned. */
static int
ops_uses_syscalls(const ops_t *ops)
{
	return ops == NULL ? cfg.use_system_calls : ops->use_system_calls;
}

/* Executes i/o operation with some predefined pre/post actions.  Returns
 * status. */
static OpsResult
exec_io_op(ops_t *ops, IoRes (*func)(io_args_t *), io_args_t *args,
		int cancellable)
{
	args->estim = (ops == NULL) ? NULL : ops->estim;

	if(ops != NULL)
	{
		if(!ops->bg)
		{
			args->confirm = &confirm_overwrite;
			args->result.errors_cb = &dispatch_error;
		}

		ioe_errlst_init(&args->result.errors);
	}

	if(cancellable)
	{
		if(ops != NULL && ops->bg)
		{
			args->cancellation.arg = ops->bg_op;
			args->cancellation.hook = &bg_cancellation_hook;
		}
		else
		{
			/* ui_cancellation_push_off() should be called outside of this unit to
			 * allow bulking several operations together. */
			ui_cancellation_enable();
			args->cancellation.hook = &ui_cancellation_hook;
		}
	}

	curr_ops = ops;
	OpsResult result = OPS_FAILED;
	switch(func(args))
	{
		case IO_RES_SUCCEEDED: result = OPS_SUCCEEDED; break;
		case IO_RES_SKIPPED:   result = OPS_SKIPPED; break;
		case IO_RES_FAILED:    result = OPS_FAILED; break;
	}
	curr_ops = NULL;

	if(cancellable && (ops == NULL || !ops->bg))
	{
		ui_cancellation_disable();
	}

	if(ops != NULL)
	{
		size_t len = (ops->errors == NULL) ? 0U : strlen(ops->errors);
		char *const suffix = ioe_errlst_to_str(&args->result.errors);

		if(len != 0U)
		{
			(void)strappend(&ops->errors, &len, "\n");
		}
		(void)strappend(&ops->errors, &len, suffix);

		free(suffix);
		ioe_errlst_free(&args->result.errors);
	}

	return result;
}

/* Asks user to confirm file overwrite.  Returns non-zero on positive user
 * answer, otherwise zero is returned. */
static int
confirm_overwrite(io_args_t *args, const char src[], const char dst[])
{
	/* TODO: think about adding "append" and "rename" options here. */
	static const response_variant responses[] = {
		{ .key = 'y', .descr = "[y]es", },
		{ .key = 'Y', .descr = "[Y]es for all", },
		{ .key = 'n', .descr = "[n]o", },
		{ .key = 'N', .descr = "[N]o for all", },
		{ },
	};

	char *title;
	char *msg;
	char response;
	char *src_dir, *dst_dir;
	const char *fname = get_last_path_component(dst);

	if(curr_ops->crp != CRP_ASK)
	{
		return (curr_ops->crp == CRP_OVERWRITE_ALL) ? 1 : 0;
	}

	src_dir = pretty_dir_path(src);
	dst_dir = pretty_dir_path(dst);

	title = format_str("File overwrite while %s", curr_ops->descr);
	msg = format_str("Overwrite \"%s\" in\n%s\nwith \"%s\" from\n%s\n?", fname,
			dst_dir, fname, src_dir);

	free(dst_dir);
	free(src_dir);

	response = prompt_user(args, title, msg, responses);

	free(msg);
	free(title);

	switch(response)
	{
		case 'Y': curr_ops->crp = CRP_OVERWRITE_ALL; /* Fall through. */
		case 'y': return 1;

		case 'N': curr_ops->crp = CRP_SKIP_ALL; /* Fall through. */
		case 'n': return 0;

		default:
			assert(0 && "Unexpected response.");
			return 0;
	}
}

/* Prepares path to presenting to the user.  Returns newly allocated string,
 * which should be freed by the caller, or NULL if there is not enough
 * memory. */
static char *
pretty_dir_path(const char path[])
{
	char dir_only[strlen(path) + 1];
	char canonic[PATH_MAX + 1];

	copy_str(dir_only, sizeof(dir_only), path);
	remove_last_path_component(dir_only);
	canonicalize_path(dir_only, canonic, sizeof(canonic));

	return strdup(canonic);
}

/* Asks user what to do with encountered error.  Returns the response. */
static IoErrCbResult
dispatch_error(io_args_t *args, const ioe_err_t *err)
{
	static const response_variant responses[] = {
		{ .key = 'r', .descr = "[r]etry", },
		{ .key = 'i', .descr = "[i]gnore", },
		{ .key = 'I', .descr = "[I]gnore for all", },
		{ .key = 'a', .descr = "[a]bort", },
		{ },
	};

	char *title;
	char *msg;
	char response;

	if(curr_ops->choose == NULL)
	{
		return IO_ECR_BREAK;
	}

	if(curr_ops->erp == ERP_IGNORE_ALL)
	{
		return IO_ECR_IGNORE;
	}

	title = format_str("Error while %s", curr_ops->descr);
	msg = format_str("%s: %s", replace_home_part(err->path), err->msg);

	response = prompt_user(args, title, msg, responses);

	free(msg);
	free(title);

	switch(response)
	{
		case 'r': return IO_ECR_RETRY;

		case 'I': curr_ops->erp = ERP_IGNORE_ALL; /* Fall through. */
		case 'i': return IO_ECR_IGNORE;

		case 'a': return IO_ECR_BREAK;

		default:
			assert(0 && "Unexpected response.");
			return 0;
	}
}

/* prompt_msg_custom() wrapper that takes care of interaction if cancellation is
 * active. */
static char
prompt_user(const io_args_t *args, const char title[], const char msg[],
		const response_variant variants[])
{
	/* Active cancellation conflicts with input processing by putting terminal in
	 * a cooked mode. */
	ui_cancellation_push_off();
	const char response = curr_ops->choose(title, msg, variants);
	ui_cancellation_pop();

	return response;
}

/* Implementation of cancellation hook for I/O unit. */
static int
ui_cancellation_hook(void *arg)
{
	return ui_cancellation_requested();
}

#ifndef _WIN32

/* Runs command in background and displays its errors to a user.  To determine
 * an error uses both stderr stream and exit status.  Returns status. */
static OpsResult
run_operation_command(ops_t *ops, char cmd[], int cancellable)
{
	if(!cancellable)
	{
		return result_from_code(bg_and_wait_for_errors(cmd, &no_cancellation));
	}

	if(ops != NULL && ops->bg)
	{
		const cancellation_t bg_cancellation_info = {
			.arg = ops->bg_op,
			.hook = &bg_cancellation_hook,
		};
		return result_from_code(bg_and_wait_for_errors(cmd, &bg_cancellation_info));
	}
	else
	{
		/* ui_cancellation_push_off() should be called outside this unit to allow
		 * bulking several operations together. */
		ui_cancellation_enable();
		OpsResult result =
			result_from_code(bg_and_wait_for_errors(cmd, &ui_cancellation_info));
		ui_cancellation_disable();
		return result;
	}
}

#endif

/* Implementation of cancellation hook for background tasks. */
static int
bg_cancellation_hook(void *arg)
{
	return bg_op_cancelled(arg);
}

/* Turns exit code into OpsResult.  Returns OpsResult. */
static OpsResult
result_from_code(int exit_code)
{
	return (exit_code == 0 ? OPS_SUCCEEDED : OPS_FAILED);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
