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

#include "dynarray.h"

#include <stddef.h> /* NULL size_t offsetof() */
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* memset() */

/* Gets dynarray_t from pointer to its data. */
#define CAST(da) (dynarray_t*)((char *)(da) - offsetof(dynarray_t, data))

/* Information storage for the dynarray. */
typedef struct
{
	size_t size;     /* Amount of actually used memory (not counting this). */
	size_t capacity; /* Amount of available memory (not counting this). */
	char data[];     /* The rest of memory. */
}
dynarray_t;

void *
dynarray_extend(void *darray, size_t more)
{
	dynarray_t *dynarray;
	dynarray_t *new;
	size_t capacity;
	size_t size;

	dynarray = (darray != NULL) ? CAST(darray) : NULL;
	capacity = (dynarray != NULL) ? dynarray->capacity : 0U;
	size = (dynarray != NULL) ? dynarray->size : 0U;
	if(capacity >= size + more)
	{
		if(dynarray != NULL)
		{
			dynarray->size += more;
		}
		return darray;
	}

	capacity = (size + more)*((dynarray != NULL) ? 2 : 1);
	new = realloc(dynarray, sizeof(dynarray_t) + capacity);
	if(new == NULL)
	{
		return NULL;
	}

	new->capacity = capacity;
	new->size = size + more;

	return new->data;
}

void *
dynarray_cextend(void *darray, size_t more)
{
	dynarray_t *dynarray = (darray != NULL) ? CAST(darray) : NULL;
	size_t size = (dynarray != NULL) ? dynarray->size : 0U;
	char *new = dynarray_extend(darray, more);
	if(new != NULL)
	{
		memset(new + size, 0, more);
	}
	return new;
}

void
dynarray_free(void *darray)
{
	if(darray != NULL)
	{
		free(CAST(darray));
	}
}

void *
dynarray_shrink(void *darray)
{
	dynarray_t *dynarray = CAST(darray);
	if(dynarray->capacity > dynarray->size)
	{
		dynarray = realloc(dynarray, sizeof(*dynarray) + dynarray->size);
		if(dynarray != NULL)
		{
			dynarray->capacity = dynarray->size;
			return dynarray->data;
		}
	}
	return darray;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
