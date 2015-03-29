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

#include <curses.h> /* noraw() raw() */

#include <sys/stat.h> /* gid_t uid_t */

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* strdup() */

#include "cfg/config.h"
#include "compat/os.h"
#include "io/ioeta.h"
#include "io/iop.h"
#include "io/ior.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/cancellation.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "background.h"
#include "status.h"
#include "trash.h"
#include "undo.h"

#ifdef SUPPORT_NO_CLOBBER
#define NO_CLOBBER "-n"
#else /* SUPPORT_NO_CLOBBER */
#define NO_CLOBBER ""
#endif /* SUPPORT_NO_CLOBBER */

#ifdef GNU_TOOLCHAIN
#define PRESERVE_FLAGS "--preserve=mode,timestamps"
#else
#define PRESERVE_FLAGS "-p"
#endif

/* Types of conflict resolution actions to perform. */
typedef enum
{
	CA_FAIL,      /* Fail with an error. */
	CA_OVERWRITE, /* Overwrite existing files. */
	CA_APPEND,    /* Append the rest of source file to destination file. */
}
ConflictAction;

/* Type of function that implements single operation. */
typedef int (*op_func)(ops_t *ops, void *data, const char *src, const char *dst);

static int op_none(ops_t *ops, void *data, const char *src, const char *dst);
static int op_remove(ops_t *ops, void *data, const char *src, const char *dst);
static int op_removesl(ops_t *ops, void *data, const char *src,
		const char *dst);
static int op_copy(ops_t *ops, void *data, const char src[], const char dst[]);
static int op_copyf(ops_t *ops, void *data, const char src[], const char dst[]);
static int op_copya(ops_t *ops, void *data, const char src[], const char dst[]);
static int op_cp(ops_t *ops, void *data, const char src[], const char dst[],
		ConflictAction conflict_action);
static int op_move(ops_t *ops, void *data, const char src[], const char dst[]);
static int op_movef(ops_t *ops, void *data, const char src[], const char dst[]);
static int op_movea(ops_t *ops, void *data, const char src[], const char dst[]);
static int op_mv(ops_t *ops, void *data, const char src[], const char dst[],
		ConflictAction conflict_action);
static IoCrs ca_to_crs(ConflictAction conflict_action);
static int op_chown(ops_t *ops, void *data, const char *src, const char *dst);
static int op_chgrp(ops_t *ops, void *data, const char *src, const char *dst);
#ifndef _WIN32
static int op_chmod(ops_t *ops, void *data, const char *src, const char *dst);
static int op_chmodr(ops_t *ops, void *data, const char *src, const char *dst);
#else
static int op_addattr(ops_t *ops, void *data, const char *src, const char *dst);
static int op_subattr(ops_t *ops, void *data, const char *src, const char *dst);
#endif
static int op_symlink(ops_t *ops, void *data, const char *src, const char *dst);
static int op_mkdir(ops_t *ops, void *data, const char *src, const char *dst);
static int op_rmdir(ops_t *ops, void *data, const char *src, const char *dst);
static int op_mkfile(ops_t *ops, void *data, const char *src, const char *dst);
static int exec_io_op(ops_t *ops, int (*func)(io_args_t *const),
		io_args_t *const args);
static int confirm_overwrite(io_args_t *args, const char src[],
		const char dst[]);
static char * pretty_dir_path(const char path[]);

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
ops_alloc(OPS main_op, const char descr[], const char base_dir[],
		const char target_dir[])
{
	ops_t *const ops = calloc(1, sizeof(*ops));
	ops->main_op = main_op;
	ops->descr = descr;
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

		if(is_on_slow_fs(src))
		{
			ops->shallow_eta = 1;
		}
	}

	ui_cancellation_enable();
	ioeta_calculate(ops->estim, src, ops->shallow_eta);
	ui_cancellation_disable();
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
	free(ops->base_dir);
	free(ops->target_dir);
	free(ops);
}

int
perform_operation(OPS op, ops_t *ops, void *data, const char src[],
		const char dst[])
{
	return op_funcs[op](ops, data, src, dst);
}

