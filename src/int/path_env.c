/* vifm
 * Copyright (C) 2012 xaizek.
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

#include "path_env.h"

#include <stdio.h> /* snprintf() sprintf() */
#include <stdlib.h> /* malloc() free() */
#include <string.h> /* strchr() strlen() */

#include "../cfg/config.h"
#include "../compat/dtype.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../engine/variables.h"
#include "../utils/env.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"

static int path_env_was_changed(int force);
static void append_scripts_dirs(void);
static void add_dirs_to_path(const char *path);
static void add_to_path(const char *path);
static void split_path_list(void);

static char **paths;
static int paths_count;

static char *clean_path;
static char *real_path;

char **
get_paths(size_t *count)
{
	update_path_env(0);
	*count = paths_count;
	return paths;
}

void
update_path_env(int force)
{
	if(path_env_was_changed(force))
	{
		append_scripts_dirs();
		split_path_list();
	}
}

/* Checks if PATH environment variable was changed. Returns non-zero if path was
 * altered since last call. */
static int
path_env_was_changed(int force)
{
	const char *path;

	path = local_getenv("PATH");

	if(clean_path != NULL && stroscmp(clean_path, path) == 0)
	{
		return force;
	}

	(void)replace_string(&clean_path, path);
	return 1;
}

/* Adds $VIFM/scripts and its subdirectories to PATH environment variable. */
static void
append_scripts_dirs(void)
{
	char scripts_dir[PATH_MAX + 16];
	snprintf(scripts_dir, sizeof(scripts_dir), "%s/" SCRIPTS_DIR, cfg.config_dir);
	add_dirs_to_path(scripts_dir);
}

/* Traverses passed directory recursively and adds it and all found
 * subdirectories to PATH environment variable. */
static void
add_dirs_to_path(const char *path)
{
	DIR *dir;
	struct dirent *dentry;
	const char *slash = "";

	dir = os_opendir(path);
	if(dir == NULL)
	{
		return;
	}

	slash = ends_with_slash(path) ? "" : "/";

	add_to_path(path);

	while((dentry = os_readdir(dir)) != NULL)
	{
		char full_path[PATH_MAX + 1];

		if(is_builtin_dir(dentry->d_name))
		{
			continue;
		}

		snprintf(full_path, sizeof(full_path), "%s%s%s", path, slash,
				dentry->d_name);
#ifndef _WIN32
		if(get_dirent_type(dentry, full_path) == DT_DIR)
#else
		if(is_dir(full_path))
#endif
		{
			add_dirs_to_path(full_path);
		}
	}

	os_closedir(dir);
}

/* Adds a path to PATH environment variable. */
static void
add_to_path(const char *path)
{
	const char *old_path = env_get_def("PATH", "");
	char *new_path = malloc(strlen(path) + 1 + strlen(old_path) + 1);
	if(new_path == NULL)
	{
		return;
	}

#ifndef _WIN32
	sprintf(new_path, "%s:%s", path, old_path);
#else
	sprintf(new_path, "%s;%s", path, old_path);
#endif
	internal_to_system_slashes(new_path);
	env_set("PATH", new_path);

	free(new_path);
}

/* Breaks PATH environment variable into list of paths. */
static void
split_path_list(void)
{
	const char *path, *p, *q;
	int i;

	path = env_get_def("PATH", "");

	if(paths != NULL)
		free_string_array(paths, paths_count);

	paths_count = 1;
	p = path;
	while((p = strchr(p, ':')) != NULL)
	{
		paths_count++;
		p++;
	}

	paths = reallocarray(NULL, paths_count, sizeof(paths[0]));
	if(paths == NULL)
	{
		paths_count = 0;
		return;
	}

	i = 0;
	p = path - 1;
	do
	{
		int j;
		char *s;

		p++;
#ifndef _WIN32
		q = strchr(p, ':');
#else
		q = strchr(p, ';');
#endif
		if(q == NULL)
		{
			q = p + strlen(p);
		}

		s = malloc(q - p + 1U);
		if(s == NULL)
		{
			free_string_array(paths, i - 1);
			paths = NULL;
			paths_count = 0;
			return;
		}
		copy_str(s, q - p + 1U, p);

		p = q;

		s = replace_tilde(s);

		/* No need to check "." path for existence. */
		if(strcmp(s, ".") != 0)
		{
			if(!path_exists(s, DEREF))
			{
				free(s);
				continue;
			}
		}

		paths[i++] = s;

		for(j = 0; j < i - 1; j++)
		{
			if(stroscmp(paths[j], s) == 0)
			{
				free(s);
				i--;
				break;
			}
		}
	}
	while(q[0] != '\0');
	paths_count = i;
}

void
load_clean_path_env(void)
{
	(void)replace_string(&real_path, env_get_def("PATH", ""));
	env_set("PATH", clean_path);
}

void
load_real_path_env(void)
{
	env_set("PATH", real_path);

	free(real_path);
	real_path = NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
