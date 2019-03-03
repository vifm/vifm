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

#ifndef VIFM__UTILS__PATH_H__
#define VIFM__UTILS__PATH_H__

#include <stddef.h> /* size_t */

/* String with path items separator supported by the system. */
#ifndef _WIN32
#define PATH_SEPARATORS "/"
#else
#define PATH_SEPARATORS "/\\"
#endif

/* Various functions to work with paths */

/* Like chomp() but removes trailing slashes. */
void chosp(char path[]);

int ends_with_slash(const char *path);

/* Checks if the path starts with given prefix. */
int path_starts_with(const char path[], const char prefix[]);

/* Checks if two paths are equal, nothing is dereferenced.  Returns non-zero for
 * same paths, otherwise zero is returned. */
int paths_are_equal(const char s[], const char t[]);

/* Removes excess slashes, "../" and "./" from the path.  buf will always
 * contain trailing forward slash. */
void canonicalize_path(const char directory[], char buf[], size_t buf_size);

/* Converts the path to make it relative to the base path.  Returns pointer to a
 * statically allocated buffer. */
const char * make_rel_path(const char path[], const char base[]);

int is_path_absolute(const char *path);

int is_root_dir(const char *path);

int is_unc_root(const char *path);

/* Escapes the string for the purpose of inserting it into the shell or
 * command-line.  type == 1 enables prepending percent sign with a percent
 * sign and not escaping newline, because we do only worse.  type == 2 only
 * skips escaping of newline.  Returns new string, caller should free it. */
char * shell_like_escape(const char string[], int type);

/* Replaces leading path to home directory with a tilde, trims trailing slash.
 * Returns pointer to a statically allocated buffer of size PATH_MAX. */
char * replace_home_part(const char path[]);

/* Same as replace_home_part(), but doesn't perform trailing slash trimming. */
char * replace_home_part_strict(const char path[]);

/* Expands tilde in the front of the path.  Does nothing for paths without
 * tilde.  Returns newly allocated string without tilde. */
char * expand_tilde(const char path[]);

/* Expands tilde in the front of the path.  Can free the path.  Does nothing for
 * paths without tilde.  Returns the path or newly allocated string without
 * tilde. */
char * replace_tilde(char path[]);

/* Find beginning of the last component in the path ignoring trailing
 * slashes. */
char * get_last_path_component(const char path[]);

/* Truncates last component from the path. */
void remove_last_path_component(char path[]);

/* Checks if path could refer to a real file system object. */
int is_path_well_formed(const char *path);

/* Checks if path could refer to a real file system object. And modifies path to
 * something meaningful if the check failed. */
void ensure_path_well_formed(char *path);

/* Ensures that path to a file is of canonic absolute form.  No trailing slash
 * in the buffer except for the root path.  Canonic paths must be absolute, so
 * if input path is relative base is prepended to it. */
void to_canonic_path(const char path[], const char base[], char buf[],
		size_t buf_len);

/* Checks if path contains slash (also checks for backward slash on Windows). */
int contains_slash(const char *path);

/* Returns position of the last slash (including backward slash on Windows) in
 * the path. */
char * find_slashr(const char *path);

/* Removes extension part from the path and returns a pointer to the first
 * character of the extension part. */
char * cut_extension(char path[]);

/* Splits path into root and extension parts.  Sets *root_len to length of the
 * root part and *ext_pos to beginning of last extension. */
void split_ext(char path[], int *root_len, const char **ext_pos);

/* Gets extension from the file path.  Returns pointer to beginning of the
 * extension, which points to trailing null character for an empty extension. */
char * get_ext(const char path[]);

/* Removes file name from path.  Does nothing if path refers to a directory. */
void exclude_file_name(char path[]);

/* Checks whether path equals to ".." or "../".  Returns non-zero if it is,
 * otherwise zero is returned. */
int is_parent_dir(const char path[]);

/* Checks whether path equals to "." or "..".  Returns non-zero if it is,
 * otherwise zero is returned. */
int is_builtin_dir(const char name[]);

/* Finds path to executable using all directories from PATH environment
 * variable.  Uses executable extensions on Windows.  Puts discovered path to
 * the path buffer if it's not NULL.  Returns zero on success, otherwise
 * non-zero is returned. */
int find_cmd_in_path(const char cmd[], size_t path_len, char path[]);

/* Generates file name inside temporary directory. */
void generate_tmp_file_name(const char prefix[], char buf[], size_t buf_len);

/* Uses environment variables to determine the correct place.  Returns path to
 * tmp directory. */
const char * get_tmpdir(void);

/* Concatenates two paths while making sure to not add extra slash between
 * them. */
void build_path(char buf[], size_t buf_len, const char p1[], const char p2[]);

/* Concatenates two paths while making sure to not add extra slash between
 * them.  Returns newly allocated string. */
char * join_paths(const char p1[], const char p2[]);

#ifdef _WIN32

int is_unc_path(const char *path);

/* Replaces slashes that are used by the system with forward slashes used
 * internally. */
void system_to_internal_slashes(char path[]);

/* Replaces forward slashes used internally with slashes that are used by the
 * system. */
void internal_to_system_slashes(char path[]);

#else

#define is_unc_path(path) (0)
#define system_to_internal_slashes(path) ((void)0)
#define internal_to_system_slashes(path) ((void)0)

#endif

#endif /* VIFM__UTILS__PATH_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