static int
op_none(ops_t *ops, void *data, const char *src, const char *dst)
{
	return 0;
}

static int
op_remove(ops_t *ops, void *data, const char *src, const char *dst)
{
	if(cfg.confirm && !curr_stats.confirmed)
	{
		curr_stats.confirmed = prompt_msg("Permanent deletion",
				"Are you sure? If you undoing a command and want to see file names, "
				"use :undolist! command");
		if(!curr_stats.confirmed)
			return SKIP_UNDO_REDO_OPERATION;
	}

	return op_removesl(ops, data, src, dst);
}

static int
op_removesl(ops_t *ops, void *data, const char *src, const char *dst)
{
	if(!cfg.use_system_calls)
	{
#ifndef _WIN32
		char *escaped;
		char cmd[16 + PATH_MAX];
		int result;
		const int cancellable = data == NULL;

		escaped = escape_filename(src, 0);
		if(escaped == NULL)
			return -1;

		snprintf(cmd, sizeof(cmd), "rm -rf %s", escaped);
		LOG_INFO_MSG("Running rm command: \"%s\"", cmd);
		result = background_and_wait_for_errors(cmd, cancellable);

		free(escaped);
		return result;
#else
		if(is_dir(src))
		{
			char path[PATH_MAX];
			int err;

			copy_str(path, sizeof(path), src);
			to_back_slash(path);

			wchar_t *const utf16_path = utf8_to_utf16(path);

			SHFILEOPSTRUCTW fo =
			{
				.hwnd = NULL,
				.wFunc = FO_DELETE,
				.pFrom = utf16_path,
				.pTo = NULL,
				.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI,
			};
			err = SHFileOperationW(&fo);

			log_msg("Error: %d", err);
			free(utf16_path);

			return err;
		}
		else
		{
			int ok;
			wchar_t *const utf16_path = utf8_to_utf16(src);
			DWORD attributes = GetFileAttributesW(utf16_path);
			if(attributes & FILE_ATTRIBUTE_READONLY)
			{
				SetFileAttributesW(utf16_path, attributes & ~FILE_ATTRIBUTE_READONLY);
			}

			ok = DeleteFileW(utf16_path);
			if(!ok)
			{
				LOG_WERROR(GetLastError());
			}

			free(utf16_path);
			return !ok;
		}
#endif
	}

	io_args_t args =
	{
		.arg1.path = src,

		.cancellable = data == NULL,
	};
	return exec_io_op(ops, &ior_rm, &args);
}

/* OP_COPY operation handler.  Copies file/directory without overwriting
 * destination files (when it's supported by the system).  Returns non-zero on
 * error, otherwise zero is returned. */
static int
op_copy(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_cp(ops, data, src, dst, CA_FAIL);
}

/* OP_COPYF operation handler.  Copies file/directory overwriting destination
 * files.  Returns non-zero on error, otherwise zero is returned. */
static int
op_copyf(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_cp(ops, data, src, dst, CA_OVERWRITE);
}

/* OP_COPYA operation handler.  Copies file appending rest of the source file to
 * the destination.  Returns non-zero on error, otherwise zero is returned. */
static int
op_copya(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_cp(ops, data, src, dst, CA_APPEND);
}

/* Copies file/directory overwriting/appending destination files if requested.
 * Returns non-zero on error, otherwise zero is returned. */
