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

#include <sys/types.h> /* mode_t */

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint32_t uint64_t */

/* Functions to deal with file system objects */

/* Named boolean values of "deref" parameter for better readability. */
enum
{
	NODEREF, /* Do not dereference symbolic links in checks. */
	DEREF,   /* Dereference symbolic links in checks. */
};

/* Link types for get_symlink_type() function. */
typedef enum
{
	SLT_UNKNOWN, /* It's either a file or the link is broken. */
	SLT_DIR,     /* It's a valid link to a directory. */
	SLT_SLOW,    /* Target of the link is at slow file system, won't access it. */
}
SymLinkType;

/* Per-entry callback type for enum_dir_content().  Should return zero on
 * success or non-zero to indicate failure which will lead to stopping of
 * directory content enumeration. */
typedef int (*dir_content_client_func)(const char name[], const void *data,
		void *param);

/* Checks if path is an existing directory.  Automatically dereferences symbolic
 * links. */
int is_dir(const char path[]);

/* Checks whether directory is empty.  Returns non-zero if it is, in case of
 * error or when directory is empty non-zero is returned. */
int is_dir_empty(const char path[]);

/* Checks if path could be a directory (e.g. it can be UNC root on Windows). */
int is_valid_dir(const char *path);

/* Checks whether file at path exists.  The path should be an absolute path.
 * Relative paths are checked relatively to the working directory, which might
 * produce incorrect results. */
int path_exists(const char path[], int deref);

/* Checks whether path/file exists. */
int path_exists_at(const char path[], const char filename[], int deref);

/* Checks if two paths refer to the same file-system object.  Returns non-zero
 * if so, otherwise zero is returned. */
int paths_are_same(const char s[], const char t[]);

/* Checks whether given path points to a symbolic link.  Returns non-zero if
 * it's so, otherwise zero is returned. */
int is_symlink(const char path[]);

/* Checks whether given path points to a shortcut file (link implemented as a
 * regular file).  Returns non-zero if it's so, otherwise zero is returned. */
int is_shortcut(const char path[]);

/* Gets approximate symbolic link type: directory, won't check, anything else.
 * Returns one of SymLinkType values. */
SymLinkType get_symlink_type(const char path[]);

/* Fills the buf of size buf_len with the absolute path to a file pointed to by
 * the link symbolic link.  Uses the cwd parameter to make absolute path from
 * relative symbolic links.  The link and buf can point to the same piece of
 * memory.  Returns non-zero on error. */
int get_link_target_abs(const char link[], const char cwd[], char buf[],
		size_t buf_len);

/* Fills the buf of size buf_len with the path symbolic link specified by link
 * parameter points to.  Returns zero on success, otherwise non-zero is
 * returned. */
int get_link_target(const char link[], char buf[], size_t buf_len);

/* Creates directory and all intermediate directories if needed using specified
 * access mode.  If target directory already exists, no error will be raised.
 * Returns zero on success, otherwise non-zero is returned. */
int make_path(const char dir_name[], mode_t mode);

/* Same as make_path(), but fails if target path already exists and is a
 * directory. */
int create_path(const char dir_name[], mode_t mode);

/* Whether symbolic links can be used.  Returns non-zero if so, otherwise zero
 * is returned. */
int symlinks_available(void);

/* Whether file rename-with-replace (deletion of file at destination) is
 * supported and atomic.  Returns non-zero if so, otherwise zero is returned. */
int has_atomic_file_replace(void);

/* Checks if one can change current directory to the path.  Returns non-zero if
 * so, otherwise zero is returned. */
int directory_accessible(const char path[]);

/* Checks if one can write in directory specified by the path, which should be
 * absolute (in order for this function to work correctly). */
int is_dir_writable(const char path[]);

/* Gets correct file size independently of platform.  Returns zero for both
 * empty files and on error. */
uint64_t get_file_size(const char path[]);

