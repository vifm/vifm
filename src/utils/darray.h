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

#ifndef VIFM__UTILS__DARRAY_H__
#define VIFM__UTILS__DARRAY_H__

/* Simple macros that wrap common operations on dynamic arrays. */

#include <assert.h> /* assert() */
#include <stddef.h> /* ptrdiff_t size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* memcpy() */

#include "../compat/reallocarray.h"

/* Instantiates additional data for dynamic array implementation.  Usage
 * example:
 *   static int *array;
 *   static DA_INSTANCE(array); */
#define DA_INSTANCE(da) DA_INSTANCE_FIELD(da) = 0U

/* DA_INSTANCE equivalent for structures. */
#define DA_INSTANCE_FIELD(da) size_t da##_count__

/* Obtains lvalue of array size, its type is size_t. */
#define DA_SIZE(da) *(&da##_count__)

/* Extends array capacity (not size) by at least one more element.  Returns
 * pointer to the element or NULL on memory allocation error.  Once element data
 * is successfully initialized DA_COMMIT should be used to update size. */
#define DA_EXTEND(da) \
	({ \
		typeof(da) last = NULL; \
		void *const ptr = reallocarray(da, da##_count__ + 1U, sizeof(*da)); \
		if(ptr != NULL) \
		{ \
			da = ptr; \
			last = &da[da##_count__]; \
		} \
		last; \
	})

/* Increments array size, to be used in pair with DA_EXTEND.  Usage example:
 *   char **const string = DA_EXTEND(strings);
 *   if(string != NULL)
 *   {
 *     *string = strdup(str);
 *     if(*string != NULL)
 *     {
 *       DA_COMMIT(strings);
 *     }
 *   } */
#define DA_COMMIT(a) do { ++a##_count__; } while(0)

/* Removes item specified by pointer. */
#define DA_REMOVE(da, item) \
	do \
	{ \
		const typeof(da) it = item; \
		size_t i; \
		assert(it >= da && "Wrong item pointer."); \
		assert(it - da < (ptrdiff_t)da##_count__ && "Wrong item pointer."); \
		for(i = it - da + 1U; i < da##_count__; ++i) \
		{ \
			memcpy(&da[i - 1], &da[i], sizeof(*da)); \
		} \
		if(--da##_count__ == 0) \
		{ \
			free(da); \
			da = NULL; \
		} \
	} \
	while(0)

/* Removes all elements starting from and including the item. */
#define DA_REMOVE_AFTER(da, item) \
	do \
	{ \
		const typeof(da) it = item; \
		assert(it >= da && "Wrong item pointer."); \
		assert(it - da <= (ptrdiff_t)da##_count__ && "Wrong item pointer."); \
		da##_count__ = it - da; \
		if(da##_count__ == 0) \
		{ \
			free(da); \
			da = NULL; \
		} \
	} \
	while(0)

/* Empties the array freeing allocated memory. */
#define DA_REMOVE_ALL(da) \
	do \
	{ \
		da##_count__ = 0; \
		free(da); \
		da = NULL; \
	} \
	while(0)

#endif /* VIFM__UTILS__DARRAY_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
