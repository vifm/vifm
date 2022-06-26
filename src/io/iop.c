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
#include <windows.h>
#endif

#include "iop.h"

#ifndef _WIN32
#include <sys/ioctl.h> /* ioctl() */
#endif
#include <sys/stat.h> /* stat */
#include <sys/types.h> /* mode_t */
#include <unistd.h> /* symlink() unlink() */

#include <assert.h> /* assert() */
#include <errno.h> /* EEXIST ENOENT EISDIR errno */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE fpos_t fclose() fgetpos() fflush() fread() fseek()
                      fsetpos() fwrite() snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strchr() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "private/ioc.h"
#include "private/ioe.h"
#include "private/ioeta.h"
#include "ioc.h"

/* Amount of data to transfer at once. */
#define BLOCK_SIZE 32*1024

/* Amount of data after which data flush should be performed. */
#define FLUSH_SIZE 256*1024*1024

/* Type of io function used by retry_wrapper(). */
typedef IoRes (*iop_func)(io_args_t *args);

static IoRes iop_mkfile_internal(io_args_t *args);
static IoRes iop_mkdir_internal(io_args_t *args);
static IoRes iop_rmfile_internal(io_args_t *args);
static IoRes iop_rmdir_internal(io_args_t *args);
static IoRes iop_cp_internal(io_args_t *args);
static int clone_file(int dst_fd, int src_fd);
#ifdef _WIN32
static DWORD CALLBACK win_progress_cb(LARGE_INTEGER total,
		LARGE_INTEGER transferred, LARGE_INTEGER stream_size,
		LARGE_INTEGER stream_transfered, DWORD stream_num, DWORD reason,
		HANDLE src_file, HANDLE dst_file, LPVOID param);
#endif
static IoRes iop_ln_internal(io_args_t *args);
static IoRes retry_wrapper(iop_func func, io_args_t *args);
static IoRes io_res_from_code(int code);

IoRes
iop_mkfile(io_args_t *args)
{
	return retry_wrapper(&iop_mkfile_internal, args);
}

/* Implementation of iop_mkfile(). */
static IoRes
iop_mkfile_internal(io_args_t *args)
{
	FILE *f;
	const char *const path = args->arg1.path;

	ioeta_update(args->estim, path, path, /*finished=*/0, /*size=*/0);

	if(path_exists(path, NODEREF))
	{
		(void)ioe_errlst_append(&args->result.errors, path, EEXIST,
				"Such file already exists");
		return IO_RES_FAILED;
	}

	f = os_fopen(path, "wb");
	if(f == NULL)
	{
		(void)ioe_errlst_append(&args->result.errors, path, errno,
				"Failed to open file for writing");
		return IO_RES_FAILED;
	}

	if(fclose(f) != 0)
	{
		(void)ioe_errlst_append(&args->result.errors, path, errno,
				"Error while closing the file");
		return IO_RES_FAILED;
	}

	ioeta_update(args->estim, path, path, /*finished=*/1, /*size=*/0);

	return IO_RES_SUCCEEDED;
}

IoRes
iop_mkdir(io_args_t *args)
{
	return retry_wrapper(&iop_mkdir_internal, args);
}

/* Implementation of iop_mkdir(). */
static IoRes
iop_mkdir_internal(io_args_t *args)
{
	const char *const path = args->arg1.path;
	const int create_parent = args->arg2.process_parents;
	const mode_t mode = args->arg3.mode;

#ifndef _WIN32
	enum { PATH_PREFIX_LEN = 0 };
#else
	enum { PATH_PREFIX_LEN = 2 };
#endif

	ioeta_update(args->estim, path, path, /*finished=*/0, /*size=*/0);

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

			/* Create intermediate directories with 0700 permissions. */
			if(os_mkdir(partial_path, S_IRWXU) != 0)
			{
				(void)ioe_errlst_append(&args->result.errors, partial_path, errno,
						"Failed to create one of intermediate directories");

				free(partial_path);
				return IO_RES_FAILED;
			}
		}

