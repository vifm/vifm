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

/* Declaration of opaque trie type. */
typedef struct trie_t trie_t;

/* Creates new empty trie.  Returns NULL on error. */
trie_t * trie_create(void);

/* Clones a trie.  Associated data pointers are copied as is.  Returns NULL on
 * error. */
trie_t * trie_clone(trie_t *trie);

/* Frees memory allocated for the trie.  Freeing of NULL trie is OK. */
void trie_free(trie_t *trie);

/* Frees memory allocated for the trie.  Freeing of NULL trie is OK.  All data
 * associated with trie entries is freed by calling free() on it. */
void trie_free_with_data(trie_t *trie);

/* Inserts string to the trie if it's not already there.  Returns negative value
 * on error, zero on successful insertion and positive number if element was
 * already in the trie. */
int trie_put(trie_t *trie, const char str[]);

/* Same as trie_put(), but also sets data. */
int trie_set(trie_t *trie, const char str[], const void *data);

/* Looks up data for the str in the trie.  trie can be NULL, which is treated as
 * an empty trie.  Returns zero when found and sets *data, otherwise returns
 * non-zero. */
int trie_get(trie_t *trie, const char str[], void **data);

#endif /* VIFM__UTILS__TRIE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
