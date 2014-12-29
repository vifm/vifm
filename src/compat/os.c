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

#include "os.h"

#ifdef _WIN32

#include <wchar.h> /* _waccess() _wchmod() _wmkdir() _wrename() _wsystem() */

#include <stddef.h> /* NULL wchar_t */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strlen() */

#include "../utils/str.h"
#include "../utils/utf8.h"

/* Fake DIR structure for directory traversing functions. */
typedef struct
{
	_WDIR *dirp;         /* Handle returned by opendir(). */
	struct dirent entry; /* Buffer for modified return value of readdir(). */
}
os_dir_t;

int
os_access(const char pathname[], int mode)
{
	wchar_t *const utf16_pathname = utf8_to_utf16(pathname);
	const int result = _waccess(utf16_pathname, mode);
	free(utf16_pathname);
	return result;
}

int
os_chdir(const char path[])
{
	wchar_t *const utf16_path = utf8_to_utf16(path);
	const int result = _wchdir(utf16_path);
	free(utf16_path);
	return result;
}

int
os_chmod(const char path[], int mode)
{
	wchar_t *const utf16_path = utf8_to_utf16(path);
	const int result = _wchmod(utf16_path, mode);
	free(utf16_path);
	return result;
}

int
os_closedir(DIR *dirp)
{
	os_dir_t *const os_dir = (os_dir_t *)dirp;
	const int result = _wclosedir(os_dir->dirp);
	free(os_dir);
	return result;
}

DIR *
os_opendir(const char name[])
{
	os_dir_t *os_dir;
	wchar_t *const utf16_name = utf8_to_utf16(name);
	_WDIR *const d = _wopendir(utf16_name);
	free(utf16_name);

	if(d == NULL)
	{
		return NULL;
	}

	os_dir = malloc(sizeof(*os_dir));
	if(os_dir == NULL)
	{
		(void)_wclosedir(d);
		return NULL;
	}

	os_dir->dirp = d;
	return (DIR *)os_dir;
}

struct dirent *
os_readdir(DIR *dirp)
{
	char *utf8_name;
	os_dir_t *const os_dir = (os_dir_t *)dirp;
	struct _wdirent *const entry = _wreaddir(os_dir->dirp);
	if(entry == NULL)
	{
		return NULL;
	}

	utf8_name = utf8_from_utf16(entry->d_name);
	copy_str(os_dir->entry.d_name, sizeof(os_dir->entry.d_name), utf8_name);
	free(utf8_name);

	os_dir->entry.d_ino = 0;
	os_dir->entry.d_reclen  = 0;
	os_dir->entry.d_namlen = strlen(os_dir->entry.d_name);

	return &os_dir->entry;
}

int
os_rename(const char oldpath[], const char newpath[])
{
	wchar_t *const utf16_oldpath = utf8_to_utf16(oldpath);
	wchar_t *const utf16_newpath = utf8_to_utf16(newpath);
	const int result = _wrename(utf16_oldpath, utf16_newpath);
	free(utf16_oldpath);
	free(utf16_newpath);
	return result;
}

int
os_mkdir(const char pathname[], int mode)
{
	wchar_t *const utf16_pathname = utf8_to_utf16(pathname);
	const int result = _wmkdir(utf16_pathname);
	free(utf16_pathname);
	return result;
}

int
os_stat(const char path[], struct stat *buf)
{
	wchar_t *const utf16_path = utf8_to_utf16(path);
	const int result = _wstat(utf16_path, (struct _stat *)buf);
	free(utf16_path);
	return result;
}

int
os_system(const char command[])
{
	wchar_t *const utf16_command = utf8_to_utf16(command);
	const int result = _wsystem(utf16_command);
	free(utf16_command);
	return result;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
