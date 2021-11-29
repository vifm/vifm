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

#include <assert.h> /* assert() */
#include <stdlib.h> /* calloc() free() malloc() */
#include <string.h> /* memcpy() strlen() strncmp() */

#include "../compat/fs_limits.h"
#include "../compat/reallocarray.h"

#include "macros.h"

/*
 * This is an implementation of a compressed 3-way trie that doesn't support
 * deletion (not necessary for the application).  Each node contains the longest
 * key it can, but it's broken on addition of new keys which differ in their
 * suffix.
 *
 * In order to not look enumerate children linearly, every node contains links
 * to siblings with smaller/larger keys, which makes the trie similar to a
 * regular binary tree without balancing.
 *
 * For better performance memory management is optimized:
 *  - monolithic allocation of node structures in large amounts to not use
 *    allocator on every node creation and be able to destruct with several
 *    calls to free()
 *  - strings are similarly appended to relatively large buffers without NUL
 *    bytes and are reused on breaking nodes (so no string copying there)
 *    Note: this puts a limit on key length, which can't be longer than buffer.
 *
 * Another optimization is caching of the first character of the key to avoid
 * pointer dereferencing when we don't need it (first character decides if we
 * need to go to left or right sibling next).
 */

/* Number of elements in each trie_t::nodes[*]. */
#define NODES_PER_BANK 1024

/* Size of each trie_t::str_bufs[*]. */
#define STR_BUF_SIZE (PATH_MAX*2)

/* Trie node. */
typedef struct trie_node_t
{
	struct trie_node_t *left;  /* Nodes with keys less than query. */
	struct trie_node_t *right; /* Nodes with keys greater than query. */
	struct trie_node_t *down;  /* Child nodes which match prefix of the query. */

	const char *key; /* Key of the node (NO trailing '\0'!). */
	char first;      /* First character of the key for faster access. */
	char exists;     /* Whether this node exists or it's an intermediate node. */
	int key_len;     /* Length of the key. */
	void *data;      /* Data associated with the key. */
}
trie_node_t;

/* Trie.  Manages storage of nodes. */
struct trie_t
{
	trie_node_t *root; /* Root node. */

	trie_node_t **nodes; /* Node storage (NODES_PER_BANK elements per item). */
	int node_count;      /* Number of (allocated) nodes. */

	char **str_bufs;   /* String storage (STR_BUF_SIZE bytes per item). */
	int str_buf_count; /* Number of string buffers. */
	int last_offset;   /* Offset in current string buffer. */

	trie_free_func free_func; /* Function for freeing dynamic data. */
};

static trie_node_t * clone_nodes(trie_t *trie, const trie_node_t *node,
		int *error);
static void free_nodes_data(trie_node_t *node, trie_free_func free_func);
static trie_node_t * make_node(trie_t *trie);
static char * alloc_string(trie_t *trie, const char str[], int len);
static int trie_get_nodes(trie_node_t *node, const char str[], void **data);

trie_t *
trie_create(trie_free_func free_func)
{
	trie_t *trie = calloc(1U, sizeof(*trie));
	trie->free_func = free_func;
	return trie;
}

trie_t *
trie_clone(trie_t *trie)
{
	if(trie == NULL)
	{
		return NULL;
	}

	assert(trie->free_func == NULL && "Can't clone a trie with dynamic data!");

	trie_t *new_trie = trie_create(/*free_func=*/NULL);
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

	trie_node_t *new_node = make_node(new_trie);
	if(new_node == NULL)
	{
		*error = 1;
		return NULL;
	}

	new_node->left = clone_nodes(new_trie, node->left, error);
	new_node->right = clone_nodes(new_trie, node->right, error);
	new_node->down = clone_nodes(new_trie, node->down, error);
	new_node->key = alloc_string(new_trie, node->key, node->key_len);
	new_node->first = node->key[0];
	new_node->key_len = node->key_len;
	new_node->exists = node->exists;
	new_node->data = node->data;

	return new_node;
}

void
trie_free(trie_t *trie)
{
	if(trie == NULL)
	{
		return;
	}

	if(trie->free_func != NULL)
	{
		free_nodes_data(trie->root, trie->free_func);
	}

	int banks = DIV_ROUND_UP(trie->node_count, NODES_PER_BANK);
	int bank;
	for(bank = 0; bank < banks; ++bank)
	{
		free(trie->nodes[bank]);
	}
	free(trie->nodes);

	int i;
	for(i = 0; i < trie->str_buf_count; ++i)
	{
		free(trie->str_bufs[i]);
	}
	free(trie->str_bufs);

	free(trie);
}

