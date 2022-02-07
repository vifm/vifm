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

#include <stdarg.h> /* va_list va_start() va_end() vsnprintf() */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* strcmp() */

#include "compat/fs_limits.h"
#include "compat/os.h"
#include "engine/completion.h"
#include "lua/vlua.h"
#include "utils/darray.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"

/* State of the unit. */
struct plugs_t
{
	struct vlua_t *vlua; /* Reference to vlua unit. */

	strlist_t blacklist; /* List of plugins to skip. */
	strlist_t whitelist; /* List of plugins to load. */

	plug_t **plugs;           /* Known plugins. */
	DA_INSTANCE_FIELD(plugs); /* Declarations to enable use of DA_* on plugs. */

	int loaded; /* Whether plugins were loaded, shouldn't load them twice. */
};

static void plug_free(plug_t *plug);
static plug_t * find_plug(plugs_t *plugs, const char real_path[]);
static int should_be_loaded(plugs_t *plugs, const char name[]);
static void plug_logf(plug_t *plug, const char format[], ...)
	_gnuc_printf(2, 3);
static void add_if_missing(strlist_t *strlist, const char item[]);
TSTATIC void plugs_sort(plugs_t *plugs);
static int plug_cmp(const void *a, const void *b);
TSTATIC int plugs_loaded(const plugs_t *plugs);
TSTATIC strlist_t plugs_get_blacklist(plugs_t *plugs);
TSTATIC strlist_t plugs_get_whitelist(plugs_t *plugs);

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
			plug_free(plugs->plugs[i]);
		}
		DA_REMOVE_ALL(plugs->plugs);

		free_string_array(plugs->blacklist.items, plugs->blacklist.nitems);
		free_string_array(plugs->whitelist.items, plugs->whitelist.nitems);

		free(plugs);
	}
}

void
plugs_load(plugs_t *plugs, const char base_dir[])
{
	if(plugs->loaded)
	{
		return;
	}

	char full_path[PATH_MAX + 1];
	snprintf(full_path, sizeof(full_path), "%s/plugins", base_dir);

	DIR *dir = os_opendir(full_path);
	if(dir == NULL)
	{
		return;
	}

	plugs->loaded = 1;

	struct dirent *entry;
	while((entry = os_readdir(dir)) != NULL)
	{
		char path[PATH_MAX + NAME_MAX + 4];
		snprintf(path, sizeof(path), "%s/%s", full_path, entry->d_name);

		/* XXX: is_dirent_targets_dir() does slowfs checks, do they harm here? */
		if(entry->d_name[0] == '.' || is_builtin_dir(entry->d_name) ||
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

		/* TODO: reserve "vifm" name of the plugin and reject it here? */
		/* TODO: reject plugins with "#" in their name. */

		plug->name = strdup(entry->d_name);
		plug->path = strdup(path);
		if(plug->name == NULL || plug->path == NULL)
		{
			plug_free(plug);
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
			plug_free(plug);
			continue;
		}

		/* Perform the check before committing this plugin. */
		plug_t *duplicate = find_plug(plugs, plug->real_path);

		plug->status = PLS_FAILURE;
		DA_COMMIT(plugs->plugs);

		if(duplicate != NULL)
		{
			plug_logf(plug, "[vifm][error]: skipped as a duplicate of %s",
					duplicate->path);
			plug->status = PLS_SKIPPED;
		}
		else if(!should_be_loaded(plugs, plug->name))
		{
			plug_log(plug, "[vifm][info]: skipped due to blacklist/whitelist");
			plug->status = PLS_SKIPPED;
		}
		else if(vlua_load_plugin(plugs->vlua, entry->d_name, plug) == 0)
		{
			plug_log(plug, "[vifm][info]: plugin was loaded successfully");
			plug->status = PLS_SUCCESS;
		}
		else
		{
			plug_log(plug, "[vifm][error]: loading plugin has failed");
		}
	}

	os_closedir(dir);
}

/* Frees a plugin.  The argument can't be NULL. */
static void
plug_free(plug_t *plug)
{
	free(plug->name);
	free(plug->path);
	free(plug->real_path);
	free(plug->log);
	free(plug);
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

/* Checks with blacklist and whitelist to decide if plugin should be loaded.
 * Returns non-zero if loading is allowed. */
static int
should_be_loaded(plugs_t *plugs, const char name[])
{
	if(plugs->whitelist.nitems != 0)
	{
		return (string_array_pos(plugs->whitelist.items, plugs->whitelist.nitems,
					name) != -1);
	}

	return (string_array_pos(plugs->blacklist.items, plugs->blacklist.nitems,
				name) == -1);
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

/* Adds formatted message to the log on a new line. */
static void
plug_logf(plug_t *plug, const char format[], ...)
{
	va_list ap;
	va_start(ap, format);

	char buf[1024];
	vsnprintf(buf, sizeof(buf), format, ap);
	plug_log(plug, buf);

	va_end(ap);
}

void
plugs_blacklist(plugs_t *plugs, const char name[])
{
	add_if_missing(&plugs->blacklist, name);
}

void
plugs_whitelist(plugs_t *plugs, const char name[])
{
	add_if_missing(&plugs->whitelist, name);
}

/* Adds an item to the string list unless it's already there. */
static void
add_if_missing(strlist_t *strlist, const char item[])
{
	if(string_array_pos(strlist->items, strlist->nitems, item) == -1)
	{
		strlist->nitems = add_to_string_array(&strlist->items, strlist->nitems,
				item);
	}
}

void
plugs_complete(plugs_t *plugs, const char prefix[])
{
	size_t i;
	const size_t prefix_len = strlen(prefix);
	for(i = 0U; i < DA_SIZE(plugs->plugs); ++i)
	{
		const char *name = plugs->plugs[i]->name;
		if(strncmp(name, prefix, prefix_len) == 0)
		{
			vle_compl_add_match(name, "");
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(prefix);
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

/* Sorts plugins by their path. */
TSTATIC void
plugs_sort(plugs_t *plugs)
{
	safe_qsort(plugs->plugs, DA_SIZE(plugs->plugs), sizeof(*plugs->plugs),
			&plug_cmp);
}

/* Compares two plugins by their paths.  Returns negative, zero or positive
 * number meaning less than, equal or greater than correspondingly. */
static int
plug_cmp(const void *a, const void *b)
{
	const plug_t *plug_a = *(const plug_t **)a;
	const plug_t *plug_b = *(const plug_t **)b;
	return strcmp(plug_a->path, plug_b->path);
}

TSTATIC int
plugs_loaded(const plugs_t *plugs)
{
	return plugs->loaded;
}

/* Retrieves blacklist.  Returns the list. */
TSTATIC strlist_t
plugs_get_blacklist(plugs_t *plugs)
{
	return plugs->blacklist;
}

/* Retrieves whitelist.  Returns the list. */
TSTATIC strlist_t
plugs_get_whitelist(plugs_t *plugs)
{
	return plugs->whitelist;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
