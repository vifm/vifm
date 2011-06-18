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

#include <limits.h>

#include <stdlib.h>
#include <string.h>

#include "tree.h"

struct node {
	char *name;
	size_t name_len;
	void *data;
	int valid;
	struct node *next;
	struct node *child;
};

static struct node * find_node(struct node *root, const char *name, int create);

tree_t
tree_create(void)
{
	struct node *tree;

	tree = malloc(sizeof(*tree));
	tree->child = NULL;
	return tree;
}

int
tree_set_data(tree_t tree, const char *path, void* data)
{
	struct node *node;
	char real_path[PATH_MAX];

	if(realpath(path, real_path) != real_path)
		return -1;

	node = find_node(tree, real_path, 1);
	node->data = data;
	node->valid = 1;
	return 0;
}

void *
tree_get_data(tree_t tree, const char *path)
{
	struct node *node;
	char real_path[PATH_MAX];

	if(tree->child == NULL)
		return NULL;

	if(realpath(path, real_path) != real_path)
		return NULL;

	node = find_node(tree, real_path, 0);
	if(node == NULL || !node->valid)
		return NULL;
	
	return node->data;
}

static struct node *
find_node(struct node *root, const char *name, int create)
{
	const char *end;
	size_t name_len;
	struct node *prev = NULL, *curr;
	struct node *new_node;

	if(*name == '/')
		name++;

	if(*name == '\0')
		return root;

	end = strchr(name, '/');
	if(end == NULL)
		end = name + strlen(name);

	name_len = end - name;
	curr = root->child;
	while(curr != NULL)
	{
		int comp = strncmp(name, curr->name, end - name);
		if(comp == 0 && curr->name_len == name_len)
			return find_node(curr, end, create);
		else if(comp < 0)
			break;
		prev = curr;
		curr = curr->next;
	}

	if(!create)
		return NULL;

	new_node = malloc(sizeof(*new_node));
	if(new_node == NULL)
		return NULL;

	if((new_node->name = malloc(name_len + 1)) == NULL)
	{
		free(new_node);
		return NULL;
	}
	strncpy(new_node->name, name, name_len);
	new_node->name[name_len] = '\0';
	new_node->name_len = name_len;
	new_node->valid = 0;
	new_node->child = NULL;
	new_node->next = curr;

	if(root->child == curr)
		root->child = new_node;
	else
		prev->next = new_node;

	return find_node(new_node, end, create);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
