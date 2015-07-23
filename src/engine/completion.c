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
#include <stdlib.h> /* qsort() */
#include <string.h> /* strdup() */

#include "../compat/reallocarray.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/string_array.h"

static enum
{
	NOT_STARTED,
	FILLING_LIST,
	COMPLETING
}
state = NOT_STARTED;

static char **lines;
static int count;
static int curr = -1;
static int group_begin;
static int order;

/* Function called . */
static vle_compl_add_path_hook_f add_path_hook = &strdup;

static int add_match(char match[]);
static void group_unique_sort(size_t start_index, size_t len);
static int sorter(const void *first, const void *second);
static size_t remove_duplicates(char **arr, size_t count);

void
vle_compl_reset(void)
{
	free_string_array(lines, count);
	lines = NULL;

	count = 0;
	state = NOT_STARTED;
	curr = -1;
	group_begin = 0;
	order = 0;
}

int
vle_compl_add_match(const char match[])
{
	return vle_compl_put_match(strdup(match));
}

int
vle_compl_put_match(char match[])
{
	return add_match(match);
}

int
vle_compl_add_path_match(const char path[])
{
	char *const match = add_path_hook(path);
	return add_match(match);
}

int
vle_compl_put_path_match(char path[])
{
	if(add_path_hook == &strdup)
	{
		return add_match(path);
	}
	else
	{
		const int result = vle_compl_add_path_match(path);
		free(path);
		return result;
	}
}

/* Adds new match to the list of matches.  Becomes an owner of memory pointed to
 * by the match.  Errors if match is NULL.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
add_match(char match[])
{
	char **p;
	assert(state != COMPLETING);

	if(match == NULL)
	{
		return -1;
	}

	p = reallocarray(lines, count + 1, sizeof(*lines));
	if(p == NULL)
	{
		free(match);
		return -1;
	}
	lines = p;

	lines[count] = match;
	count++;

	state = FILLING_LIST;
	return 0;
}

int
vle_compl_add_last_match(const char origin[])
{
	return vle_compl_add_match(origin);
}

int
vle_compl_add_last_path_match(const char origin[])
{
	return vle_compl_add_path_match(origin);
}

void
vle_compl_finish_group(void)
{
	const size_t n_group_items = count - group_begin;
	group_unique_sort(group_begin, n_group_items);
}

void
vle_compl_unite_groups(void)
{
	group_unique_sort(0, count);
}

/* Sorts items of the list in range [start_index, start_index + len) with
 * de-duplication.  Updates number of list elements and next group beginning. */
static void
group_unique_sort(size_t start_index, size_t len)
{
	char **const group_start = lines + start_index;

	assert(state != COMPLETING);

	qsort(group_start, len, sizeof(*group_start), sorter);
	count = start_index + remove_duplicates(group_start, len);
	group_begin = count;
}

static int
sorter(const void *first, const void *second)
{
	const char *stra = *(const char **)first;
	const char *strb = *(const char **)second;
	size_t lena = strlen(stra), lenb = strlen(strb);
	if(stroscmp(stra, "./") == 0)
		return -1;
	if(stroscmp(strb, "./") == 0)
		return 1;
	if(stra[lena - 1] == '/' && strb[lenb - 1] == '/')
	{
		size_t len = MIN(lena - 1, lenb - 1);
		/* compare case sensitive strings even on Windows */
		int res = strncmp(stra, strb, len);
		if(res == 0)
			return lena - lenb;
	}
	/* compare case sensitive strings even on Windows */
	return strcmp(stra, strb);
}

char *
vle_compl_next(void)
{
	assert(state != NOT_STARTED);
	state = COMPLETING;

	if(curr == -1)
	{
		size_t n_group_items = count - group_begin;
		count = group_begin + remove_duplicates(lines + group_begin, n_group_items);
	}

	if(count == 2)
	{
		int t = (curr == -1) ? 0 : curr;
		curr = 0;
		return strdup(lines[t]);
	}

	if(!order) /* straight order */
	{
		curr = (curr + 1) % count;
	}
	else /* reverse order */
	{
		if(curr == -1)
			curr = count - 2;
		else
			curr--;
		if(curr < 0)
			curr = count - 1;
	}
	return strdup(lines[curr]);
}

/* Removes series consecutive duplicates.
 * Returns new count. */
static size_t
remove_duplicates(char **arr, size_t count)
{
	size_t i, j;
	j = 1;
	for(i = j; i < count; i++)
	{
		/* compare case sensitive strings even on Windows */
		if(strcmp(arr[i], arr[j - 1]) == 0)
		{
			free(arr[i]);
			continue;
		}
		arr[j++] = arr[i];
	}
	return (count == 0) ? 0 : j;
}

int
vle_compl_get_count(void)
{
	return count;
}

void
vle_compl_set_order(int reversed)
{
	order = reversed;
}

const char **
vle_compl_get_list(void)
{
	return (const char **)lines;
}

int
vle_compl_get_pos(void)
{
	return curr;
}

void
vle_compl_rewind(void)
{
	assert(state == COMPLETING);

	if(count == 2)
		curr = 1;
	else if(count > 2)
		curr = count - 2;
}

void
vle_compl_set_add_path_hook(vle_compl_add_path_hook_f hook)
{
	add_path_hook = (hook == NULL) ? &strdup : hook;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
