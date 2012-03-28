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

#ifndef __FS_H__
#define __FS_H__

#ifdef _WIN32
#include <windef.h>
#endif

#include <sys/types.h> /* size_t mode_t */

/* Functions to deal with file system objects */

/* Checks if path is an existing directory. */
int is_dir(const char *path);
/* Checks if path could be a directory (e.g. it can be UNC root on Windows). */
int is_valid_dir(const char *path);
/* if path is NULL, file assumed to contain full path */
int file_exists(const char *path, const char *file);
int check_link_is_dir(const char *filename);
int get_link_target(const char *link, char *buf, size_t buf_len);
int make_dir(const char *dir_name, mode_t mode);
int symlinks_available(void);
/* Checks if one can change directory to a path. */
int directory_accessible(const char *path);

#ifdef _WIN32

char * realpath(const char *path, char *buf);
int S_ISLNK(mode_t mode);
int readlink(const char *path, char *buf, size_t len);
int is_on_fat_volume(const char *path);
int drive_exists(TCHAR letter);

#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
