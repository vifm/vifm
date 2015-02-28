/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "abbrevs.h"

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* memmove() strcmp() strdup() */

typedef struct
{
	char *lhs;
	char *rhs;
	int no_remap;
}
abbrev_t;

static int add_abbrev(const char lhs[], const char rhs[], int no_remap);
static void remove_abbrev(size_t idx);
static size_t find_abbrev(const char lhs[]);
static void free_abbrev(size_t idx);

static size_t abbrev_count;
static abbrev_t *abbrevs;

int
vle_abbr_add(const char lhs[], const char rhs[])
{
	return add_abbrev(lhs, rhs, 0);
}

int
vle_abbr_add_no_remap(const char lhs[], const char rhs[])
{
	return add_abbrev(lhs, rhs, 1);
}

static int
add_abbrev(const char lhs[], const char rhs[], int no_remap)
{
	size_t i;
	abbrev_t *new_abbrev;
	abbrev_t *new_abbrevs;

	i = find_abbrev(lhs);
	if(i != (size_t)-1)
	{
		return -1;
	}

	new_abbrevs = realloc(abbrevs, sizeof(*abbrevs)*(abbrev_count + 1));
	if(new_abbrevs == NULL) {
		return 1;
	}
	abbrevs = new_abbrevs;

	new_abbrev = &new_abbrevs[abbrev_count];
	new_abbrev->lhs = strdup(lhs);
	new_abbrev->rhs = strdup(rhs);
	new_abbrev->no_remap = no_remap;

	++abbrev_count;
	return 0;
}

int
vle_abbr_remove(const char str[])
{
	size_t i;

	i = find_abbrev(str);
	if(i != (size_t)-1)
	{
		remove_abbrev(i);
		return 0;
	}

	for(i = 0UL; i < abbrev_count; ++i)
	{
		abbrev_t *abbrev = &abbrevs[i];
		if(strcmp(abbrev->rhs, str) == 0)
		{
			remove_abbrev(i);
			return 0;
		}
	}

	return -1;
}

static void
remove_abbrev(size_t idx)
{
	memmove(&abbrevs[idx], &abbrevs[idx + 1],
			(abbrev_count - 1 - idx)*sizeof(*abbrevs));
	free_abbrev(idx);
	--abbrev_count;
}

const char *
vle_abbr_expand(const char str[], int *no_remap)
{
	size_t i;

	i = find_abbrev(str);
	if(i == (size_t)-1)
	{
		return NULL;
	}

	*no_remap = abbrevs[i].no_remap;
	return abbrevs[i].rhs;
}

static size_t
find_abbrev(const char lhs[])
{
	size_t i;

	for(i = 0UL; i < abbrev_count; ++i)
	{
		abbrev_t *abbrev = &abbrevs[i];
		if(strcmp(abbrev->lhs, lhs) == 0)
		{
			return i;
		}
	}

	return (size_t)-1;
}

void
vle_abbr_reset(void)
{
	size_t i;
	for(i = 0UL; i < abbrev_count; ++i)
	{
		free_abbrev(i);
	}

	free(abbrevs);
	abbrevs = NULL;
	abbrev_count = 0UL;
}

static void
free_abbrev(size_t idx)
{
	free(abbrevs[idx].lhs);
	free(abbrevs[idx].rhs);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
