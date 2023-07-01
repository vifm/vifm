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

#include "../compat/reallocarray.h"
#include "../utils/macros.h"
#include "../utils/utils.h"
#include "colors.h"

/* Number of color pairs preallocated by curses library. */
#define PREALLOCATED_COUNT 1

/* Maximum allowed number of unsorted color pairs. */
#define MAX_UNSORTED_PAIRS  512
/* Initial size of the pair cache. */
#define START_CACHE_SIZE    128
/* Size factor for growing pair cache. */
#define CACHE_GROWTH_FACTOR 2

/* Pair cache entry that maps foreground-background pair to its index. */
typedef struct
{
	int fg;   /* Foreground color. */
	int bg;   /* Background color. */
	int pair; /* Index. */
}
pair_cache_t;

static int find_pair(int fg, int bg);
static int allocate_pair(int fg, int bg);
static int compress_pair_space(void);
static int pair_cache_cmp(const void *a, const void *b);

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

/* Pair cache to avoid expensive linear lookup of thousands of elements. */
static pair_cache_t *pair_cache;
/* Current size of the cache. */
static int pair_cache_size;
/* Current maximum size of the cache. */
static int pair_cache_capacity;
/* Size of the sorted prefix of the cache.  The unsorted tail needs to be looked
 * up linearly. */
static int pair_cache_sorted;

static pair_cache_t *
add_to_cache(void)
{
	if(pair_cache_size == pair_cache_capacity)
	{
		/* Start small, then grow cache size fast. */
		int new_capacity = (pair_cache_capacity == 0)
		                 ? START_CACHE_SIZE
		                 : pair_cache_capacity*CACHE_GROWTH_FACTOR;

		void *ptr = reallocarray(pair_cache, new_capacity, sizeof(*pair_cache));
		if(ptr == NULL)
		{
			return NULL;
		}

		pair_cache_capacity = new_capacity;
		pair_cache = ptr;
	}

	return &pair_cache[pair_cache_size++];
}

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

	pair_cache_size = 0;
	pair_cache_sorted = 0;
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

	/* The check is to not call bsearch() on NULL. */
	if(pair_cache_sorted > 0)
	{
		pair_cache_t key = { .fg = fg, .bg = bg };
		pair_cache_t *needle = bsearch(&key, pair_cache, pair_cache_sorted,
				sizeof(*pair_cache), &pair_cache_cmp);
		if(needle != NULL)
		{
			return needle->pair;
		}
	}

	int i;
	for(i = pair_cache_sorted; i < pair_cache_size; ++i)
	{
		pair_cache_t *entry = &pair_cache[i];
		if(entry->fg == fg && entry->bg == bg)
		{
			return entry->pair;
		}
	}

	return -1;
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

	pair_cache_t *cache_entry = add_to_cache();
	if(cache_entry == NULL)
	{
		return -1;
	}

	cache_entry->fg = fg;
	cache_entry->bg = bg;
	cache_entry->pair = used_pairs;

	/* Color pairs are not resorted on every allocation to avoid performance
	 * hit. */
	if(pair_cache_size - pair_cache_sorted > MAX_UNSORTED_PAIRS)
	{
		safe_qsort(pair_cache, pair_cache_size, sizeof(*pair_cache),
				&pair_cache_cmp);
		pair_cache_sorted = pair_cache_size;
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

	pair_cache_size = 0;
	for(i = PREALLOCATED_COUNT; i < used_pairs; ++i)
	{
		int fg, bg;
		if(conf.pair_content(i, &fg, &bg) == 0)
		{
			pair_cache_t *cache_entry = add_to_cache();
			if(cache_entry != NULL)
			{
				cache_entry->fg = fg;
				cache_entry->bg = bg;
				cache_entry->pair = i;
			}
		}
	}

	safe_qsort(pair_cache, pair_cache_size, sizeof(*pair_cache), &pair_cache_cmp);
	pair_cache_sorted = pair_cache_size;

	return 0;
}

/* Comparator for pair_cache_t.  Returns values <0, =0 and >0 to signify that
 * first argument is less than, equal or greater than the second one. */
static int
pair_cache_cmp(const void *a, const void *b)
{
	const pair_cache_t *s = a;
	const pair_cache_t *t = b;
	return (s->fg != t->fg ? s->fg - t->fg : s->bg - t->bg);
}

int
colmgr_used_pairs(void)
{
	assert(initialized && "colmgr_init() must be called before this function!");

	return used_pairs;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
