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
	trie_t *left;     /* Nodes with values less than value. */
	trie_t *right;    /* Nodes with values greater than value. */
	trie_t *children; /* Child nodes. */
	void *data;       /* Data associated with the key. */
	char value;       /* Value of the node. */
	char exists;      /* Whether this node exists or it's an intermediate node. */
};

static trie_t *clone_nodes(trie_t *trie, int *error);
static void get_or_create(trie_t *trie, const char str[], void *data,
		int *result);

trie_t *
trie_create(void)
{
	return calloc(1U, sizeof(struct trie_t));
}

trie_t *
trie_clone(trie_t *trie)
{
	int error = 0;

	trie = clone_nodes(trie, &error);
	if(error)
	{
		trie_free(trie);
		return NULL;
	}

	return trie;
}

/* Clones node and all its relatives.  Sets *error to non-zero on error.
 * Returns new node. */
static trie_t *
clone_nodes(trie_t *trie, int *error)
{
	trie_t *new_trie;

	if(trie == NULL)
	{
		return NULL;
	}

	new_trie = malloc(sizeof(*new_trie));
	if(new_trie == NULL)
	{
		*error = 1;
		return NULL;
	}

	*new_trie = *trie;
	new_trie->left = clone_nodes(trie->left, error);
	new_trie->right = clone_nodes(trie->right, error);
	new_trie->children = clone_nodes(trie->children, error);

	return new_trie;
}

void
trie_free(trie_t *trie)
{
	if(trie != NULL)
	{
		trie_free(trie->left);
		trie_free(trie->right);
		trie_free(trie->children);
		free(trie);
	}
}

void
trie_free_with_data(trie_t *trie, trie_free_func free_func)
{
	if(trie != NULL)
	{
		trie_free_with_data(trie->left, free_func);
		trie_free_with_data(trie->right, free_func);
		trie_free_with_data(trie->children, free_func);
		free_func(trie->data);
		free(trie);
	}
}

int
trie_put(trie_t *trie, const char str[])
{
	return trie_set(trie, str, NULL);
}

int
trie_set(trie_t *trie, const char str[], const void *data)
{
	int result;

	if(trie == NULL)
	{
		return -1;
	}

	get_or_create(trie, str, (void *)data, &result);
	return result;
}

/* Gets node which might involve its creation.  Sets *return to negative value
 * on error, to zero on successful insertion and to positive number if element
 * was already in the trie. */
static void
get_or_create(trie_t *trie, const char str[], void *data, int *result)
{
	trie_t **link = &trie;
	while(1)
	{
		/* Create inexistent node. */
		if(trie == NULL)
		{
			trie = trie_create();
			if(trie == NULL)
			{
				*result = -1;
				break;
			}
			trie->value = *str;
			*link = trie;
		}

		if(trie->value == *str)
		{
			if(*str == '\0')
			{
				/* Found full match. */
				*result = (trie->exists != 0);
				trie->exists = 1;
				trie->data = data;
				break;
			}

			link = &trie->children;
			++str;
		}
		else
		{
			link = (*str < trie->value) ? &trie->left : &trie->right;
		}
		trie = *link;
	}
}

int
trie_get(trie_t *trie, const char str[], void **data)
{
	while(1)
	{
		if(trie == NULL)
		{
			return 1;
		}

		if(trie->value == *str)
		{
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

			trie = trie->children;
			++str;
			continue;
		}

		trie = (*str < trie->value) ? trie->left : trie->right;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
