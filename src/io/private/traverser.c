/* vifm
 * Copyright (C) 2014 xaizek.
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

#include "traverser.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "../../compat/os.h"
#include "../../utils/fs.h"
#include "../../utils/path.h"
#include "../../utils/str.h"

static int traverse_subtree(const char path[], subtree_visitor visitor,
		void *param);

IoRes
traverse(const char path[], subtree_visitor visitor, void *param)
{
	/* Duplication with traverse_subtree(), but this way traverse_subtree() can
	 * use information from dirent structure to save some operations. */

	/* Treat symbolic links to directories as files as well. */
	if(is_symlink(path) || !is_dir(path))
	{
		return (visitor(path, VA_FILE, param) == VR_OK) ? IO_RES_SUCCEEDED
		                                                : IO_RES_FAILED;
	}
	else
	{
		return (traverse_subtree(path, visitor, param) == 0) ? IO_RES_SUCCEEDED
		                                                     : IO_RES_FAILED;
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
	VisitResult enter_result;

	dir = os_opendir(path);
	if(dir == NULL)
	{
		return 1;
	}

	enter_result = visitor(path, VA_DIR_ENTER, param);
	if(enter_result == VR_ERROR)
	{
		(void)os_closedir(dir);
		return 1;
	}

	result = 0;
	while((d = os_readdir(dir)) != NULL)
	{
		char *full_path;

		if(is_builtin_dir(d->d_name))
		{
			continue;
		}

		full_path = join_paths(path, d->d_name);
		if(entry_is_link(full_path, d))
		{
			/* Treat symbolic links to directories as files as well. */
			result = (visitor(full_path, VA_FILE, param) != VR_OK);
		}
		else if(entry_is_dir(full_path, d))
		{
			result = traverse_subtree(full_path, visitor, param);
		}
		else
		{
			result = (visitor(full_path, VA_FILE, param) != VR_OK);
		}
		free(full_path);

		if(result != 0)
		{
			break;
		}
	}
	(void)os_closedir(dir);

	if(result == 0 && enter_result != VR_SKIP_DIR_LEAVE &&
			enter_result != VR_CANCELLED)
	{
		result = (visitor(path, VA_DIR_LEAVE, param) != VR_OK);
	}

	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
