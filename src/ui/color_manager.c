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

#include "../utils/macros.h"
#include "colors.h"

/* Number of color pairs preallocated by curses library. */
#define PREALLOCATED_COUNT 1

static int find_pair(int fg, int bg);
static int color_pair_matches(int pair, int fg, int bg);
static int allocate_pair(int fg, int bg);
static int compress_pair_space(void);

/* Number of color pairs available. */
static int avail_pairs;

/* Number of used color pairs. */
static int used_pairs;

/* Configuration data passed in during initialization. */
static colmgr_conf_t conf;

/* Flag, which is set after the unit is initialized. */
static int initialized;

/* Default foreground and background colors. */
static int def_fg, def_bg;

void
colmgr_init(const colmgr_conf_t *conf_init)
{
	assert(conf_init != NULL && "conf_init structure is required.");
	assert(conf_init->init_pair != NULL && "init_pair must be set.");
	assert(conf_init->pair_content != NULL && "pair_content must be set.");
	assert(conf_init->pair_in_use != NULL && "pair_in_use must be set.");
	assert(conf_init->move_pair != NULL && "move_pair must be set.");

	conf = *conf_init;
	initialized = 1;

	colmgr_reset();

	/* Query default colors as implementation might not return -1 via
	 * pair_content(), which will confuse this unit. */
	if(conf.pair_content(0, &def_fg, &def_bg) != 0)
	{
		def_fg = -1;
		def_bg = -1;
	}
}

void
colmgr_reset(void)
{
	assert(initialized && "colmgr_init() must be called before this function!");

	used_pairs = PREALLOCATED_COUNT;
	avail_pairs = conf.max_color_pairs - used_pairs;
}

int
colmgr_get_pair(int fg, int bg)
{
	assert(initialized && "colmgr_init() must be called before this function!");

	if(fg < 0)
	{
		fg = -1;
	}

	if(bg < 0)
	{
		bg = -1;
	}

	int p = find_pair(fg, bg);
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
	if(fg < 0)
	{
		fg = def_fg;
	}
	if(bg < 0)
	{
		bg = def_bg;
	}

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
	int pair_fg, pair_bg;
	conf.pair_content(pair, &pair_fg, &pair_bg);
	return (pair_fg == fg && pair_bg == bg);
}

/* Allocates new color pair.  Returns new pair index, or -1 on failure. */
static int
allocate_pair(int fg, int bg)
{
	if(avail_pairs == 0)
	{
		/* Out of pairs, free unused ones. */
		if(compress_pair_space() != 0)
		{
			return -1;
		}
	}

	if(conf.init_pair(used_pairs, fg, bg) != 0)
	{
		return -1;
	}

	--avail_pairs;
	return used_pairs++;
}

/* Returns zero if at least one pair is now available, otherwise non-zero is
 * returned. */
static int
compress_pair_space(void)
{
	/* TODO: in case of performance issues cache pair_in_use() in the first loop
	 * or change the function to fill in bit field of pairs. */

	int i;
	int j;
	int in_use;
	int first_unused;

	in_use = 0;
	first_unused = -1;
	for(i = PREALLOCATED_COUNT; i < used_pairs; ++i)
	{
		if(conf.pair_in_use(i))
		{
			++in_use;
		}
		else if(first_unused == -1)
		{
			first_unused = i;
		}
	}

	if(first_unused == -1)
	{
		/* No unused pairs. */
		return -1;
	}

	j = first_unused;
	for(i = PREALLOCATED_COUNT + in_use; i < used_pairs; ++i)
	{
		if(conf.pair_in_use(i))
		{
			conf.move_pair(i, j);

			/* Advance to next unused pair. */
			do
			{
				++j;
			}
			while(conf.pair_in_use(j));
		}
	}

	used_pairs = j;
	avail_pairs = conf.max_color_pairs - used_pairs;

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
