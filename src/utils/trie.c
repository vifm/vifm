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
typedef struct trie_node_t
{
	struct trie_node_t *left;     /* Nodes with values less than value. */
	struct trie_node_t *right;    /* Nodes with values greater than value. */
	struct trie_node_t *children; /* Child nodes. */

	void *data;  /* Data associated with the key. */
	char value;  /* Value of the node. */
	char exists; /* Whether this node exists or it's an intermediate node. */
}
trie_node_t;

/* Trie. */
struct trie_t
{
	trie_node_t *root; /* Root node. */
};

static trie_node_t * clone_nodes(trie_t *trie, const trie_node_t *node,
		int *error);
static void free_nodes(trie_node_t *node);
static void free_nodes_data(trie_node_t *trie, trie_free_func free_func);
static void get_or_create(trie_t *trie, const char str[], void *data,
		int *result);
static int trie_get_nodes(trie_node_t *trie, const char str[], void **data);

trie_t *
trie_create(void)
{
	return calloc(1U, sizeof(struct trie_t));
}

trie_t *
trie_clone(trie_t *trie)
{
	if(trie == NULL)
	{
		return NULL;
	}

	trie_t *new_trie = trie_create();
	if(new_trie == NULL)
	{
		return NULL;
	}

	int error = 0;
	new_trie->root = clone_nodes(new_trie, trie->root, &error);
	if(error)
	{
		trie_free(new_trie);
		return NULL;
	}

	return new_trie;
}

/* Clones node and all its relatives.  Sets *error to non-zero on error.
 * Returns new node. */
static trie_node_t *
clone_nodes(trie_t *new_trie, const trie_node_t *node, int *error)
{
	if(node == NULL)
	{
		return NULL;
	}

	trie_node_t *new_node = malloc(sizeof(*new_node));
	if(new_node == NULL)
	{
		*error = 1;
		return NULL;
	}

	new_node->left = clone_nodes(new_trie, node->left, error);
	new_node->right = clone_nodes(new_trie, node->right, error);
	new_node->children = clone_nodes(new_trie, node->children, error);
	new_node->data = node->data;
	new_node->value = node->value;
	new_node->exists = node->exists;

	return new_node;
}

void
trie_free(trie_t *trie)
{
	if(trie != NULL)
	{
		free_nodes(trie->root);
		free(trie);
	}
}

/* Frees each node. */
static void
free_nodes(trie_node_t *node)
{
	if(node != NULL)
	{
		free_nodes(node->left);
		free_nodes(node->right);
		free_nodes(node->children);
		free(node);
	}
}

void
trie_free_with_data(trie_t *trie, trie_free_func free_func)
{
	if(trie != NULL)
	{
		free_nodes_data(trie->root, free_func);
		trie_free(trie);
	}
}

/* Calls custom free function on data stored in every node. */
static void
free_nodes_data(trie_node_t *node, trie_free_func free_func)
{
	if(node != NULL)
	{
		free_nodes_data(node->left, free_func);
		free_nodes_data(node->right, free_func);
		free_nodes_data(node->children, free_func);
		free_func(node->data);
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
	trie_node_t **link = &trie->root;
	trie_node_t *node = trie->root;
	while(1)
	{
		/* Create inexistent node. */
		if(node == NULL)
		{
			node = calloc(1U, sizeof(*node));
			if(node == NULL)
			{
				*result = -1;
				break;
			}
			node->value = *str;
			*link = node;
		}

		if(node->value == *str)
		{
			if(*str == '\0')
			{
				/* Found full match. */
				*result = (node->exists != 0);
				node->exists = 1;
				node->data = data;
				break;
			}

			link = &node->children;
			++str;
		}
		else
		{
			link = (*str < node->value) ? &node->left : &node->right;
		}
		node = *link;
	}
}

int
trie_get(trie_t *trie, const char str[], void **data)
{
	if(trie == NULL)
	{
		return 1;
	}
	return trie_get_nodes(trie->root, str, data);
}

/* Looks up data for the str.  Node can be NULL.  Returns zero when found and
 * sets *data, otherwise returns non-zero. */
static int
trie_get_nodes(trie_node_t *node, const char str[], void **data)
{
	while(1)
	{
		if(node == NULL)
		{
			return 1;
		}

		if(node->value == *str)
		{
			if(*str == '\0')
			{
				/* Found full match. */
				if(!node->exists)
				{
					return 1;
				}
				*data = node->data;
				return 0;
			}

			node = node->children;
			++str;
			continue;
		}

		node = (*str < node->value) ? node->left : node->right;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