#ifndef _WIN32
		if(os_chmod(path, mode) != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, partial_path, errno,
					"Failed to setup directory permissions");
			free(partial_path);
			return IO_RES_FAILED;
		}
#endif

		free(partial_path);
		return IO_RES_SUCCEEDED;
	}

	if(os_mkdir(path, mode) != 0)
	{
		(void)ioe_errlst_append(&args->result.errors, path, errno,
				"Failed to create directory");
		return IO_RES_FAILED;
	}

	ioeta_update(args->estim, path, path, /*finished=*/1, /*size=*/0);

	return IO_RES_SUCCEEDED;
}

IoRes
iop_rmfile(io_args_t *args)
{
	return retry_wrapper(&iop_rmfile_internal, args);
}

/* Implementation of iop_rmfile(). */
static IoRes
iop_rmfile_internal(io_args_t *args)
{
	const char *const path = args->arg1.path;

	uint64_t size;
	IoRes result;

	ioeta_update(args->estim, path, path, 0, 0);

	size = get_file_size(path);

#ifndef _WIN32
	result = io_res_from_code(unlink(path));
	if(result == IO_RES_FAILED)
	{
		(void)ioe_errlst_append(&args->result.errors, path, errno,
				"Failed to unlink file");
	}
#else
	{
		wchar_t *const utf16_path = utf8_to_utf16(path);
		const DWORD attributes = GetFileAttributesW(utf16_path);

		if(attributes & FILE_ATTRIBUTE_READONLY)
		{
			SetFileAttributesW(utf16_path, attributes & ~FILE_ATTRIBUTE_READONLY);
		}
		result = io_res_from_code(!DeleteFileW(utf16_path));

		if(result != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, path, IO_ERR_UNKNOWN,
					"File removal failed");
		}

		free(utf16_path);
	}
#endif

	ioeta_update(args->estim, NULL, NULL, 1, size);

	return result;
}

IoRes
iop_rmdir(io_args_t *args)
{
	return retry_wrapper(&iop_rmdir_internal, args);
}

/* Implementation of iop_rmdir(). */
static IoRes
iop_rmdir_internal(io_args_t *args)
{
	const char *const path = args->arg1.path;

	IoRes result;

	ioeta_update(args->estim, path, path, 0, 0);

#ifndef _WIN32
	result = io_res_from_code(os_rmdir(path));
	if(result != 0)
	{
		(void)ioe_errlst_append(&args->result.errors, path, errno,
				"Failed to remove directory");
	}
#else
	{
		wchar_t *const utf16_path = utf8_to_utf16(path);

		result = io_res_from_code(!RemoveDirectoryW(utf16_path));
		if(result != 0)
		{
			/* FIXME: use real system error message here. */
			(void)ioe_errlst_append(&args->result.errors, path, IO_ERR_UNKNOWN,
					"Failed to remove directory");
		}

		free(utf16_path);
	}
#endif

	ioeta_update(args->estim, NULL, NULL, 1, 0);

	return result;
}

IoRes
iop_cp(io_args_t *args)
{
	return retry_wrapper(&iop_cp_internal, args);
}

