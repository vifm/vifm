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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "string_array.h"
#include "utils.h"

#include "completion.h"

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

static int sorter(const void *first, const void *second);

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
	assert(state != COMPLETING);

	qsort(lines + group_begin, count - group_begin, sizeof(*lines), sorter);

	group_begin = count;
}

static int
sorter(const void *first, const void *second)
{
	const char *stra = *(const char **)first;
	const char *strb = *(const char **)second;
	size_t lena = strlen(stra), lenb = strlen(strb);
	if(pathcmp(stra, "./") == 0)
		return -1;
	if(pathcmp(strb, "./") == 0)
		return 1;
	if(stra[lena - 1] == '/' && strb[lenb - 1] == '/')
	{
		size_t len = MIN(lena - 1, lenb - 1);
		// compare case sensitive strings even on Windows
		int res = strncmp(stra, strb, len);
		if(res == 0)
			return lena - lenb;
	}
	// compare case sensitive strings even on Windows
	return strcmp(stra, strb);
}

char *
next_completion(void)
{
	assert(state != NOT_STARTED);
	state = COMPLETING;

	if(curr == -1)
	{
		int i, j;
		/* remove consecutive duplicates */
		j = 1;
		for(i = 1; i < count; i++)
		{
			// compare case sensitive strings even on Windows
			if(strcmp(lines[i], lines[j - 1]) == 0)
			{
				free(lines[i]);
				continue;
			}
			lines[j++] = lines[i];
		}
		count = j;
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
