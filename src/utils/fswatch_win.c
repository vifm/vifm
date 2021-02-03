/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "fswatch.h"

#include <windows.h>

#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strdup */

#include "../compat/fs_limits.h"
#include "macros.h"
#include "str.h"
#include "utf8.h"

/* Watcher data. */
struct fswatch_t
{
	FILETIME dir_mtime;
	HANDLE dir_watcher;
	wchar_t *wpath;
};

static int get_dir_mtime(const wchar_t dir_path[], FILETIME *ft);

fswatch_t *
fswatch_create(const char path[])
{
	fswatch_t *const w = malloc(sizeof(*w));
	if(w == NULL)
	{
		return NULL;
	}

	w->wpath = utf8_to_utf16(path);
	if(w->wpath == NULL)
	{
		free(w);
		return NULL;
	}

	if(get_dir_mtime(w->wpath, &w->dir_mtime) != 0)
	{
		free(w->wpath);
		free(w);
		return NULL;
	}

	w->dir_watcher = FindFirstChangeNotificationW(w->wpath, 1,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY);
	if(w->dir_watcher == INVALID_HANDLE_VALUE)
	{
		free(w->wpath);
		free(w);
		return NULL;
	}

	return w;
}

void
fswatch_free(fswatch_t *w)
{
	if(w != NULL)
	{
		FindCloseChangeNotification(w->dir_watcher);
		free(w->wpath);
		free(w);
	}
}

FSWatchState
fswatch_poll(fswatch_t *w)
{
	FILETIME ft;
	if(get_dir_mtime(w->wpath, &ft) != 0)
	{
		return FSWS_ERRORED;
	}

	int changed = CompareFileTime(&w->dir_mtime, &ft) != 0;
	w->dir_mtime = ft;

	if(WaitForSingleObject(w->dir_watcher, 0) == WAIT_OBJECT_0)
	{
		FindNextChangeNotification(w->dir_watcher);
		changed = 1;
	}

	return (changed ? FSWS_UPDATED : FSWS_UNCHANGED);
}

/* Gets last directory modification time.  Returns non-zero on error, otherwise
 * zero is returned. */
static int
get_dir_mtime(const wchar_t dir_path[], FILETIME *ft)
{
	wchar_t selfref_path[PATH_MAX + 1];
	HANDLE hfile;
	int error;

	vifm_swprintf(selfref_path, ARRAY_LEN(selfref_path), L"%" WPRINTF_WSTR L"/.",
			dir_path);

	hfile = CreateFileW(selfref_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if(hfile == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	error = GetFileTime(hfile, NULL, NULL, ft) == FALSE;
	CloseHandle(hfile);

	return error;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