static int
op_cp(ops_t *ops, void *data, const char src[], const char dst[],
		ConflictAction conflict_action)
{
	if(!cfg.use_system_calls)
	{
#ifndef _WIN32
		char *escaped_src, *escaped_dst;
		char cmd[6 + PATH_MAX*2 + 1];
		int result;
		const int cancellable = data == NULL;

		escaped_src = escape_filename(src, 0);
		escaped_dst = escape_filename(dst, 0);
		if(escaped_src == NULL || escaped_dst == NULL)
		{
			free(escaped_dst);
			free(escaped_src);
			return -1;
		}

		snprintf(cmd, sizeof(cmd),
				"cp %s -R " PRESERVE_FLAGS " %s %s",
				(conflict_action == CA_FAIL) ? NO_CLOBBER : "",
				escaped_src, escaped_dst);
		LOG_INFO_MSG("Running cp command: \"%s\"", cmd);
		result = background_and_wait_for_errors(cmd, cancellable);

		free(escaped_dst);
		free(escaped_src);
		return result;
#else
		int ret;

		if(is_dir(src))
		{
			char cmd[6 + PATH_MAX*2 + 1];
			snprintf(cmd, sizeof(cmd), "xcopy \"%s\" \"%s\" ", src, dst);
			to_back_slash(cmd);

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

		return ret;
#endif
	}

	io_args_t args =
	{
		.arg1.src = src,
		.arg2.dst = dst,
		.arg3.crs = ca_to_crs(conflict_action),

		.cancellable = data == NULL,
	};
	return exec_io_op(ops, &ior_cp, &args);
}

/* OP_MOVE operation handler.  Moves file/directory without overwriting
 * destination files (when it's supported by the system).  Returns non-zero on
 * error, otherwise zero is returned. */
static int
op_move(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_mv(ops, data, src, dst, CA_FAIL);
}

/* OP_MOVEF operation handler.  Moves file/directory overwriting destination
 * files.  Returns non-zero on error, otherwise zero is returned. */
static int
op_movef(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_mv(ops, data, src, dst, CA_OVERWRITE);
}

/* OP_MOVEA operation handler.  Moves file appending rest of the source file to
 * the destination.  Returns non-zero on error, otherwise zero is returned. */
static int
op_movea(ops_t *ops, void *data, const char src[], const char dst[])
{
	return op_mv(ops, data, src, dst, CA_APPEND);
}

/* Moves file/directory overwriting/appending destination files if requested.
 * Returns non-zero on error, otherwise zero is returned. */
static int
op_mv(ops_t *ops, void *data, const char src[], const char dst[],
		ConflictAction conflict_action)
{
	int result;

	if(!cfg.use_system_calls)
	{
#ifndef _WIN32
		struct stat st;
		char *escaped_src, *escaped_dst;
		char cmd[6 + PATH_MAX*2 + 1];
		const int cancellable = data == NULL;

		if(conflict_action == CA_FAIL && os_lstat(dst, &st) == 0)
		{
			return -1;
		}

		escaped_src = escape_filename(src, 0);
		escaped_dst = escape_filename(dst, 0);
		if(escaped_src == NULL || escaped_dst == NULL)
		{
			free(escaped_dst);
			free(escaped_src);
			return -1;
		}

		snprintf(cmd, sizeof(cmd), "mv %s %s %s",
				(conflict_action == CA_FAIL) ? NO_CLOBBER : "",
				escaped_src, escaped_dst);
		free(escaped_dst);
		free(escaped_src);

		LOG_INFO_MSG("Running mv command: \"%s\"", cmd);
		result = background_and_wait_for_errors(cmd, cancellable);
		if(result != 0)
		{
			return result;
		}
#else
		wchar_t *const utf16_src = utf8_to_utf16(src);
		wchar_t *const utf16_dst = utf8_to_utf16(dst);

		BOOL ret = MoveFileW(utf16_src, utf16_dst);

		free(utf16_src);
		free(utf16_dst);

		if(!ret && GetLastError() == 5)
		{
			const int r = op_cp(ops, data, src, dst, conflict_action);
			if(r != 0)
			{
				return r;
			}
			return op_removesl(ops, data, src, NULL);
		}
		result = (ret == 0);
#endif
	}
	else
	{
		io_args_t args =
		{
			.arg1.src = src,
			.arg2.dst = dst,
			.arg3.crs = ca_to_crs(conflict_action),

			.cancellable = data == NULL,
		};
		result = exec_io_op(ops, &ior_mv, &args);
	}

	if(result == 0)
	{
		if(is_under_trash(dst))
		{
			add_to_trash(src, dst);
		}
		else if(is_under_trash(src))
		{
			remove_from_trash(src);
		}
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

static int
op_chown(ops_t *ops, void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char cmd[10 + 32 + PATH_MAX];
	char *escaped;
	uid_t uid = (uid_t)(long)data;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "chown -fR %u %s", uid, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chown command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd, 1);
#else
	return -1;
#endif
}

static int
op_chgrp(ops_t *ops, void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char cmd[10 + 32 + PATH_MAX];
	char *escaped;
	gid_t gid = (gid_t)(long)data;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "chown -fR :%u %s", gid, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chgrp command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd, 1);
#else
	return -1;
#endif
}

#ifndef _WIN32
static int
op_chmod(ops_t *ops, void *data, const char *src, const char *dst)
{
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "chmod %s %s", (char *)data, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chmod command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd, 1);
}

