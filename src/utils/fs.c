/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
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

#include "fs.h"

#ifdef _WIN32
#define REQUIRED_WINVER 0x0500
#include "windefs.h"
#include <windows.h>
#include <ntdef.h>
#include <winioctl.h>
#endif

#include <sys/stat.h> /* statbuf stat() lstat() mkdir() */
#include <sys/types.h> /* size_t mode_t */
#include <dirent.h> /* DIR dirent opendir() readdir() closedir() */
#include <unistd.h> /* access() */

#include <assert.h> /* assert() */
#include <ctype.h> /* touuper() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() remove() rename() */
#include <stdlib.h> /* realpath() */
#include <string.h> /* strdup() strncmp() strncpy() */

#include "fs_limits.h"
#include "log.h"
#include "path.h"
#ifdef _WIN32
#include "str.h"
#endif
#include "string_array.h"
#include "utils.h"

static int path_exists_internal(const char *path, const char *filename);

int
is_dir(const char *path)
{
#ifndef _WIN32
	struct stat statbuf;
	if(stat(path, &statbuf) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't stat \"%s\"", path);
		log_cwd();
		return 0;
	}

	return S_ISDIR(statbuf.st_mode);
#else
	DWORD attr;

	if(is_path_absolute(path) && !is_unc_path(path))
	{
		char buf[] = {path[0], ':', '\\', '\0'};
		UINT type = GetDriveTypeA(buf);
		if(type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
			return 0;
	}

	attr = GetFileAttributesA(path);
	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		LOG_SERROR_MSG(errno, "Can't get attributes of \"%s\"", path);
		log_cwd();
		return 0;
	}

	return (attr & FILE_ATTRIBUTE_DIRECTORY);
#endif
}

int
is_valid_dir(const char *path)
{
	return is_dir(path) || is_unc_root(path);
}

int
path_exists(const char path[])
{
	if(!is_path_absolute(path))
	{
		LOG_ERROR_MSG("Passed relative path where absolute one is expected: %s",
				path);
	}
	return path_exists_internal(NULL, path);
}

int
path_exists_at(const char *path, const char *filename)
{
	return path_exists_internal(path, filename);
}

/* Checks whether path/file exists. If path is NULL, filename is assumed to
 * contain full path. */
static int
path_exists_internal(const char *path, const char *filename)
{
	const char *path_to_check;
	char full[PATH_MAX];
	if(path == NULL)
	{
		path_to_check = filename;
	}
	else
	{
		snprintf(full, sizeof(full), "%s/%s", path, filename);
		path_to_check = full;
	}
#ifndef _WIN32
	return access(path_to_check, F_OK) == 0;
#else
	if(is_path_absolute(path_to_check) && !is_unc_path(path_to_check))
	{
		if(!drive_exists(path_to_check[0]))
		{
			return 0;
		}
	}

	return (GetFileAttributesA(path_to_check) != INVALID_FILE_ATTRIBUTES);
#endif
}

int
check_link_is_dir(const char *filename)
{
	char linkto[PATH_MAX + NAME_MAX];
	int saved_errno;
	char *filename_copy;
	char *p;

	filename_copy = strdup(filename);
	chosp(filename_copy);

	p = realpath(filename, linkto);
	saved_errno = errno;

	free(filename_copy);

	if(p == linkto)
	{
		return is_dir(linkto);
	}
	else
	{
		LOG_SERROR_MSG(saved_errno, "Can't readlink \"%s\"", filename);
		log_cwd();
	}

	return 0;
}

int
get_link_target_abs(const char link[], const char cwd[], char buf[],
		size_t buf_len)
{
	char link_target[PATH_MAX];
	if(get_link_target(link, link_target, sizeof(link_target)) != 0)
	{
		return 1;
	}
	if(is_path_absolute(link_target))
	{
		strncpy(buf, link_target, buf_len);
		buf[buf_len - 1] = '\0';
	}
	else
	{
		snprintf(buf, buf_len, "%s/%s", cwd, link_target);
	}
	return 0;
}

int
get_link_target(const char *link, char *buf, size_t buf_len)
{
	LOG_FUNC_ENTER;

#ifndef _WIN32
	char *filename;
	ssize_t len;

	filename = strdup(link);
	chosp(filename);

	len = readlink(filename, buf, buf_len);

	free(filename);

	if(len == -1)
		return -1;

	buf[len] = '\0';
	return 0;
#else
	char filename[PATH_MAX];
	DWORD attr;
	HANDLE hfind;
	WIN32_FIND_DATAA ffd;
	HANDLE hfile;
	char rdb[2048];
	char *t;
	REPARSE_DATA_BUFFER *sbuf;
	WCHAR *path;

	attr = GetFileAttributes(link);
	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
		return -1;

	snprintf(filename, sizeof(filename), "%s", link);
	chosp(filename);
	hfind = FindFirstFileA(filename, &ffd);
	if(hfind == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}

	if(!FindClose(hfind))
	{
		LOG_WERROR(GetLastError());
	}

	if(ffd.dwReserved0 != IO_REPARSE_TAG_SYMLINK)
		return -1;

	hfile = CreateFileA(filename, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);
	if(hfile == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}

	if(!DeviceIoControl(hfile, FSCTL_GET_REPARSE_POINT, NULL, 0, rdb,
			sizeof(rdb), &attr, NULL))
	{
		LOG_WERROR(GetLastError());
		CloseHandle(hfile);
		return -1;
	}
	CloseHandle(hfile);

	sbuf = (REPARSE_DATA_BUFFER *)rdb;
	path = sbuf->SymbolicLinkReparseBuffer.PathBuffer;
	path[sbuf->SymbolicLinkReparseBuffer.PrintNameOffset/sizeof(WCHAR) +
			sbuf->SymbolicLinkReparseBuffer.PrintNameLength/sizeof(WCHAR)] = L'\0';
	t = to_multibyte(path +
			sbuf->SymbolicLinkReparseBuffer.PrintNameOffset/sizeof(WCHAR));
	if(strncmp(t, "\\??\\", 4) == 0)
		strncpy(buf, t + 4, buf_len);
	else
		strncpy(buf, t, buf_len);
	buf[buf_len - 1] = '\0';
	free(t);
	to_forward_slash(buf);
	return 0;
#endif
}

