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

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* calloc() free() */
#include <string.h> /* memmove() */

#include "../compat/reallocarray.h"
#include "macros.h"

#define NO_POS (-1)

static int move_to_first_position(hist_t *hist, const char item[]);
static int insert_at_first_position(hist_t *hist, size_t size,
		const char item[]);

int
hist_init(hist_t *hist, size_t size)
{
	hist->pos = NO_POS;
	hist->items = calloc(size, sizeof(*hist->items));
	return hist->items == NULL;
}

void
hist_reset(hist_t *hist, size_t size)
{
	size_t i;
	for(i = 0; i < size; ++i)
	{
		free(hist->items[i].text);
	}

	free(hist->items);
	hist->items = NULL;
	hist->pos = NO_POS;
}

int
hist_is_empty(const hist_t *hist)
{
	return hist->pos == NO_POS;
}

void
hist_resize(hist_t *hist, size_t old_size, size_t new_size)
{
	if(new_size == 0)
	{
		hist_reset(hist, old_size);
		return;
	}

	const int delta = (int)new_size - (int)old_size;

	if(delta < 0)
	{
		size_t i;
		for(i = new_size; i < new_size - delta; ++i)
		{
			free(hist->items[i].text);
		}
		hist->pos = MIN(hist->pos, (int)new_size - 1);
	}

	hist->items = reallocarray(hist->items, new_size, sizeof(*hist->items));

	if(delta > 0)
	{
		memset(hist->items + old_size, 0, sizeof(*hist->items)*delta);
	}
}

int
hist_contains(const hist_t *hist, const char item[])
{
	int i;
	for(i = 0; i <= hist->pos; ++i)
	{
		if(strcmp(hist->items[i].text, item) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int
hist_add(hist_t *hist, const char item[], size_t size)
{
	if(size > 0 && item[0] != '\0')
	{
		if(move_to_first_position(hist, item) != 0)
		{
			return insert_at_first_position(hist, size, item);
		}
	}
	return 0;
}

/* Moves item to the first position.  Returns zero on success or non-zero when
 * item wasn't found in the history. */
static int
move_to_first_position(hist_t *hist, const char item[])
{
	if(hist->pos >= 0 && strcmp(hist->items[0].text, item) == 0)
	{
		return 0;
	}

	int i;
	for(i = 1; i <= hist->pos; ++i)
	{
		if(strcmp(hist->items[i].text, item) == 0)
		{
			hist_item_t item = hist->items[i];
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
insert_at_first_position(hist_t *hist, size_t size, const char item[])
{
	char *const item_copy = strdup(item);
	if(item_copy == NULL)
	{
		return 1;
	}

	hist->pos = MIN(hist->pos + 1, (int)size - 1);
	if(hist->pos > 0)
	{
		free(hist->items[hist->pos].text);
		memmove(hist->items + 1, hist->items, sizeof(*hist->items)*hist->pos);
	}

	hist->items[0].text = item_copy;
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
