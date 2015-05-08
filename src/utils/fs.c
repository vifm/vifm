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

#include "utf8.h"
#endif

#include <sys/stat.h> /* S_* statbuf */
#include <sys/types.h> /* size_t mode_t */
#include <unistd.h> /* getcwd() readlink() */

#include <errno.h> /* errno */
#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() remove() */
#include <stdlib.h> /* free() realpath() */
#include <string.h> /* strcpy() strdup() strlen() strncmp() strncpy() */

#include "../compat/os.h"
#include "../io/iop.h"
#include "fs_limits.h"
#include "log.h"
#include "path.h"
#include "str.h"
#include "string_array.h"
#include "utils.h"

static int is_dir_fast(const char path[]);
static int path_exists_internal(const char path[], const char filename[],
		int deref);

#ifndef _WIN32
static int is_directory(const char path[], int dereference_links);
#else
static DWORD win_get_file_attrs(const char path[]);
#endif

int
is_dir(const char path[])
{
	if(is_dir_fast(path))
	{
		return 1;
	}

#ifndef _WIN32
	return is_directory(path, 1);
#else
	{
		const DWORD attrs = win_get_file_attrs(path);
		return attrs != INVALID_FILE_ATTRIBUTES
		    && (attrs & FILE_ATTRIBUTE_DIRECTORY);
	}
#endif
}

/* Checks if path is an existing directory faster than is_dir() does this at the
 * cost of less accurate results (fails on non-sufficient rights).
 * Automatically deferences symbolic links.  Returns non-zero if path points to
 * a directory, otherwise zero is returned. */
static int
is_dir_fast(const char path[])
{
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(BSD)
	/* Optimization idea: is_dir() ends up using stat() call, which in turn has
	 * to:
	 *  1) resolve path to an inode number;
	 *  2) find and load inode for the directory.
	 * Checking "path/." for existence is a "hack" to omit 2).
	 * Negative answer of this method doesn't guarantee directory absence, but
	 * positive answer provides correct answer faster than is_dir() would. */

	const size_t len = strlen(path);
	char path_to_selfref[len + 1 + 1 + 1];

	strcpy(path_to_selfref, path);
	path_to_selfref[len] = '/';
	path_to_selfref[len + 1] = '.';
	path_to_selfref[len + 2] = '\0';

	return os_access(path_to_selfref, F_OK) == 0;
#else
	/* Some systems report that "/path/to/file/." is directory... */
	return 0;
#endif
}

int
is_dir_empty(const char path[])
{
	DIR *dir;
	struct dirent *d;

	if((dir = os_opendir(path)) == NULL)
	{
		return 0;
	}

	while((d = os_readdir(dir)) != NULL)
	{
		if(!is_builtin_dir(d->d_name))
		{
			break;
		}
	}
	os_closedir(dir);

	return d == NULL;
}

int
is_valid_dir(const char *path)
{
	return is_dir(path) || is_unc_root(path);
}

int
path_exists(const char path[], int deref)
{
	if(!is_path_absolute(path))
	{
		LOG_ERROR_MSG("Passed relative path where absolute one is expected: %s",
				path);
	}
	return path_exists_internal(NULL, path, deref);
}

int
path_exists_at(const char path[], const char filename[], int deref)
{
	return path_exists_internal(path, filename, deref);
}

/* Checks whether path/file exists. If path is NULL, filename is assumed to
 * contain full path. */
static int
path_exists_internal(const char path[], const char filename[], int deref)
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

	if(!deref)
	{
		struct stat st;
		return os_lstat(path_to_check, &st) == 0;
	}
	return os_access(path_to_check, F_OK) == 0;
}

int
paths_are_same(const char s[], const char t[])
{
	char s_real[PATH_MAX];
	char t_real[PATH_MAX];

	if(realpath(s, s_real) != s_real)
	{
		return 0;
	}
	if(realpath(t, t_real) != t_real)
	{
		return 0;
	}
	return (stroscmp(s_real, t_real) == 0);
}

