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

/* Various functions to work with paths */

void chosp(char *path);
int ends_with_slash(const char *path);
/* Checks if path starts with given base path. */
int path_starts_with(const char *path, const char *begin);
/* Checks if two paths point to the same location, treating ending slashes
 * correctly.  Returns non-zero for same path. */
int paths_are_equal(const char s[], const char t[]);
/* Removes excess slashes, "../" and "./" from the path.  buf will always
 * contain trailing forward slash. */
void canonicalize_path(const char directory[], char buf[], size_t buf_size);
const char * make_rel_path(const char *path, const char *base);
int is_path_absolute(const char *path);
int is_root_dir(const char *path);
int is_unc_root(const char *path);
char * escape_filename(const char *string, int quote_percent);
/* Returns pointer to a statically allocated buffer */
char * replace_home_part(const char *directory);
char * expand_tilde(char path[]);
/* Find beginning of the last component in the path ignoring trailing
 * slashes. */
char * get_last_path_component(const char path[]);
/* Modifies path. */
void remove_last_path_component(char *path);
/* Checks if path could refer to a real file system object. */
int is_path_well_formed(const char *path);
/* Checks if path could refer to a real file system object. And modifies path to
 * something meaningful if the check failed. */
void ensure_path_well_formed(char *path);
/* Checks if path contains slash (also checks for backward slash on Windows). */
int contains_slash(const char *path);
/* Returns position of the last slash (including backward slash on Windows) in
 * the path. */
char * find_slashr(const char *path);
/* Removes extension part from the path and returns a pointer to the first
 * character of the extension part. */
char * extract_extension(char *path);
/* Removes file name from path. */
void exclude_file_name(char *path);
/* Checks wether path equals to ".." or "../".  Returns non-zero if it is,
 * otherwise zero is returned. */
int is_parent_dir(const char path[]);

#ifdef _WIN32

int is_unc_path(const char *path);
void to_forward_slash(char *path);
void to_back_slash(char *path);

#endif

#endif /* VIFM__UTILS__PATH_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
