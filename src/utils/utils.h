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

#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef _WIN32
#include <windef.h>
#endif

#include <regex.h>

#include <sys/types.h> /* mode_t */
#ifndef _WIN32
#include <sys/wait.h> /* WEXITSTATUS() WIFEXITED() */
#endif

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */
#include <wchar.h> /* wchar_t ... */

#include "macros.h"

#ifdef _WIN32
#ifndef WEXITSTATUS
#define WEXITSTATUS(a) (a)
#endif

#ifndef WIFEXITED
#define WIFEXITED(a) 1
#endif

#define lstat stat
#endif

/* Regular expressions. */

int get_regexp_cflags(const char *pattern);
const char * get_regexp_error(int err, regex_t *re);

/* Program running. */

int my_system(char *command);
void _gnuc_noreturn run_from_fork(int pipe[2], int err, char *cmd);

/* Other functions. */

void get_perm_string(char *buf, int len, mode_t mode);
int is_on_slow_fs(const char *full_path);
/* Fills supplied buffer with user friendly representation of file size.
 * Returns non-zero in case resulting string is a shortened variant of size. */
int friendly_size_notation(uint64_t num, int str_size, char *str);
const char * enclose_in_dquotes(const char *str);
int my_chdir(const char *path);
/* Makes filename unique by adding an unique suffix to it.
 * Returns pointer to a statically allocated buffer */
const char * make_name_unique(const char *filename);
/* Finds command name in the command line and writes it to the buf.
 * Raw mode will preserve quotes on Windows.
 * Returns a pointer to the argument list. */
char * get_command_name(const char line[], int raw, size_t buf_len, char buf[]);

#ifndef _WIN32
int get_uid(const char *user, uid_t *uid);
int get_gid(const char *group, gid_t *gid);
int S_ISEXE(mode_t mode);
#else
int wcwidth(wchar_t c);
int wcswidth(const wchar_t *str, size_t len);
int exec_program(TCHAR *cmd);
/* Checks executable existence trying to add executable extensions if needed. */
int win_executable_exists(const char *path);
int is_win_executable(const char *name);
int is_vista_and_above(void);
const char *attr_str(DWORD attr);
const char *attr_str_long(DWORD attr);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
