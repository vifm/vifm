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
#include <string.h> /* strcasecmp() strdup() */

#include "../compat/reallocarray.h"
#include "../utils/darray.h"
#include "../utils/str.h"

/* Describes single registered autocommand. */
typedef struct
{
	char *event;               /* Name of the event (case is ignored). */
	char *pattern;             /* Pattern for the path. */
	char *action;              /* Action to perform via handler. */
	vle_aucmd_handler handler; /* Handler to invoke on event firing. */
}
aucmd_info_t;

static void free_autocmd_data(aucmd_info_t *autocmd);

/* List of registered autocommands. */
static aucmd_info_t *autocmds;
/* Declarations to enable use of DA_* on autocmds. */
static DA_INSTANCE(autocmds);

int
vle_aucmd_on_execute(const char event[], const char pattern[],
		const char action[], vle_aucmd_handler handler)
{
	aucmd_info_t *const autocmd = DA_EXTEND(autocmds);
	if(autocmd == NULL)
	{
		return 1;
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
vle_aucmd_execute(const char event[], const char path[])
{
	size_t i;
	for(i = 0U; i < DA_SIZE(autocmds); ++i)
	{
		if(strcasecmp(event, autocmds[i].event) == 0 &&
				stroscmp(path, autocmds[i].pattern) == 0)
		{
			autocmds[i].handler(autocmds[i].action);
		}
	}
}

void
vle_aucmd_remove(const char event[], const char pattern[])
{
	int i;
	for(i = (int)DA_SIZE(autocmds) - 1; i >= 0; --i)
	{
		if(event != NULL && strcasecmp(event, autocmds[i].event) != 0)
		{
			continue;
		}
		if(pattern != NULL && stroscmp(pattern, autocmds[i].pattern) != 0)
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
vle_aucmd_list(const char event[], const char pattern[], vle_aucmd_list_cb cb)
{
	size_t i;
	for(i = 0U; i < DA_SIZE(autocmds); ++i)
	{
		if(event != NULL && strcasecmp(event, autocmds[i].event) != 0)
		{
			continue;
		}
		if(pattern != NULL && stroscmp(pattern, autocmds[i].pattern) != 0)
		{
			continue;
		}

		cb(autocmds[i].event, autocmds[i].pattern, autocmds[i].action);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