static int
op_chmodr(ops_t *ops, void *data, const char *src, const char *dst)
{
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "chmod -R %s %s", (char *)data, escaped);
	free(escaped);
	start_background_job(cmd, 0);
	return 0;
}
#else
static int
op_addattr(ops_t *ops, void *data, const char *src, const char *dst)
{
	const DWORD add_mask = (size_t)data;
	wchar_t *const utf16_path = utf8_to_utf16(src);
	const DWORD attrs = GetFileAttributesW(utf16_path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return -1;
	}
	if(!SetFileAttributesW(utf16_path, attrs | add_mask))
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return -1;
	}
	free(utf16_path);
	return 0;
}

static int
op_subattr(ops_t *ops, void *data, const char *src, const char *dst)
{
	const DWORD sub_mask = (size_t)data;
	wchar_t *const utf16_path = utf8_to_utf16(src);
	const DWORD attrs = GetFileAttributesW(utf16_path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return -1;
	}
	if(!SetFileAttributesW(utf16_path, attrs & ~sub_mask))
	{
		free(utf16_path);
		LOG_WERROR(GetLastError());
		return -1;
	}
	free(utf16_path);
	return 0;
}
#endif

static int
op_symlink(ops_t *ops, void *data, const char *src, const char *dst)
{
	if(!cfg.use_system_calls)
	{
		char *escaped_src, *escaped_dst;
		char cmd[6 + PATH_MAX*2 + 1];
		int result;
#ifdef _WIN32
		char exe_dir[PATH_MAX + 2];
#endif

		escaped_src = escape_filename(src, 0);
		escaped_dst = escape_filename(dst, 0);
		if(escaped_src == NULL || escaped_dst == NULL)
		{
			free(escaped_dst);
			free(escaped_src);
			return -1;
		}

#ifndef _WIN32
		snprintf(cmd, sizeof(cmd), "ln -s %s %s", escaped_src, escaped_dst);
		LOG_INFO_MSG("Running ln command: \"%s\"", cmd);
		result = background_and_wait_for_errors(cmd, 1);
#else
		if(get_exe_dir(exe_dir, ARRAY_LEN(exe_dir)) != 0)
		{
			free(escaped_dst);
			free(escaped_src);
			return -1;
		}

		snprintf(cmd, sizeof(cmd), "%s\\win_helper -s %s %s", exe_dir, escaped_src,
				escaped_dst);
		result = os_system(cmd);
#endif

		free(escaped_dst);
		free(escaped_src);
		return result;
	}

	io_args_t args =
	{
		.arg1.path = src,
		.arg2.target = dst,
		.arg3.crs = IO_CRS_REPLACE_FILES,
	};
	return exec_io_op(ops, &iop_ln, &args);
}

