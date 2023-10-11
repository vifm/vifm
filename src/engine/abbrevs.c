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

#include <stddef.h> /* NULL offsetof() size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* memmove() strlen() strncmp() */
#include <wchar.h> /* wcscmp() */

#include "../compat/reallocarray.h"
#include "../utils/str.h"
#include "../bracket_notation.h"
#include "completion.h"

/* Information about single abbreviation. */
typedef struct
{
	wchar_t *lhs; /* What is expanded. */
	wchar_t *rhs; /* To what it's expanded.  This field is updated at runtime if
	                 handler is not NULL. */
	int no_remap; /* Whether user mappings should be processed on expansion. */
	char *descr;  /* Brief description of the abbreviation (can be NULL). */

	/* Function invoked to retrieve `rhs` (can be NULL). */
	abbrev_handler handler;
	/* User data for the handler (can be NULL). */
	void *user_data;
}
abbrev_t;

static int add_abbrev(const wchar_t lhs[], const wchar_t rhs[], int no_remap);
static int replace_abbrev(abbrev_t *abbrev, const wchar_t rhs[], int no_remap);
static abbrev_t * extend_abbrevs(void);
static int setup_abbrev(abbrev_t *abbrev, const wchar_t lhs[],
		const wchar_t rhs[], int no_remap);
static int setup_foreign_abbrev(abbrev_t *abbrev, const wchar_t lhs[],
		const char descr[], int no_remap, abbrev_handler handler, void *user_data);
static void remove_abbrev(abbrev_t *abbrev);
static abbrev_t * find_abbrev(const wchar_t lhs[], size_t offset);
static void free_abbrev(abbrev_t *abbrev);

/* Number of registered abbreviations. */
static size_t abbrev_count;
/* Array of registered abbreviations. */
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

/* Adds new/updates existing abbreviation.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
add_abbrev(const wchar_t lhs[], const wchar_t rhs[], int no_remap)
{
	abbrev_t *abbrev;

	abbrev = find_abbrev(lhs, offsetof(abbrev_t, lhs));
	if(abbrev != NULL)
	{
		return replace_abbrev(abbrev, rhs, no_remap);
	}

	abbrev = extend_abbrevs();
	if(abbrev == NULL)
	{
		return 1;
	}

	return setup_abbrev(abbrev, lhs, rhs, no_remap);
}

int
vle_abbr_add_foreign(const wchar_t lhs[], const char descr[], int no_remap,
		abbrev_handler handler, void *user_data)
{
	abbrev_t *abbrev;

	abbrev = find_abbrev(lhs, offsetof(abbrev_t, lhs));
	if(abbrev != NULL)
	{
		return 1;
	}

	abbrev = extend_abbrevs();
	if(abbrev == NULL)
	{
		return 1;
	}

	return setup_foreign_abbrev(abbrev, lhs, descr, no_remap, handler, user_data);
}

/* Overwrites properties of existing abbreviation.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
replace_abbrev(abbrev_t *abbrev, const wchar_t rhs[], int no_remap)
{
	wchar_t *const rhs_copy = vifm_wcsdup(rhs);
	if(rhs_copy == NULL)
	{
		return 1;
	}

	free(abbrev->rhs);
	abbrev->rhs = rhs_copy;
	abbrev->no_remap = no_remap;

	free(abbrev->descr);
	abbrev->descr = NULL;
	abbrev->handler = NULL;
	abbrev->user_data = NULL;

	return 0;
}

/* Reallocates array of abbreviations to get at least one new element.  Returns
 * pointer to new element or NULL. */
static abbrev_t *
extend_abbrevs(void)
{
	abbrev_t *new_abbrevs;
	new_abbrevs = reallocarray(abbrevs, abbrev_count + 1, sizeof(*abbrevs));
	if(new_abbrevs == NULL)
	{
		return NULL;
	}

	abbrevs = new_abbrevs;
	return &new_abbrevs[abbrev_count];
}

/* Initializes properties of previously inexistent abbreviation.  Returns zero
 * on success, otherwise non-zero is returned. */
static int
setup_abbrev(abbrev_t *abbrev, const wchar_t lhs[], const wchar_t rhs[],
		int no_remap)
{
	abbrev->lhs = vifm_wcsdup(lhs);
	abbrev->rhs = vifm_wcsdup(rhs);
	abbrev->no_remap = no_remap;

	abbrev->descr = NULL;
	abbrev->handler = NULL;
	abbrev->user_data = NULL;

	if(abbrev->lhs == NULL || abbrev->rhs == NULL)
	{
		free_abbrev(abbrev);
		return 1;
	}

	++abbrev_count;
	return 0;
}

