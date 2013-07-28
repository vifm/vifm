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

#ifndef VIFM__UTILS__TREE_H__
#define VIFM__UTILS__TREE_H__

#include <stdint.h> /* uint64_t */

#define NULL_TREE NULL

typedef struct root_t *tree_t;
typedef uint64_t tree_val_t;

/* longest parameter determines whether implementation will return the last
 * value found during searching in the tree. mem parameter shows whether tree
 * will contain pointers, which will be freed by tree_free(). Returns
 * NULL_TREE on error. */
tree_t tree_create(int longest, int mem);

/* Frees memory from values if tree was created with mem parameter set to
 * true. Freeing of NULL_TREE tree is OK. */
void tree_free(tree_t tree);

/* Returns non-zero on error. */
int tree_set_data(tree_t tree, const char *path, tree_val_t data);

/* Won't change data content if path absent in tree. Returns non-zero on
 * error. */
int tree_get_data(tree_t tree, const char *path, tree_val_t *data);

#endif /* VIFM__UTILS__TREE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
