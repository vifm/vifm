/* vifm
 * Copyright (C) 2011 xaizek.
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

#include "completion.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <string.h> /* strdup() */

#include "../compat/reallocarray.h"
#include "../utils/darray.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/utils.h"

static enum
{
	NOT_STARTED,
	FILLING_LIST,
	COMPLETING
}
state = NOT_STARTED;

/* List of completion items. */
static vle_compl_t *items;
/* Declarations to enable use of DA_* on items. */
static DA_INSTANCE(items);
static int curr = -1;
static int group_begin;
static int order;

/* Function called to pre-process path for completion list. */
static vle_compl_add_path_hook_f add_path_hook = &strdup;

static int add_match(char match[], const char descr[]);
static void group_unique_sort(size_t start_index, size_t len);
static int sorter(const void *first, const void *second);
static void remove_duplicates(vle_compl_t arr[], size_t count);

void
vle_compl_reset(void)
{
	size_t i;

	for(i = 0U; i < DA_SIZE(items); ++i)
	{
		free(items[i].text);
		free(items[i].descr);
	}
	DA_REMOVE_ALL(items);

	state = NOT_STARTED;
	curr = -1;
	group_begin = 0;
	order = 0;
}

int
vle_compl_add_match(const char match[], const char descr[])
{
	return add_match(strdup(match), descr);
}

int
vle_compl_put_match(char match[], const char descr[])
{
	return add_match(match, descr);
}

int
vle_compl_add_path_match(const char path[])
{
	return vle_compl_put_match(add_path_hook(path), "");
}

int
vle_compl_put_path_match(char path[])
{
	int ret = vle_compl_add_path_match(path);
	free(path);
	return ret;
}

/* Adds new match to the list of matches.  Becomes an owner of memory pointed to
 * by the match.  Errors if match is NULL.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
add_match(char match[], const char descr[])
{
	vle_compl_t *item;

	assert(state != COMPLETING);

	if(match == NULL)
	{
		return -1;
	}

	item = DA_EXTEND(items);
	if(item == NULL)
	{
		free(match);
		return 1;
	}

	item->text = match;
	item->descr = strdup(descr);
	if(item->descr == NULL)
	{
		free(match);
		return 1;
	}

	DA_COMMIT(items);

	state = FILLING_LIST;
	return 0;
}

int
vle_compl_add_last_match(const char origin[])
{
	return vle_compl_add_match(origin, "");
}

int
vle_compl_add_last_path_match(const char origin[])
{
	return vle_compl_add_path_match(origin);
}

void
vle_compl_finish_group(void)
{
	const size_t n_group_items = DA_SIZE(items) - group_begin;
	group_unique_sort(group_begin, n_group_items);
}

void
vle_compl_unite_groups(void)
{
	group_unique_sort(0, DA_SIZE(items));
}

/* Sorts items of the list in range [start_index, start_index + len) with
 * deduplication.  Updates number of list elements and next group beginning. */
static void
group_unique_sort(size_t start_index, size_t len)
{
	vle_compl_t *const group_start = items + start_index;

	assert(state != COMPLETING);

	safe_qsort(group_start, len, sizeof(*group_start), &sorter);
	remove_duplicates(group_start, len);
	group_begin = DA_SIZE(items);
}

/* qsort() comparison criterion implementation.  Returns standard < 0, = 0,
 * > 0.*/
static int
sorter(const void *first, const void *second)
{
	const char *const stra = ((const vle_compl_t *)first)->text;
	const char *const strb = ((const vle_compl_t *)second)->text;
	const size_t lena = strlen(stra);
	const size_t lenb = strlen(strb);

	if(strcmp(stra, "./") == 0 || strcmp(strb, "./") == 0)
	{
		return 1;
	}

	if(stra[lena - 1] == '/' && strb[lenb - 1] == '/')
	{
		size_t len = MIN(lena - 1, lenb - 1);
		/* Compare case sensitive strings even on Windows. */
		if(strncmp(stra, strb, len) == 0)
		{
			return lena - lenb;
		}
	}

	/* Compare case sensitive strings even on Windows. */
	return strcmp(stra, strb);
}

char *
vle_compl_next(void)
{
	assert(state != NOT_STARTED && "Invalid unit state.");
	state = COMPLETING;

	if(curr == -1)
	{
		const size_t n_group_items = DA_SIZE(items) - group_begin;
		remove_duplicates(items + group_begin, n_group_items);
	}

	if(DA_SIZE(items) == 2)
	{
		const int idx = (curr == -1) ? 0 : curr;
		curr = 0;
		return strdup(items[idx].text);
	}

	if(!order)
	{
		/* Straight order. */
		curr = (curr + 1) % DA_SIZE(items);
	}
	else
	{
		/* Reverse order. */
		if(curr == -1)
		{
			curr = DA_SIZE(items) - 2U;
		}
		else
		{
			--curr;
		}
		if(curr < 0)
		{
			curr = DA_SIZE(items) - 1U;
		}
	}
	return strdup(items[curr].text);
}

/* Removes series of consecutive duplicates. */
static void
remove_duplicates(vle_compl_t arr[], size_t count)
{
	size_t i, j;
	j = 1U;
	for(i = j; i < count; ++i)
	{
		/* Compare case sensitive strings even on Windows. */
		if(strcmp(arr[i].text, arr[j - 1].text) == 0)
		{
			free(arr[i].text);
			free(arr[i].descr);
			continue;
		}
		arr[j++] = arr[i];
	}
	if(count != 0U)
	{
		DA_REMOVE_AFTER(items, &arr[j]);
	}
}

int
vle_compl_get_count(void)
{
	return DA_SIZE(items);
}

void
vle_compl_set_order(int reversed)
{
	order = reversed;
}

const vle_compl_t *
vle_compl_get_items(void)
{
	return items;
}

int
vle_compl_get_pos(void)
{
	return curr;
}

void
vle_compl_rewind(void)
{
	assert(state == COMPLETING && "Invalid unit state.");

	if(DA_SIZE(items) == 2)
	{
		curr = 1;
	}
	else if(DA_SIZE(items) > 2)
	{
		curr = DA_SIZE(items) - 2;
	}
}

void
vle_compl_set_add_path_hook(vle_compl_add_path_hook_f hook)
{
	add_path_hook = (hook == NULL) ? &strdup : hook;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