static int
op_mkdir(ops_t *ops, void *data, const char *src, const char *dst)
{
	if(!cfg.use_system_calls)
	{
#ifndef _WIN32
		char cmd[128 + PATH_MAX];
		char *escaped;

		escaped = escape_filename(src, 0);
		snprintf(cmd, sizeof(cmd), "mkdir %s %s", (data == NULL) ? "" : "-p",
				escaped);
		free(escaped);
		LOG_INFO_MSG("Running mkdir command: \"%s\"", cmd);
		return background_and_wait_for_errors(cmd, 1);
#else
		if(data == NULL)
		{
			wchar_t *const utf16_path = utf8_to_utf16(src);
			int r = CreateDirectoryW(utf16_path, NULL) == 0;
			free(utf16_path);
			return r;
		}
		else
		{
			char *p;
			char t;

			p = strchr(src + 2, '/');
			do
			{
				t = *p;
				*p = '\0';

				if(!is_dir(src))
				{
					wchar_t *const utf16_path = utf8_to_utf16(src);
					if(!CreateDirectoryW(utf16_path, NULL))
					{
						free(utf16_path);
						*p = t;
						return -1;
					}
					free(utf16_path);
				}

				*p = t;
				if((p = strchr(p + 1, '/')) == NULL)
					p = (char *)src + strlen(src);
			}
			while(t != '\0');
			return 0;
		}
#endif
	}

	io_args_t args =
	{
		.arg1.path = src,
		.arg2.process_parents = data != NULL,
		.arg3.mode = 0755,
	};
	return exec_io_op(ops, &iop_mkdir, &args);
}

static int
op_rmdir(ops_t *ops, void *data, const char *src, const char *dst)
{
	if(!cfg.use_system_calls)
	{
#ifndef _WIN32
		char cmd[128 + PATH_MAX];
		char *escaped;

		escaped = escape_filename(src, 0);
		snprintf(cmd, sizeof(cmd), "rmdir %s", escaped);
		free(escaped);
		LOG_INFO_MSG("Running rmdir command: \"%s\"", cmd);
		return background_and_wait_for_errors(cmd, 1);
#else
		wchar_t *const utf16_path = utf8_to_utf16(src);
		int r = RemoveDirectoryW(utf16_path);
		free(utf16_path);
		return r;
#endif
	}

	io_args_t args =
	{
		.arg1.path = src,
	};
	return exec_io_op(ops, &iop_rmdir, &args);
}

static int
op_mkfile(ops_t *ops, void *data, const char *src, const char *dst)
{
	if(!cfg.use_system_calls)
	{
#ifndef _WIN32
		char cmd[128 + PATH_MAX];
		char *escaped;

		escaped = escape_filename(src, 0);
		snprintf(cmd, sizeof(cmd), "touch %s", escaped);
		free(escaped);
		LOG_INFO_MSG("Running touch command: \"%s\"", cmd);
		return background_and_wait_for_errors(cmd, 1);
#else
		HANDLE hfile;

		wchar_t *const utf16_path = utf8_to_utf16(src);
		hfile = CreateFileW(utf16_path, 0, 0, NULL, CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL, NULL);
		free(utf16_path);
		if(hfile == INVALID_HANDLE_VALUE)
		{
			return -1;
		}

		CloseHandle(hfile);
		return 0;
#endif
	}

	io_args_t args =
	{
		.arg1.path = src,
	};
	return exec_io_op(ops, &iop_mkfile, &args);
}

/* Executes i/o operation with some predefined pre/post actions.  Returns exit
 * code of i/o operation. */
static int
exec_io_op(ops_t *ops, int (*func)(io_args_t *const), io_args_t *const args)
{
	int result;

	args->estim = (ops == NULL) ? NULL : ops->estim;
	args->confirm = &confirm_overwrite;

	if(args->cancellable)
	{
		ui_cancellation_enable();
	}

	curr_ops = ops;
	result = func(args);
	curr_ops = NULL;

	if(args->cancellable)
	{
		ui_cancellation_disable();
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

	msg = format_str("Overwrite \"%s\" in\n%s\nwith \"%s\" from\n%s\n?", fname,
			dst_dir, fname, src_dir);

	free(dst_dir);
	free(src_dir);

	/* Active cancellation conflicts with input processing by putting terminal in
	 * a cocked mode. */
	if(args->cancellable)
	{
		raw();
	}
	response = prompt_msg_custom("File overwrite", msg, responses);
	if(args->cancellable)
	{
		noraw();
	}

	free(msg);

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
	char canonic[PATH_MAX];

	copy_str(dir_only, sizeof(dir_only), path);
	remove_last_path_component(dir_only);
	canonicalize_path(dir_only, canonic, sizeof(canonic));

	return strdup(canonic);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