int
is_symlink(const char path[])
{
#ifndef _WIN32
	struct stat st;
	return os_lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
#else
	char filename[PATH_MAX];
	DWORD attr;
	wchar_t *utf16_filename;
	HANDLE hfind;
	WIN32_FIND_DATAW ffd;

	attr = win_get_file_attrs(path);
	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return 0;
	}

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
	{
		return 0;
	}

	copy_str(filename, sizeof(filename), path);
	chosp(filename);

	utf16_filename = utf8_to_utf16(path);
	hfind = FindFirstFileW(utf16_filename, &ffd);
	free(utf16_filename);

	if(hfind == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return 0;
	}

	if(!FindClose(hfind))
	{
		LOG_WERROR(GetLastError());
	}

	return ffd.dwReserved0 == IO_REPARSE_TAG_SYMLINK;
#endif
}

SymLinkType
get_symlink_type(const char path[])
{
	char cwd[PATH_MAX];
	char linkto[PATH_MAX + NAME_MAX];
	int saved_errno;
	char *filename_copy;
	char *p;

	if(getcwd(cwd, sizeof(cwd)) == NULL)
	{
		/* getcwd() failed, just use "." rather than fail. */
		strcpy(cwd, ".");
	}

	/* Use readlink() (in get_link_target_abs) before realpath() to check for
	 * target at slow file system.  realpath() doesn't fit in this case as it
	 * resolves chains of symbolic links and we want to try only the first one. */
	if(get_link_target_abs(path, cwd, linkto, sizeof(linkto)) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't readlink \"%s\"", path);
		log_cwd();
		return SLT_UNKNOWN;
	}
	if(refers_to_slower_fs(path, linkto))
	{
		return SLT_SLOW;
	}

	filename_copy = strdup(path);
	chosp(filename_copy);

	p = realpath(filename_copy, linkto);
	saved_errno = errno;

	free(filename_copy);

	if(p == linkto)
	{
		return is_dir(linkto) ? SLT_DIR : SLT_UNKNOWN;
	}

	LOG_SERROR_MSG(saved_errno, "Can't realpath \"%s\"", path);
	log_cwd();
	return SLT_UNKNOWN;
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
	wchar_t *utf16_filename;
	HANDLE hfile;
	char rdb[2048];
	char *t;
	REPARSE_DATA_BUFFER *sbuf;
	WCHAR *path;

	if(!is_symlink(link))
	{
		return -1;
	}

	copy_str(filename, sizeof(filename), link);
	chosp(filename);

	utf16_filename = utf8_to_utf16(filename);
	hfile = CreateFileW(utf16_filename, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	free(utf16_filename);

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
make_path(const char dir_name[], mode_t mode)
{
	io_args_t args = {
		.arg1.path = dir_name,
		.arg2.process_parents = 1,
		.arg3.mode = mode,
	};

	return iop_mkdir(&args);
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
case_insensitive_paths(void)
{
#ifndef _WIN32
	return 0;
#else
	return 1;
#endif
}

int
has_atomic_file_replace(void)
{
#ifndef _WIN32
	return 1;
#else
	return 0;
#endif
}

int
directory_accessible(const char path[])
{
	return os_access(path, X_OK) == 0 || is_unc_root(path);
}

int
is_dir_writable(const char path[])
{
	if(!is_unc_root(path))
	{
#ifdef _WIN32
		HANDLE hdir;
		wchar_t *utf16_path;

		if(is_on_fat_volume(path))
		{
			return 1;
		}

		utf16_path = utf8_to_utf16(path);
		hdir = CreateFileW(utf16_path, GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
		free(utf16_path);

		if(hdir != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hdir);
			return 1;
		}
#else
	if(os_access(path, W_OK) == 0)
		return 1;
#endif
	}

	return 0;
}

uint64_t
get_file_size(const char path[])
{
#ifndef _WIN32
	struct stat st;
	if(os_lstat(path, &st) == 0)
	{
		return (uint64_t)st.st_size;
	}
	return 0;
#else
	wchar_t *utf16_path;
	int ok;
	WIN32_FILE_ATTRIBUTE_DATA attrs;
	LARGE_INTEGER size;

	utf16_path = utf8_to_utf16(path);
	ok = GetFileAttributesExW(utf16_path, GetFileExInfoStandard, &attrs);
	free(utf16_path);
	if(!ok)
	{
		LOG_WERROR(GetLastError());
		return 0;
	}

	size.u.LowPart = attrs.nFileSizeLow;
	size.u.HighPart = attrs.nFileSizeHigh;
	return size.QuadPart;
#endif
}

char **
list_regular_files(const char path[], char *extension, int *len)
{
	DIR *dir;
	char **list = NULL;
	*len = 0;

	if((dir = os_opendir(path)) != NULL)
	{
		struct dirent *d;
		while((d = os_readdir(dir)) != NULL)
		{
			char full_path[PATH_MAX];

			if (extension != NULL)
			{
				char *file_extension;
				file_extension = strchr(d->d_name, '.');
				if(file_extension == NULL)
					continue;
				if(strcmp(file_extension, extension) != 0)
					continue;
			}
			snprintf(full_path, sizeof(full_path), "%s/%s", path, d->d_name);

			if(is_regular_file(full_path))
			{
				*len = add_to_string_array(&list, *len, 1, d->d_name);
			}
		}
		os_closedir(dir);
	}

	return list;
}

int
is_regular_file(const char path[])
{
#ifndef _WIN32
	struct stat s;
	return os_stat(path, &s) == 0 && (s.st_mode & S_IFMT) == S_IFREG;
#else
	const DWORD attrs = win_get_file_attrs(path);
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
	if((error = os_rename(src, dst)))
	{
		LOG_SERROR_MSG(errno, "Rename operation failed: {%s => %s}", src, dst);
	}
	return error != 0;
}

void
remove_dir_content(const char path[])
{
	DIR *dir;
	struct dirent *d;

	dir = os_opendir(path);
	if(dir == NULL)
	{
		return;
	}

	while((d = os_readdir(dir)) != NULL)
	{
		if(!is_builtin_dir(d->d_name))
		{
			char *const full_path = format_str("%s/%s", path, d->d_name);
			if(entry_is_dir(full_path, d))
			{
				remove_dir_content(full_path);
			}
			(void)remove(full_path);
			free(full_path);
		}
	}
	os_closedir(dir);
}

int
entry_is_link(const char path[], const struct dirent* dentry)
{
#ifndef _WIN32
	if(dentry->d_type == DT_LNK)
	{
		return 1;
	}
#endif
	return is_symlink(path);
}

int
entry_is_dir(const char full_path[], const struct dirent* dentry)
{
#ifndef _WIN32
	return (dentry->d_type == DT_UNKNOWN)
	     ? is_directory(full_path, 0)
	     : dentry->d_type == DT_DIR;
#else
	const DWORD MASK = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY;
	const DWORD attrs = win_get_file_attrs(full_path);
	return attrs != INVALID_FILE_ATTRIBUTES
	    && (attrs & MASK) == FILE_ATTRIBUTE_DIRECTORY;
#endif
}

int
is_dirent_targets_dir(const struct dirent *d)
{
#ifdef _WIN32
	return is_dir(d->d_name);
#else
	if(d->d_type == DT_UNKNOWN)
	{
		return is_dir(d->d_name);
	}

	return  d->d_type == DT_DIR
	    || (d->d_type == DT_LNK && get_symlink_type(d->d_name) != SLT_UNKNOWN);
#endif
}

int
is_in_subtree(const char path[], const char root[])
{
	char path_copy[PATH_MAX];
	char path_real[PATH_MAX];
	char root_real[PATH_MAX];

	copy_str(path_copy, sizeof(path_copy), path);
	remove_last_path_component(path_copy);

	if(realpath(path_copy, path_real) != path_real)
	{
		return 0;
	}

	if(realpath(root, root_real) != root_real)
	{
		return 0;
	}

	return path_starts_with(path_real, root_real);
}

int
are_on_the_same_fs(const char s[], const char t[])
{
	struct stat s_stat, t_stat;
	if(os_lstat(s, &s_stat) != 0 || os_lstat(t, &t_stat) != 0)
	{
		return 0;
	}

	return s_stat.st_dev == t_stat.st_dev;
}

int
is_case_change(const char src[], const char dst[])
{
	if(!case_insensitive_paths())
	{
		return 0;
	}

	return strcasecmp(src, dst) == 0 && strcmp(src, dst) != 0;
}

int
enum_dir_content(const char path[], dir_content_client_func client, void *param)
{
#ifndef _WIN32
	DIR *dir;
	struct dirent *d;

	if((dir = os_opendir(path)) == NULL)
	{
		return -1;
	}

	while((d = os_readdir(dir)) != NULL)
	{
		if(client(d->d_name, d, param) != 0)
		{
			break;
		}
	}
	os_closedir(dir);

	return 0;
#else
	char find_pat[PATH_MAX];
	wchar_t *utf16_path;
	HANDLE hfind;
	WIN32_FIND_DATAW ffd;

	snprintf(find_pat, sizeof(find_pat), "%s/""*", path);
	utf16_path = utf8_to_utf16(find_pat);
	hfind = FindFirstFileW(utf16_path, &ffd);
	free(utf16_path);

	if(hfind == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	do
	{
		char *const utf8_name = utf8_from_utf16(ffd.cFileName);
		client(utf8_name, &ffd, param);
		free(utf8_name);
	}
	while(FindNextFileW(hfind, &ffd));
	FindClose(hfind);

	return 0;
#endif
}

char *
get_cwd(void)
{
	char cwd[PATH_MAX];
	if(getcwd(cwd, sizeof(cwd)) != cwd)
	{
		return NULL;
	}
	return strdup(cwd);
}

void
restore_cwd(char saved_cwd[])
{
	if(saved_cwd != NULL)
	{
		(void)vifm_chdir(saved_cwd);
		free(saved_cwd);
	}
}

#ifndef _WIN32

/* Checks if path (dereferencer or not symbolic link) is an existing directory.
 * Automatically deferences symbolic links. */
static int
is_directory(const char path[], int dereference_links)
{
	struct stat statbuf;
	if((dereference_links ? &os_stat : &os_lstat)(path, &statbuf) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't stat \"%s\"", path);
		log_cwd();
		return 0;
	}

	return S_ISDIR(statbuf.st_mode);
}

#else

/* Obtains attributes of a file.  Skips check for unmounted disks.  Returns the
 * attributes, which is INVALID_FILE_ATTRIBUTES on error. */
static DWORD
win_get_file_attrs(const char path[])
{
	DWORD attr;
	wchar_t *utf16_path;

	if(is_path_absolute(path) && !is_unc_path(path))
	{
		if(!drive_exists(path[0]))
		{
			return INVALID_FILE_ATTRIBUTES;
		}
	}

	utf16_path = utf8_to_utf16(path);
	attr = GetFileAttributesW(utf16_path);
	free(utf16_path);

	return attr;
}

char *
realpath(const char path[], char buf[])
{
	const char *const resolved_path = win_resolve_mount_points(path);

	if(!is_path_absolute(resolved_path))
	{
		/* Try to compose absolute path. */
		char cwd[PATH_MAX];
		if(getcwd(cwd, sizeof(cwd)) == cwd)
		{
			to_forward_slash(cwd);
			snprintf(buf, PATH_MAX, "%s/%s", cwd, resolved_path);
			return buf;
		}
	}

	copy_str(buf, PATH_MAX, resolved_path);
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

int
drive_exists(char letter)
{
	const wchar_t drive[] = { (wchar_t)letter, L':', L'\\', L'\0' };
	const int drive_type = GetDriveTypeW(drive);

	switch(drive_type)
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

int
is_win_symlink(uint32_t attr, uint32_t tag)
{
	return (attr & FILE_ATTRIBUTE_REPARSE_POINT)
	    && (tag == IO_REPARSE_TAG_SYMLINK);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
