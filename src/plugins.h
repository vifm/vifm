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

#ifndef VIFM__PLUGINS_H__
#define VIFM__PLUGINS_H__

#include <stddef.h> /* size_t */

#include "utils/test_helpers.h"

/* Declaration of opaque state of the unit. */
typedef struct plugs_t plugs_t;

/* Status of a plugin. */
typedef enum
{
	PLS_SUCCESS, /* Loaded successfully. */
	PLS_SKIPPED, /* Loading was skipped. */
	PLS_FAILURE  /* Failed to load. */
}
PluginLoadStatus;

/* Information about a plugin. */
typedef struct plug_t
{
	char *name;      /* Plugin name. */
	char *path;      /* Full original path to the plugin. */
	char *real_path; /* Full resolved path to the plugin. */

	char *log;      /* Log output of the plugin. */
	size_t log_len; /* Length of the log field. */

	PluginLoadStatus status; /* Status. */
}
plug_t;

struct vlua_t;

/* Creates new instance of the unit.  Returns the instance or NULL. */
plugs_t * plugs_create(struct vlua_t *vlua);

/* Frees resources of the unit. */
void plugs_free(plugs_t *plugs);

/* Loads plugins. */
void plugs_load(plugs_t *plugs, const char base_dir[]);

/* Assigns *plug to information structure about a plugin specified by its index.
 * Returns non-zero on success, otherwise zero is returned. */
int plugs_get(const plugs_t *plugs, int idx, const plug_t **plug);

/* Blacklists the plugin. */
void plugs_blacklist(plugs_t *plugs, const char name[]);

/* Whitelists the plugin. */
void plugs_whitelist(plugs_t *plugs, const char name[]);

/* Performs completion of plugin names. */
void plugs_complete(plugs_t *plugs, const char prefix[]);

/* Adds message to the log of the plugin on a new line. */
void plug_log(plug_t *plug, const char msg[]);

TSTATIC_DEFS(
	void plugs_sort(plugs_t *plugs);
	int plugs_loaded(const plugs_t *plugs);
	struct strlist_t;
	struct strlist_t plugs_get_blacklist(plugs_t *plugs);
	struct strlist_t plugs_get_whitelist(plugs_t *plugs);
)

#endif /* VIFM__PLUGINS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
