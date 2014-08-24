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

#include "ior.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <sys/stat.h> /* stat */
#include <dirent.h> /* DIR dirent opendir() readdir() closedir() */
#include <unistd.h> /* lstat() unlink() */

#include <assert.h> /* assert() */
#include <errno.h> /* EEXIST EISDIR ENOTEMPTY EXDEV errno */
#include <stddef.h> /* NULL */
#include <stdio.h> /* removee() snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */

#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/log.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../background.h"
#include "ioc.h"
#include "iop.h"

/* Reason why file system traverse visitor is called. */
typedef enum
{
	VA_DIR_ENTER, /* After opening a directory. */
	VA_FILE,      /* For a file. */
	VA_DIR_LEAVE, /* After closing a directory. */
}
VisitAction;

/* Generic handler for file system traversing algorithm.  Must return 0 on
 * success, otherwise directory traverse will be stopped. */
typedef int (*subtree_visitor)(const char full_path[], VisitAction action,
		void *param);

static int cp_visitor(const char full_path[], VisitAction action, void *param);
static int mv_visitor(const char full_path[], VisitAction action, void *param);
static int cp_mv_visitor(const char full_path[], VisitAction action,
		void *param, int cp);
static int traverse(const char path[], subtree_visitor visitor, void *param);
static int traverse_subtree(const char path[], subtree_visitor visitor,
		void *param);

int
ior_rm(io_args_t *const args)
{
	const char *const path = args->arg1.path;

#ifndef _WIN32
	if(!is_dir(path))
	{
		return unlink(path);
	}

	/* XXX: update function and check its result?  If it actually make sense, as
	 * result of iop_rmdir() will reflect whether remove_dir_content() was
	 * successful or not. */
	remove_dir_content(path);
	return iop_rmdir(args);
#else
	if(is_dir(path))
	{
		char buf[PATH_MAX];
		int err;
		SHFILEOPSTRUCTA fo =
		{
			.hwnd = NULL,
			.wFunc = FO_DELETE,
			.pFrom = buf,
			.pTo = NULL,
			.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI,
		};

		/* The string should be terminated with two null characters. */
		snprintf(buf, sizeof(buf), "%s%c", path, '\0');
		to_back_slash(buf);
		err = SHFileOperation(&fo);
		log_msg("Error: %d", err);
		return err;
	}
	else
	{
		int ok;
		DWORD attributes = GetFileAttributesA(path);
		if(attributes & FILE_ATTRIBUTE_READONLY)
		{
			SetFileAttributesA(path, attributes & ~FILE_ATTRIBUTE_READONLY);
		}
		ok = DeleteFile(path);
		if(!ok)
		{
			LOG_WERROR(GetLastError());
		}
		return !ok;
	}
#endif
}

int
ior_cp(io_args_t *const args)
{
	const char *const src = args->arg1.src;
	const char *const dst = args->arg2.dst;
	const int overwrite = args->arg3.crs == IO_CRS_REPLACE_ALL;
	const int cancellable = args->cancellable;

	if(is_in_subtree(dst, src))
	{
		return 1;
	}

	if(overwrite)
	{
		io_args_t args =
		{
			.arg1.path = dst,

			.cancellable = cancellable,
		};
		const int result = ior_rm(&args);
		if(result != 0)
		{
			return result;
		}
	}

	return traverse(src, &cp_visitor, args);
}

/* Implementation of traverse() visitor for subtree copying.  Returns 0 on
 * success, otherwise non-zero is returned. */
static int
cp_visitor(const char full_path[], VisitAction action, void *param)
{
	return cp_mv_visitor(full_path, action, param, 1);
}

