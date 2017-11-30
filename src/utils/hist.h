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

#ifndef VIFM__UTILS__HIST_H__
#define VIFM__UTILS__HIST_H__

/* Generic implementation of history represented as list of strings. */

#include <stddef.h> /* size_t */

/* History object structure.  Doesn't store its length. */
typedef struct
{
	/* List of history items.  Can be NULL for empty list. */
	char **items;
	/* Position of the last item in the items list.  Undefined (likely to be
	 * negative) for empty lists. */
	int pos;
}
hist_t;

/* Initializes empty history structure of given size.  Return zero on success,
 * otherwise non-zero is returned. */
int hist_init(hist_t *hist, size_t size);

/* Resets content of the history of given size and empties it.  All associated
 * resources are freed. */
void hist_reset(hist_t *hist, size_t size);

/* Checks whether history is empty.  Returns non-zero for empty history,
 * otherwise non-zero is returned. */
int hist_is_empty(const hist_t *hist);

/* Reduces amount of memory taken by the history.  The new_size specifies new
 * size of the history, while removed_count parameter designates number of
 * removed elements. */
void hist_trunc(hist_t *hist, size_t new_size, size_t removed_count);

/* Checks whether given item present in the history.  Returns non-zero if
 * present, otherwise non-zero is returned. */
int hist_contains(const hist_t *hist, const char item[]);

/* Adds new item to the front of the history, thus it becomes its first
 * element.  If item already present in history list, it's moved.  Returns zero
 * when item is added/moved or rejected, on failure non-zero is returned. */
int hist_add(hist_t *hist, const char item[], size_t size);

#endif /* VIFM__UTILS__HIST_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
