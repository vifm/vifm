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

#include "shmem.h"

#include <windows.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* malloc() free() */

#include "str.h"

/* Data of a single shmem instance. */
struct shmem_t
{
	char *name;      /* Name of this object as known to the system. */
	HANDLE handle;   /* File handle obtained from CreateFileMapping(). */
	int created;     /* This instance was created by us. */
	void *ptr;       /* The shared memory as an unstructured blob of bytes. */
	size_t max_size; /* Maximum size of shared memory region. */
};

shmem_t *
shmem_create(const char name[], size_t initial_size, size_t max_size)
{
	shmem_t *const shmem = malloc(sizeof(*shmem));
	if(shmem == NULL)
	{
		return NULL;
	}

	shmem->name = format_str("Local\\%s-shmem", name);
	if(shmem->name == NULL)
	{
		free(shmem);
		return NULL;
	}

	shmem->ptr = NULL;
	shmem->max_size = max_size;

  shmem->handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
			0, max_size, shmem->name);
	if(shmem->handle == INVALID_HANDLE_VALUE)
	{
		free(shmem->name);
		free(shmem);
		return NULL;
	}

	shmem->created = (GetLastError() != ERROR_ALREADY_EXISTS);

	shmem->ptr = MapViewOfFile(shmem->handle, FILE_MAP_ALL_ACCESS, 0, 0,
			max_size);

	if(shmem->ptr == NULL)
	{
		shmem_free(shmem);
		return NULL;
	}

	return shmem;
}

void
shmem_destroy(shmem_t *shmem)
{
	shmem_free(shmem);
}

void
shmem_free(shmem_t *shmem)
{
	if(shmem == NULL)
	{
		return;
	}

	if(shmem->ptr != NULL)
	{
		UnmapViewOfFile(shmem->ptr);
	}

	CloseHandle(shmem->handle);
	free(shmem->name);
	free(shmem);
}

int
shmem_created_by_us(shmem_t *shmem)
{
	return shmem->created;
}

void *
shmem_get_ptr(shmem_t *shmem)
{
	return shmem->ptr;
}

int
shmem_resize(shmem_t *shmem, size_t new_size)
{
	return (new_size <= shmem->max_size);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
