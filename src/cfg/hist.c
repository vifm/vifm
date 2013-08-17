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
#include <stdlib.h> /* calloc() */

#include "../utils/macros.h"
#include "../utils/string_array.h"

#define NO_POS (-1)

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

void
hist_clear(hist_t *hist)
{
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
	hist->pos = MIN(hist->pos, new_size - 1);
}

int
hist_contains(const hist_t *hist, const char item[])
{
	if(hist->pos == NO_POS)
	{
		return 0;
	}
	return is_in_string_array(hist->items, hist->pos + 1, item);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
