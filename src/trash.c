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

#include "trash.h"

#include <sys/stat.h> /* stat chmod() */
#include <unistd.h> /* getuid() */

#include <assert.h> /* assert() */
#include <errno.h> /* EROFS errno */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* remove() snprintf() */
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* memmove() strchr() strcmp() strdup() strlen() strspn() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "compat/mntent.h"
#include "compat/reallocarray.h"
#include "modes/dialogs/msg_dialog.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "background.h"
#include "ops.h"
#include "registers.h"
#include "undo.h"

#define ROOTED_SPEC_PREFIX "%r/"
#define ROOTED_SPEC_PREFIX_LEN (sizeof(ROOTED_SPEC_PREFIX) - 1U)

/* Describes file location relative to one of registered trash directories.
 * Argument for get_resident_type_traverser().*/
typedef enum
{
	TRT_OUT_OF_TRASH,         /* Not in trash. */
	TRT_IN_TRASH,             /* Direct child of one of trash directories. */
	TRT_IN_REMOVED_DIRECTORY, /* Child of one of removed directories. */
}
TrashResidentType;

/* Type of path check. */
typedef enum
{
	PREFIXED_WITH, /* Checks that path starts with a prefix. */
	SAME_AS,       /* Checks that path is equal to the other one. */
}
PathCheckType;

/* Client of the traverse_specs() function.  Should return non-zero to stop
 * traversal. */
typedef int (*traverser)(const char base_path[], const char trash_dir[],
		int user_specific, void *arg);

/* List of trash directories. */
typedef struct
{
	char **trashes;   /* List of trashes. */
	char *can_delete; /* Whether can delete some trash on empty ('1'/'0'). */
	int ntrashes;     /* Number of elements in trashes array. */
}
trashes_list;

/* State for get_list_of_trashes() traverser. */
typedef struct
{
	trashes_list *list; /* List of valid trash directories. */
	const char *spec;   /* Trash directory name specification. */
	int allow_empty;    /* Whether empty directories should be included. */
}
get_list_of_trashes_traverser_state;

static int validate_spec(const char spec[]);
static int create_trash_dir(const char trash_dir[], int user_specific);
static int try_create_trash_dir(const char trash_dir[], int user_specific);
static void empty_trash_dirs(void);
static void empty_trash_dir(const char trash_dir[], int can_delete);
static void empty_trash_in_bg(bg_op_t *bg_op, void *arg);
static void remove_trash_entries(const char trash_dir[]);
static int find_in_trash(const char original_path[], const char trash_path[]);
static trashes_list get_list_of_trashes(int allow_empty);
static int get_list_of_trashes_traverser(struct mntent *entry, void *arg);
static int is_trash_valid(const char trash_dir[], int allow_empty);
static void add_trash_to_list(trashes_list *list, const char path[],
		int can_delete);
static void remove_from_trash(const char trash_name[]);
static void free_entry(const trash_entry_t *entry);
static int pick_trash_dir_traverser(const char base_path[],
		const char trash_dir[], int user_specific, void *arg);
static int is_rooted_trash_dir(const char spec[]);
static TrashResidentType get_resident_type(const char path[]);
static int get_resident_type_traverser(const char path[],
		const char trash_dir[], int user_specific, void *arg);
static int is_trash_directory_traverser(const char path[],
		const char trash_dir[], int user_specific, void *arg);
static int path_is(PathCheckType check, const char path[], const char other[]);
static int entry_is(PathCheckType check, trash_entry_t *entry,
		const char other[]);
static const char * get_real_trash_name(trash_entry_t *entry);
static void make_real_path(const char path[], char buf[], size_t buf_len);
static void traverse_specs(const char base_path[], traverser client, void *arg);
static char * expand_uid(const char spec[], int *expanded);
static char * get_rooted_trash_dir(const char base_path[], const char spec[]);
static char * format_root_spec(const char spec[], const char mount_point[]);

trash_entry_t *trash_list;
int trash_list_size;

TSTATIC char **specs;
TSTATIC int nspecs;

