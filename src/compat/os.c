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

#include <windows.h>
#include <ntdef.h>
#include <winioctl.h>

#include <sys/stat.h> /* stat */
#include <sys/types.h> /* stat */

#include <errno.h> /* errno */
#include <stddef.h> /* NULL wchar_t */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* memset() strcpy() strdup() strlen() */
#include <stdio.h> /* FILE snprintf() */
#include <wchar.h> /* _waccess() _wchmod() _wmkdir() _wrename() _wsystem() */

#include "../utils/fs.h"
#include "../utils/str.h"
#include "../utils/path.h"
#include "../utils/utf8.h"

/* Fake DIR structure for directory traversing functions. */
typedef struct
{
	_WDIR *dirp;         /* Handle returned by opendir(). */
	struct dirent entry; /* Buffer for modified return value of readdir(). */
}
os_dir_t;

static char * resolve_mount_points(const char path[]);

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

FILE *
os_fopen(const char path[], const char mode[])
{
	wchar_t *const utf16_path = utf8_to_utf16(path);
	wchar_t *const utf16_mode = utf8_to_utf16(mode);
	FILE *const result = _wfopen(utf16_path, utf16_mode);
	free(utf16_mode);
	free(utf16_path);
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

char *
os_realpath(const char path[], char resolved_path[])
{
	char *const resolved = resolve_mount_points(path);

	if(!path_exists(path, NODEREF))
	{
		errno = ENOENT;
		free(resolved);
		return NULL;
	}

	if(!is_path_absolute(resolved))
	{
		/* Try to compose absolute path. */
		char cwd[PATH_MAX];
		if(get_cwd(cwd, sizeof(cwd)) == cwd)
		{
			snprintf(resolved_path, PATH_MAX, "%s/%s", cwd, resolved);
			free(resolved);
			return resolved_path;
		}
	}

	copy_str(resolved_path, PATH_MAX, resolved);
	free(resolved);
	return resolved_path;
}

/* Resolves path to its destination.  Returns pointer a newly allocated
 * memory. */
static char *
resolve_mount_points(const char path[])
{
	char resolved_path[PATH_MAX];

	DWORD attr;
	wchar_t *utf16_path;
	HANDLE hfind;
	WIN32_FIND_DATAW ffd;
	HANDLE hfile;
	char rdb[2048];
	char *t;
	int offset;
	REPARSE_DATA_BUFFER *rdbp;

	utf16_path = utf8_to_utf16(path);
	attr = GetFileAttributesW(utf16_path);
	free(utf16_path);

	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		return strdup(path);
	}

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
	{
		return strdup(path);
	}

	copy_str(resolved_path, sizeof(resolved_path), path);
	chosp(resolved_path);

	utf16_path = utf8_to_utf16(resolved_path);
	hfind = FindFirstFileW(utf16_path, &ffd);

	if(hfind == INVALID_HANDLE_VALUE)
	{
		free(utf16_path);
		return strdup(path);
	}

	FindClose(hfind);

	if(ffd.dwReserved0 != IO_REPARSE_TAG_MOUNT_POINT)
	{
		free(utf16_path);
		return strdup(path);
	}

	hfile = CreateFileW(utf16_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);

	free(utf16_path);

	if(hfile == INVALID_HANDLE_VALUE)
	{
		return strdup(path);
	}

	if(!DeviceIoControl(hfile, FSCTL_GET_REPARSE_POINT, NULL, 0, rdb, sizeof(rdb),
			&attr, NULL))
	{
		CloseHandle(hfile);
		return strdup(path);
	}
	CloseHandle(hfile);

	rdbp = (REPARSE_DATA_BUFFER *)rdb;
	t = to_multibyte(rdbp->MountPointReparseBuffer.PathBuffer);

	offset = starts_with_lit(t, "\\??\\") ? 4 : 0;
	strcpy(resolved_path, t + offset);

	free(t);

	return strdup(resolved_path);
}

int
os_stat(const char path[], struct stat *buf)
{
	HANDLE hfile;
	struct _stat st;
	wchar_t *const utf16_path = utf8_to_utf16(path);
	const int result = _wstat(utf16_path, &st);

	memset(buf, 0, sizeof(*buf));
	buf->st_dev = st.st_dev;
	buf->st_ino = st.st_ino;
	buf->st_mode = st.st_mode;
	buf->st_nlink = st.st_nlink;
	buf->st_uid = st.st_uid;
	buf->st_gid = st.st_gid;
	buf->st_rdev = st.st_rdev;
	buf->st_size = st.st_size;
	buf->st_atime = st.st_atime;
	buf->st_mtime = st.st_mtime;
	buf->st_ctime = st.st_ctime;

	/* M$ CRT stands for Crappiest Run-Time, _wstat returns invalid time for files
	 * that were created when DST had different value and time adjustment setting
	 * is enabled. */
	hfile = CreateFileW(utf16_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);
	free(utf16_path);
	if(hfile != INVALID_HANDLE_VALUE)
	{
		FILETIME ctime, atime, mtime;
		if(GetFileTime(hfile, &ctime, &atime, &mtime))
		{
			buf->st_ctime = win_to_unix_time(ctime);
			buf->st_atime = win_to_unix_time(atime);
			buf->st_mtime = win_to_unix_time(mtime);
		}
		CloseHandle(hfile);
	}

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

char *
os_getcwd(char buf[], size_t size)
{
	char *utf8_buf;
	wchar_t wbuf[size];
	if(_wgetcwd(wbuf, sizeof(wbuf)) == NULL)
	{
		return NULL;
	}

	utf8_buf = utf8_from_utf16(wbuf);
	if(utf8_buf == NULL)
	{
		return NULL;
	}

	if(strlen(utf8_buf) + 1U > size)
	{
		free(utf8_buf);
		return NULL;
	}

	copy_str(buf, size, utf8_buf);
	free(utf8_buf);
	return buf;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
