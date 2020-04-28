/* vifm
 * Copyright (C) 2013 xaizek.
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

#include "hist.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* memmove() */
#include <time.h> /* time_t */

#include "../compat/reallocarray.h"
#include "macros.h"

static int move_to_first_position(hist_t *hist, const char item[],
		time_t timestamp);
static int insert_at_first_position(hist_t *hist, const char item[],
		time_t timestamp);

int
hist_init(hist_t *hist, int capacity)
{
	if(capacity < 0)
	{
		capacity = 0;
	}

	hist->size = 0;
	hist->capacity = 0;

	hist->items = calloc(capacity, sizeof(*hist->items));
	if(hist->items == NULL)
	{
		return 1;
	}

	hist->capacity = capacity;
	return 0;
}

void
hist_reset(hist_t *hist)
{
	int i;
	for(i = 0; i < hist->size; ++i)
	{
		free(hist->items[i].text);
	}
	free(hist->items);

	hist->items = NULL;
	hist->size = 0;
	hist->capacity = 0;
}

int
hist_is_empty(const hist_t *hist)
{
	return (hist->size == 0);
}

void
hist_resize(hist_t *hist, int new_capacity)
{
	if(new_capacity <= 0)
	{
		hist_reset(hist);
		return;
	}

	/* Free truncated elements, if any. */
	int i;
	for(i = new_capacity; i < hist->size; ++i)
	{
		free(hist->items[i].text);
	}

	hist->items = reallocarray(hist->items, new_capacity, sizeof(*hist->items));
	hist->size = MIN(hist->size, new_capacity);
	hist->capacity = new_capacity;
}

int
hist_add(hist_t *hist, const char item[], time_t timestamp)
{
	if(hist->capacity > 0 && item[0] != '\0')
	{
		if(move_to_first_position(hist, item, timestamp) != 0)
		{
			return insert_at_first_position(hist, item, timestamp);
		}
	}
	return 0;
}

/* Moves item to the first position.  Returns zero on success or non-zero when
 * item wasn't found in the history. */
static int
move_to_first_position(hist_t *hist, const char item[], time_t timestamp)
{
	if(hist->size > 0 && strcmp(hist->items[0].text, item) == 0)
	{
		return 0;
	}

	int i;
	for(i = 1; i < hist->size; ++i)
	{
		if(strcmp(hist->items[i].text, item) == 0)
		{
			hist_item_t item = hist->items[i];
			item.timestamp = timestamp;
			memmove(hist->items + 1, hist->items, sizeof(*hist->items)*i);
			hist->items[0] = item;
			return 0;
		}
	}

	return 1;
}

/* Inserts item at the first position.  Returns zero on success or non-zero on
 * failure. */
static int
insert_at_first_position(hist_t *hist, const char item[], time_t timestamp)
{
	char *const item_copy = strdup(item);
	if(item_copy == NULL)
	{
		return 1;
	}

	if(hist->size == hist->capacity)
	{
		free(hist->items[hist->size - 1].text);
	}
	else
	{
		++hist->size;
	}
	memmove(hist->items + 1, hist->items,
			sizeof(*hist->items)*(hist->size - 1));

	hist->items[0].text = item_copy;
	hist->items[0].timestamp = timestamp;
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
