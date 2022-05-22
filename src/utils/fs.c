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
#include <windows.h>
#include <ntdef.h>
#include <winioctl.h>

#include "utf8.h"
#endif

#include <sys/stat.h> /* S_* statbuf */
#include <sys/types.h> /* size_t mode_t */
#include <unistd.h> /* pathconf() readlink() */

#include <ctype.h> /* isalpha() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() remove() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strdup() strlen() strncmp() strncpy() */

#include "../compat/dtype.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../io/iop.h"
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
 * Automatically dereferences symbolic links.  Returns non-zero if path points
 * to a directory, otherwise zero is returned. */
static int
is_dir_fast(const char path[])
{
#ifdef __linux__
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
	/* Other systems report that "/path/to/file/." is a directory either for all
	 * files or for executable ones, always or sometimes... */
	return 0;
#endif
}

int
is_dir_empty(const char path[])
{
	DIR *dir;
	struct dirent *d;

	dir = os_opendir(path);
	if(dir == NULL)
	{
		return 1;
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
	if(is_path_absolute(filename))
	{
		LOG_ERROR_MSG("Passed absolute path where relative one is expected: %s",
				filename);
		path = NULL;
	}
	return path_exists_internal(path, filename, deref);
}

/* Checks whether path/file exists. If path is NULL, filename is assumed to
 * contain full path. */
static int
path_exists_internal(const char path[], const char filename[], int deref)
{
	char full[PATH_MAX + 1];
	if(path == NULL)
	{
		copy_str(full, sizeof(full), filename);
	}
	else
	{
		snprintf(full, sizeof(full), "%s/%s", path, filename);
	}

	/* At least on Windows extra trailing slash can mess up the check, so get rid
	 * of it. */
	if(!is_root_dir(full))
	{
		chosp(full);
	}

	if(!deref)
	{
		struct stat st;
		return os_lstat(full, &st) == 0;
	}
	return os_access(full, F_OK) == 0;
}

int
paths_are_same(const char s[], const char t[])
{
	char s_real[PATH_MAX + 1];
	char t_real[PATH_MAX + 1];

	if(os_realpath(s, s_real) != s_real || os_realpath(t, t_real) != t_real)
	{
		return (stroscmp(s, t) == 0);
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
	return (win_get_reparse_point_type(path) == IO_REPARSE_TAG_SYMLINK);
#endif
}

int
is_shortcut(const char path[])
{
#ifndef _WIN32
	return 0;
#else
	return (strcasecmp(get_ext(path), "lnk") == 0);
#endif
}

SymLinkType
get_symlink_type(const char path[])
{
	char cwd[PATH_MAX + 1];
	char linkto[PATH_MAX + NAME_MAX];
	int saved_errno;
	char *filename_copy;
	char *p;

	if(get_cwd(cwd, sizeof(cwd)) == NULL)
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

	p = os_realpath(filename_copy, linkto);
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
	char link_target[PATH_MAX + 1];
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
get_link_target(const char link[], char buf[], size_t buf_len)
{
#ifndef _WIN32
	char *filename;
	ssize_t len;

	if(buf_len == 0)
	{
		return -1;
	}

	filename = strdup(link);
	chosp(filename);

	len = readlink(filename, buf, buf_len - 1);

	free(filename);

	if(len == -1)
	{
		return -1;
	}

	buf[len] = '\0';
	return 0;
#else
	if(win_symlink_read(link, buf, buf_len) == 0)
	{
		return 0;
	}
	return win_shortcut_read(link, buf, buf_len);
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

	return (iop_mkdir(&args) == IO_RES_SUCCEEDED ? 0 : 1);
}

int
create_path(const char dir_name[], mode_t mode)
{
	return is_dir(dir_name) ? 1 : make_path(dir_name, mode);
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
list_regular_files(const char path[], char *list[], int *len)
{
	DIR *dir;
	struct dirent *d;

	dir = os_opendir(path);
	if(dir == NULL)
	{
		return list;
	}

	while((d = os_readdir(dir)) != NULL)
	{
		char full_path[PATH_MAX + 1];
		snprintf(full_path, sizeof(full_path), "%s/%s", path, d->d_name);

		if(is_regular_file(full_path))
		{
			*len = add_to_string_array(&list, *len, d->d_name);
		}
	}
	os_closedir(dir);

	return list;
}

char **
list_all_files(const char path[], int *len)
{
	DIR *dir;
	struct dirent *d;
	char **list = NULL;

	dir = os_opendir(path);
	if(dir == NULL)
	{
		*len = -1;
		return NULL;
	}

	*len = 0;
	while((d = os_readdir(dir)) != NULL)
	{
		if(!is_builtin_dir(d->d_name))
		{
			*len = add_to_string_array(&list, *len, d->d_name);
		}
	}
	os_closedir(dir);

	return list;
}

char **
list_sorted_files(const char path[], int *len)
{
	char **const list = list_all_files(path, len);
	if(*len > 0)
	{
		/* The check above is needed because *len might be negative. */
		safe_qsort(list, *len, sizeof(*list), &strossorter);
	}
	return list;
}

int
is_regular_file(const char path[])
{
	char path_real[PATH_MAX + 1];
	if(os_realpath(path, path_real) != path_real)
	{
		return 0;
	}

	return is_regular_file_noderef(path_real);
}

int
is_regular_file_noderef(const char path[])
{
#ifndef _WIN32
	struct stat s;
	return os_lstat(path, &s) == 0 && (s.st_mode & S_IFMT) == S_IFREG;
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
				/* Attempt to make sure that we can change the directory we are
				 * descending into. */
				(void)os_chmod(full_path, 0777);
				remove_dir_content(full_path);
				(void)os_rmdir(full_path);
			}
			else
			{
				(void)remove(full_path);
			}
			free(full_path);
		}
	}
	os_closedir(dir);
}

int
entry_is_link(const char path[], const struct dirent *dentry)
{
#ifndef _WIN32
	if(get_dirent_type(dentry, path) == DT_LNK)
	{
		return 1;
	}
#endif
	return is_symlink(path);
}

int
entry_is_dir(const char full_path[], const struct dirent *dentry)
{
#ifndef _WIN32
	const unsigned char type = get_dirent_type(dentry, full_path);
	return (type == DT_UNKNOWN)
	     ? is_directory(full_path, 0)
	     : type == DT_DIR;
#else
	const DWORD MASK = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY;
	const DWORD attrs = win_get_file_attrs(full_path);
	return attrs != INVALID_FILE_ATTRIBUTES
	    && (attrs & MASK) == FILE_ATTRIBUTE_DIRECTORY;
#endif
}

int
is_dirent_targets_dir(const char full_path[], const struct dirent *d)
{
#ifdef _WIN32
	return is_dir(full_path);
#else
	const unsigned char type = get_dirent_type(d, full_path);

	if(type == DT_UNKNOWN)
	{
		return is_dir(full_path);
	}

	return  type == DT_DIR
	    || (type == DT_LNK && get_symlink_type(full_path) != SLT_UNKNOWN);
#endif
}

int
is_in_subtree(const char path[], const char root[], int include_root)
{
	/* This variable must remain on this level because it can be being pointed
	 * to. */
	char path_copy[PATH_MAX + 1];
	if(!include_root)
	{
		copy_str(path_copy, sizeof(path_copy), path);
		remove_last_path_component(path_copy);
		path = path_copy;
	}

	char path_real[PATH_MAX + 1];
	if(os_realpath(path, path_real) != path_real)
	{
		return 0;
	}

	char root_real[PATH_MAX + 1];
	if(os_realpath(root, root_real) != root_real)
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
	if(case_sensitive_paths(src))
	{
		return 0;
	}

	return strcasecmp(src, dst) == 0 && strcmp(src, dst) != 0;
}

int
case_sensitive_paths(const char at[])
{
#if HAVE_DECL__PC_CASE_SENSITIVE
	return pathconf(at, _PC_CASE_SENSITIVE) != 0;
#elif !defined(_WIN32)
	return 1;
#else
	return 0;
#endif
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
	char find_pat[PATH_MAX + 1];
	wchar_t *utf16_path;
	HANDLE hfind;
	WIN32_FIND_DATAW ffd;

	snprintf(find_pat, sizeof(find_pat), "%s/*", path);
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
		if(client(utf8_name, &ffd, param) != 0)
		{
			break;
		}
		free(utf8_name);
	}
	while(FindNextFileW(hfind, &ffd));
	FindClose(hfind);

	return 0;
#endif
}

int
count_dir_items(const char path[])
{
	DIR *dir;
	struct dirent *d;
	int count;

	dir = os_opendir(path);
	if(dir == NULL)
	{
		return -1;
	}

	count = 0;
	while((d = os_readdir(dir)) != NULL)
	{
		if(!is_builtin_dir(d->d_name))
		{
			++count;
		}
	}
	os_closedir(dir);

	return count;
}

char *
get_cwd(char buf[], size_t size)
{
	if(os_getcwd(buf, size) == NULL)
	{
		return NULL;
	}
	return buf;
}

char *
save_cwd(void)
{
	char cwd[PATH_MAX + 1];
	if(os_getcwd(cwd, sizeof(cwd)) != cwd)
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

/* Checks if path (dereferenced for a symbolic link) is an existing directory.
 * Automatically dereferences symbolic links. */
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
	char buf[NAME_MAX + 1];
	char fs[16];
	if(is_unc_path(path))
	{
		int i = 4, j = 0;
		copy_str(buf, sizeof(buf), path);
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
/* vim: set cinoptions+=t0 filetype=c : */