/* Appends all regular files inside the path directory.  Reallocates array of
 * strings if necessary to fit all elements.  Returns pointer to reallocated
 * array or source list (on error). */
char ** list_regular_files(const char path[], char *list[], int *len);

/* Enumerates content of the path.  Returns list of names of lengths *len, which
 * can be NULL on empty list, error is indicated by negative *len. */
char ** list_all_files(const char path[], int *len);

/* Enumerates content of the path in sorted order.  Returns list of names of
 * length *len, which can be NULL on empty list, error is indicated by negative
 * *len. */
char ** list_sorted_files(const char path[], int *len);

/* Returns non-zero if file (or symbolic link target) path points to is a
 * regular file. */
int is_regular_file(const char path[]);

/* Checks whether file path points to a regular file (not a symbolic link or
 * anything else).  Returns non-zero if so, otherwise zero is returned. */
int is_regular_file_noderef(const char path[]);

/* Renames file specified by the src argument to the path specified by the dst
 * argument.  Overwrites destination file if it exists.  Returns non-zero on
 * error, otherwise zero is returned. */
int rename_file(const char src[], const char dst[]);

/* Removes directory content, but not the directory itself. */
void remove_dir_content(const char path[]);

struct dirent;

/* Uses dentry or full path to check whether target is symbolic link.  Returns
 * non-zero if so, otherwise zero is returned. */
int entry_is_link(const char path[], const struct dirent *dentry);

/* Uses dentry or full path to check file type.  Returns non-zero for
 * directories, otherwise zero is returned.  Symbolic links are _not_
 * dereferenced. */
int entry_is_dir(const char full_path[], const struct dirent *dentry);

/* Uses dentry or path to check file type.  Assumes that file is located in
 * current working directory.  Returns non-zero for directories, otherwise zero
 * is returned.  Symbolic links are dereferenced. */
int is_dirent_targets_dir(const char full_path[], const struct dirent *d);

/* Checks that entity pointed to by the path is located under the root
 * directory.  Non-zero include_root parameter makes root being considered to be
 * part of the subtree that it defines.  Returns non-zero if so, otherwise zero
 * is returned. */
int is_in_subtree(const char path[], const char root[], int include_root);

/* Checks whether to paths belong to the same file system.  Returns non-zero if
 * so, otherwise zero is returned. */
int are_on_the_same_fs(const char s[], const char t[]);

/* Checks whether file moving from src to dst corresponds to file rename that
 * just changes case of file name on case insensitive file system.  Returns
 * non-zero if so, otherwise zero is returned. */
int is_case_change(const char src[], const char dst[]);

/* Checks whether paths are case sensitive at specified location.  Returns
 * non-zero if so, otherwise zero is returned. */
int case_sensitive_paths(const char at[]);

/* Calls the client callback for each entry of the directory.  Returns zero on
 * success, otherwise non-zero is returned. */
int enum_dir_content(const char path[], dir_content_client_func client,
		void *param);

/* Counts number of files in the directory excluding . and .. entries.  Returns
 * the count. */
int count_dir_items(const char path[]);

/* getcwd() wrapper that always uses forward slashes.  Returns buf on success or
 * NULL on error. */
char * get_cwd(char buf[], size_t size);

/* Remembers current working directory.  If path can't be obtained, does
 * nothing.  Result should be passed to restore_cwd(), no checks are needed (but
 * NULL indicates that we failed to get CWD). */
char * save_cwd(void);

/* Restores previously remembered working directory via save_cwd().  If nothing
 * was remembered, does nothing. */
void restore_cwd(char saved_cwd[]);

#ifdef _WIN32

int S_ISLNK(mode_t mode);

int readlink(const char *path, char *buf, size_t len);

int is_on_fat_volume(const char *path);

/* Checks specified drive for existence.  Returns non-zero if it exists,
 * otherwise zero is returned. */
int drive_exists(char letter);

int is_win_symlink(uint32_t attr, uint32_t tag);

#endif

#endif /* VIFM__UTILS__FS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