/* Initializes properties of previously inexistent foreign abbreviation.
 * Returns zero on success, otherwise non-zero is returned. */
static int
setup_foreign_abbrev(abbrev_t *abbrev, const wchar_t lhs[], const char descr[],
		int no_remap, abbrev_handler handler, void *user_data)
{
	abbrev->lhs = vifm_wcsdup(lhs);
	abbrev->rhs = NULL;
	abbrev->no_remap = no_remap;
	abbrev->descr = strdup(descr);
	abbrev->handler = handler;
	abbrev->user_data = user_data;

	if(abbrev->lhs == NULL || abbrev->descr == NULL)
	{
		free_abbrev(abbrev);
		return 1;
	}

	++abbrev_count;
	return 0;
}

int
vle_abbr_remove(const wchar_t str[])
{
	abbrev_t *abbrev;

	abbrev = find_abbrev(str, offsetof(abbrev_t, lhs));
	if(abbrev == NULL)
	{
		abbrev = find_abbrev(str, offsetof(abbrev_t, rhs));
	}

	if(abbrev == NULL)
	{
		return -1;
	}

	remove_abbrev(abbrev);
	return 0;
}

/* Removes abbreviation from the array freeing all related data. */
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
	abbrev_t *abbrev = find_abbrev(str, offsetof(abbrev_t, lhs));
	if(abbrev == NULL)
	{
		return NULL;
	}

	if(abbrev->handler != NULL)
	{
		free(abbrev->rhs);
		abbrev->rhs = abbrev->handler(abbrev->user_data);
	}

	*no_remap = abbrev->no_remap;
	return abbrev->rhs;
}

/* Looks for an abbreviation by one of its wide string fields specified via
 * offset in the structure.  Returns pointer to the abbreviation if found,
 * otherwise NULL is returned. */
static abbrev_t *
find_abbrev(const wchar_t str[], size_t offset)
{
	size_t i;

	for(i = 0UL; i < abbrev_count; ++i)
	{
		abbrev_t *abbrev = &abbrevs[i];
		const wchar_t *field = *(wchar_t **)((char *)abbrev + offset);
		if(field != NULL && wcscmp(field, str) == 0)
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

/* Frees resources allocated for the abbreviation. */
static void
free_abbrev(abbrev_t *abbrev)
{
	free(abbrev->lhs);
	free(abbrev->rhs);
	free(abbrev->descr);
}

void
vle_abbr_complete(const char prefix[])
{
	const size_t prefix_len = strlen(prefix);
	size_t i;
	for(i = 0UL; i < abbrev_count; ++i)
	{
		char *const mb_lhs = to_multibyte(abbrevs[i].lhs);
		if(strncmp(mb_lhs, prefix, prefix_len) == 0)
		{
			char *rhs_descr = vle_abbr_describe(abbrevs[i].rhs, abbrevs[i].descr);
			(void)vle_compl_add_match(mb_lhs, rhs_descr);
			free(rhs_descr);
		}
		free(mb_lhs);
	}

	vle_compl_finish_group();
	vle_compl_add_last_match(prefix);
}

char *
vle_abbr_describe(const wchar_t rhs[], const char descr[])
{
	if(descr != NULL)
	{
		return strdup(descr);
	}
	if(rhs == NULL || rhs[0] == L'\0')
	{
		return strdup("<nop>");
	}
	return wstr_to_spec(rhs);
}

int
vle_abbr_iter(const wchar_t **lhs, const wchar_t **rhs, const char **descr,
		int *no_remap, void **param)
{
	size_t i = (*param == NULL) ? 0 : ((abbrev_t *)*param - abbrevs + 1);
	abbrev_t *abbrev;

	if(i >= abbrev_count)
	{
		*lhs = NULL;
		*rhs = NULL;
		*descr = NULL;
		*param = NULL;
		return 0;
	}

	abbrev = &abbrevs[i];
	*lhs = abbrev->lhs;
	*rhs = abbrev->rhs;
	*descr = abbrev->descr;
	*no_remap = abbrev->no_remap;
	*param = abbrev;
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
