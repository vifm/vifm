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
#include <string.h> /* memmove() */
#include <wchar.h> /* wcscmp() */

#include "../utils/str.h"

typedef struct
{
	wchar_t *lhs;
	wchar_t *rhs;
	int no_remap;
}
abbrev_t;

static int add_abbrev(const wchar_t lhs[], const wchar_t rhs[], int no_remap);
static abbrev_t * extend_abbrevs(void);
static void remove_abbrev(abbrev_t *abbrev);
static abbrev_t * find_abbrev(const wchar_t lhs[]);
static void free_abbrev(abbrev_t *abbrev);

static size_t abbrev_count;
static abbrev_t *abbrevs;

int
vle_abbr_add(const wchar_t lhs[], const wchar_t rhs[])
{
	return add_abbrev(lhs, rhs, 0);
}

int
vle_abbr_add_no_remap(const wchar_t lhs[], const wchar_t rhs[])
{
	return add_abbrev(lhs, rhs, 1);
}

static int
add_abbrev(const wchar_t lhs[], const wchar_t rhs[], int no_remap)
{
	abbrev_t *abbrev;

	abbrev = find_abbrev(lhs);
	if(abbrev != NULL)
	{
		wchar_t *const rhs_copy = vifm_wcsdup(rhs);
		if(rhs_copy == NULL)
		{
			return -1;
		}

		free(abbrev->rhs);
		abbrev->rhs = rhs_copy;
		abbrev->no_remap = no_remap;
		return 0;
	}

	abbrev = extend_abbrevs();
	if(abbrev == NULL)
	{
		return 1;
	}

	abbrev->lhs = vifm_wcsdup(lhs);
	abbrev->rhs = vifm_wcsdup(rhs);
	abbrev->no_remap = no_remap;

	if(abbrev->lhs == NULL || abbrev->rhs == NULL)
	{
		free_abbrev(abbrev);
		return 1;
	}

	++abbrev_count;
	return 0;
}

static abbrev_t *
extend_abbrevs(void)
{
	abbrev_t *new_abbrevs = realloc(abbrevs, sizeof(*abbrevs)*(abbrev_count + 1));
	if(new_abbrevs == NULL)
	{
		return NULL;
	}

	abbrevs = new_abbrevs;
	return &new_abbrevs[abbrev_count];
}

int
vle_abbr_remove(const wchar_t str[])
{
	size_t i;
	abbrev_t *abbrev;

	abbrev = find_abbrev(str);
	if(abbrev != NULL)
	{
		remove_abbrev(abbrev);
		return 0;
	}

	for(i = 0UL; i < abbrev_count; ++i)
	{
		abbrev_t *abbrev = &abbrevs[i];
		if(wcscmp(abbrev->rhs, str) == 0)
		{
			remove_abbrev(abbrev);
			return 0;
		}
	}

	return -1;
}

static void
remove_abbrev(abbrev_t *abbrev)
{
	free_abbrev(abbrev);
	memmove(abbrev, abbrev + 1,
			(&abbrevs[abbrev_count - 1] - abbrev)*sizeof(*abbrevs));
	--abbrev_count;
}

const wchar_t *
vle_abbr_expand(const wchar_t str[], int *no_remap)
{
	abbrev_t *abbrev = find_abbrev(str);
	if(abbrev == NULL)
	{
		return NULL;
	}

	*no_remap = abbrev->no_remap;
	return abbrev->rhs;
}

static abbrev_t *
find_abbrev(const wchar_t lhs[])
{
	size_t i;

	for(i = 0UL; i < abbrev_count; ++i)
	{
		abbrev_t *abbrev = &abbrevs[i];
		if(wcscmp(abbrev->lhs, lhs) == 0)
		{
			return abbrev;
		}
	}

	return NULL;
}

void
vle_abbr_reset(void)
{
	size_t i;
	for(i = 0UL; i < abbrev_count; ++i)
	{
		free_abbrev(&abbrevs[i]);
	}

	free(abbrevs);
	abbrevs = NULL;
	abbrev_count = 0UL;
}

static void
free_abbrev(abbrev_t *abbrev)
{
	free(abbrev->lhs);
	free(abbrev->rhs);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
