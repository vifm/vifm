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

#include "selector.h"

#include <windows.h>

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() malloc() */

#include "../compat/reallocarray.h"

/* Selector object. */
struct selector_t
{
	selector_item_t *items; /* Set of items to watch. */
	int size;               /* Used amount of items. */
	int capacity;           /* Reserved amount of items. */
	selector_item_t ready;  /* Item that is ready to be read from or invalid. */
};

selector_t *
selector_alloc(void)
{
	selector_t *selector = malloc(sizeof(*selector));
	if(selector != NULL)
	{
		selector->items = NULL;
		selector->capacity = 0;
		selector_reset(selector);
	}
	return selector;
}

void
selector_free(selector_t *selector)
{
	free(selector->items);
	free(selector);
}

void
selector_reset(selector_t *selector)
{
	selector->size = 0;
	selector->ready = INVALID_HANDLE_VALUE;
}

void
selector_add(selector_t *selector, selector_item_t item)
{
	int i;
	for(i = 0; i < selector->size; ++i)
	{
		if(selector->items[i] == item)
		{
			return;
		}
	}

	if(selector->size == selector->capacity)
	{
		int new_capacity = (selector->capacity == 0 ? 4 : selector->capacity*2);
		selector_item_t *items = reallocarray(selector->items, new_capacity,
				sizeof(*selector->items));
		if(items == NULL)
		{
			return;
		}

		selector->items = items;
		selector->capacity = new_capacity;
	}

	selector->items[selector->size++] = item;
}

void
selector_remove(selector_t *selector, selector_item_t item)
{
	int i;
	for(i = 0; i < selector->size; ++i)
	{
		if(selector->items[i] == item)
		{
			selector->items[i] = selector->items[--selector->size];
			break;
		}
	}
}

int
selector_wait(selector_t *selector, int delay)
{
	if(delay < 0)
	{
		delay = 0;
	}

	DWORD res = WaitForMultipleObjects(selector->size, selector->items, 0, delay);
	if(res < WAIT_OBJECT_0 || res >= WAIT_OBJECT_0 + selector->size)
	{
		selector->ready = INVALID_HANDLE_VALUE;
		return 0;
	}

	selector->ready = selector->items[res - WAIT_OBJECT_0];
	return 1;
}

int
selector_is_ready(selector_t *selector, selector_item_t item)
{
	return selector->ready != INVALID_HANDLE_VALUE
	    && selector->ready == item;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
