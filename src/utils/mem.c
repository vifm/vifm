/* vifm
 * Copyright (C) 2023 xaizek.
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

#include "mem.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <stdint.h> /* uint8_t */
#include <string.h> /* memcpy() memmove() */

void
mem_ror(void *ptr, size_t count, size_t item_len)
{
	assert(count > 0 && "Can't rotate if item count is 0!");
	assert(item_len > 0 && "Can't rotate if item size is 0!");

	size_t slice_size = item_len*(count - 1);
	char *p = ptr;

	/* Stash last item. */
	uint8_t buf[item_len];
	memcpy(buf, p + slice_size, item_len);

	/* Move all but the last item. */
	memmove(p + item_len, p, slice_size);

	/* Put last item in front. */
	memcpy(p, buf, item_len);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
