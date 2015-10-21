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

/* The implementation is a tree (with links to leftmost child and right
 * sibling), which is traversed according to slash separated path.  Siblings are
 * sorted by name. */

#include "fsdata.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() malloc() */
#include <string.h>

#ifdef _WIN32
#include "fs.h"
#endif
#include "../compat/fs_limits.h"
#include "str.h"

/* Tree node type. */
typedef struct node_t
{
	char *name;           /* Name of this node. */
	size_t name_len;      /* Length of the name. */
	tree_val_t data;      /* Data associated with the node. */
	int valid;            /* Whether data in this node is meaningful. */
	struct node_t *next;  /* Next sibling on this level. */
	struct node_t *child; /* Leftmost child of this node. */
}
node_t;

/* A node subtype that holds additional data. */
struct fsdata_t
{
	node_t node; /* Root node data.  Must be the first field. */
	int longest; /* Whether we use last seen value on searches. */
	int mem;     /* Whether stored data is a heap pointer. */
};

static void nodes_free(node_t *node, int mem);
static node_t * find_node(node_t *root, const char path[], int create,
		node_t **last);

fsdata_t *
fsdata_create(int longest, int mem)
{
	fsdata_t *const fsd = malloc(sizeof(*fsd));
	if(fsd == NULL)
	{
		return NULL;
	}

	fsd->node.child = NULL;
	fsd->node.next = NULL;
	fsd->node.name = NULL;
	fsd->node.valid = 0;
	fsd->longest = longest;
	fsd->mem = mem;
	return fsd;
}

void
fsdata_free(fsdata_t *fsd)
{
	if(fsd != NULL)
	{
		/* Here root is also a node as it includes node_t as its first field. */
		nodes_free(&fsd->node, fsd->mem);
	}
}

/* Recursively frees all the nodes and everything associated with them. */
static void
nodes_free(node_t *node, int mem)
{
	if(node == NULL)
	{
		return;
	}

	if(node->valid && mem)
	{
		union
		{
			tree_val_t l;
			void *p;
		} u = {
			.l = node->data,
		};

		free(u.p);
	}

	nodes_free(node->child, mem);
	nodes_free(node->next, mem);

	free(node->name);
	free(node);
}

int
fsdata_set(fsdata_t *fsd, const char path[], tree_val_t data)
{
	node_t *node;
	char real_path[PATH_MAX];

	if(realpath(path, real_path) != real_path)
		return -1;

	node = find_node(&fsd->node, real_path, 1, NULL);
	if(node->valid && fsd->mem)
	{
		union
		{
			tree_val_t l;
			void *p;
		} u = {
			.l = node->data,
		};

		free(u.p);
	}
	node->data = data;
	node->valid = 1;
	return 0;
}

int
fsdata_get(fsdata_t *fsd, const char path[], tree_val_t *data)
{
	node_t *last = NULL;
	node_t *node;
	char real_path[PATH_MAX];

	if(fsd->node.child == NULL)
		return -1;

	if(realpath(path, real_path) != real_path)
		return -1;

	node = find_node(&fsd->node, real_path, 0, fsd->longest ? &last : NULL);
	if((node == NULL || !node->valid) && last == NULL)
		return -1;

	if(node != NULL && node->valid)
		*data = node->data;
	else
		*data = last->data;
	return 0;
}

static node_t *
find_node(node_t *root, const char path[], int create, node_t **last)
{
	const char *end;
	size_t name_len;
	node_t *prev = NULL, *curr;
	node_t *new_node;

	path = skip_char(path, '/');
	if(*path == '\0')
		return root;

	end = until_first(path, '/');

	name_len = end - path;
	curr = root->child;
	while(curr != NULL)
	{
		int comp = strnoscmp(path, curr->name, name_len);
		if(comp == 0 && curr->name_len == name_len)
		{
			if(curr->valid && last != NULL)
				*last = curr;
			return find_node(curr, end, create, last);
		}
		else if(comp < 0)
		{
			break;
		}
		prev = curr;
		curr = curr->next;
	}

	if(!create)
		return NULL;

	new_node = malloc(sizeof(*new_node));
	if(new_node == NULL)
		return NULL;

	new_node->name = malloc(name_len + 1);
	if(new_node->name == NULL)
	{
		free(new_node);
		return NULL;
	}
	copy_str(new_node->name, name_len + 1, path);
	new_node->name[name_len] = '\0';
	new_node->name_len = name_len;
	new_node->valid = 0;
	new_node->child = NULL;
	new_node->next = curr;

	if(root->child == curr)
		root->child = new_node;
	else
		prev->next = new_node;

	return find_node(new_node, end, create, last);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