int
ior_mv(io_args_t *const args)
{
	const char *const src = args->arg1.src;
	const char *const dst = args->arg2.dst;
	const IoCrs crs = args->arg3.crs;
	const int cancellable = args->cancellable;

	if(path_exists(dst) && crs == IO_CRS_FAIL)
	{
		return 1;
	}

	if(rename(src, dst) == 0)
	{
		return 0;
	}

	switch(errno)
	{
		case EXDEV:
			{
				const int result = ior_cp(args);
				return (result == 0) ? ior_rm(args) : result;
			}
		case EISDIR:
		case ENOTEMPTY:
		case EEXIST:
			if(crs == IO_CRS_REPLACE_ALL)
			{
				io_args_t args =
				{
					.arg1.path = dst,

					.cancellable = cancellable,
				};

				const int error = ior_rm(&args);
				if(error != 0)
				{
					return error;
				}

				return rename(src, dst);
			}
			else if(crs == IO_CRS_REPLACE_FILES)
			{
				return traverse(src, &mv_visitor, args);
			}
			/* Break is intentionally omitted. */

		default:
			return errno;
	}
}

/* Implementation of traverse() visitor for subtree moving.  Returns 0 on
 * success, otherwise non-zero is returned. */
static int
mv_visitor(const char full_path[], VisitAction action, void *param)
{
	return cp_mv_visitor(full_path, action, param, 0);
}

/* Generic implementation of traverse() visitor for subtree copying/moving.
 * Returns 0 on success, otherwise non-zero is returned. */
static int
cp_mv_visitor(const char full_path[], VisitAction action, void *param, int cp)
{
	const io_args_t *const cp_args = param;
	const char *dst_full_path;
	char *free_me = NULL;
	int result;
	const char *rel_part;

	if(action == VA_DIR_LEAVE)
	{
		return 0;
	}

	/* TODO: come up with something better than this. */
	rel_part = full_path + strlen(cp_args->arg1.src);
	dst_full_path = (rel_part[0] == '\0')
	              ? cp_args->arg2.dst
	              : (free_me = format_str("%s/%s", cp_args->arg2.dst, rel_part));

	result = 0;
	switch(action)
	{
		case VA_DIR_ENTER:
			if(cp_args->arg3.crs != IO_CRS_REPLACE_FILES || !is_dir(dst_full_path))
			{
				struct stat src_st;

				if(lstat(full_path, &src_st) == 0)
				{
					io_args_t args =
					{
						.arg1.path = dst_full_path,
						.arg3.mode = src_st.st_mode & 07777,

						.cancellable = cp_args->cancellable,
					};

					result = iop_mkdir(&args);
				}
				else
				{
					result = 1;
				}
			}
			break;
		case VA_FILE:
			{
				io_args_t args =
				{
					.arg1.src = full_path,
					.arg2.dst = dst_full_path,
					.arg3.crs = cp_args->arg3.crs,

					.cancellable = cp_args->cancellable,
				};

				result = cp ? iop_cp(&args) : ior_mv(&args);
				break;
			}

		default:
			assert(0 && "Unexpected visitor action.");
			result = 1;
			break;
	}

	free(free_me);

	return result;
}

/* A generic recursive file system traversing entry point.  Returns zero on
 * success, otherwise non-zero is returned. */
static int
traverse(const char path[], subtree_visitor visitor, void *param)
{
	if(is_dir(path))
	{
		return traverse_subtree(path, visitor, param);
	}
	else
	{
		return visitor(path, VA_FILE, param);
	}
}

/* A generic subtree traversing.  Returns zero on success, otherwise non-zero is
 * returned. */
static int
traverse_subtree(const char path[], subtree_visitor visitor, void *param)
{
	DIR *dir;
	struct dirent *d;
	int result;

	dir = opendir(path);
	if(dir == NULL)
	{
		return 1;
	}

	result = visitor(path, VA_DIR_ENTER, param);
	if(result != 0)
	{
		(void)closedir(dir);
		return result;
	}

	while((d = readdir(dir)) != NULL)
	{
		if(!is_builtin_dir(d->d_name))
		{
			char *const full_path = format_str("%s/%s", path, d->d_name);
			if(entry_is_dir(full_path, d))
			{
				result = traverse_subtree(full_path, visitor, param);
			}
			else
			{
				result = visitor(full_path, VA_FILE, param);
			}
			free(full_path);

			if(result != 0)
			{
				break;
			}
		}
	}
	(void)closedir(dir);

	if(result == 0)
	{
		result = visitor(path, VA_DIR_LEAVE, param);
	}

	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
