/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "autocmds.h"

#include <stddef.h> /* size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strcasecmp() strchr() strdup() */

#include "../compat/fs_limits.h"
#include "../compat/reallocarray.h"
#include "../utils/darray.h"
#include "../utils/path.h"
#include "../utils/str.h"

/* Describes single registered autocommand. */
typedef struct
{
	char *event;               /* Name of the event (case is ignored). */
	char *pattern;             /* Pattern for the path. */
	char *action;              /* Action to perform via handler. */
	vle_aucmd_handler handler; /* Handler to invoke on event firing. */
	int negated;               /* Whether pattern is negated. */
}
aucmd_info_t;

static int is_pattern_match(const aucmd_info_t *autocmd, const char path[]);
static void free_autocmd_data(aucmd_info_t *autocmd);

/* List of registered autocommands. */
static aucmd_info_t *autocmds;
/* Declarations to enable use of DA_* on autocmds. */
static DA_INSTANCE(autocmds);

int
vle_aucmd_on_execute(const char event[], const char pattern[],
		const char action[], vle_aucmd_handler handler)
{
	char canonic_path[PATH_MAX];

	aucmd_info_t *const autocmd = DA_EXTEND(autocmds);
	if(autocmd == NULL)
	{
		return 1;
	}

	autocmd->negated = (pattern[0] == '!');
	if(autocmd->negated)
	{
		++pattern;
	}

	if(strchr(pattern, '/') != NULL)
	{
		canonicalize_path(pattern, canonic_path, sizeof(canonic_path));
		pattern = canonic_path;
	}

	autocmd->event = strdup(event);
	autocmd->pattern = strdup(pattern);
	autocmd->action = strdup(action);
	autocmd->handler = handler;
	if(autocmd->event == NULL || autocmd->pattern == NULL ||
			autocmd->action == NULL)
	{
		free_autocmd_data(autocmd);
		return 1;
	}

	DA_COMMIT(autocmds);
	return 0;
}

void
vle_aucmd_execute(const char event[], const char path[], void *arg)
{
	size_t i;
	char canonic_path[PATH_MAX];

	canonicalize_path(path, canonic_path, sizeof(canonic_path));

	for(i = 0U; i < DA_SIZE(autocmds); ++i)
	{
		if(strcasecmp(event, autocmds[i].event) == 0 &&
				is_pattern_match(&autocmds[i], canonic_path))
		{
			autocmds[i].handler(autocmds[i].action, arg);
		}
	}
}

/* Checks whether path matches pattern in the autocommand.  Returns non-zero if
 * so, otherwise zero is returned. */
static int
is_pattern_match(const aucmd_info_t *autocmd, const char path[])
{
	int match = (strchr(autocmd->pattern, '/') == NULL)
	          ? paths_are_equal(autocmd->pattern, get_last_path_component(path))
	          : (stroscmp(path, autocmd->pattern) == 0);
	return match^autocmd->negated;
}

void
vle_aucmd_remove(const char event[], const char pattern[])
{
	int i;
	char canonic_path[PATH_MAX];

	if(pattern != NULL)
	{
		canonicalize_path(pattern, canonic_path, sizeof(canonic_path));
	}

	for(i = (int)DA_SIZE(autocmds) - 1; i >= 0; --i)
	{
		if(event != NULL && strcasecmp(event, autocmds[i].event) != 0)
		{
			continue;
		}
		if(pattern != NULL && stroscmp(canonic_path, autocmds[i].pattern) != 0)
		{
			continue;
		}

		free_autocmd_data(&autocmds[i]);
		DA_REMOVE(autocmds, &autocmds[i]);
	}
}

/* Frees data allocated for the autocommand. */
static void
free_autocmd_data(aucmd_info_t *autocmd)
{
	free(autocmd->event);
	free(autocmd->pattern);
	free(autocmd->action);
}

void
vle_aucmd_list(const char event[], const char pattern[], vle_aucmd_list_cb cb,
		void *arg)
{
	size_t i;
	char canonic_path[PATH_MAX];

	if(pattern != NULL)
	{
		canonicalize_path(pattern, canonic_path, sizeof(canonic_path));
	}

	for(i = 0U; i < DA_SIZE(autocmds); ++i)
	{
		if(event != NULL && strcasecmp(event, autocmds[i].event) != 0)
		{
			continue;
		}
		if(pattern != NULL && stroscmp(canonic_path, autocmds[i].pattern) != 0)
		{
			continue;
		}

		cb(autocmds[i].event, autocmds[i].pattern, autocmds[i].action, arg);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
