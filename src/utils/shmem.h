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

#ifndef VIFM__UTILS__SHMEM_H__
#define VIFM__UTILS__SHMEM_H__

#include <stddef.h> /* size_t */

/* Implementation of named memory area that's shared between processes. */

/* Opaque type for shared memory. */
typedef struct shmem_t shmem_t;

/* Creates or opens shared memory object.  On systems which allow it, the object
 * starts as at initial_size and can later be resized to max_size without
 * changing its address.  On all other systems, max_size is allocated from the
 * beginning, but the API works the same.  Returns shared memory object on
 * success or NULL on error. */
shmem_t * shmem_create(const char name[], size_t initial_size, size_t max_size);

/* Erases shared memory from the system and releases all resources associated
 * with it.  shmem can be NULL. */
void shmem_destroy(shmem_t *shmem);

/* Frees a watcher.  shmem can be NULL. */
void shmem_free(shmem_t *shmem);

/* Checks whether we are the ones who created this shared area.  Returns
 * non-zero if so, otherwise zero is returned. */
int shmem_created_by_us(shmem_t *shmem);

/* Retrieves pointer to the area mapped into current address space.  Returns the
 * pointer. */
void * shmem_get_ptr(shmem_t *shmem);

/* Resizes size of memory mapping.  Returns non-zero on success, zero on
 * failure. */
int shmem_resize(shmem_t *shmem, size_t new_size);

#endif /* VIFM__UTILS__SHMEM_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
