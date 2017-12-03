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

#include "macros.h"
#include "string_array.h"

#define NO_POS (-1)

static int move_to_first_position(hist_t *hist, const char item[]);
static int insert_at_first_position(hist_t *hist, size_t size, const char item[]);

int
hist_init(hist_t *hist, size_t size)
{
	hist->pos = NO_POS;
	hist->items = calloc(size, sizeof(char *));
	return hist->items == NULL;
}

void
hist_reset(hist_t *hist, size_t size)
{
	free_string_array(hist->items, size);
	hist->items = NULL;
	hist->pos = NO_POS;
}

int
hist_is_empty(const hist_t *hist)
{
	return hist->pos == NO_POS;
}

void
hist_trunc(hist_t *hist, size_t new_size, size_t removed_count)
{
	free_strings(hist->items + new_size, removed_count);
	hist->pos = MIN(hist->pos, (int)new_size - 1);
}

int
hist_contains(const hist_t *hist, const char item[])
{
	if(hist_is_empty(hist))
	{
		return 0;
	}
	return is_in_string_array(hist->items, hist->pos + 1, item);
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
	const int pos = string_array_pos(hist->items, hist->pos + 1, item);
	if(pos == 0)
	{
		return 0;
	}
	else if(pos > 0)
	{
		char *const item = hist->items[pos];
		memmove(hist->items + 1, hist->items, sizeof(char *)*pos);
		hist->items[0] = item;
		return 0;
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
		free(hist->items[hist->pos]);
		memmove(hist->items + 1, hist->items, sizeof(char *)*hist->pos);
		hist->items[0] = NULL;
	}

	hist->items[0] = item_copy;
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
