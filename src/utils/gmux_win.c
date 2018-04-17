/* vifm
 * Copyright (C) 2018 xaizek.
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

#include "gmux.h"

#include <windows.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* malloc() free() */

#include "str.h"

/* Data of a single gmux instance. */
struct gmux_t
{
	char *name;    /* Name of this object as known to the system. */
	HANDLE handle; /* File handle obtained from CreateFileMapping(). */
};

gmux_t *
gmux_create(const char name[])
{
	gmux_t *const gmux = malloc(sizeof(*gmux));
	if(gmux == NULL)
	{
		return NULL;
	}

	gmux->name = format_str("Local\\%s-gmux", name);
	if(gmux->name == NULL)
	{
		free(gmux);
		return NULL;
	}

	gmux->handle = CreateMutex(NULL, FALSE, gmux->name);
	if(gmux->handle == NULL)
	{
		free(gmux->name);
		free(gmux);
		return NULL;
	}

	return gmux;
}

void
gmux_destroy(gmux_t *gmux)
{
	gmux_free(gmux);
}

void
gmux_free(gmux_t *gmux)
{
	if(gmux != NULL)
	{
		CloseHandle(gmux->handle);
		free(gmux->name);
		free(gmux);
	}
}

int
gmux_lock(gmux_t *gmux)
{
	return (WaitForSingleObject(gmux->handle, INFINITE) == WAIT_FAILED);
}

int
gmux_unlock(gmux_t *gmux)
{
	return (ReleaseMutex(gmux->handle) == FALSE);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
