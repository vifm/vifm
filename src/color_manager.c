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

#include "color_manager.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL */

#include "utils/macros.h"
#include "colors.h"

/* Number of preallocated color pairs. */
#define PREALLOCATED_COUNT 64

static int find_pair(int fg, int bg);
static int color_pair_matches(int pair, int fg, int bg);
static int allocate_pair(int fg, int bg);

/* Number of color pairs available. */
static int avail_pairs;

/* Number of used color pairs. */
static int used_pairs;

/* Configuration data passed in during initialization. */
static colmgr_conf_t conf;

void
colmgr_init(const colmgr_conf_t *conf_init)
{
	int fg, bg;

	assert(conf_init != NULL && "conf_init structure is required.");
	assert(conf_init->init_pair != NULL && "init_pair must be set.");
	assert(conf_init->pair_content != NULL && "pair_content must be set.");

	conf = *conf_init;

	/* Pre-allocate 64 lower pairs for 8-color values. */
	for(fg = 0; fg < 8; ++fg)
	{
		for(bg = 0; bg < 8; ++bg)
		{
			conf.init_pair(colmgr_get_pair(fg, bg), fg, bg);
		}
	}

	colmgr_reset();
}

void
colmgr_reset(void)
{
	used_pairs = PREALLOCATED_COUNT;
	avail_pairs = conf.max_color_pairs - used_pairs;
}

int
colmgr_get_pair(int fg, int bg)
{
	int p;

	if(fg < 8 && bg < 8)
	{
		return fg*8 + bg;
	}

	p = find_pair(fg, bg);
	if(p != -1)
	{
		return p;
	}

	p = allocate_pair(fg, bg);
	return (p == -1) ? 0 : p;
}

/* Tries to find pair with specified colors among already allocated pairs.
 * Returns -1 when search fails. */
static int
find_pair(int fg, int bg)
{
	int i;

	for(i = PREALLOCATED_COUNT; i < used_pairs; ++i)
	{
		if(color_pair_matches(i, fg, bg))
		{
			return i;
		}
	}

	return -1;
}

/* Checks whether color pair number pair has specified foreground (fg) and
 * background (bg) colors. */
static int
color_pair_matches(int pair, int fg, int bg)
{
	short pair_fg, pair_bg;
	conf.pair_content(pair, &pair_fg, &pair_bg);
	return (pair_fg == fg && pair_bg == bg);
}

/* Allocates new color pair.  Returns new pair index, or -1 on failure. */
static int
allocate_pair(int fg, int bg)
{
	if(avail_pairs == 0)
	{
		/* Out of pairs. TODO: free unused (LRU?) pairs. */
		return -1;
	}

	conf.init_pair(used_pairs, fg, bg);

	--avail_pairs;
	return used_pairs++;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
