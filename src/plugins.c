/* vifm
 * Copyright (C) 2020 xaizek.
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

#include "plugins.h"

#include <stdlib.h> /* calloc() free() */

#include "compat/fs_limits.h"
#include "compat/os.h"
#include "lua/vlua.h"
#include "utils/darray.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"

/* State of the unit. */
struct plugs_t
{
	struct vlua_t *vlua;      /* Reference to vlua unit. */
	plug_t **plugs;           /* Known plugins. */
	DA_INSTANCE_FIELD(plugs); /* Declarations to enable use of DA_* on plugs. */
};

static plug_t * find_plug(plugs_t *plugs, const char real_path[]);

plugs_t *
plugs_create(struct vlua_t *vlua)
{
	plugs_t *plugs = calloc(1, sizeof(*plugs));
	if(plugs == NULL)
	{
		return NULL;
	}

	plugs->vlua = vlua;
	return plugs;
}

void
plugs_free(plugs_t *plugs)
{
	if(plugs != NULL)
	{
		size_t i;
		for(i = 0U; i < DA_SIZE(plugs->plugs); ++i)
		{
			free(plugs->plugs[i]->path);
			free(plugs->plugs[i]->real_path);
			free(plugs->plugs[i]->log);
			free(plugs->plugs[i]);
		}
		DA_REMOVE_ALL(plugs->plugs);

		free(plugs);
	}
}

void
plugs_load(plugs_t *plugs, const char base_dir[])
{
	char full_path[PATH_MAX + 1];
	snprintf(full_path, sizeof(full_path), "%s/plugins", base_dir);

	DIR *dir = os_opendir(full_path);
	if(dir == NULL)
	{
		return;
	}

	struct dirent *entry;
	while((entry = os_readdir(dir)) != NULL)
	{
		char path[PATH_MAX + NAME_MAX + 4];
		snprintf(path, sizeof(path), "%s/%s", full_path, entry->d_name);

		/* XXX: is_dirent_targets_dir() does slowfs checks, do they harm here? */
		if(is_builtin_dir(entry->d_name) ||
				!is_dirent_targets_dir(path, entry))
		{
			continue;
		}

		plug_t **plug_ptr = DA_EXTEND(plugs->plugs);
		if(plug_ptr == NULL)
		{
			continue;
		}

		plug_t *plug = calloc(1, sizeof(*plug));
		if(plug == NULL)
		{
			continue;
		}
		*plug_ptr = plug;

		plug->path = strdup(path);
		if(plug->path == NULL)
		{
			free(plug);
			continue;
		}

		if(os_realpath(plug->path, path) == NULL)
		{
			plug->real_path = strdup(plug->path);
		}
		else
		{
			plug->real_path = strdup(path);
		}
		if(plug->real_path == NULL)
		{
			free(plug->path);
			free(plug);
			continue;
		}

		/* Do the check before committing this plugin. */
		plug_t *duplicate = find_plug(plugs, plug->real_path);

		plug->status = PLS_FAILURE;
		DA_COMMIT(plugs->plugs);

		if(duplicate != NULL)
		{
			plug->status = PLS_SKIPPED;
		}
		else if(vlua_load_plugin(plugs->vlua, entry->d_name, plug) == 0)
		{
			plug->status = PLS_SUCCESS;
		}
	}

	os_closedir(dir);
}

/* Looks up a plugin specified by real path among already loaded plugins.
 * Returns pointer to it or NULL. */
static plug_t *
find_plug(plugs_t *plugs, const char real_path[])
{
	size_t i;
	for(i = 0U; i < DA_SIZE(plugs->plugs); ++i)
	{
		if(stroscmp(plugs->plugs[i]->real_path, real_path) == 0)
		{
			return plugs->plugs[i];
		}
	}
	return NULL;
}

int
plugs_get(const plugs_t *plugs, int idx, const plug_t **plug)
{
	if(idx < 0 || idx >= (int)DA_SIZE(plugs->plugs))
	{
		return 0;
	}

	*plug = plugs->plugs[idx];
	return 1;
}

void
plug_log(plug_t *plug, const char msg[])
{
	if(plug->log_len != 0)
	{
		(void)strappendch(&plug->log, &plug->log_len, '\n');
	}
	(void)strappend(&plug->log, &plug->log_len, msg);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
