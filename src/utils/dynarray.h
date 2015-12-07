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

#ifndef VIFM__UTILS__DYNARRAY_H__
#define VIFM__UTILS__DYNARRAY_H__

/* Simple dynamic array implementation that provides less memory reallocations,
 * hence it's faster for growing arrays. */

#include <stddef.h> /* size_t */

/* Reallocates darray previously allocated by this function (pass NULL the first
 * time) by adding at least more bytes to it.  Returns rellocated pointer or
 * NULL (on error, or on (NULL, 0)-call). */
void * dynarray_extend(void *darray, size_t more);

/* Same as dynarray_extend(), but zeroes newly allocated memory. */
void * dynarray_cextend(void *darray, size_t more);

/* Frees memory previously allocated with dynarray_[c]extend().  NULL argument
 * is fine. */
void dynarray_free(void *darray);

/* Frees unused memory of the darray.  Returns possibly reallocated pointer. */
void * dynarray_shrink(void *darray);

#endif /* VIFM__UTILS__DYNARRAY_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
