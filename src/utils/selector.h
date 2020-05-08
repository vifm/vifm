/* vifm
 * Copyright (C) 2020 xaizek.
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

#ifndef VIFM__UTILS__SELECTOR_H__
#define VIFM__UTILS__SELECTOR_H__

/* This unit is meant to provide abstraction over platform-specific mechanism
 * for waiting for available data. */

/* Type of object selector works with. */
#ifdef _WIN32
#include <windows.h>
typedef HANDLE selector_item_t;
#else
typedef int selector_item_t;
#endif

/* Opaque selector type. */
typedef struct selector_t selector_t;

/* Allocates new selector object.  Returns the selector or NULL on error. */
selector_t * selector_alloc(void);

/* Frees selector.  The parameter can be NULL. */
void selector_free(selector_t *selector);

/* Resets selector to empty state. */
void selector_reset(selector_t *selector);

/* Adds item to the set of objects to watch.  If error occurs, its ignored. */
void selector_add(selector_t *selector, selector_item_t item);

/* Removes item from the set of objects to watch. */
void selector_remove(selector_t *selector, selector_item_t item);

/* Waits for at least one of watched objects to become available for reading
 * from during the period of time specified by the delay in milliseconds.
 * Returns zero on error or if timeout was reached without any of the objects
 * becoming available for read, otherwise non-zero is returned. */
int selector_wait(selector_t *selector, int delay);

/* Checks whether specified element is ready for read.  Use this function after
 * selector_wait().  Returns non-zero if so, otherwise zero is returned. */
int selector_is_ready(selector_t *selector, selector_item_t item);

#endif /* VIFM__UTILS__SELECTOR_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
