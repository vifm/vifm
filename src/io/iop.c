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

#include "iop.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strchr() */

#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../background.h"
#include "ioc.h"

int
iop_mkfile(io_args_t *const args)
{
	const char *const path = args->arg1.path;

#ifndef _WIN32
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(path, 0);
	snprintf(cmd, sizeof(cmd), "touch %s", escaped);
	free(escaped);
	LOG_INFO_MSG("Running touch command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd, 1);
#else
	HANDLE file;

	file = CreateFileA(path, 0, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
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
	const int create_parent = args->arg3.process_parents;

#ifndef _WIN32
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(path, 0);
	snprintf(cmd, sizeof(cmd), "mkdir %s %s", create_parent ? "-p" : "", escaped);
	free(escaped);
	LOG_INFO_MSG("Running mkdir command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd, 1);
#else
	if(create_parent)
	{
		char *p;
		char t;

		p = strchr(path + 2, '/');
		do
		{
			t = *p;
			*p = '\0';

			if(!is_dir(path))
			{
				if(!CreateDirectory(path, NULL))
				{
					*p = t;
					return -1;
				}
			}

			*p = t;
			p = until_first(p + 1, '/');
		}
		while(t != '\0');
		return 0;
	}
	else
	{
		return CreateDirectory(path, NULL) == 0;
	}
#endif
}

int
iop_rmdir(io_args_t *const args)
{
	const char *const path = args->arg1.path;

#ifndef _WIN32
	char cmd[128 + PATH_MAX];
	char *escaped;

	escaped = escape_filename(path, 0);
	snprintf(cmd, sizeof(cmd), "rmdir %s", escaped);
	free(escaped);
	LOG_INFO_MSG("Running rmdir command: \"%s\"", cmd);
	return background_and_wait_for_errors(cmd, 1);
#else
	return RemoveDirectory(path) == 0;
#endif
}

int
iop_ln(io_args_t *const args)
{
	const char *const path = args->arg1.path;
	const char *const target = args->arg2.target;
	const int overwrite = args->arg3.overwrite;

	char *escaped_path, *escaped_target;
	char cmd[6 + PATH_MAX*2 + 1];
	int result;
#ifdef _WIN32
	char base_dir[PATH_MAX + 2];
#endif

	escaped_path = escape_filename(path, 0);
	escaped_target = escape_filename(target, 0);
	if(escaped_path == NULL || escaped_target == NULL)
	{
		free(escaped_target);
		free(escaped_path);
		return -1;
	}

#ifndef _WIN32
	snprintf(cmd, sizeof(cmd), "ln -s %s %s %s",
			(overwrite && is_symlink(target)) ? "-f" : "", escaped_path,
			escaped_target);
	LOG_INFO_MSG("Running ln command: \"%s\"", cmd);
	result = background_and_wait_for_errors(cmd, 1);
#else
	(void)overwrite;
	if(GetModuleFileNameA(NULL, base_dir, ARRAY_LEN(base_dir)) == 0)
	{
		free(escaped_target);
		free(escaped_path);
		return -1;
	}

	break_atr(base_dir, '\\');
	snprintf(cmd, sizeof(cmd), "%s\\win_helper -s %s %s", base_dir, escaped_path,
			escaped_target);
	result = system(cmd);
#endif

	free(escaped_target);
	free(escaped_path);
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
