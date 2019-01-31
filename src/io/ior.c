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

#include <sys/stat.h> /* stat */
#include <unistd.h> /* unlink() */

#include <errno.h> /* EEXIST EISDIR ENOTEMPTY EXDEV errno */
#include <stddef.h> /* NULL */
#include <stdio.h> /* remove() snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utils.h"
#include "../background.h"
#include "private/ioc.h"
#include "private/ioe.h"
#include "private/ioeta.h"
#include "private/traverser.h"
#include "ioc.h"
#include "iop.h"

static VisitResult rm_visitor(const char full_path[], VisitAction action,
		void *param);
static VisitResult cp_visitor(const char full_path[], VisitAction action,
		void *param);
static int is_file(const char path[]);
static VisitResult mv_visitor(const char full_path[], VisitAction action,
		void *param);
static VisitResult cp_mv_visitor(const char full_path[], VisitAction action,
		void *param, int cp);

int
ior_rm(io_args_t *args)
{
	const char *const path = args->arg1.path;
	return traverse(path, &rm_visitor, args);
}

/* Implementation of traverse() visitor for subtree removal.  Returns 0 on
 * success, otherwise non-zero is returned. */
static VisitResult
rm_visitor(const char full_path[], VisitAction action, void *param)
{
	io_args_t *const rm_args = param;
	VisitResult result = VR_OK;

	if(io_cancelled(rm_args))
	{
		return VR_CANCELLED;
	}

	switch(action)
	{
		case VA_DIR_ENTER:
			/* Do nothing, directories are removed on leaving them. */
			result = VR_OK;
			break;
		case VA_FILE:
			{
				io_args_t args = {
					.arg1.path = full_path,

					.cancellation = rm_args->cancellation,
					.estim = rm_args->estim,

					.result = rm_args->result,
				};

				result = (iop_rmfile(&args) == 0) ? VR_OK : VR_ERROR;
				rm_args->result = args.result;
				break;
			}
		case VA_DIR_LEAVE:
			{
				io_args_t args = {
					.arg1.path = full_path,

					.cancellation = rm_args->cancellation,
					.estim = rm_args->estim,

					.result = rm_args->result,
				};

				result = (iop_rmdir(&args) == 0) ? VR_OK : VR_ERROR;
				rm_args->result = args.result;
				break;
			}
	}

	return result;
}

int
ior_cp(io_args_t *args)
{
	const char *const src = args->arg1.src;
	const char *const dst = args->arg2.dst;

	if(is_in_subtree(dst, src, 0))
	{
		(void)ioe_errlst_append(&args->result.errors, src, IO_ERR_UNKNOWN,
				"Can't copy parent path into subpath");
		return 1;
	}

	if(args->arg3.crs == IO_CRS_REPLACE_ALL)
	{
		io_args_t rm_args = {
			.arg1.path = dst,

			.cancellation = args->cancellation,
			.estim = args->estim,

			.result = args->result,
		};

		const int result = ior_rm(&rm_args);
		args->result = rm_args.result;
		if(result != 0)
		{
			if(!io_cancelled(args))
			{
				(void)ioe_errlst_append(&args->result.errors, dst, IO_ERR_UNKNOWN,
						"Failed to remove");
			}
			return result;
		}
	}

	return traverse(src, &cp_visitor, args);
}

/* Implementation of traverse() visitor for subtree copying.  Returns 0 on
 * success, otherwise non-zero is returned. */
static VisitResult
cp_visitor(const char full_path[], VisitAction action, void *param)
{
	return cp_mv_visitor(full_path, action, param, 1);
}

int
ior_mv(io_args_t *args)
{
	const char *const src = args->arg1.src;
	const char *const dst = args->arg2.dst;
	const IoCrs crs = args->arg3.crs;
	const io_confirm confirm = args->confirm;
	int confirmed = 0;

	if(crs == IO_CRS_FAIL && path_exists(dst, DEREF) && !is_case_change(src, dst))
	{
		(void)ioe_errlst_append(&args->result.errors, dst, EEXIST,
				"Destination path already exists");
		return 1;
	}

	if(crs == IO_CRS_APPEND_TO_FILES)
	{
		if(!is_file(src))
		{
			(void)ioe_errlst_append(&args->result.errors, src, EISDIR,
					"Can't append when source is not a file");
			return 1;
		}
		if(!is_file(dst))
		{
			(void)ioe_errlst_append(&args->result.errors, dst, EISDIR,
					"Can't append when destination is not a file");
			return 1;
		}
	}
	else if(crs == IO_CRS_REPLACE_FILES && path_exists(dst, DEREF))
	{
		/* Ask user whether to overwrite destination file. */
		if(confirm != NULL && !confirm(args, src, dst))
		{
			return 0;
		}
		confirmed = 1;
	}

	if(os_rename(src, dst) == 0)
	{
		ioeta_update(args->estim, src, dst, 1, 0);
		return 0;
	}

	switch(errno)
	{
		case EXDEV:
#ifndef _WIN32
		/* At least SSHFS fails to propagate EXDEV and reports EPERM.  So we try to
		 * do the copy across mounts anyway, which might actually work despite the
		 * error code. */
		case EPERM:
		case EACCES:
#endif
			{
				args->confirm = (confirmed ? NULL : confirm);
				int result = ior_cp(args);
				args->confirm = confirm;
				if(result == 0)
				{
					io_args_t rm_args = {
						.arg1.path = src,

						.cancellation = args->cancellation,
						.estim = args->estim,

						.result = args->result,
					};

					/* Disable progress reporting for this "secondary" operation. */
					const int silent = ioeta_silent_on(rm_args.estim);
					result = ior_rm(&rm_args);
					args->result = rm_args.result;
					ioeta_silent_set(rm_args.estim, silent);
				}
				return result;
			}
		case EISDIR:
		case ENOTEMPTY:
		case EEXIST:
#ifdef _WIN32
		/* For MXE builds running in Wine. */
		case EPERM:
		case EACCES:
#endif
			if(crs == IO_CRS_REPLACE_ALL)
			{
				int error;
				io_args_t rm_args = {
					.arg1.path = dst,

					.cancellation = args->cancellation,
					.estim = args->estim,

					.result = args->result,
				};

				/* Ask user whether to overwrite destination file. */
				if(confirm != NULL && !confirm(args, src, dst))
				{
					return 0;
				}

				error = ior_rm(&rm_args);
				args->result = rm_args.result;
				if(error != 0)
				{
					if(!io_cancelled(args))
					{
						(void)ioe_errlst_append(&args->result.errors, dst, IO_ERR_UNKNOWN,
								"Failed to remove");
					}
					return error;
				}

				if(os_rename(src, dst) != 0)
				{
					(void)ioe_errlst_append(&args->result.errors, src, errno,
							"Rename operation failed");
					return 1;
				}
				return 0;
			}
			else if(crs == IO_CRS_REPLACE_FILES ||
					(!has_atomic_file_replace() && crs == IO_CRS_APPEND_TO_FILES))
			{
				if(!has_atomic_file_replace() && is_file(dst))
				{
					io_args_t rm_args = {
						.arg1.path = dst,

						.cancellation = args->cancellation,
						.estim = args->estim,

						.result = args->result,
					};

					const int error = iop_rmfile(&rm_args);
					args->result = rm_args.result;
					if(error != 0)
					{
						if(!io_cancelled(args))
						{
							(void)ioe_errlst_append(&args->result.errors, dst, IO_ERR_UNKNOWN,
									"Failed to remove");
						}
						return error;
					}
				}

				return traverse(src, &mv_visitor, args);
			}
			/* Break is intentionally omitted. */

		default:
			(void)ioe_errlst_append(&args->result.errors, src, errno,
					"Rename operation failed");
			return errno;
	}
}

