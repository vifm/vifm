/* vifm
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

#ifndef VIFM__TRASH_H__
#define VIFM__TRASH_H__

/* Description of a single trash item. */
typedef struct
{
	char *path;            /* Original full path of file before its removal. */
	char *trash_name;      /* Full path of file inside trash directory. */
	char *real_trash_name; /* trash_name with symlinks resolved. */
}
trash_entry_t;

/* List of items in all trashes.  Sorted by a compound key or path and
 * real_trash_name. */
trash_entry_t *trash_list;

/* Number of items in the trash_list. */
int nentries;

/* Parses trash directory name specification.  Sets value of cfg.trash_dir as a
 * side effect.  Returns non-zero in case of error, otherwise zero is
 * returned. */
int set_trash_dir(const char trash_dir[]);

/* Empties specified trash directory. */
void trash_empty(const char trash_dir[]);

/* Starts process of emptying all trashes in background. */
void trash_empty_all(void);

/* Callback-like function which triggers some trash-specific updates after file
 * move/rename. */
void trash_file_moved(const char src[], const char dst[]);

int add_to_trash(const char original_path[], const char trash_name[]);

/* Checks whether given combination of original and trash paths is registered.
 * Returns non-zero if so, otherwise zero is returned. */
int trash_includes(const char original_path[], const char trash_path[]);

/* Lists all non-empty trash directories.  Puts number of elements to *ntrashes.
 * Caller should free array and all its elements using free().  On error returns
 * NULL and sets *ntrashes to zero. */
char ** list_trashes(int *ntrashes);

/* Restores a file specified by its trash_name (from trash_list array).  Returns
 * zero on success, otherwise non-zero is returned. */
int restore_from_trash(const char trash_name[]);

/* Generates unique name for a file at base_path location named name (doesn't
 * have to be base_path/name as long as base_path is at same mount) in a trash
 * directory.  Returns string containing full path that needs to be freed by
 * caller, if no trash directory available NULL is returned. */
char * gen_trash_name(const char base_path[], const char name[]);

/* Picks trash directory basing on original path for a file that is being
 * trashed.  Returns absolute path to picked trash directory on success which
 * should be freed by the caller, otherwise NULL is returned. */
char * pick_trash_dir(const char base_path[]);

/* Checks whether given absolute path points to a file under trash directory.
 * Returns non-zero if so, otherwise zero is returned. */
int is_under_trash(const char path[]);

/* Checks whether given path belongs to the trash directory.  NULL trash_dir
 * makes this function act as is_under_trash().  Returns non-zero if so,
 * otherwise zero is returned. */
int trash_contains(const char trash_dir[], const char path[]);

/* Checks whether given absolute path points to a trash directory.  Returns
 * non-zero if so, otherwise zero is returned. */
int is_trash_directory(const char path[]);

/* Gets pointer to real name part of the trash path (which must be absolute).
 * Returns that pointer. */
const char * get_real_name_from_trash_name(const char trash_path[]);

/* Removes entries that correspond to nonexistent files in trashes. */
void trash_prune_dead_entries(void);

#endif /* VIFM__TRASH_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