int
trash_set_specs(const char new_specs[])
{
	char **dirs = NULL;
	int ndirs = 0;

	int error;
	char *free_this;
	char *spec;

	error = 0;
	free_this = strdup(new_specs);
	spec = free_this;

	for(;;)
	{
		char *const p = until_first(spec, ',');
		const int last_element = *p == '\0';
		*p = '\0';

		spec = expand_path(spec);

		if(!validate_spec(spec))
		{
			free(spec);
			error = 1;
			break;
		}

		ndirs = put_into_string_array(&dirs, ndirs, spec);

		if(last_element)
		{
			break;
		}
		spec = p + 1;
	}

	free(free_this);

	if(!error)
	{
		free_string_array(specs, nspecs);
		specs = dirs;
		nspecs = ndirs;

		copy_str(cfg.trash_dir, sizeof(cfg.trash_dir), new_specs);
	}
	else
	{
		free_string_array(dirs, ndirs);
	}

	return error;
}

/* Validates trash directory specification.  Returns non-zero if it's OK,
 * otherwise zero is returned and an error message is displayed. */
static int
validate_spec(const char spec[])
{
	int valid = 1;
	int with_uid;
	char *const expanded_spec = expand_uid(spec, &with_uid);

	if(is_path_absolute(expanded_spec))
	{
		if(create_trash_dir(expanded_spec, with_uid) != 0)
		{
			valid = 0;
		}
	}
	else if(!is_rooted_trash_dir(expanded_spec))
	{
		show_error_msgf("Error Setting Trash Directory",
				"The path specification is of incorrect format: %s", expanded_spec);
		valid = 0;
	}

	free(expanded_spec);
	return valid;
}

/* Ensures existence of trash directory.  Displays error message dialog, if
 * directory creation failed.  Returns zero on success, otherwise non-zero value
 * is returned. */
static int
create_trash_dir(const char trash_dir[], int user_specific)
{
	LOG_FUNC_ENTER;

	if(try_create_trash_dir(trash_dir, 0) != 0)
	{
		show_error_msgf("Error Setting Trash Directory",
				"Could not set trash directory to %s: %s", trash_dir, strerror(errno));
		return 1;
	}

	return 0;
}

/* Tries to create trash directory.  Returns zero on success, otherwise non-zero
 * value is returned. */
static int
try_create_trash_dir(const char trash_dir[], int user_specific)
{
	LOG_FUNC_ENTER;

	if(!is_dir_writable(trash_dir))
	{
		if(make_path(trash_dir, 0777) != 0)
		{
#ifndef _WIN32
			/* Do not treat it as an error if trash is not writable because
			 * file-system is mounted read-only.  User should be aware of it. */
			return (errno != EROFS);
#else
			return 1;
#endif
		}
	}

	if(user_specific)
	{
		if(chmod(trash_dir, 0700) != 0)
		{
			return 1;
		}
	}

	return 0;
}

void
trash_empty_all(void)
{
	regs_remove_trashed_files(NULL);
	empty_trash_dirs();
	un_clear_cmds_with_trash(NULL);
	remove_trash_entries(NULL);
}

/* Empties all trash directories (all specifications on all mount points are
 * expanded). */
static void
empty_trash_dirs(void)
{
	const trashes_list list = get_list_of_trashes(1);
	int i;
	for(i = 0; i < list.ntrashes; ++i)
	{
		empty_trash_dir(list.trashes[i], list.can_delete[i] == '1');
	}

	free_string_array(list.trashes, list.ntrashes);
	free(list.can_delete);
}

void
trash_empty(const char trash_dir[])
{
	regs_remove_trashed_files(trash_dir);
	empty_trash_dir(trash_dir, 0);
	un_clear_cmds_with_trash(trash_dir);
	remove_trash_entries(trash_dir);
}

/* Removes all files inside given trash directory (even those that this instance
 * of vifm is not aware of). */
static void
empty_trash_dir(const char trash_dir[], int can_delete)
{
	/* XXX: should we rename directory and delete files from it to exclude
	 *      possibility of deleting newly added files? */

	char *const task_desc = format_str("Empty trash: %s", trash_dir);
	char *const op_desc = format_str("Emptying %s", replace_home_part(trash_dir));

	/* Yes, this isn't pretty.  It's a simple way to bundle string and bool. */
	char *trash_dir_copy = format_str("%c%s", can_delete ? '1' : '0', trash_dir);

	if(bg_execute(task_desc, op_desc, BG_UNDEFINED_TOTAL, 1, &empty_trash_in_bg,
			trash_dir_copy) != 0)
	{
		free(trash_dir_copy);
	}

	free(op_desc);
	free(task_desc);
}

/* Entry point for a background task that removes files in a single trash
 * directory. */