/* Implementation of iop_cp(). */
static IoRes
iop_cp_internal(io_args_t *args)
{
	const char *const src = args->arg1.src;
	const char *const dst = args->arg2.dst;
	const IoCrs crs = args->arg3.crs;
	const io_confirm confirm = args->confirm;
	struct stat st;

	FILE *in, *out;
	int error;
	int cloned;
	struct stat src_st;
	const char *open_mode = "wb";

	uint64_t orig_out_size = 0U;
	int correct_out_size = 0;

	ioeta_update(args->estim, src, dst, 0, 0);

#ifdef _WIN32
	if(is_symlink(src) || crs != IO_CRS_APPEND_TO_FILES)
	{
		DWORD flags;
		wchar_t *utf16_src, *utf16_dst;

		flags = COPY_FILE_COPY_SYMLINK;
		if(crs == IO_CRS_FAIL)
		{
			flags |= COPY_FILE_FAIL_IF_EXISTS;
		}
		else if(path_exists(dst, NODEREF))
		{
			/* Ask user whether to overwrite destination file. */
			if(confirm != NULL && !confirm(args, src, dst))
			{
				return IO_RES_SUCCEEDED;
			}
		}

		utf16_src = utf8_to_utf16(src);
		utf16_dst = utf8_to_utf16(dst);

		int success =
			CopyFileExW(utf16_src, utf16_dst, &win_progress_cb, args, NULL, flags);

		if(!success)
		{
			/* FIXME: use real system error message here. */
			(void)ioe_errlst_append(&args->result.errors, dst, IO_ERR_UNKNOWN,
					"Copy file failed");
		}

		free(utf16_src);
		free(utf16_dst);

		if(success)
		{
			clone_attribs(dst, src, NULL);
		}

		ioeta_update(args->estim, NULL, NULL, 1, 0);

		return io_res_from_code(!success);
	}
#endif

	/* Create symbolic link rather than copying file it points to.  This check
	 * should go before directory check as is_dir() resolves symbolic links. */
	if(is_symlink(src))
	{
		char link_target[PATH_MAX + 1];

		io_args_t ln_args = {
			.arg1.path = link_target,
			.arg2.target = dst,
			.arg3.crs = crs,

			.cancellation = args->cancellation,

			.result = args->result,
		};

		if(get_link_target(src, link_target, sizeof(link_target)) != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, src, IO_ERR_UNKNOWN,
					"Failed to get symbolic link target");
			return IO_RES_FAILED;
		}

		IoRes result = iop_ln(&ln_args);
		args->result = ln_args.result;

		if(result == IO_RES_FAILED)
		{
			(void)ioe_errlst_append(&args->result.errors, src, IO_ERR_UNKNOWN,
					"Failed to make symbolic link");
		}
		return IO_RES_SUCCEEDED;
	}

	if(is_dir(src))
	{
		(void)ioe_errlst_append(&args->result.errors, src, EISDIR,
				"Target path specifies existing directory");
		return IO_RES_FAILED;
	}

	if(os_stat(src, &st) != 0)
	{
		(void)ioe_errlst_append(&args->result.errors, src, errno,
				"Failed to stat() source file");
		return IO_RES_FAILED;
	}

#ifndef _WIN32
	/* Fifo/socket/device files don't need to be opened, their content is not
	 * accessed. */
	if(S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISBLK(st.st_mode) ||
			S_ISCHR(st.st_mode))
	{
		in = NULL;
	}
	else
#endif
	{
		in = os_fopen(src, "rb");
		if(in == NULL)
		{
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Failed to open source file");
			return IO_RES_FAILED;
		}
	}

	if(crs == IO_CRS_APPEND_TO_FILES)
	{
		open_mode = "ab";
	}
	else if(crs != IO_CRS_FAIL)
	{
		if(path_exists(dst, NODEREF))
		{
			/* Ask user whether to overwrite destination file. */
			if(confirm != NULL && !confirm(args, src, dst))
			{
				if(in != NULL && fclose(in) != 0)
				{
					(void)ioe_errlst_append(&args->result.errors, src, errno,
							"Error while closing source file");
				}
				return IO_RES_SUCCEEDED;
			}
		}

		int ec = unlink(dst);
		if(ec != 0 && errno != ENOENT)
		{
			(void)ioe_errlst_append(&args->result.errors, dst, errno,
					"Failed to unlink file");
			if(in != NULL && fclose(in) != 0)
			{
				(void)ioe_errlst_append(&args->result.errors, src, errno,
						"Error while closing source file");
			}
			return io_res_from_code(ec);
		}

		/* XXX: possible improvement would be to generate temporary file name in the
		 * destination directory, write to it and then overwrite destination file,
		 * but this approach has disadvantage of requiring more free space on
		 * destination file system. */
	}
	else if(path_exists(dst, NODEREF))
	{
		(void)ioe_errlst_append(&args->result.errors, src, EEXIST,
				"Destination path exists");
		if(in != NULL && fclose(in) != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Error while closing source file");
		}
		return IO_RES_FAILED;
	}

