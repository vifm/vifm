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
#include <stdlib.h> /* qsort() */
#include <string.h>

#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/string_array.h"

static enum
{
	NOT_STARTED,
	FILLING_LIST,
	COMPLETING
}state = NOT_STARTED;

static char **lines;
static int count;
static int curr = -1;
static int group_begin;
static int order;

static void group_unique_sort(size_t start_index, size_t len);
static int sorter(const void *first, const void *second);
static size_t remove_duplicates(char **arr, size_t count);

void
reset_completion(void)
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
add_completion(const char *completion)
{
	char **p;
	assert(state != COMPLETING);

	/* add new line */
	if((p = realloc(lines, sizeof(*lines)*(count + 1))) == NULL)
		return -1;
	lines = p;

	if((lines[count] = strdup(completion)) == NULL)
		return -1;

	count++;
	state = FILLING_LIST;
	return 0;
}

void
completion_group_end(void)
{
	const size_t n_group_items = count - group_begin;
	group_unique_sort(group_begin, n_group_items);
}

void
completion_groups_unite(void)
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
next_completion(void)
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
get_completion_count(void)
{
	return count;
}

void
set_completion_order(int reversed)
{
	order = reversed;
}

const char **
get_completion_list(void)
{
	return (const char **)lines;
}

int
get_completion_pos(void)
{
	return curr;
}

void
rewind_completion(void)
{
	assert(state == COMPLETING);

	if(count == 2)
		curr = 1;
	else if(count > 2)
		curr = count - 2;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
