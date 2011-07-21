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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#include "completion.h"

enum state {
	NOT_STARTED,
	FILLING_LIST,
	COMPLETING
};

static enum state state = NOT_STARTED;

static char **lines;
static int count;
static int curr = -1;
static int group_begin;

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
	return strcmp(stra, strb);
}

char *
next_completion(void)
{
	assert(state != NOT_STARTED);
	state = COMPLETING;

	if(count == 2)
		return strdup(lines[0]);

	curr = (curr + 1) % count;
	return strdup(lines[curr]);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
