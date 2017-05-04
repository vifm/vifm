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
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* strchr() strcmp() strdup() strlen() strspn() */

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
#include "utils/strnrep.h"
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
	char **trashes; /* List of trashes. */
	int ntrashes;   /* Number of elements in trashes array. */
}
trashes_list;

/* State for get_list_of_trashes() traverser. */
typedef struct
{
	trashes_list *list; /* List of valid trash directories. */
	const char *spec;   /* Trash directory name specification. */
}
get_list_of_trashes_traverser_state;

static int validate_spec(const char spec[]);
static int create_trash_dir(const char trash_dir[], int user_specific);
static int try_create_trash_dir(const char trash_dir[], int user_specific);
static void empty_trash_dirs(void);
static void empty_trash_dir(const char trash_dir[]);
static void empty_trash_in_bg(bg_op_t *bg_op, void *arg);
static void remove_trash_entries(const char trash_dir[]);
static trashes_list get_list_of_trashes(void);
static int get_list_of_trashes_traverser(struct mntent *entry, void *arg);
static int is_trash_valid(const char trash_dir[]);
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
static void make_real_path(const char path[], char buf[], size_t buf_len);
static void traverse_specs(const char base_path[], traverser client, void *arg);
static char * expand_uid(const char spec[], int *expanded);
static char * get_rooted_trash_dir(const char base_path[], const char spec[]);
static char * format_root_spec(const char spec[], const char mount_point[]);

static char **specs;
static int nspecs;

int
set_trash_dir(const char new_specs[])
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

		if(!validate_spec(spec))
		{
			error = 1;
			break;
		}

		ndirs = add_to_string_array(&dirs, ndirs, 1, spec);

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
			return 1;
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
	const trashes_list list = get_list_of_trashes();
	int i;
	for(i = 0; i < list.ntrashes; ++i)
	{
		empty_trash_dir(list.trashes[i]);
	}

	free_string_array(list.trashes, list.ntrashes);
}

void
trash_empty(const char trash_dir[])
{
	regs_remove_trashed_files(trash_dir);
	empty_trash_dir(trash_dir);
	un_clear_cmds_with_trash(trash_dir);
	remove_trash_entries(trash_dir);
}

/* Removes all files inside given trash directory (even those that this instance
 * of vifm is not aware of). */
static void
empty_trash_dir(const char trash_dir[])
{
	char *const task_desc = format_str("Empty trash: %s", trash_dir);
	char *const op_desc = format_str("Emptying %s", replace_home_part(trash_dir));

	char *const trash_dir_copy = strdup(trash_dir);

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
	char *const trash_dir = arg;

	remove_dir_content(trash_dir);

	free(trash_dir);
}

/* Removes entries that belong to specified trash directory.  Removes all if
 * trash_dir is NULL. */
static void
remove_trash_entries(const char trash_dir[])
{
	int i;
	int j = 0;

	for(i = 0; i < nentries; ++i)
	{
		if(trash_dir == NULL || entry_is(PREFIXED_WITH, &trash_list[i], trash_dir))
		{
			free_entry(&trash_list[i]);
			continue;
		}

		trash_list[j++] = trash_list[i];
	}

	nentries = j;
	if(nentries == 0)
	{
		free(trash_list);
		trash_list = NULL;
	}
}

void
trash_file_moved(const char src[], const char dst[])
{
	if(is_under_trash(dst))
	{
		add_to_trash(src, dst);
	}
	else if(is_under_trash(src))
	{
		remove_from_trash(src);
	}
}

int
add_to_trash(const char path[], const char trash_name[])
{
	void *p;

	if(trash_includes(path))
	{
		return 0;
	}

	p = reallocarray(trash_list, nentries + 1, sizeof(*trash_list));
	if(p == NULL)
	{
		return -1;
	}
	trash_list = p;

	trash_list[nentries].path = strdup(path);
	trash_list[nentries].trash_name = strdup(trash_name);
	trash_list[nentries].real_trash_name = NULL;
	if(trash_list[nentries].path == NULL ||
			trash_list[nentries].trash_name == NULL)
	{
		free_entry(&trash_list[nentries]);
		return -1;
	}

	nentries++;
	return 0;
}

int
trash_includes(const char original_path[])
{
	int i;
	for(i = 0; i < nentries; ++i)
	{
		/* Assuming canonicalized paths for this unit. */
		if(stroscmp(original_path, trash_list[i].path) == 0)
		{
			return 1;
		}
	}
	return 0;
}

char **
list_trashes(int *ntrashes)
{
	trashes_list list = get_list_of_trashes();
	*ntrashes = list.ntrashes;
	return list.trashes;
}

/* Lists all non-empty trash directories.  Caller should free array and all its
 * elements using free(). */
static trashes_list
get_list_of_trashes(void)
{
	trashes_list list = {
		.trashes = NULL,
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
			};
			(void)traverse_mount_points(&get_list_of_trashes_traverser, &state);
		}
		else if(is_trash_valid(spec))
		{
			list.ntrashes = add_to_string_array(&list.trashes, list.ntrashes, 1,
					spec);
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
	if(is_trash_valid(trash_dir))
	{
		list->ntrashes = add_to_string_array(&list->trashes, list->ntrashes, 1,
				trash_dir);
	}
	free(trash_dir);

	return 0;
}

/* Checks whether trash directory is valid (e.g. exists, writable and
 * non-empty).  Returns non-zero if so, otherwise zero is returned. */
static int
is_trash_valid(const char trash_dir[])
{
	return is_dir_writable(trash_dir) && !is_dir_empty(trash_dir);
}

int
exists_in_trash(const char trash_name[])
{
	return path_exists(trash_name, NODEREF);
}