static void
empty_trash_in_bg(bg_op_t *bg_op, void *arg)
{
	char *const trash_info = arg;

	remove_dir_content(trash_info + 1);
	if(trash_info[0] == '1')
	{
		(void)os_rmdir(trash_info + 1);
	}

	free(trash_info);
}

/* Removes entries that belong to specified trash directory.  Removes all if
 * trash_dir is NULL. */
static void
remove_trash_entries(const char trash_dir[])
{
	int i;
	int j = 0;

	for(i = 0; i < trash_list_size; ++i)
	{
		if(trash_dir == NULL || entry_is(PREFIXED_WITH, &trash_list[i], trash_dir))
		{
			free_entry(&trash_list[i]);
			continue;
		}

		trash_list[j++] = trash_list[i];
	}

	trash_list_size = j;
	if(trash_list_size == 0)
	{
		free(trash_list);
		trash_list = NULL;
	}
}

void
trash_file_moved(const char src[], const char dst[])
{
	if(trash_has_path(dst))
	{
		if(trash_add_entry(src, dst) != 0)
		{
			LOG_ERROR_MSG("Failed to add to trash: (`%s`, `%s`)", src, dst);
		}
	}
	else if(trash_has_path(src))
	{
		remove_from_trash(src);
	}
}

int
trash_add_entry(const char original_path[], const char trash_name[])
{
	/* XXX: we check duplicates by original_path+trash_name, which allows
	 *      multiple original path to be mapped to one trash file, might want to
	 *      forbid this.  */
	int pos = find_in_trash(original_path, trash_name);
	if(pos >= 0)
	{
		LOG_INFO_MSG("File is already in trash: (`%s`, `%s`)", original_path,
				trash_name);
		return 0;
	}
	pos = -(pos + 1);

	void *p = reallocarray(trash_list, trash_list_size + 1, sizeof(*trash_list));
	if(p == NULL)
	{
		return -1;
	}
	trash_list = p;

	trash_entry_t entry = {
		.path = strdup(original_path),
		.trash_name = strdup(trash_name),
		.real_trash_name = NULL,
	};
	if(entry.path == NULL || entry.trash_name == NULL)
	{
		free_entry(&entry);
		return -1;
	}

	++trash_list_size;
	memmove(trash_list + pos + 1, trash_list + pos,
			sizeof(*trash_list)*(trash_list_size - 1 - pos));
	trash_list[pos] = entry;
	return 0;
}

int
trash_has_entry(const char original_path[], const char trash_path[])
{
	return (find_in_trash(original_path, trash_path) >= 0);
}

/* Finds position of an entry in trash_list or whereto it should be inserted in
 * it.  Returns non-negative number of successful search and negative index
 * offset by one otherwise (0 -> -1, 1 -> -2, etc.). */
static int
find_in_trash(const char original_path[], const char trash_path[])
{
	char real_trash_path[PATH_MAX*2 + 1];
	real_trash_path[0] = '\0';

	int l = 0;
	int u = trash_list_size - 1;
	while(l <= u)
	{
		const int i = l + (u - l)/2;

		int cmp = stroscmp(trash_list[i].path, original_path);
		if(cmp == 0)
		{
			const char *entry_real = get_real_trash_name(&trash_list[i]);
			if(real_trash_path[0] == '\0')
			{
				make_real_path(trash_path, real_trash_path, sizeof(real_trash_path));
			}
			cmp = stroscmp(entry_real, real_trash_path);
		}

		if(cmp == 0)
		{
			return i;
		}
		else if(cmp < 0)
		{
			l = i + 1;
		}
		else
		{
			u = i - 1;
		}
	}
	return -l - 1;
}

char **
trash_list_trashes(int *ntrashes)
{
	trashes_list list = get_list_of_trashes(0);
	free(list.can_delete);
	*ntrashes = list.ntrashes;
	return list.trashes;
}

/* Lists trash directories.  Caller should free array and all its elements using
 * free(). */
static trashes_list
get_list_of_trashes(int allow_empty)
{
	trashes_list list = {
		.trashes = NULL,
		.can_delete = NULL,
		.ntrashes = 0,
	};

	int i;
	for(i = 0; i < nspecs; ++i)
	{
		int with_uid;
		char *const spec = expand_uid(specs[i], &with_uid);
		if(is_rooted_trash_dir(spec))
		{
			get_list_of_trashes_traverser_state state = {
				.list = &list,
				.spec = spec,
				.allow_empty = allow_empty,
			};
			(void)traverse_mount_points(&get_list_of_trashes_traverser, &state);
		}
		else if(is_trash_valid(spec, allow_empty))
		{
			add_trash_to_list(&list, spec, with_uid);
		}
		free(spec);
	}

	return list;
}