int
make_dir(const char *dir_name, mode_t mode)
{
#ifndef _WIN32
	return mkdir(dir_name, mode);
#else
	return mkdir(dir_name);
#endif
}

int
symlinks_available(void)
{
#ifndef _WIN32
	return 1;
#else
	return is_vista_and_above();
#endif
}

int
directory_accessible(const char *path)
{
	return (path_exists(path) && access(path, X_OK) == 0) || is_unc_root(path);
}

int
is_dir_writable(const char path[])
{
	if(!is_unc_root(path))
	{
#ifdef _WIN32
		HANDLE hdir;
		if(is_on_fat_volume(path))
			return 1;
		hdir = CreateFileA(path, GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
		if(hdir != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hdir);
			return 1;
		}
#else
	if(access(path, W_OK) == 0)
		return 1;
#endif
	}

	return 0;
}

uint64_t
get_file_size(const char *path)
{
#ifndef _WIN32
	struct stat st;
	if(lstat(path, &st) == 0)
	{
		return (size_t)st.st_size;
	}
	return 0;
#else
	HANDLE hfile;
	LARGE_INTEGER size;

	hfile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hfile == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return 0;
	}

	if(GetFileSizeEx(hfile, &size))
	{
		CloseHandle(hfile);
		return size.QuadPart;
	}
	CloseHandle(hfile);
	return 0;
#endif
}

char **
list_regular_files(const char path[], int *len)
{
	DIR *dir;
	char **list = NULL;
	*len = 0;

	if((dir = opendir(path)) != NULL)
	{
		struct dirent *d;
		while((d = readdir(dir)) != NULL)
		{
			char full_path[PATH_MAX];
			snprintf(full_path, sizeof(full_path), "%s/%s", path, d->d_name);

			if(is_regular_file(full_path))
			{
				*len = add_to_string_array(&list, *len, 1, d->d_name);
			}
		}
		closedir(dir);
	}

	return list;
}

int
is_regular_file(const char path[])
{
#ifndef _WIN32
	struct stat s;
	return stat(path, &s) == 0 && (s.st_mode & S_IFMT) == S_IFREG;
#else
	const DWORD attrs = GetFileAttributesA(path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		return 0;
	}
	return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0UL;
#endif
}

int
rename_file(const char src[], const char dst[])
{
	int error;
#ifdef _WIN32
	(void)remove(dst);
#endif
	if((error = rename(src, dst)))
	{
		LOG_SERROR_MSG(errno, "Rename operation failed: {%s => %s}", src, dst);
	}
	return error != 0;
}

#ifdef _WIN32

char *
realpath(const char *path, char *buf)
{
	if(get_link_target(path, buf, PATH_MAX) == 0)
		return buf;

	buf[0] = '\0';
	if(!is_path_absolute(path) && GetCurrentDirectory(PATH_MAX, buf) > 0)
	{
		to_forward_slash(buf);
		chosp(buf);
		strcat(buf, "/");
	}

	strcat(buf, path);
	return buf;
}

int
S_ISLNK(mode_t mode)
{
	return 0;
}

int
readlink(const char *path, char *buf, size_t len)
{
	return -1;
}

int
is_on_fat_volume(const char *path)
{
	char buf[NAME_MAX];
	char fs[16];
	if(is_unc_path(path))
	{
		int i = 4, j = 0;
		snprintf(buf, sizeof(buf), "%s", path);
		while(i > 0 && buf[j] != '\0')
			if(buf[j++] == '/')
				i--;
		if(i == 0)
			buf[j - 1] = '\0';
	}
	else
	{
		strcpy(buf, "x:\\");
		buf[0] = path[0];
	}
	if(GetVolumeInformationA(buf, NULL, 0, NULL, NULL, NULL, fs, sizeof(fs)))
	{
		if(strncasecmp(fs, "fat", 3) == 0)
			return 1;
	}
	return 0;
}

/* Checks specified drive for existence */
int
drive_exists(char letter)
{
	const char drive[] = {letter, ':', '\\', '\0'};
	int type = GetDriveTypeA(drive);

	switch(type)
	{
		case DRIVE_CDROM:
		case DRIVE_REMOTE:
		case DRIVE_RAMDISK:
		case DRIVE_REMOVABLE:
		case DRIVE_FIXED:
			return 1;

		case DRIVE_UNKNOWN:
		case DRIVE_NO_ROOT_DIR:
		default:
			return 0;
	}
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
