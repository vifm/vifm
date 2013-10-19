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
#endif

#include <stddef.h> /* size_t */
#include <string.h>

#include "cfg/config.h"
#include "menus/menus.h"
#ifdef _WIN32
#include "utils/fs.h"
#endif
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "background.h"
#include "fileops.h"
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

static int op_none(void *data, const char *src, const char *dst);
static int op_remove(void *data, const char *src, const char *dst);
static int op_removesl(void *data, const char *src, const char *dst);
static int op_copy(void *data, const char src[], const char dst[]);
static int op_copyf(void *data, const char src[], const char dst[]);
static int op_cp(void *data, const char src[], const char dst[], int overwrite);
static int op_move(void *data, const char src[], const char dst[]);
static int op_movef(void *data, const char src[], const char dst[]);
static int op_mv(void *data, const char src[], const char dst[], int overwrite);
static int op_chown(void *data, const char *src, const char *dst);
static int op_chgrp(void *data, const char *src, const char *dst);
#ifndef _WIN32
static int op_chmod(void *data, const char *src, const char *dst);
static int op_chmodr(void *data, const char *src, const char *dst);
#else
static int op_addattr(void *data, const char *src, const char *dst);
static int op_subattr(void *data, const char *src, const char *dst);
#endif
static int op_symlink(void *data, const char *src, const char *dst);
static int op_mkdir(void *data, const char *src, const char *dst);
static int op_rmdir(void *data, const char *src, const char *dst);
static int op_mkfile(void *data, const char *src, const char *dst);

typedef int (*op_func)(void *data, const char *src, const char *dst);

static op_func op_funcs[] = {
	op_none,     /* OP_NONE */
	op_none,     /* OP_USR */
	op_remove,   /* OP_REMOVE */
	op_removesl, /* OP_REMOVESL */
	op_copy,     /* OP_COPY */
	op_copyf,    /* OP_COPYF */
	op_move,     /* OP_MOVE */
	op_movef,    /* OP_MOVEF */
	op_move,     /* OP_MOVETMP1 */
	op_move,     /* OP_MOVETMP2 */
	op_move,     /* OP_MOVETMP3 */
	op_move,     /* OP_MOVETMP4 */
	op_chown,    /* OP_CHOWN */
	op_chgrp,    /* OP_CHGRP */
#ifndef _WIN32
	op_chmod,    /* OP_CHMOD */
	op_chmodr,   /* OP_CHMODR */
#else
	op_addattr,  /* OP_ADDATTR */
	op_subattr,  /* OP_SUBATTR */
#endif
	op_symlink,  /* OP_SYMLINK */
	op_symlink,  /* OP_SYMLINK2 */
	op_mkdir,    /* OP_MKDIR */
	op_rmdir,    /* OP_RMDIR */
	op_mkfile,   /* OP_MKFILE */
};
ARRAY_GUARD(op_funcs, OP_COUNT);

int
perform_operation(OPS op, void *data, const char *src, const char *dst)
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
	if(cfg.confirm && !curr_stats.confirmed)
	{
		curr_stats.confirmed = query_user_menu("Permanent deletion",
				"Are you sure? If you undoing a command and want to see file names, "
				"use :undolist! command");
		if(!curr_stats.confirmed)
			return SKIP_UNDO_REDO_OPERATION;
	}

	return op_removesl(data, src, dst);
}

static int
op_removesl(void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char *escaped;
	char cmd[16 + PATH_MAX];
	int result;

	escaped = escape_filename(src, 0);
	if(escaped == NULL)
		return -1;

	snprintf(cmd, sizeof(cmd), "rm -rf %s", escaped);
	LOG_INFO_MSG("Running rm command: \"%s\"", cmd);
	result = background_and_wait_for_errors(cmd);

	free(escaped);
	return result;
#else
	if(is_dir(src))
	{
		char buf[PATH_MAX];
		int err;
		int i;
		snprintf(buf, sizeof(buf), "%s%c", src, '\0');
		for(i = 0; buf[i] != '\0'; i++)
			if(buf[i] == '/')
				buf[i] = '\\';
		SHFILEOPSTRUCTA fo = {
			.hwnd = NULL,
			.wFunc = FO_DELETE,
			.pFrom = buf,
			.pTo = NULL,
			.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI,
		};
		err = SHFileOperation(&fo);
		log_msg("Error: %d", err);
		return err;
	}
	else
	{
		int ok;
		DWORD attributes = GetFileAttributesA(src);
		if(attributes & FILE_ATTRIBUTE_READONLY)
			SetFileAttributesA(src, attributes & ~FILE_ATTRIBUTE_READONLY);
		ok = DeleteFile(src);
		if(!ok)
			LOG_WERROR(GetLastError());
		return !ok;
	}
#endif
}

/* OP_COPY operation handler.  Copies file/directory without overwriting
 * destination files (when it's supported by the system).  Returns non-zero on
 * error, otherwise zero is returned. */
static int
op_copy(void *data, const char src[], const char dst[])
{
	return op_cp(data, src, dst, 0);
}

/* OP_COPYF operation handler.  Copies file/directory overwriting destination
 * files.  Returns non-zero on error, otherwise zero is returned. */
static int
op_copyf(void *data, const char src[], const char dst[])
{
	return op_cp(data, src, dst, 1);
}

/* Copies file/directory overwriting destination files if requested.  Returns
 * non-zero on error, otherwise zero is returned. */
static int
op_cp(void *data, const char src[], const char dst[], int overwrite)
{
#ifndef _WIN32
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

	snprintf(cmd, sizeof(cmd),
			"cp %s -R " PRESERVE_FLAGS " %s %s",
			overwrite ? "" : NO_CLOBBER, escaped_src, escaped_dst);
	LOG_INFO_MSG("Running cp command: \"%s\"", cmd);
	result = background_and_wait_for_errors(cmd);

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
		if(overwrite)
		{
			strcat(cmd, "/Y ");
		}
		strcat(cmd, "/E /I /H /R > NUL");
		ret = system(cmd);
	}
	else
	{
		ret = (CopyFileA(src, dst, 0) == 0);
	}

	return ret;
