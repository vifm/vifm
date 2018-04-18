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

#include <sys/stat.h> /* S_* */
#include <fcntl.h> /* O_* open() */
#include <unistd.h> /* close() lockf() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* malloc() free() */

#include "path.h"
#include "str.h"

/* Data of a single gmux instance. */
struct gmux_t
{
	char *name; /* Name of this object as known to the system. */
	int fd;     /* File handle obtained from open(). */
};

gmux_t *
gmux_create(const char name[])
{
	gmux_t *const gmux = malloc(sizeof(*gmux));
	if(gmux == NULL)
	{
		return NULL;
	}

	gmux->name = format_str("%s/vifm-%s.gmux", get_tmpdir(), name);
	if(gmux->name == NULL)
	{
		free(gmux);
		return NULL;
	}

	gmux->fd = open(gmux->name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
	if(gmux->fd == -1)
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
	if(gmux != NULL)
	{
		unlink(gmux->name);
		gmux_free(gmux);
	}
}

void
gmux_free(gmux_t *gmux)
{
	if(gmux != NULL)
	{
		close(gmux->fd);
		free(gmux->name);
		free(gmux);
	}
}

int
gmux_lock(gmux_t *gmux)
{
	return lockf(gmux->fd, F_LOCK, 0);
}

int
gmux_unlock(gmux_t *gmux)
{
	return lockf(gmux->fd, F_ULOCK, 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
