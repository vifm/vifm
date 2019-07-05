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

#include "fsddata.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */

#include "private/fsdata.h"
#include "fsdata.h"

static void cleanup(void *data);

fsddata_t *
fsddata_create(int prefix, int resolve_paths)
{
	fsdata_t *const fsd = fsdata_create(prefix, resolve_paths);
	if(fsd != NULL)
	{
		fsdata_set_cleanup(fsd, &cleanup);
	}
	return (fsddata_t *)fsd;
}

/* fsdata cleanup function that frees memory associated with a node. */
static void
cleanup(void *data)
{
	void **p = data;
	free(*p);
}

void
fsddata_free(fsddata_t *fsdd)
{
	fsdata_free((fsdata_t *)fsdd);
}

int
fsddata_set(fsddata_t *fsdd, const char path[], void *data)
{
	return fsdata_set((fsdata_t *)fsdd, path, &data, sizeof(data));
}

int
fsddata_get(fsddata_t *fsdd, const char path[], void **data)
{
	return fsdata_get((fsdata_t *)fsdd, path, data, sizeof(*data));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
