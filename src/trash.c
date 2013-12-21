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

#ifdef _WIN32
#include <dirent.h> /* DIR */
#endif

#include <sys/stat.h> /* stat */
#include <unistd.h> /* lstat */

#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strchr() strdup() strlen() strspn() */

#include "cfg/config.h"
#include "menus/menus.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "background.h"
#include "ops.h"
#include "registers.h"
#include "undo.h"

static int validate_trash_dir(const char trash_dir[]);
static int create_trash_dir(const char trash_dir[]);
static void empty_trash_dir(void);
static void empty_trash_list(void);
static char * pick_trash_dir(const char base_dir[]);
static char * get_ideal_trash_dir(const char base_dir[]);

static char **trash_dirs;
static int ntrash_dirs;

int
set_trash_dir(const char trash_dir[])
{
	char **dirs = NULL;
	int ndirs = 0;

	int error = 0;
	char *const free_this = strdup(trash_dir);
	char *element = free_this;

	for(;;)
	{
		char *const p = until_first(element, ',');
		const int last_element = *p == '\0';
		*p = '\0';

		if(!validate_trash_dir(element))
		{
			error = 1;
			break;
		}

		ndirs = add_to_string_array(&dirs, ndirs, 1, element);

		if(last_element)
		{
			break;
		}
		element = p + 1;
	}

	free(free_this);

	if(!error)
	{
		free_string_array(trash_dirs, ntrash_dirs);
		trash_dirs = dirs;
		ntrash_dirs = ndirs;

		copy_str(cfg.trash_dir, sizeof(cfg.trash_dir), trash_dir);
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
validate_trash_dir(const char trash_dir[])
{
	if(is_path_absolute(trash_dir))
	{
		if(create_trash_dir(trash_dir) != 0)
		{
			return 0;
		}
	}
	else if(!starts_with_lit(trash_dir, "%r/") || trash_dir[3] == '\0')
	{
		show_error_msgf("Error Setting Trash Directory",
				"The path specification is of incorrect format: %s", trash_dir);
		return 0;
	}
	return 1;
}

/* Ensures existence of trash directory.  Displays error message dialog, if
 * directory creation failed.  Returns zero on success, otherwise non-zero value
 * is returned. */
static int
create_trash_dir(const char trash_dir[])
{
	LOG_FUNC_ENTER;

	if(try_create_trash_dir(trash_dir) != 0)
	{
		show_error_msgf("Error Setting Trash Directory",
				"Could not set trash directory to %s: %s", trash_dir, strerror(errno));
		return 1;
	}

	return 0;
}

int
try_create_trash_dir(const char trash_dir[])
{
	LOG_FUNC_ENTER;

	if(is_dir_writable(trash_dir))
	{
		return 0;
	}

	return make_dir(trash_dir, 0777);
}

void
empty_trash(void)
{
	clean_regs_with_trash();
	empty_trash_dir();
	clean_cmds_with_trash();
	empty_trash_list();
}

static void
empty_trash_dir(void)
{
#ifndef _WIN32
	char cmd[25 + strlen(cfg.trash_dir)*2 + 1];
	char *escaped;

	escaped = escape_filename(cfg.trash_dir, 0);
	snprintf(cmd, sizeof(cmd), "sh -c 'rm -rf %s/* %s/.[!.]*'", escaped, escaped);
	free(escaped);

	start_background_job(cmd, 0);
#else
	DIR *dir;
	struct dirent *d;

	dir = opendir(cfg.trash_dir);
	if(dir == NULL)
		return;
	while((d = readdir(dir)) != NULL)
	{
		char full[PATH_MAX];
		if(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;
		snprintf(full, sizeof(full), "%s/%s", cfg.trash_dir, d->d_name);
		perform_operation(OP_REMOVESL, NULL, full, NULL);
	}
	closedir(dir);
#endif
}

static void
empty_trash_list(void)
{
	int i;

	for(i = 0; i < nentries; i++)
	{
		free(trash_list[i].path);
		free(trash_list[i].trash_name);
	}
	free(trash_list);
	trash_list = NULL;
	nentries = 0;
}

int
add_to_trash(const char path[], const char trash_name[])
{
	void *p;

	if(!exists_in_trash(trash_name))
	{
		return -1;
	}

	if(is_in_trash(trash_name))
	{
		return 0;
	}

	if((p = realloc(trash_list, sizeof(*trash_list)*(nentries + 1))) == NULL)
	{
		return -1;
	}
	trash_list = p;

	trash_list[nentries].path = strdup(path);
	trash_list[nentries].trash_name = strdup(trash_name);
	if(trash_list[nentries].path == NULL ||
			trash_list[nentries].trash_name == NULL)
	{
		free(trash_list[nentries].path);
		free(trash_list[nentries].trash_name);
		return -1;
	}

	nentries++;
	return 0;
}

int
is_in_trash(const char trash_name[])
{
	int i;
	for(i = 0; i < nentries; i++)
	{
		if(stroscmp(trash_list[i].trash_name, trash_name) == 0)
			return 1;
	}
	return 0;
}

int
exists_in_trash(const char trash_name[])
{
	if(is_path_absolute(trash_name))
	{
		return path_exists(trash_name);
	}
	else
	{
		return path_exists_at(cfg.trash_dir, trash_name);
	}
}

int
restore_from_trash(const char trash_name[])
{
	int i;
	char full[PATH_MAX];
	char buf[PATH_MAX];

	for(i = 0; i < nentries; i++)
	{
		if(stroscmp(trash_list[i].trash_name, trash_name) == 0)
			break;
	}
	if(i >= nentries)
		return -1;
	
	copy_str(buf, sizeof(buf), trash_list[i].path);
	copy_str(full, sizeof(full), trash_list[i].trash_name);
	if(perform_operation(OP_MOVE, NULL, full, trash_list[i].path) == 0)
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

		add_operation(OP_MOVE, NULL, NULL, full, buf);
		cmd_group_end();
		remove_from_trash(trash_name);
		return 0;
	}
	return -1;
}

int
remove_from_trash(const char trash_name[])
{
	int i;
	for(i = 0; i < nentries; i++)
	{
		if(stroscmp(trash_list[i].trash_name, trash_name) == 0)
			break;
	}
	if(i >= nentries)
		return -1;

	free(trash_list[i].path);
	free(trash_list[i].trash_name);
	memmove(trash_list + i, trash_list + i + 1,
			sizeof(*trash_list)*((nentries - 1) - i));

	nentries--;
	return 0;
}

char *
gen_trash_name(const char base_dir[], const char name[])
{
	struct stat st;
	char buf[PATH_MAX];
	int i = 0;
	char *const trash_dir = pick_trash_dir(base_dir);

	do
	{
		snprintf(buf, sizeof(buf), "%s/%03d_%s", trash_dir, i++, name);
		chosp(buf);
	}
	while(lstat(buf, &st) == 0);

	free(trash_dir);

	return strdup(buf);
}

/* Picks trash directory basing on original directory of a file that is being
 * trashed.  Returns absolute path to picked trash directory. */
static char *
pick_trash_dir(const char base_dir[])
{
	char *const trash_dir = get_ideal_trash_dir(base_dir);
	if(try_create_trash_dir(trash_dir) == 0)
	{
		return trash_dir;
	}
	return strdup(cfg.trash_dir);
}

int
is_under_trash(const char path[])
{
	char *const ideal_trash_dir = get_ideal_trash_dir(path);
	const int under_trash = path_starts_with(path, ideal_trash_dir);
	free(ideal_trash_dir);
	return under_trash;
}

int
is_trash_directory(const char path[])
{
	char *const ideal_trash_dir = get_ideal_trash_dir(path);
	const int trash_directory = stroscmp(path, ideal_trash_dir) == 0;
	free(ideal_trash_dir);
	return trash_directory;
}

/* Gets path to a preferred trash directory for files of a base directory.
 * Returns newly allocated string that should be freed by the caller. */
static char *
get_ideal_trash_dir(const char base_dir[])
{
	char full[PATH_MAX];
	if(get_mount_point(base_dir, sizeof(full), full) == 0)
	{
		return format_str("%s/.vifm-Trash", full);
	}

	return strdup(cfg.trash_dir);
}

const char *
get_real_name_from_trash_name(const char trash_name[])
{
	size_t prefix_len;
	const char *real_name = trash_name;

	if(is_path_absolute(real_name))
	{
		real_name += strlen(cfg.trash_dir);
		real_name = skip_char(real_name, '/');
	}

	prefix_len = strspn(real_name, "0123456789");
	if(real_name[prefix_len] == '_')
	{
		real_name += prefix_len + 1;
	}

	return real_name;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