/* traverse_mount_points() client that collects valid trash directories into a
 * list of trash directories. */
static int
get_list_of_trashes_traverser(struct mntent *entry, void *arg)
{
	get_list_of_trashes_traverser_state *const params = arg;
	trashes_list *const list = params->list;
	const char *const spec = params->spec;

	char *const trash_dir = format_root_spec(spec, entry->mnt_dir);
	if(is_trash_valid(trash_dir, params->allow_empty))
	{
		add_trash_to_list(list, trash_dir, 1);
	}
	free(trash_dir);

	return 0;
}

/* Adds trash to list of trashes. */
static void
add_trash_to_list(trashes_list *list, const char path[], int can_delete)
{
	size_t len = list->ntrashes;
	if(strappendch(&list->can_delete, &len, can_delete ? '1' : '0') == 0)
	{
		list->ntrashes = add_to_string_array(&list->trashes, list->ntrashes, path);
	}
}

/* Checks whether trash directory is valid (at least exists and is writable).
 * Returns non-zero if so, otherwise zero is returned. */
static int
is_trash_valid(const char trash_dir[], int allow_empty)
{
	return is_dir_writable(trash_dir)
	    && (allow_empty || !is_dir_empty(trash_dir));
}

int
trash_restore(const char trash_name[])
{
	int i;
	char full[PATH_MAX + 1];
	char path[PATH_MAX + 1];

	for(i = 0; i < trash_list_size; ++i)
	{
		if(entry_is(SAME_AS, &trash_list[i], trash_name))
		{
			break;
		}
	}
	if(i >= trash_list_size)
	{
		return -1;
	}

	copy_str(path, sizeof(path), trash_list[i].path);
	copy_str(full, sizeof(full), trash_list[i].trash_name);
	if(perform_operation(OP_MOVE, NULL, NULL, full, path) == OPS_SUCCEEDED)
	{
		char *msg, *p;
		size_t len;

		un_group_reopen_last();

		msg = un_replace_group_msg(NULL);
		len = strlen(msg);
		p = realloc(msg, COMMAND_GROUP_INFO_LEN);
		if(p == NULL)
			len = COMMAND_GROUP_INFO_LEN;
		else
			msg = p;

		snprintf(msg + len, COMMAND_GROUP_INFO_LEN - len, "%s%s",
				(msg[len - 2] != ':') ? ", " : "", strchr(trash_name, '_') + 1);
		un_replace_group_msg(msg);
		free(msg);

		un_group_add_op(OP_MOVE, NULL, NULL, full, path);
		un_group_close();
		remove_from_trash(trash_name);
		return 0;
	}
	return -1;
}

/* Removes record about the file in the trash.  Does nothing if no such record
 * found. */
static void
remove_from_trash(const char trash_name[])
{
	int i;
	for(i = 0; i < trash_list_size; ++i)
	{
		if(entry_is(SAME_AS, &trash_list[i], trash_name))
		{
			break;
		}
	}
	if(i >= trash_list_size)
	{
		return;
	}

	free_entry(&trash_list[i]);
	memmove(trash_list + i, trash_list + i + 1,
			sizeof(*trash_list)*((trash_list_size - 1) - i));

	--trash_list_size;
}

/* Frees memory allocated by given trash entry. */
static void
free_entry(const trash_entry_t *entry)
{
	free(entry->path);
	free(entry->trash_name);
	free(entry->real_trash_name);
}

char *
trash_gen_path(const char base_path[], const char name[])
{
	struct stat st;
	char buf[PATH_MAX + 1];
	int i;
	char *const trash_dir = trash_pick_dir(base_path);

	if(trash_dir == NULL)
	{
		return NULL;
	}

	i = 0;
	do
	{
		snprintf(buf, sizeof(buf), "%s/%03d_%s", trash_dir, i++, name);
		chosp(buf);
	}
	while(os_lstat(buf, &st) == 0);

	free(trash_dir);

	return strdup(buf);
}

