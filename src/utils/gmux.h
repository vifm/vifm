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

#ifndef VIFM__UTILS__GMUX_H__
#define VIFM__UTILS__GMUX_H__

/* Implementation of named mutex that's shared between processes and is
 * therefore global (hence the "g" prefix).
 *
 * The mutex is automatically released when the process exits normally or
 * unexpectedly terminates. */

/* Opaque type of the mutex. */
typedef struct gmux_t gmux_t;

/* Creates or opens a mutex with specified name.  Returns it on success or NULL
 * on error. */
gmux_t * gmux_create(const char name[]);

/* Erases mutex from the system and releases all resources associated with it.
 * gmux can be NULL. */
void gmux_destroy(gmux_t *gmux);

/* Frees in process resources associated with the mutex.  gmux can be NULL. */
void gmux_free(gmux_t *gmux);

/* Locks the mutex.  Returns zero on success, otherwise non-zero is returned. */
int gmux_lock(gmux_t *gmux);

/* Unlocks the mutex.  Returns zero on success, otherwise non-zero is
 * returned. */
int gmux_unlock(gmux_t *gmux);

#endif /* VIFM__UTILS__GMUX_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
