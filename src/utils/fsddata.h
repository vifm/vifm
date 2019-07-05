/* vifm
 * Copyright (C) 2011 xaizek.
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

#ifndef VIFM__UTILS__FSDDATA_H__
#define VIFM__UTILS__FSDDATA_H__

/* Thin type-safe wrapper around fsdata_t that manages heap allocated pointers.
 * Once put into this structure the structure becomes responsible for handling
 * the pointers. */

/* Declaration of opaque fsddata type. */
typedef struct fsddata_t fsddata_t;

/* prefix mode causes queries to return nearest match when exact match is not
 * available.  Non-zero resolve_paths enables path resolution, which also
 * forbids use of nonexistent files.  Returns NULL on error. */
fsddata_t * fsddata_create(int prefix, int resolve_paths);

/* Frees the structure.  Freeing of NULL fsdd is OK. */
void fsddata_free(fsddata_t *fsdd);

/* Assigns (possibly with overwrite) data to specified path.  Returns zero on
 * success, otherwise non-zero is returned. */
int fsddata_set(fsddata_t *fsdd, const char path[], void *data);

/* Retrieves data associated with the path (or closest predecessor on prefix
 * matches).  Doesn't change data content if path absent.  Returns zero on
 * success and non-zero on error. */
int fsddata_get(fsddata_t *fsdd, const char path[], void **data);

#endif /* VIFM__UTILS__FSDDATA_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