/* Checks that path points to a file or symbolic link.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_file(const char path[])
{
	return !is_dir(path)
	    || (is_symlink(path) && get_symlink_type(path) != SLT_UNKNOWN);
}

/* Implementation of traverse() visitor for subtree moving.  Returns 0 on
 * success, otherwise non-zero is returned. */
static VisitResult
mv_visitor(const char full_path[], VisitAction action, void *param)
{
	return cp_mv_visitor(full_path, action, param, 0);
}

/* Generic implementation of traverse() visitor for subtree copying/moving.
 * Returns 0 on success, otherwise non-zero is returned. */
static VisitResult
cp_mv_visitor(const char full_path[], VisitAction action, void *param, int cp)
{
	io_args_t *const cp_args = param;
	const char *dst_full_path;
	char *free_me = NULL;
	VisitResult result = VR_OK;
	const char *rel_part;

	if(io_cancelled(cp_args))
	{
		return VR_CANCELLED;
	}

	/* TODO: come up with something better than this. */
	rel_part = full_path + strlen(cp_args->arg1.src);
	dst_full_path = (rel_part[0] == '\0')
	              ? cp_args->arg2.dst
	              : (free_me = format_str("%s/%s", cp_args->arg2.dst, rel_part));

	switch(action)
	{
		case VA_DIR_ENTER:
			if(cp_args->arg3.crs != IO_CRS_REPLACE_FILES || !is_dir(dst_full_path))
			{
				io_args_t args = {
					.arg1.path = dst_full_path,

					/* Temporary fake rights so we can add files to the directory. */
					.arg3.mode = 0700,

					.cancellation = cp_args->cancellation,
					.estim = cp_args->estim,

					.result = cp_args->result,
				};

				result = (iop_mkdir(&args) == 0) ? VR_OK : VR_ERROR;
				cp_args->result = args.result;
			}
			break;
		case VA_FILE:
			{
				io_args_t args = {
					.arg1.src = full_path,
					.arg2.dst = dst_full_path,
					.arg3.crs = cp_args->arg3.crs,
					/* It's safe to always use fast file cloning on moving files. */
					.arg4.fast_file_cloning = cp ? cp_args->arg4.fast_file_cloning : 1,

					.cancellation = cp_args->cancellation,
					.confirm = cp_args->confirm,
					.estim = cp_args->estim,

					.result = cp_args->result,
				};

				result = ((cp ? iop_cp(&args) : ior_mv(&args)) == 0) ? VR_OK : VR_ERROR;
				cp_args->result = args.result;
				break;
			}
		case VA_DIR_LEAVE:
			{
				struct stat st;

				if(cp_args->arg3.crs == IO_CRS_REPLACE_FILES && !cp)
				{
					io_args_t rm_args = {
						.arg1.path = full_path,

						.cancellation = cp_args->cancellation,
						.estim = cp_args->estim,

						.result = cp_args->result,
					};

					result = (iop_rmdir(&rm_args) == 0) ? VR_OK : VR_ERROR;
				}
				else if(os_stat(full_path, &st) == 0)
				{
					result = (os_chmod(dst_full_path, st.st_mode & 07777) == 0)
									? VR_OK
									: VR_ERROR;
					if(result == VR_ERROR)
					{
						(void)ioe_errlst_append(&cp_args->result.errors, dst_full_path,
								errno, "Failed to setup directory permissions");
					}
					clone_attribs(dst_full_path, full_path, &st);
				}
				else
				{
					(void)ioe_errlst_append(&cp_args->result.errors, full_path, errno,
							"Failed to stat() source directory");
					result = VR_ERROR;
				}
				break;
			}
	}

	free(free_me);

	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
