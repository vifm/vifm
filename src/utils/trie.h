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

#ifndef VIFM__UTILS__TRIE_H__
#define VIFM__UTILS__TRIE_H__

#include <stddef.h> /* NULL */

/* NULL equivalent for variables of type trie_t. */
#define NULL_TRIE NULL

/* Declaration of opaque trie type. */
typedef struct trie_t *trie_t;

/* Creates new empty trie.  Returns NULL_TRIE on error. */
trie_t trie_create(void);

/* Frees memory allocated for the trie.  Freeing of NULL_TRIE trie is OK. */
void trie_free(trie_t trie);

/* Inserts string to the trie if it's not already there.  Returns negative value
 * on error, zero on successful insertion and positive number if element was
 * already in the trie. */
int trie_put(trie_t trie, const char str[]);

#endif /* VIFM__UTILS__TRIE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
