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

#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */

#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/log.h"
#include "../utils/path.h"
#include "../background.h"
#include "ioc.h"

int
ior_rm(io_args_t *const args)
{
	const char *const src = args->arg1.src;

#ifndef _WIN32
	char *escaped;
	char cmd[16 + PATH_MAX];
	int result;

	escaped = escape_filename(src, 0);
	if(escaped == NULL)
	{
		return -1;
	}
	snprintf(cmd, sizeof(cmd), "rm -rf %s", escaped);
	free(escaped);

	LOG_INFO_MSG("Running rm command: \"%s\"", cmd);
	result = background_and_wait_for_errors(cmd, args->cancellable);

	return result;
#else
	if(is_dir(src))
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
		snprintf(buf, sizeof(buf), "%s%c", src, '\0');
		to_back_slash(buf);
		err = SHFileOperation(&fo);
		log_msg("Error: %d", err);
		return err;
	}
	else
	{
		int ok;
		DWORD attributes = GetFileAttributesA(src);
		if(attributes & FILE_ATTRIBUTE_READONLY)
		{
			SetFileAttributesA(src, attributes & ~FILE_ATTRIBUTE_READONLY);
		}
		ok = DeleteFile(src);
		if(!ok)
		{
			LOG_WERROR(GetLastError());
		}
		return !ok;
	}
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