/* Calls custom free function on data stored in every node. */
static void
free_nodes_data(trie_node_t *node, trie_free_func free_func)
{
	if(node != NULL)
	{
		free_nodes_data(node->left, free_func);
		free_nodes_data(node->right, free_func);
		free_nodes_data(node->down, free_func);
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
	if(trie == NULL)
	{
		return -1;
	}

	trie_node_t **link = &trie->root;
	trie_node_t *node = trie->root;

	if(*str == '\0')
	{
		return -1;
	}

	while(1)
	{
		/* Create inexistent node. */
		if(node == NULL)
		{
			node = make_node(trie);
			if(node == NULL)
			{
				return -1;
			}
			node->key_len = strlen(str);
			node->key = alloc_string(trie, str, node->key_len);
			node->first = str[0];
			*link = node;
			break;
		}

		int i = 0;
		while(i < node->key_len && node->key[i] == str[i])
		{
			++i;
		}

		if(i == node->key_len)
		{
			link = &node->down;
			str += node->key_len;

			if(*str == '\0')
			{
				/* Found full match. */
				break;
			}
		}
		else if(i == 0)
		{
			link = (*str < node->first) ? &node->left : &node->right;
		}
		else
		{
			/* Break current node in two as me matched in the middle. */

			trie_node_t *new_node = make_node(trie);
			if(new_node == NULL)
			{
				return -1;
			}
			new_node->key = node->key + i;
			new_node->first = node->key[i];
			new_node->key_len = node->key_len - i;
			new_node->exists = node->exists;
			new_node->data = node->data;
			new_node->down = node->down;

			node->key_len = i;
			node->down = new_node;
			node->exists = 0;
			node->data = NULL;

			/* Return the leading part of the node we just broke in two. */
			if(str[i] == '\0')
			{
				break;
			}

			str += i;
			link = &node->down;
		}
		node = *link;
	}

	const int result = (node->exists != 0);
	node->exists = 1;
	node->data = (void *)data;
	return result;
}

/* Allocates a new node for the trie.  Returns pointer to the node or NULL. */
static trie_node_t *
make_node(trie_t *trie)
{
	const int bank = trie->node_count/NODES_PER_BANK;
	const int bank_index = trie->node_count%NODES_PER_BANK;
	++trie->node_count;

	if(bank_index == 0)
	{
		void *nodes = reallocarray(trie->nodes, bank + 1, sizeof(*trie->nodes));
		if(nodes == NULL)
		{
			return NULL;
		}

		trie->nodes = nodes;
		trie->nodes[bank] = calloc(NODES_PER_BANK, sizeof(**trie->nodes));
	}

	return &trie->nodes[bank][bank_index];
}

/* Allocates string in a trie.  Returns pointer to it or NULL if out of
 * memory. */
static char *
alloc_string(trie_t *trie, const char str[], int len)
{
	assert(len <= STR_BUF_SIZE && "Key is too large.");

	if(trie->last_offset + len > STR_BUF_SIZE || trie->str_buf_count == 0)
	{
		void *str_bufs = reallocarray(trie->str_bufs, trie->str_buf_count + 1,
				sizeof(*trie->str_bufs));
		if(str_bufs == NULL)
		{
			return NULL;
		}

		trie->str_bufs = str_bufs;
		trie->str_bufs[trie->str_buf_count] = malloc(STR_BUF_SIZE);

		++trie->str_buf_count;
		trie->last_offset = 0;
	}

	char *buf = trie->str_bufs[trie->str_buf_count - 1] + trie->last_offset;
	memcpy(buf, str, len);

	trie->last_offset += len;
	return buf;
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
	if(*str == '\0')
	{
		return 1;
	}

	int str_len = strlen(str);

	while(1)
	{
		if(node == NULL)
		{
			return 1;
		}

		if(*str == node->first)
		{
			if(str_len < node->key_len ||
					memcmp(node->key + 1, str + 1, node->key_len - 1) != 0)
			{
				return 1;
			}

			str += node->key_len;
			str_len -= node->key_len;

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

			node = node->down;
			continue;
		}

		node = (*str < node->first) ? node->left : node->right;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
