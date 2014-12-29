/* vifm
 * Copyright (C) 2013 xaizek.
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

#ifdef _WIN32
#define REQUIRED_WINVER 0x0600
#include "../utils/windefs.h"
#include <windows.h>
#endif

#include "iop.h"

#include <sys/stat.h> /* stat */
#include <sys/types.h> /* mode_t */
#include <unistd.h> /* rmdir() symlink() unlink() */

#include <errno.h> /* EEXIST errno */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE fpos_t fclose() fgetpos() fopen() fread() fseek()
                      fsetpos() fwrite() snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strchr() */

#include "../compat/os.h"
#include "../ui/cancellation.h"
#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../background.h"
#include "private/ioeta.h"
#include "ioc.h"

/* Amount of data to transfer at once. */
#define BLOCK_SIZE 32*1024

#ifdef _WIN32
static DWORD CALLBACK win_progress_cb(LARGE_INTEGER total,
		LARGE_INTEGER transferred, LARGE_INTEGER stream_size,
		LARGE_INTEGER stream_transfered, DWORD stream_num, DWORD reason,
		HANDLE src_file, HANDLE dst_file, LPVOID param);
#endif

int
iop_mkfile(io_args_t *const args)
{
	const char *const path = args->arg1.path;

#ifndef _WIN32
	FILE *const f = fopen(path, "wb");
	return (f == NULL) ? -1 : fclose(f);
#else
	HANDLE file;

	wchar_t *const utf16_path = utf8_to_utf16(path);
	file = CreateFileW(utf16_path, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
			NULL);
	free(utf16_path);

	if(file == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	CloseHandle(file);
	return 0;
#endif
}

int
iop_mkdir(io_args_t *const args)
{
	const char *const path = args->arg1.path;
	const int create_parent = args->arg2.process_parents;
	const mode_t mode = args->arg3.mode;

#ifndef _WIN32
	enum { PATH_PREFIX_LEN = 0 };
#else
	enum { PATH_PREFIX_LEN = 2 };
#endif

	if(create_parent)
	{
		char *const partial_path = strdup(path);
		char *part = partial_path + PATH_PREFIX_LEN, *state = NULL;

		while((part = split_and_get(part, '/', &state)) != NULL)
		{
			if(is_dir(partial_path))
			{
				continue;
			}

			/* Create intermediate directories with 0755 permissions. */
			if(make_dir(partial_path, 0755) != 0)
			{
				free(partial_path);
				return -1;
			}
		}

		free(partial_path);
#ifndef _WIN32
		return os_chmod(path, mode);
#else
		return 0;
#endif
	}

	return make_dir(path, mode);
}

int
iop_rmfile(io_args_t *const args)
{
	const char *const path = args->arg1.path;

	uint64_t size;
	int result;

	ioeta_update(args->estim, path, 0, 0);

	size = get_file_size(path);

#ifndef _WIN32
	result = unlink(path);
#else
	{
		wchar_t *const utf16_path = utf8_to_utf16(path);
		const DWORD attributes = GetFileAttributesW(utf16_path);
		if(attributes & FILE_ATTRIBUTE_READONLY)
		{
			SetFileAttributesW(utf16_path, attributes & ~FILE_ATTRIBUTE_READONLY);
		}
		result = !DeleteFileW(utf16_path);
		free(utf16_path);
	}
#endif

	ioeta_update(args->estim, path, 1, size);

	return result;
}

int
iop_rmdir(io_args_t *const args)
{
	const char *const path = args->arg1.path;

	int result;

	ioeta_update(args->estim, path, 0, 0);

#ifndef _WIN32
	result = rmdir(path);
#else
	{
		wchar_t *const utf16_path = utf8_to_utf16(path);
		result = RemoveDirectoryW(utf16_path) == 0;
		free(utf16_path);
	}
#endif

	ioeta_update(args->estim, path, 1, 0);

	return result;
}

int
iop_cp(io_args_t *const args)
{
	const char *const src = args->arg1.src;
	const char *const dst = args->arg2.dst;
	const IoCrs crs = args->arg3.crs;
	const int cancellable = args->cancellable;

	char block[BLOCK_SIZE];
	FILE *in, *out;
	size_t nread;
	int error;
	struct stat src_st;
	const char *open_mode = "wb";

	ioeta_update(args->estim, src, 0, 0);

#ifdef _WIN32
	if(is_symlink(src) || crs != IO_CRS_APPEND_TO_FILES)
	{
		DWORD flags;
		int error;
		wchar_t *utf16_src, *utf16_dst;

		flags = COPY_FILE_COPY_SYMLINK;
		if(crs == IO_CRS_FAIL)
		{
			flags |= COPY_FILE_FAIL_IF_EXISTS;
		}

		utf16_src = utf8_to_utf16(src);
		utf16_dst = utf8_to_utf16(dst);
		error = CopyFileExW(utf16_src, utf16_dst, &win_progress_cb, args, NULL,
				flags) == 0;
		free(utf16_src);
		free(utf16_dst);

		ioeta_update(args->estim, src, 1, 0);

		return error;
	}
#endif

	/* Create symbolic link rather than copying file it points to.  This check
	 * should go before directory check as is_dir() resolves symbolic links. */
	if(is_symlink(src))
	{
		char link_target[PATH_MAX];

		io_args_t args =
		{
			.arg1.path = link_target,
			.arg2.target = dst,
			.arg3.crs = crs,

			.cancellable = cancellable,
		};

		if(get_link_target(src, link_target, sizeof(link_target)) != 0)
		{
			return 1;
		}

		return iop_ln(&args);
	}

	if(is_dir(src))
	{
		return 1;
	}

	in = fopen(src, "rb");
	if(in == NULL)
	{
		return 1;
	}

	if(crs == IO_CRS_APPEND_TO_FILES)
	{
		open_mode = "ab";
	}
	else if(crs != IO_CRS_FAIL)
	{
		const int ec = unlink(dst);
		if(ec != 0 && errno != ENOENT)
		{
			fclose(in);
			return ec;
		}

		/* XXX: possible improvement would be to generate temporary file name in the
		 * destination directory, write to it and then overwrite destination file,
		 * but this approach has disadvantage of requiring more free space on
		 * destination file system. */
	}
	else if(path_exists(dst, DEREF))
	{
		fclose(in);
		return 1;
	}

	out = fopen(dst, open_mode);
	if(out == NULL)
	{
		fclose(in);
		return 1;
	}

	error = 0;

	if(crs == IO_CRS_APPEND_TO_FILES)
	{
		fpos_t pos;
		/* The following line is required for stupid Windows sometimes.  Why?
		 * Probably because it's stupid...  Won't harm other systems. */
		fseek(out, 0, SEEK_END);
		error = fgetpos(out, &pos) != 0 || fsetpos(in, &pos) != 0;

		if(!error)
		{
			ioeta_update(args->estim, src, 0, get_file_size(dst));
		}
	}

	/* TODO: use sendfile() if platform supports it. */

	while((nread = fread(&block, 1, sizeof(block), in)) != 0U)
	{
		if(cancellable && ui_cancellation_requested())
		{
			error = 1;
			break;
		}

		if(fwrite(&block, 1, nread, out) != nread)
		{
			error = 1;
			break;
		}

		ioeta_update(args->estim, src, 0, nread);
	}

	fclose(in);
	fclose(out);

	if(error == 0 && os_lstat(src, &src_st) == 0)
	{
		error = os_chmod(dst, src_st.st_mode & 07777);
	}

	ioeta_update(args->estim, src, 1, 0);

	return error;
}

#ifdef _WIN32

static DWORD CALLBACK win_progress_cb(LARGE_INTEGER total,
		LARGE_INTEGER transferred, LARGE_INTEGER stream_size,
		LARGE_INTEGER stream_transfered, DWORD stream_num, DWORD reason,
		HANDLE src_file, HANDLE dst_file, LPVOID param)
{
	static LONGLONG last_size;

	io_args_t *const args = param;

	const char *const src = args->arg1.src;
	ioeta_estim_t *const estim = args->estim;

	if(transferred.QuadPart < last_size)
	{
		last_size = 0;
	}

	ioeta_update(estim, src, 0, transferred.QuadPart - last_size);

	last_size = transferred.QuadPart;

	if(args->cancellable && ui_cancellation_requested())
	{
		return PROGRESS_CANCEL;
	}

	return PROGRESS_CONTINUE;
}

#endif

int iop_chown(io_args_t *const args);

int iop_chgrp(io_args_t *const args);

int iop_chmod(io_args_t *const args);

int
iop_ln(io_args_t *const args)
{
	const char *const path = args->arg1.path;
	const char *const target = args->arg2.target;
	const int overwrite = args->arg3.crs != IO_CRS_FAIL;

	int result;

#ifdef _WIN32
	char cmd[6 + PATH_MAX*2 + 1];
	char *escaped_path, *escaped_target;
	char base_dir[PATH_MAX + 2];
#endif

#ifndef _WIN32
	result = symlink(path, target);
	if(result != 0 && errno == EEXIST && overwrite && is_symlink(target))
	{
		result = remove(target);
		if(result == 0)
		{
			result = symlink(path, target);
		}
	}
#else
	if(!overwrite && path_exists(target, DEREF))
	{
		return -1;
	}

	if(overwrite && !is_symlink(target))
	{
		return -1;
	}

	escaped_path = escape_filename(path, 0);
	escaped_target = escape_filename(target, 0);
	if(escaped_path == NULL || escaped_target == NULL)
	{
		free(escaped_target);
		free(escaped_path);
		return -1;
	}

	if(GetModuleFileNameA(NULL, base_dir, ARRAY_LEN(base_dir)) == 0)
	{
		free(escaped_target);
		free(escaped_path);
		return -1;
	}

	break_atr(base_dir, '\\');
	snprintf(cmd, sizeof(cmd), "%s\\win_helper -s %s %s", base_dir, escaped_path,
			escaped_target);

	result = os_system(cmd);

	free(escaped_target);
	free(escaped_path);
#endif

	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
