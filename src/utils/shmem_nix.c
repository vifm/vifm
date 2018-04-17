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

#include <sys/mman.h> /* mmap, shm_*() */
#include <fcntl.h>    /* O_RDWR, O_* */
#include <unistd.h>   /* ftruncate() */

#include <errno.h>  /* EEXIST ENOENT errno */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* malloc() free() */

#include "str.h"

/* Data of a single shmem instance. */
struct shmem_t
{
	char *name;      /* Name of this object as known to the system. */
	int fd;          /* File descriptor obtained from shm_open(). */
	int created;     /* This instance was created by us. */
	void *ptr;       /* The shared memory as an unstructured blob of bytes. */
	size_t max_size; /* Maximum size of shared memory region. */
};

shmem_t *
shmem_create(const char name[], size_t initial_size, size_t max_size)
{
	int error_other;
	int error_excl_already_exists;
	int error_normal_does_not_exist;
	int fd;

	shmem_t *const shmem = malloc(sizeof(*shmem));
	if(shmem == NULL)
	{
		return NULL;
	}

	shmem->name = format_str("/vifm-%s", name);
	if(shmem->name == NULL)
	{
		free(shmem);
		return NULL;
	}

	shmem->ptr = NULL;
	shmem->max_size = max_size;

	do {
		/* Reset errors. */
		error_other = 0;
		error_excl_already_exists = 0;
		error_normal_does_not_exist = 0;

		/* Try exclusive access first. */
		fd = shm_open(shmem->name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if(fd != -1)
		{
			/* No error exclusive / we know we are init! */
			break;
		}

		/* Error exclusive. */
		error_excl_already_exists = (errno == EEXIST);
		if(error_excl_already_exists)
		{
			/* Already exists => try to attach to existing. */
			fd = shm_open(shmem->name, O_RDWR, 0600);
			if(fd == -1)
			{
				/* We first got EEXIST now ENOENT => file deleted in the meantime
				 * => retry! */
				error_normal_does_not_exist = (errno == ENOENT);
				error_other = !error_normal_does_not_exist;
			}
		}
		else
		{
			error_other = 1;
		}
	}
	while(!error_other && error_excl_already_exists &&
		error_normal_does_not_exist);

	/* Possible cases:
	 * (1) error_other                => FAIL
	 * (2) error_excl_already_exists  => OK, just attached normally
	 * (3) !error_excl_already_exists => OK, responsible for initialization */

	if(error_other)
	{
		free(shmem->name);
		free(shmem);
		return NULL;
	}

	shmem->fd = fd;
	shmem->created = !error_excl_already_exists;

	/* Set initial size if we are the ones who created this shared memory. */
	if(shmem->created && ftruncate(fd, initial_size) == -1)
	{
		shmem_destroy(shmem);
		return NULL;
	}

	shmem->ptr = mmap(NULL, max_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(shmem->ptr == MAP_FAILED)
	{
		if(shmem->created)
		{
			shmem_destroy(shmem);
		}
		else
		{
			shmem_free(shmem);
		}
		return NULL;
	}

	return shmem;
}

void
shmem_destroy(shmem_t *shmem)
{
	if(shmem != NULL)
	{
		shm_unlink(shmem->name);
		shmem_free(shmem);
	}
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
		munmap(shmem->ptr, shmem->max_size);
	}

	close(shmem->fd);
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
	return (new_size <= shmem->max_size && ftruncate(shmem->fd, new_size) == 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
