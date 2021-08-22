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

#ifndef VIFM__COMPAT__OS_H__
#define VIFM__COMPAT__OS_H__

#include <sys/stat.h> /* S_* */
#include <dirent.h> /* DIR dirent opendir() readdir() closedir() */

#include <stdio.h> /* FILE */

/* Windows lacks support for UTF-8, that's why file system related functions
 * that work with paths are wrapped here.  Wrappers basically do transparent
 * string encoding conversion. */

#ifndef _WIN32

#include <sys/stat.h> /* mkdir() */
#include <unistd.h> /* access() chdir() chmod() getcwd() lstat() rename() */

#include <stdlib.h> /* realpath() system() */

#define os_access access
#define os_chdir chdir
#define os_chmod chmod
#define os_closedir closedir
#define os_fopen fopen
#define os_opendir opendir
#define os_readdir readdir
#define os_lstat lstat
#define os_mkdir mkdir
#define os_realpath realpath
#define os_rename rename
#define os_rmdir rmdir
#define os_stat stat
#define os_system system
#define os_tmpfile tmpfile
#define os_getcwd getcwd

int os_fdatasync(int fd);

#else

#include <fcntl.h> /* *_OK */
#include "../utils/utils.h"

/* Not straight forward for Windows and not very important. */
#define os_lstat os_stat

/* Windows has tmpfile(), but (prepare yourself) it requires administrative
 * privileges... */
#define os_tmpfile win_tmpfile

struct stat;

int os_access(const char pathname[], int mode);

int os_chdir(const char path[]);

int os_chmod(const char path[], int mode);

int os_closedir(DIR *dirp);

FILE * os_fopen(const char path[], const char mode[]);

DIR * os_opendir(const char name[]);

struct dirent * os_readdir(DIR *dirp);

int os_rename(const char oldpath[], const char newpath[]);

int os_rmdir(const char path[]);

int os_mkdir(const char pathname[], int mode);

/* Resolves the path to the real path without any symbolic links.  buf should be
 * at least PATH_MAX in length.  Returns resolved_path on success, otherwise
 * NULL is returned. */
char * os_realpath(const char path[], char resolved_path[]);

int os_stat(const char path[], struct stat *buf);

int os_system(const char command[]);

/* Retrieves current working directory.  Returns buf on success and NULL on
 * failure. */
char * os_getcwd(char buf[], size_t size);

#endif

#endif /* VIFM__COMPAT__OS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
