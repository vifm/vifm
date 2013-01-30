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

#include <curses.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* calloc() */
#include <string.h> /* memset() */

#include "colors.h"

#include "color_manager.h"

static int find_pair(int fg, int bg);
static int allocate_pair(int fg, int bg);

/* Number of color pairs available. */
static int avail_pairs;
/* Map of allocated color pairs.  If element has non-zero value, it means it's
 * allocated. */
static char *color_pair_map;

void
colmgr_init(void)
{
	avail_pairs = COLOR_PAIRS - FCOLOR_BASE;
	assert(avail_pairs >= 0 && "Too few color pairs available.");

	color_pair_map = calloc(avail_pairs, avail_pairs);
	assert((color_pair_map != NULL || avail_pairs == 0) && "Not enough memory.");
}

void
colmgr_reset(void)
{
	memset(color_pair_map, '\0', avail_pairs);
}

int
colmgr_alloc_pair(int fg, int bg)
{
	const int pair_index = find_pair(fg, bg);
	return (pair_index != -1) ? pair_index : allocate_pair(fg, bg);
}

/* Tries to find pair with specified colors among already allocated pairs.
 * Returns -1 when search fails. */
static int
find_pair(int fg, int bg)
{
	int i;
	for(i = 0; i < avail_pairs; i++)
	{
		if(color_pair_map[i])
		{
			short pair_fg, pair_bg;
			pair_content(FCOLOR_BASE + i, &pair_fg, &pair_bg);
			if(pair_fg == fg && pair_bg == bg)
			{
				break;
			}
		}
	}
	return (i < avail_pairs) ? (FCOLOR_BASE + i) : -1;
}

/* Allocates new color pair.  Returns new pair index, or -1 on failure. */
static int
allocate_pair(int fg, int bg)
{
	int i;
	for(i = 0; i < avail_pairs; i++)
	{
		if(!color_pair_map[i])
		{
			init_pair(FCOLOR_BASE + i, fg, bg);
			color_pair_map[i] = 1;
			break;
		}
	}
	return (i < avail_pairs) ? (FCOLOR_BASE + i) : -1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