#ifndef _WIN32
	/* Replicate fifo without even opening it. */
	if(S_ISFIFO(st.st_mode))
	{
		if(mkfifo(dst, st.st_mode & 07777) != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Failed to create FIFO");
			return IO_RES_FAILED;
		}
		return IO_RES_SUCCEEDED;
	}

	/* Replicate socket or device file without even opening it. */
	if(S_ISSOCK(st.st_mode) || S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))
	{
		if(mknod(dst, st.st_mode & (S_IFMT | 07777), st.st_rdev) != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Failed to create node");
			return IO_RES_FAILED;
		}
		return IO_RES_SUCCEEDED;
	}
#endif

	out = os_fopen(dst, open_mode);
	if(out == NULL)
	{
		(void)ioe_errlst_append(&args->result.errors, dst, errno,
				"Failed to open destination file");
		if(fclose(in) != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Error while closing source file");
		}
		return IO_RES_FAILED;
	}

	error = 0;
	cloned = 0;

	if(crs == IO_CRS_APPEND_TO_FILES)
	{
		fpos_t pos;
		/* The following line is required for stupid Windows sometimes.  Why?
		 * Probably because it's stupid...  Won't harm other systems. */
		error |= (fseek(out, 0, SEEK_END) != 0);
		error |= (fgetpos(out, &pos) != 0 || fsetpos(in, &pos) != 0);

		orig_out_size = get_file_size(dst);
		correct_out_size = (orig_out_size != 0U || ftell(out) == 0);
		error |= !correct_out_size;

		if(!error)
		{
			ioeta_update(args->estim, NULL, NULL, 0, orig_out_size);
		}
	}
	else if(args->arg4.fast_file_cloning)
	{
		if(clone_file(fileno(out), fileno(in)) == 0)
		{
			cloned = 1;
		}
	}

	/* TODO: use sendfile() if platform supports it. */

	if(!error && !cloned)
	{
		char block[BLOCK_SIZE];
		/* Suppress possible false-positive compiler warning. */
		size_t nread = (size_t)-1;
#ifndef _WIN32
		size_t ncopied = 0U;
		const int data_sync = args->arg4.data_sync;
#endif
		while((nread = fread(&block, 1, sizeof(block), in)) != 0U)
		{
			if(io_cancelled(args))
			{
				error = 1;
				break;
			}

			if(fwrite(&block, 1, nread, out) != nread)
			{
				(void)ioe_errlst_append(&args->result.errors, dst, errno,
						"Write to destination file failed");
				error = 1;
				break;
			}

			ioeta_update(args->estim, NULL, NULL, 0, nread);

#ifndef _WIN32
			/* Force flushing data to disk to not pollute RAM with this data too
			 * much. */
			ncopied += nread;
			if(data_sync && ncopied >= FLUSH_SIZE)
			{
				(void)os_fdatasync(fileno(out));
				ncopied -= FLUSH_SIZE;
			}
#endif
		}

		if(nread == 0U && !feof(in) && ferror(in))
		{
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Read from source file failed");
		}

		/* fwrite() does caching, so we need to force flush to catch output errors
		 * before fclose() (which also does fflush() internally). */
		if(fflush(out) != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, dst, errno,
					"Write to destination file failed");
			error = 1;
		}
	}

	/* Note that we truncate output file even if operation was cancelled by the
	 * user. */
	if(crs == IO_CRS_APPEND_TO_FILES && error != 0 && correct_out_size)
	{
		int error;
#ifndef _WIN32
		error = ftruncate(fileno(out), (off_t)orig_out_size);
#else
		error = _chsize(fileno(out), orig_out_size);
#endif
		if(error != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Error while truncating output file back to original size");
		}
	}

	if(fclose(in) != 0)
	{
		(void)ioe_errlst_append(&args->result.errors, src, errno,
				"Error while closing source file");
	}
	if(fclose(out) != 0)
	{
		(void)ioe_errlst_append(&args->result.errors, dst, errno,
				"Error while closing destination file");
		error = 1;
	}

	if(error == 0 && os_lstat(src, &src_st) == 0)
	{
		error = os_chmod(dst, src_st.st_mode & 07777);
		if(error != 0)
		{
			(void)ioe_errlst_append(&args->result.errors, dst, errno,
					"Failed to setup file permissions");
		}
	}

	if(error == 0)
	{
		clone_attribs(dst, src, &st);
	}

	ioeta_update(args->estim, NULL, NULL, 1, 0);

	return io_res_from_code(error);
}