char *
trash_pick_dir(const char base_path[])
{
	char real_path[PATH_MAX + 1];
	char *trash_dir = NULL;

	/* We want all links resolved to do not mistakenly attribute removed files to
	 * location of a symbolic link. */
	if(os_realpath(base_path, real_path) == real_path)
	{
		/* If realpath() fails, just go with the original path. */
		system_to_internal_slashes(real_path);
		base_path = real_path;
	}

	traverse_specs(base_path, &pick_trash_dir_traverser, &trash_dir);
	return trash_dir;
}

/* traverse_specs client that finds first available trash directory suitable for
 * the base_path. */
static int
pick_trash_dir_traverser(const char base_path[], const char trash_dir[],
		int user_specific, void *arg)
{
	if(try_create_trash_dir(trash_dir, user_specific) == 0)
	{
		char **const result = arg;
		*result = strdup(trash_dir);
		return 1;
	}
	return 0;
}

/* Checks whether the spec refers to a rooted trash directory.  Returns non-zero
 * if so, otherwise zero is returned. */
static int
is_rooted_trash_dir(const char spec[])
{
	return starts_with_lit(spec, ROOTED_SPEC_PREFIX)
	    && spec[ROOTED_SPEC_PREFIX_LEN] != '\0';
}

int
trash_has_path(const char path[])
{
	return get_resident_type(path) != TRT_OUT_OF_TRASH;
}

int
trash_has_path_at(const char trash_dir[], const char path[])
{
	if(trash_dir == NULL)
	{
		return trash_has_path(path);
	}

	return path_is(PREFIXED_WITH, path, trash_dir);
}

/* Gets status of file relative to trash directories.  Returns the status. */
static TrashResidentType
get_resident_type(const char path[])
{
	TrashResidentType resident_type = TRT_OUT_OF_TRASH;
	traverse_specs(path, &get_resident_type_traverser, &resident_type);
	return resident_type;
}

/* traverse_specs client that check that the path is under one of trash
 * directories.  Accepts pointer to TrashResidentType as argument. */
static int
get_resident_type_traverser(const char path[], const char trash_dir[],
		int user_specific, void *arg)
{
	if(path_is(PREFIXED_WITH, path, trash_dir))
	{
		TrashResidentType *const result = arg;
		char *const parent_dir = strdup(path);

		remove_last_path_component(parent_dir);
		*result = path_is(SAME_AS, parent_dir, trash_dir)
		        ? TRT_IN_TRASH
		        : TRT_IN_REMOVED_DIRECTORY;

		free(parent_dir);

		return 1;
	}
	return 0;
}

int
trash_is_at_path(const char path[])
{
	int trash_directory = 0;
	traverse_specs(path, &is_trash_directory_traverser, &trash_directory);
	return trash_directory;
}

/* traverse_specs client that check that the path is one of trash
 * directories. */
static int
is_trash_directory_traverser(const char path[], const char trash_dir[],
		int user_specific, void *arg)
{
	if(path_is(SAME_AS, path, trash_dir))
	{
		int *const result = arg;
		*result = 1;
		return 1;
	}
	return 0;
}

/* Performs specified check on a path.  The check is constructed in such a way
 * that all path components of the path parameter except for the last one are
 * expanded.  Returns non-zero if path passes the check, otherwise zero is
 * returned. */
static int
path_is(PathCheckType check, const char path[], const char other[])
{
	char path_real[PATH_MAX*2], other_real[PATH_MAX + 1];

	make_real_path(path, path_real, sizeof(path_real));
	if(os_realpath(other, other_real) != other_real)
	{
		copy_str(other_real, sizeof(other_real), other);
	}

	return (check == PREFIXED_WITH)
	     ? path_starts_with(path_real, other_real)
	     : (stroscmp(path_real, other_real) == 0);
}

/* An optimized for entries version of path_is(). */
static int
entry_is(PathCheckType check, trash_entry_t *entry, const char other[])
{
	char other_real[PATH_MAX*2 + 1];
	make_real_path(other, other_real, sizeof(other_real));

	const char *entry_real = get_real_trash_name(entry);

	return (check == PREFIXED_WITH)
	     ? path_starts_with(entry_real, other_real)
	     : (stroscmp(entry_real, other_real) == 0);
}

/* Retrieves path to the entry with all symbolic but last one resolved.
 * Returns the path. */
static const char *
get_real_trash_name(trash_entry_t *entry)
{
	if(entry->real_trash_name == NULL)
	{
		char real[PATH_MAX*2 + 1];
		make_real_path(entry->trash_name, real, sizeof(real));
		entry->real_trash_name = strdup(real);
	}
	return entry->real_trash_name;
}