#endif
}

/* OP_MOVE operation handler.  Moves file/directory without overwriting
 * destination files (when it's supported by the system).  Returns non-zero on
 * error, otherwise zero is returned. */
static int
op_move(void *data, const char src[], const char dst[])
{
	return op_mv(data, src, dst, 0);
}

/* OP_MOVEF operation handler.  Moves file/directory overwriting destination
 * files.  Returns non-zero on error, otherwise zero is returned. */
static int
op_movef(void *data, const char src[], const char dst[])
{
	return op_mv(data, src, dst, 1);
}

/* Moves file/directory overwriting destination files if requested.  Returns
 * non-zero on error, otherwise zero is returned. */
static int
op_mv(void *data, const char src[], const char dst[], int overwrite)
{
#ifndef _WIN32
	struct stat st;
	char *escaped_src, *escaped_dst;
	char cmd[6 + PATH_MAX*2 + 1];
	int result;

	if(lstat(dst, &st) == 0)
		return -1;

	escaped_src = escape_filename(src, 0);
	escaped_dst = escape_filename(dst, 0);
	if(escaped_src == NULL || escaped_dst == NULL)
	{
		free(escaped_dst);
		free(escaped_src);
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "mv %s %s %s", overwrite ? "" : NO_CLOBBER,
			escaped_src, escaped_dst);
	free(escaped_dst);
	free(escaped_src);

	LOG_INFO_MSG("Running mv command: \"%s\"", cmd);
	if((result = background_and_wait_for_errors(cmd)) != 0)
		return result;

	if(path_starts_with(dst, cfg.trash_dir))
		add_to_trash(src, strrchr(dst, '/') + 1);
	else if(path_starts_with(src, cfg.trash_dir))
		remove_from_trash(strrchr(src, '/') + 1);
	return 0;
#else
	BOOL ret = MoveFile(src, dst);
	if(!ret && GetLastError() == 5)
	{
		int r = op_cp(data, src, dst, overwrite);
		if(r != 0)
			return r;
		return op_removesl(data, src, NULL);
	}
	return ret == 0;
#endif
}

static int
op_chown(void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char cmd[10 + 32 + PATH_MAX];
	char *escaped;
	uid_t uid = (uid_t)(long)data;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "chown -fR %u %s", uid, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chown command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd);
#else
	return -1;
#endif
}

static int
op_chgrp(void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char cmd[10 + 32 + PATH_MAX];
	char *escaped;
	gid_t gid = (gid_t)(long)data;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "chown -fR :%u %s", gid, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chgrp command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd);
#else
	return -1;
#endif
}

#ifndef _WIN32
static int
op_chmod(void *data, const char *src, const char *dst)
{
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "chmod %s %s", (char *)data, escaped);
	free(escaped);

	LOG_INFO_MSG("Running chmod command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd);
}

static int
op_chmodr(void *data, const char *src, const char *dst)
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
op_addattr(void *data, const char *src, const char *dst)
{
	const DWORD add_mask = (size_t)data;
	const DWORD attrs = GetFileAttributesA(src);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}
	if(!SetFileAttributesA(src, attrs | add_mask))
	{
		LOG_WERROR(GetLastError());
		return -1;
	}
	return 0;
}

static int
op_subattr(void *data, const char *src, const char *dst)
{
	const DWORD sub_mask = (size_t)data;
	const DWORD attrs = GetFileAttributesA(src);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}
	if(!SetFileAttributesA(src, attrs & ~sub_mask))
	{
		LOG_WERROR(GetLastError());
		return -1;
	}
	return 0;
}
#endif

static int
op_symlink(void *data, const char *src, const char *dst)
{
	char *escaped_src, *escaped_dst;
	char cmd[6 + PATH_MAX*2 + 1];
	int result;
#ifdef _WIN32
	char buf[PATH_MAX + 2];
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
	result = background_and_wait_for_errors(cmd);
#else
	if(GetModuleFileNameA(NULL, buf, ARRAY_LEN(buf)) == 0)
	{
		free(escaped_dst);
		free(escaped_src);
		return -1;
	}

	*strrchr(buf, '\\') = '\0';
	snprintf(cmd, sizeof(cmd), "%s\\win_helper -s %s %s", buf, escaped_src,
			escaped_dst);
	result = system(cmd);
#endif

	free(escaped_dst);
	free(escaped_src);
	return result;
}

static int
op_mkdir(void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "mkdir %s %s", (data == NULL) ? "" : "-p",
			escaped);
	free(escaped);
	LOG_INFO_MSG("Running mkdir command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd);
#else
	if(data == NULL)
	{
		return CreateDirectory(src, NULL) == 0;
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
				if(!CreateDirectory(src, NULL))
				{
					*p = t;
					return -1;
				}
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

static int
op_rmdir(void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "rmdir %s", escaped);
	free(escaped);
	LOG_INFO_MSG("Running rmdir command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd);
#else
	return RemoveDirectory(src) == 0;
#endif
}

static int
op_mkfile(void *data, const char *src, const char *dst)
{
#ifndef _WIN32
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(src, 0);
	snprintf(cmd, sizeof(cmd), "touch %s", escaped);
	free(escaped);
	LOG_INFO_MSG("Running touch command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd);
#else
	HANDLE hfile;

	hfile = CreateFileA(src, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hfile == INVALID_HANDLE_VALUE)
		return -1;

	CloseHandle(hfile);
	return 0;
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
