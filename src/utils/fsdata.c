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
#include "private/fsdata.h"

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* memcpy() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "str.h"

/* Special value for get_or_create_node()'s data_size argument to prevent it
 * from creating a node. */
#define NO_CREATE (size_t)-1

/* Tree node type. */
typedef struct node_t
{
	char *name;           /* Name of this node. */
	size_t name_len;      /* Length of the name. */
	int valid;            /* Whether data in this node is meaningful. */
	struct node_t *next;  /* Next sibling on this level. */
	struct node_t *child; /* Leftmost child of this node. */
	char data[];          /* Data associated with the node follows. */
}
node_t;

/* Tree head that holds its settings. */
struct fsdata_t
{
	node_t *root;             /* Root node data. */
	int prefix;               /* Whether we use last seen value on searches. */
	int resolve_paths;        /* Whether input paths should be resolved. */
	fsd_cleanup_func cleanup; /* Node data cleanup function. */
};

static void do_nothing(void *data);
static void nodes_free(node_t *node, fsd_cleanup_func cleanup);
static node_t * get_or_create_node(node_t *root, const char path[],
		size_t data_size, node_t **last, node_t **link);
static node_t * make_node(const char name[], size_t name_len, size_t data_size);
static int map_parents(node_t *root, const char path[],
		fsdata_visit_func visitor, void *arg);
static int resolve_path(const fsdata_t *fsd, const char path[],
		char real_path[]);
static int traverse_node(node_t *node, const node_t *parent,
		fsdata_traverser_func traverser, void *arg);

fsdata_t *
fsdata_create(int prefix, int resolve_paths)
{
	fsdata_t *const fsd = malloc(sizeof(*fsd));
	if(fsd == NULL)
	{
		return NULL;
	}

	fsd->root = NULL;
	fsd->prefix = prefix;
	fsd->resolve_paths = resolve_paths;
	fsd->cleanup = &do_nothing;
	return fsd;
}

/* Node cleanup stub, that does nothing. */
static void
do_nothing(void *data)
{
	(void)data;
}

void
fsdata_set_cleanup(fsdata_t *fsd, fsd_cleanup_func cleanup)
{
	fsd->cleanup = cleanup;
}

void
fsdata_free(fsdata_t *fsd)
{
	if(fsd != NULL)
	{
		nodes_free(fsd->root, fsd->cleanup);
		free(fsd);
	}
}

/* Recursively frees all the nodes and everything associated with them. */
static void
nodes_free(node_t *node, fsd_cleanup_func cleanup)
{
	if(node == NULL)
	{
		return;
	}

	if(node->valid)
	{
		cleanup(&node->data);
	}

	nodes_free(node->child, cleanup);
	nodes_free(node->next, cleanup);

	free(node->name);
	free(node);
}

int
fsdata_set(fsdata_t *fsd, const char path[], const void *data, size_t len)
{
	node_t *node;
	char real_path[PATH_MAX + 1];

	if(resolve_path(fsd, path, real_path) != 0)
	{
		return -1;
	}

	/* Create root node lazily, when we know data size. */
	if(fsd->root == NULL)
	{
		fsd->root = make_node("/", 1U, len);
		if(fsd->root == NULL)
		{
			return -1;
		}
	}

	node = get_or_create_node(fsd->root, real_path, len, NULL, &fsd->root);
	if(node == NULL)
	{
		return -1;
	}

	if(node->valid)
	{
		fsd->cleanup(&node->data);
	}

	node->valid = 1;
	memcpy(node->data, data, len);
	return 0;
}

int
fsdata_get(fsdata_t *fsd, const char path[], void *data, size_t len)
{
	node_t *last = NULL;
	node_t *node;
	const void *src;
	char real_path[PATH_MAX + 1];

	if(fsd->root == NULL)
	{
		return -1;
	}

	if(resolve_path(fsd, path, real_path) != 0)
	{
		return -1;
	}

	node = get_or_create_node(fsd->root, real_path, NO_CREATE,
			fsd->prefix ? &last : NULL, NULL);
	if((node == NULL || !node->valid) && last == NULL)
	{
		return -1;
	}

	src = (node != NULL && node->valid) ? node->data : last->data;
	memcpy(data, src, len);
	return 0;
}

/* Looks up a node by its path.  Inserts a node if it doesn't exist and
 * data_size is not equal to NO_CREATE.  If last is not NULL *last is assigned
 * closest valid parent node.  Optionally reallocates and updates *link.
 * Returns the node at the path or NULL on error. */