/* Try to clone file fast on btrfs.  Returns 0 on success, otherwise non-zero is
 * returned. */
static int
clone_file(int dst_fd, int src_fd)
{
#ifdef __linux__
#undef BTRFS_IOCTL_MAGIC
#define BTRFS_IOCTL_MAGIC 0x94
#undef BTRFS_IOC_CLONE
#define BTRFS_IOC_CLONE _IOW(BTRFS_IOCTL_MAGIC, 9, int)
	return ioctl(dst_fd, BTRFS_IOC_CLONE, src_fd);
#else
	(void)dst_fd;
	(void)src_fd;
	return -1;
#endif
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
	const char *const dst = args->arg2.dst;
	ioeta_estim_t *const estim = args->estim;

	if(transferred.QuadPart < last_size)
	{
		last_size = 0;
	}

	ioeta_update(estim, src, dst, 0, transferred.QuadPart - last_size);

	last_size = transferred.QuadPart;

	return io_cancelled(args) ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}

#endif

/* TODO: implement iop_chown(). */
IoRes iop_chown(io_args_t *args);

/* TODO: implement iop_chgrp(). */
IoRes iop_chgrp(io_args_t *args);

/* TODO: implement iop_chmod(). */
IoRes iop_chmod(io_args_t *args);

IoRes
iop_ln(io_args_t *args)
{
	return retry_wrapper(&iop_ln_internal, args);
}

