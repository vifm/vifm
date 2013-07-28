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

#ifndef VIFM__UTILS__FS_H__
#define VIFM__UTILS__FS_H__

#include <sys/types.h> /* size_t mode_t */

#include <stdint.h> /* uint64_t */

/* Functions to deal with file system objects */

/* Checks if path is an existing directory. */
int is_dir(const char *path);
/* Checks if path could be a directory (e.g. it can be UNC root on Windows). */
int is_valid_dir(const char *path);
/* Checks whether file at path exists.  The path should be an absolute path.
 * Relative paths are checked relatively to the working directory, which might
 * produce incorrect results. */
int path_exists(const char path[]);
/* Checks whether path/file exists. */
int path_exists_at(const char *path, const char *filename);
int check_link_is_dir(const char *filename);
/* Fills the buf of size buf_len with the absolute path to a file pointed to by
 * the link symbolic link.  Uses the cwd parameter to make absolute path from
 * relative symbolic links.  The link and buf can point to the same piece of
 * memory.  Returns non-zero on error. */
int get_link_target_abs(const char link[], const char cwd[], char buf[],
		size_t buf_len);
int get_link_target(const char *link, char *buf, size_t buf_len);
int make_dir(const char *dir_name, mode_t mode);
int symlinks_available(void);
/* Checks if one can change current directory to a path. */
int directory_accessible(const char *path);
/* Checks if one can write in directory specified by the path, which should be
 * absolute (in order for this function to work correctly). */
int is_dir_writable(const char path[]);
/* Gets correct file size independently of platform. */
uint64_t get_file_size(const char *path);
/* Lists all regular files inside the path directory.  Allocates an array of
 * strings, which should be freed by the caller.  Always sets *len.  Returns
 * NULL on error. */
char ** list_regular_files(const char path[], int *len);
/* Returns non-zero if file (or symbolic link target) path points to is a
 * regular file. */
int is_regular_file(const char path[]);
/* Renames file specified by the src argument to the path specified by the dst
 * argument.  Overwrites destination file if it exists.  Returns non-zero on
 * error, otherwise zero is returned. */
int rename_file(const char src[], const char dst[]);

#ifdef _WIN32

char * realpath(const char *path, char *buf);
int S_ISLNK(mode_t mode);
int readlink(const char *path, char *buf, size_t len);
int is_on_fat_volume(const char *path);
int drive_exists(char letter);

#endif

#endif /* VIFM__UTILS__FS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
