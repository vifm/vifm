/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
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

typedef struct Fuse_List {
	char source_file_name[PATH_MAX];
	char source_file_dir[PATH_MAX];
	char mount_point[PATH_MAX];
	int mount_point_id;
	struct Fuse_List *next;
} Fuse_List;

extern struct Fuse_List *fuse_mounts;

int S_ISEXE(mode_t mode);
void chomp(char *text);
void chosp(char *text);
int is_dir(const char *file);
char * escape_filename(const char *string, int quote_percent);
size_t get_char_width(const char* string);
size_t get_normal_utf8_string_length(const char *string);
size_t get_real_string_width(char *string, size_t max_len);
size_t get_normal_utf8_string_widthn(const char *string, size_t max);
size_t get_normal_utf8_string_width(const char *string);
size_t get_utf8_string_length(const char *string);
size_t get_utf8_overhead(const char *string);
wchar_t * to_wide(const char *s);
void _gnuc_noreturn run_from_fork(int pipe[2], int err, char *cmd);
wchar_t * my_wcsdup(const wchar_t *ws);
char * uchar2str(wchar_t *c, size_t *len);
void get_perm_string(char *buf, int len, mode_t mode);
int path_starts_with(const char *path, const char *begin);

/* When list is NULL returns maximum number of lines, otherwise returns number
 * of filled lines */
int fill_version_info(char **list);

void friendly_size_notation(unsigned long long num, int str_size, char *str);
int check_link_is_dir(const char *filename);
/* Returns new size */
int add_to_string_array(char ***array, int len, int count, ...);
int is_in_string_array(char **array, size_t len, const char *key);
int is_in_string_array_case(char **array, size_t len, const char *key);
int string_array_pos(char **array, size_t len, const char *key);
int string_array_pos_case(char **array, size_t len, const char *key);
void free_string_array(char **array, size_t len);
void free_wstring_array(wchar_t **array, size_t len);
void canonicalize_path(const char *directory, char *buf, size_t buf_size);
const char * make_rel_path(const char *path, const char *base);
/* Returns pointer to a statically allocated buffer */
const char *replace_home_part(const char *directory);
const char *find_tail(const char *path);
char * expand_tilde(char *path);
int get_regexp_cflags(const char *pattern);
const char * get_regexp_error(int err, regex_t *re);
int is_root_dir(const char *path);
int is_unc_root(const char *path);
int is_path_absolute(const char *path);
int ends_with(const char* str, const char* suffix);
char * strchar2str(const char *str);
char * to_multibyte(const wchar_t *s);
int get_link_target(const char *link, char *buf, size_t buf_len);
void strtoupper(char *s);
const char * enclose_in_dquotes(const char *str);

#ifndef _WIN32
int get_uid(const char *user, uid_t *uid);
int get_gid(const char *group, gid_t *gid);
#else
int wcwidth(wchar_t c);
int wcswidth(const wchar_t *str, size_t len);
int S_ISLNK(mode_t mode);
int readlink(const char *path, char *buf, size_t len);
char * realpath(const char *path, char *buf);
int is_unc_path(const char *path);
int exec_program(TCHAR *cmd);
int is_win_executable(const char *name);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