/* Implementation of iop_ln(). */
static IoRes
iop_ln_internal(io_args_t *args)
{
	const char *const path = args->arg1.path;
	const char *const target = args->arg2.target;
	const int overwrite = args->arg3.crs != IO_CRS_FAIL;

	IoRes result;

#ifndef _WIN32
	result = io_res_from_code(symlink(path, target));
	if(result == IO_RES_FAILED && errno == EEXIST && overwrite &&
			is_symlink(target))
	{
		result = io_res_from_code(remove(target));
		if(result == IO_RES_SUCCEEDED)
		{
			result = io_res_from_code(symlink(path, target));
			if(result == IO_RES_FAILED)
			{
				(void)ioe_errlst_append(&args->result.errors, path, errno,
						"Error while creating symbolic link");
			}
		}
		else
		{
			(void)ioe_errlst_append(&args->result.errors, target, errno,
					"Error while removing existing destination");
		}
	}
	else if(result == IO_RES_FAILED && errno != 0)
	{
		(void)ioe_errlst_append(&args->result.errors, target, errno,
				"Error while creating symbolic link");
	}
#else
	char cmd[6 + PATH_MAX*2 + 1];
	char *escaped_path, *escaped_target;
	char base_dir[PATH_MAX + 2];

	if(path_exists(target, NODEREF))
	{
		if(!overwrite)
		{
			(void)ioe_errlst_append(&args->result.errors, target, EEXIST,
					"Destination path already exists");
			return IO_RES_FAILED;
		}

		if(!is_symlink(target))
		{
			(void)ioe_errlst_append(&args->result.errors, target, IO_ERR_UNKNOWN,
					"Target is not a symbolic link");
			return IO_RES_FAILED;
		}
	}

	/* We're using os_system() below, hence ST_CMD is hard-coded. */
	escaped_path = strdup(enclose_in_dquotes(path, ST_CMD));
	escaped_target = strdup(enclose_in_dquotes(target, ST_CMD));
	if(escaped_path == NULL || escaped_target == NULL)
	{
		(void)ioe_errlst_append(&args->result.errors, target, IO_ERR_UNKNOWN,
				"Not enough memory");
		free(escaped_target);
		free(escaped_path);
		return IO_RES_FAILED;
	}

	if(GetModuleFileNameA(NULL, base_dir, ARRAY_LEN(base_dir)) == 0)
	{
		(void)ioe_errlst_append(&args->result.errors, target, IO_ERR_UNKNOWN,
				"Failed to find win_helper");
		free(escaped_target);
		free(escaped_path);
		return IO_RES_FAILED;
	}

	break_atr(base_dir, '\\');
	snprintf(cmd, sizeof(cmd), "%s\\win_helper -s %s %s", base_dir, escaped_path,
			escaped_target);

	result = io_res_from_code(os_system(cmd));
	if(result == IO_RES_FAILED)
	{
		(void)ioe_errlst_append(&args->result.errors, target, IO_ERR_UNKNOWN,
				"Running win_helper has failed");
	}

	free(escaped_target);
	free(escaped_path);
#endif

	return result;
}

/* Implements detecting errors and querying user to decide whether to abort,
 * retry or ignore.  Returns zero on success and non-zero on error. */
static IoRes
retry_wrapper(iop_func func, io_args_t *args)
{
	IoRes result;
	ioeta_estim_t estim = {};

	/* Replace error list with an empty one to control which errors are going to
	 * be remembered. */
	ioe_errlst_t orig_errlist = args->result.errors;
	ioe_errlst_t empty_errlist = { .active = args->result.errors.active };
	args->result.errors = empty_errlist;

	/* Make restoration point for the estimation if necessary. */
	if(args->estim != NULL)
	{
		estim = ioeta_save(args->estim);
	}

	while(1)
	{
		result = func(args);
		if(result == IO_RES_SUCCEEDED || args->result.errors_cb == NULL ||
				args->result.errors.error_count == 0U)
		{
			ioe_errlst_splice(&orig_errlist, &args->result.errors);
			break;
		}

		switch(args->result.errors_cb(args, &args->result.errors.errors[0]))
		{
			case IO_ECR_RETRY:
				ioe_errlst_free(&args->result.errors);
				args->result.errors = empty_errlist;
				if(args->estim != NULL)
				{
					/* Restore previous state of progress before retrying. */
					ioeta_restore(args->estim, &estim);
				}
				continue;

			case IO_ECR_IGNORE:
				result = IO_RES_SKIPPED;
				if(args->estim != NULL)
				{
					/* When we ignore a file, in order to make progress look nice pretend
					 * that this file was processed in full. */
					ioeta_update(args->estim, args->estim->item, args->estim->target, 1,
							args->estim->total_file_bytes - args->estim->current_file_byte);
				}
				ioe_errlst_splice(&orig_errlist, &args->result.errors);
				break;

			case IO_ECR_BREAK:
				ioe_errlst_splice(&orig_errlist, &args->result.errors);
				break;

			default:
				assert(0 && "Unknown error handling result.");
				break;
		}

		break;
	}

	ioeta_release(&estim);

	ioe_errlst_free(&args->result.errors);
	args->result.errors = orig_errlist;

	return result;
}

/* Turns exit code into IoRes.  Returns IoRes. */
static IoRes
io_res_from_code(int code)
{
	return (code == 0 ? IO_RES_SUCCEEDED : IO_RES_FAILED);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
