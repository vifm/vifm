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

#include <limits.h> /* PATH_MAX */
#include <sys/types.h> /* mode_t */
#include <wchar.h> /* wchar_t ... */

#include "macros.h"

#ifndef WEXITSTATUS
#define WEXITSTATUS(a) (a)
#endif

#ifndef WIFEXITED
#define WIFEXITED(a) 1
#endif

#ifdef _WIN32
#define lstat stat
#endif

typedef struct fuse_mount_t
{
	char source_file_name[PATH_MAX];
	char source_file_dir[PATH_MAX];
	char mount_point[PATH_MAX];
	int mount_point_id;
	struct fuse_mount_t *next;
}fuse_mount_t;

extern struct fuse_mount_t *fuse_mounts;

void chomp(char *text);
void chosp(char *text);
int is_dir(const char *file);
/* if path is NULL, file assumed to contain full path */
int file_exists(const char *path, const char *file);
int my_system(char *command);
char * escape_filename(const char *string, int quote_percent);
wchar_t * to_wide(const char *s);
void _gnuc_noreturn run_from_fork(int pipe[2], int err, char *cmd);
wchar_t * my_wcsdup(const wchar_t *ws);
void get_perm_string(char *buf, int len, mode_t mode);
int path_starts_with(const char *path, const char *begin);
int my_chdir(const char *path);
int is_on_slow_fs(const char *full_path);

/* When list is NULL returns maximum number of lines, otherwise returns number
 * of filled lines */
int fill_version_info(char **list);

void friendly_size_notation(unsigned long long num, int str_size, char *str);
int check_link_is_dir(const char *filename);
void canonicalize_path(const char *directory, char *buf, size_t buf_size);
const char * make_rel_path(const char *path, const char *base);
/* Returns pointer to a statically allocated buffer */
char *replace_home_part(const char *directory);
const char *find_tail(const char *path);
char * expand_tilde(char *path);
int get_regexp_cflags(const char *pattern);
const char * get_regexp_error(int err, regex_t *re);
int is_root_dir(const char *path);
int is_unc_root(const char *path);
int is_path_absolute(const char *path);
int ends_with(const char* str, const char* suffix);
char * to_multibyte(const wchar_t *s);
int get_link_target(const char *link, char *buf, size_t buf_len);
void strtolower(char *s);
const char * enclose_in_dquotes(const char *str);
void set_term_title(const char *full_path);
const char *get_mode_str(mode_t mode);
int symlinks_available(void);
int make_dir(const char *dir_name, mode_t mode);
int pathcmp(const char *file_name_a, const char *file_name_b);
int pathncmp(const char *file_name_a, const char *file_name_b, size_t n);

/* Various string functions. */
void break_at(char *str, char c);
void break_atr(char *str, char c);
char * skip_non_whitespace(const char *str);
char * skip_whitespace(const char *str);

/* File stream reading related functions, that treat all eols (unix, mac, dos)
 * right. */

/* Reads line from file stream. */
char * get_line(FILE *fp, char *buf, size_t bufsz);
/* Returns next character from file stream. */
int get_char(FILE *fp);
/* Skips file stream content until and including eol character. */
void skip_until_eol(FILE *fp);
/* Removes eol symbols from file stream. */
void remove_eol(FILE *fp);

/* Environment variables related functions */

/* Returns environment variable value or NULL if it doesn't exist */
const char * env_get(const char *name);
/* Sets new value of environment variable or creates it if it doesn't exist */
void env_set(const char *name, const char *value);
/* Removes environment variable */
void env_remove(const char *name);

#ifndef _WIN32
int get_uid(const char *user, uid_t *uid);
int get_gid(const char *group, gid_t *gid);
int S_ISEXE(mode_t mode);
#else
int wcwidth(wchar_t c);
int wcswidth(const wchar_t *str, size_t len);
int S_ISLNK(mode_t mode);
int readlink(const char *path, char *buf, size_t len);
char * realpath(const char *path, char *buf);
int is_unc_path(const char *path);
int exec_program(TCHAR *cmd);
int is_win_executable(const char *name);
int is_vista_and_above(void);
void to_forward_slash(char *path);
void to_back_slash(char *path);
int is_on_fat_volume(const char *path);
const char *attr_str(DWORD attr);
const char *attr_str_long(DWORD attr);
int drive_exists(TCHAR letter);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