static node_t *
get_or_create_node(node_t *root, const char path[], size_t data_size,
		node_t **last, node_t **link)
{
	const char *end;
	size_t name_len;
	node_t *prev = NULL, *curr;
	node_t *new_node;

	path = skip_char(path, '/');
	if(*path == '\0')
	{
		if(link != NULL)
		{
			root = realloc(root, sizeof(*root) + data_size);
			if(root != NULL)
			{
				*link = root;
			}
		}
		return root;
	}

	end = until_first(path, '/');

	name_len = end - path;
	curr = root->child;
	while(curr != NULL)
	{
		int comp = strnoscmp(path, curr->name, name_len);
		if(comp == 0 && curr->name_len == name_len)
		{
			if(curr->valid && last != NULL)
			{
				*last = curr;
			}
			return get_or_create_node(curr, end, data_size, last,
					(data_size == NO_CREATE) ? NULL :
					(root->child == curr) ? &root->child : &prev->next);
		}
		else if(comp < 0)
		{
			break;
		}
		prev = curr;
		curr = curr->next;
	}

	if(data_size == NO_CREATE)
	{
		return NULL;
	}

	new_node = make_node(path, name_len, data_size);
	if(new_node == NULL)
	{
		return NULL;
	}

	new_node->next = curr;

	if(root->child == curr)
	{
		root->child = new_node;
	}
	else
	{
		prev->next = new_node;
	}
	/* No need to update anything here, because size is guaranteed to be the
	 * same. */
	return get_or_create_node(new_node, end, data_size, last, NULL);
}

/* Creates new node for the tree.  Returns the node or NULL on memory allocation
 * error. */
static node_t *
make_node(const char name[], size_t name_len, size_t data_size)
{
	node_t *new_node = malloc(sizeof(*new_node) + data_size);
	if(new_node == NULL)
	{
		return NULL;
	}

	new_node->name = malloc(name_len + 1U);
	if(new_node->name == NULL)
	{
		free(new_node);
		return NULL;
	}

	copy_str(new_node->name, name_len + 1U, name);
	new_node->name_len = name_len;
	new_node->valid = 0;
	new_node->child = NULL;
	new_node->next = NULL;

	return new_node;
}

int
fsdata_map_parents(fsdata_t *fsd, const char path[], fsdata_visit_func visitor,
		void *arg)
{
	char real_path[PATH_MAX + 1];
	if(resolve_path(fsd, path, real_path) != 0)
	{
		return 1;
	}

	return map_parents(fsd->root, real_path, visitor, arg);
}

/* Performs optional path resolution (configured at tree creation).  real_path
 * should be at least PATH_MAX chars in length.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
resolve_path(const fsdata_t *fsd, const char path[], char real_path[])
{
	if(fsd->resolve_paths)
	{
		return (os_realpath(path, real_path) != real_path);
	}
	copy_str(real_path, PATH_MAX, path);
	return 0;
}

/* Invokes visitor once per valid parent node of specified path.  Returns zero
 * on success or non-zero if path wasn't found. */
static int
map_parents(node_t *root, const char path[], fsdata_visit_func visitor,
		void *arg)
{
	const char *end;
	size_t name_len;
	node_t *curr;

	path = skip_char(path, '/');
	if(*path == '\0')
	{
		return 0;
	}

	end = until_first(path, '/');

	name_len = end - path;
	curr = root->child;
	while(curr != NULL)
	{
		const int cmp = strnoscmp(path, curr->name, name_len);
		if(cmp == 0 && curr->name_len == name_len)
		{
			if(map_parents(curr, end, visitor, arg) == 0)
			{
				if(root->valid)
				{
					visitor(&root->data, arg);
				}
				return 0;
			}
			break;
		}
		else if(cmp < 0)
		{
			break;
		}
		curr = curr->next;
	}

	return 1;
}

int
fsdata_traverse(fsdata_t *fsd, fsdata_traverser_func traverser, void *arg)
{
	node_t *node;

	if(fsd->root == NULL)
	{
		return 0;
	}

	for(node = fsd->root->child; node != NULL; node = node->next)
	{
		if(traverse_node(node, NULL, traverser, arg) != 0)
		{
			return 1;
		}
	}
	return 0;
}

/* fsdata_traverse() helper which works with node_t type.  Return non-zero if
 * traversing was stopped prematurely, otherwise zero is returned. */
static int
traverse_node(node_t *node, const node_t *parent,
		fsdata_traverser_func traverser, void *arg)
{
	const void *const parent_data = (parent == NULL ? NULL : &parent->data);
	if(traverser(node->name, node->valid, parent_data, &node->data, arg) != 0)
	{
		return 1;
	}

	for(parent = node, node = node->child; node != NULL; node = node->next)
	{
		if(traverse_node(node, parent, traverser, arg) != 0)
		{
			return 1;
		}
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
