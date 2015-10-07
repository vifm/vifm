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

#include "trie.h"

#include <stdlib.h> /* calloc() free() */

/* Trie node. */
struct trie_t
{
	trie_t left;     /* Nodes with values less than value. */
	trie_t right;    /* Nodes with values greater than value. */
	trie_t children; /* Child nodes. */
	void *data;      /* Data associated with the key. */
	char value;      /* Value of the node. */
	char exists;     /* Whether this node exists or it's an intermediate node. */
};

static trie_t get_or_create(trie_t trie, const char str[], void *data,
		int *result);

trie_t
trie_create(void)
{
	return calloc(1U, sizeof(struct trie_t));
}

void
trie_free(trie_t trie)
{
	if(trie != NULL_TRIE)
	{
		trie_free(trie->left);
		trie_free(trie->right);
		trie_free(trie->children);
		free(trie);
	}
}

void
trie_free_with_data(trie_t trie)
{
	if(trie != NULL_TRIE)
	{
		trie_free_with_data(trie->left);
		trie_free_with_data(trie->right);
		trie_free_with_data(trie->children);
		free(trie->data);
		free(trie);
	}
}

int
trie_put(trie_t trie, const char str[])
{
	return trie_set(trie, str, NULL);
}

int
trie_set(trie_t trie, const char str[], const void *data)
{
	int result;

	if(trie == NULL_TRIE)
	{
		return -1;
	}

	(void)get_or_create(trie, str, (void *)data, &result);
	return result;
}

/* Gets node which might involve its creation.  Sets *return to negative value
 * on error, to zero on successful insertion and to positive number if element
 * was already in the trie. */
static trie_t
get_or_create(trie_t trie, const char str[], void *data, int *result)
{
	/* Create inexistent node. */
	if(trie == NULL_TRIE)
	{
		trie = trie_create();
		if(trie == NULL_TRIE)
		{
			*result = -1;
			return trie;
		}
		trie->value = *str;
	}

	if(trie->value == *str)
	{
		if(*str == '\0')
		{
			/* Found full match. */
			*result = (trie->exists != 0);
			trie->exists = 1;
			trie->data = data;
		}
		else
		{
			trie->children = get_or_create(trie->children, str + 1, data, result);
		}
	}
	else if(*str < trie->value)
	{
		trie->left = get_or_create(trie->left, str, data, result);
	}
	else
	{
		trie->right = get_or_create(trie->right, str, data, result);
	}

	return trie;
}

int
trie_get(trie_t trie, const char str[], void **data)
{
	if(trie == NULL_TRIE)
	{
		return 1;
	}

	if(trie->value != *str)
	{
		return trie_get((*str < trie->value) ? trie->left : trie->right, str, data);
	}

	if(*str == '\0')
	{
		/* Found full match. */
		if(!trie->exists)
		{
			return 1;
		}
		*data = trie->data;
		return 0;
	}

	return trie_get(trie->children, str + 1, data);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