/* Resolves all but last path components in the path.  Permanently caches
 * results of previous invocations. */
static void
make_real_path(const char path[], char buf[], size_t buf_len)
{
	/* This cache only grows, but as number of distinct trash directories is
	 * likely to be limited, not much space should be wasted. */
	static char **link, **real;
	static int nrecords;

	int pos;
	char copy[PATH_MAX*2];
	char real_dir[PATH_MAX*2];

	copy_str(copy, sizeof(copy), path);
	remove_last_path_component(copy);

	pos = string_array_pos(link, nrecords, copy);
	if(pos != -1)
	{
		copy_str(real_dir, sizeof(real_dir), real[pos]);
	}
	else if(os_realpath(copy, real_dir) != real_dir)
	{
		copy_str(real_dir, sizeof(real_dir), path);
	}
	else
	{
		if(add_to_string_array(&link, nrecords, copy) == nrecords + 1)
		{
			if(add_to_string_array(&real, nrecords, real_dir) == nrecords + 1)
			{
				++nrecords;
			}
			else
			{
				free(link[nrecords]);
			}
		}
	}

	build_path(buf, buf_len, real_dir, get_last_path_component(path));
}

/* Calls client traverser for each trash directory specification defined by
 * specs array. */
static void
traverse_specs(const char base_path[], traverser client, void *arg)
{
	int i;
	for(i = 0; i < nspecs; ++i)
	{
		char *to_free = NULL;
		const char *trash_dir;
		int with_uid;

		char *const spec = expand_uid(specs[i], &with_uid);
		if(is_rooted_trash_dir(spec))
		{
			to_free = get_rooted_trash_dir(base_path, spec);
			trash_dir = to_free;
		}
		else
		{
			trash_dir = spec;
		}

		if(trash_dir != NULL && client(base_path, trash_dir, with_uid, arg))
		{
			free(to_free);
			free(spec);
			break;
		}

		free(to_free);
		free(spec);
	}
}

/* Expands trailing %u into real user ID.  Sets *expanded to reflect the fact
 * whether expansion took place.  Returns newly allocated string. */
static char *
expand_uid(const char spec[], int *expanded)
{
#ifndef _WIN32
	char *copy = strdup(spec);
	*expanded = cut_suffix(copy, "%u");
	if(*expanded)
	{
		char uid_str[32];
		size_t len = strlen(copy);
		snprintf(uid_str, sizeof(uid_str), "%lu", (unsigned long)getuid());
		(void)strappend(&copy, &len, uid_str);
	}
	return copy;
#else
	*expanded = 0;
	return strdup(spec);
#endif
}

/* Expands rooted trash directory specification into a string.  Returns NULL on
 * error, otherwise newly allocated string that should be freed by the caller is
 * returned. */
static char *
get_rooted_trash_dir(const char base_path[], const char spec[])
{
	char full[PATH_MAX + 1];
	if(get_mount_point(base_path, sizeof(full), full) == 0)
	{
		return format_root_spec(spec, full);
	}

	return NULL;
}

/* Expands rooted trash directory specification into a string.  Returns newly
 * allocated string that should be freed by the caller. */
static char *
format_root_spec(const char spec[], const char mount_point[])
{
	return format_str("%s%s%s", mount_point,
			ends_with_slash(mount_point) ? "" : "/", spec + ROOTED_SPEC_PREFIX_LEN);
}

const char *
trash_get_real_name_of(const char trash_path[])
{
	const char *real_name = after_last(trash_path, '/');

	assert(is_path_absolute(trash_path) && "Expected full path to a file.");

	if(get_resident_type(trash_path) == TRT_IN_TRASH)
	{
		const size_t prefix_len = strspn(real_name, "0123456789");
		if(real_name[prefix_len] == '_')
		{
			real_name += prefix_len + 1;
		}
	}

	return real_name;
}

void
trash_prune_dead_entries(void)
{
	int i, j;

	j = 0;
	for(i = 0; i < trash_list_size; ++i)
	{
		if(!path_exists(trash_list[i].trash_name, NODEREF))
		{
			free(trash_list[i].path);
			free(trash_list[i].trash_name);
			continue;
		}

		trash_list[j++] = trash_list[i];
	}
	trash_list_size = j;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