int
restore_from_trash(const char trash_name[])
{
	int i;
	char full[PATH_MAX];
	char path[PATH_MAX];

	for(i = 0; i < nentries; ++i)
	{
		if(entry_is(SAME_AS, &trash_list[i], trash_name))
		{
			break;
		}
	}
	if(i >= nentries)
	{
		return -1;
	}

	copy_str(path, sizeof(path), trash_list[i].path);
	copy_str(full, sizeof(full), trash_list[i].trash_name);
	if(perform_operation(OP_MOVE, NULL, NULL, full, path) == 0)
	{
		char *msg, *p;
		size_t len;

		cmd_group_continue();

		msg = replace_group_msg(NULL);
		len = strlen(msg);
		p = realloc(msg, COMMAND_GROUP_INFO_LEN);
		if(p == NULL)
			len = COMMAND_GROUP_INFO_LEN;
		else
			msg = p;

		snprintf(msg + len, COMMAND_GROUP_INFO_LEN - len, "%s%s",
				(msg[len - 2] != ':') ? ", " : "", strchr(trash_name, '_') + 1);
		replace_group_msg(msg);
		free(msg);

		add_operation(OP_MOVE, NULL, NULL, full, path);
		cmd_group_end();
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
	for(i = 0; i < nentries; ++i)
	{
		if(entry_is(SAME_AS, &trash_list[i], trash_name))
		{
			break;
		}
	}
	if(i >= nentries)
	{
		return;
	}

	free_entry(&trash_list[i]);
	memmove(trash_list + i, trash_list + i + 1,
			sizeof(*trash_list)*((nentries - 1) - i));

	--nentries;
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
gen_trash_name(const char base_path[], const char name[])
{
	struct stat st;
	char buf[PATH_MAX];
	char count[10];
	int i;
	strmap_t *map;

	char *const trash_dir = pick_trash_dir(base_path);
	if(trash_dir == NULL)
	{
		return NULL;
	}

	map = strmap_malloc(2);
	if (map == NULL)
	{
		return NULL;
	}
	strmap_add(map, "%name", name);

	i = 0;
	do
	{
		snprintf(count, sizeof(count), "%d", i);
		strmap_set(map, "%count", count);
		if (i == 0) {
		  snprintf(buf, sizeof(buf), "%s/%s", trash_dir, cfg.trash_filefmt_zero);
		} else {
		  snprintf(buf, sizeof(buf), "%s/%s", trash_dir, cfg.trash_filefmt_more);
		}
		strnrep(buf, buf, sizeof(buf), map);
		chosp(buf);
		i++;
	}
	while(os_lstat(buf, &st) == 0);

	free(trash_dir);

	return strdup(buf);
}

char *
pick_trash_dir(const char base_path[])
{
	char real_path[PATH_MAX];
	char *trash_dir = NULL;

	/* We want all links resolved to do not mistakenly attribute removed files to
	 * location of a symbolic link. */
	if(os_realpath(base_path, real_path) == real_path)
	{
		/* If realpath() fails, just go with the original path. */
#ifdef _WIN32
		to_forward_slash(real_path);
#endif
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
 * if so, otherwise non-zero is returned. */
static int
is_rooted_trash_dir(const char spec[])
{
	return starts_with_lit(spec, ROOTED_SPEC_PREFIX)
	    && spec[ROOTED_SPEC_PREFIX_LEN] != '\0';
}

int
is_under_trash(const char path[])
{
	return get_resident_type(path) != TRT_OUT_OF_TRASH;
}

int
trash_contains(const char trash_dir[], const char path[])
{
	if(trash_dir == NULL)
	{
		return is_under_trash(path);
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
is_trash_directory(const char path[])
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
 * that all path components except for the last one are expanded.  Returns
 * non-zero if path passes the check, otherwise zero is returned. */
static int
path_is(PathCheckType check, const char path[], const char other[])
{
	char path_real[PATH_MAX*2], other_real[PATH_MAX*2];

	make_real_path(path, path_real, sizeof(path_real));
	make_real_path(other, other_real, sizeof(other_real));

	return (check == PREFIXED_WITH)
	     ? path_starts_with(path_real, other_real)
	     : paths_are_same(path_real, other_real);
}

/* An optimized for entries version of path_is(). */
static int
entry_is(PathCheckType check, trash_entry_t *entry, const char other[])
{
	char real[PATH_MAX*2];

	if(entry->real_trash_name == NULL)
	{
		char real[PATH_MAX*2];
		make_real_path(entry->trash_name, real, sizeof(real));
		entry->real_trash_name = strdup(real);
	}

	make_real_path(other, real, sizeof(real));

	return (check == PREFIXED_WITH)
	     ? path_starts_with(entry->real_trash_name, real)
	     : paths_are_same(entry->real_trash_name, real);
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
		if(add_to_string_array(&link, nrecords, 1, copy) == nrecords + 1)
		{
			if(add_to_string_array(&real, nrecords, 1, real_dir) == nrecords + 1)
			{
				++nrecords;
			}
			else
			{
				free(link[nrecords]);
			}
		}
	}

	snprintf(buf, buf_len, "%s%s%s", real_dir,
			ends_with_slash(real_dir) ? "" : "/", get_last_path_component(path));
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
	char full[PATH_MAX];
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
get_real_name_from_trash_name(const char trash_path[])
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
	for(i = 0; i < nentries; ++i)
	{
		if(!path_exists(trash_list[i].trash_name, NODEREF))
		{
			free(trash_list[i].path);
			free(trash_list[i].trash_name);
			continue;
		}

		trash_list[j++] = trash_list[i];
	}
	nentries = j;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
