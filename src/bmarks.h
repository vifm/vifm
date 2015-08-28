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

#ifndef VIFM__BMARKS_H__
#define VIFM__BMARKS_H__

/* Paths are stored and compared as is, additional no canonicalization is
 * performed.  No particular order of bookmarks is preserved. */

/* Type of callback function. */
typedef void (*bmarks_find_cb)(const char path[], const char tags[], void *arg);

/* Associates (overwriting existing associations) comma-separated list of tags
 * with the path.  Returns zero on-success, otherwise non-zero is returned. */
int bmarks_set(const char path[], const char tags[]);

/* Same as bmarks_setup(), but also sets timestamp. */
int bmarks_setup(const char path[], const char tags[]);

/* Clear bookmarks for the specified path. */
void bmarks_remove(const char path[]);

/* Lists all available records by calling the callback. */
void bmarks_list(bmarks_find_cb cb, void *arg);

/* Looks up paths with matching list of associated tags. */
void bmarks_find(const char tags[], bmarks_find_cb cb, void *arg);

/* Removes all bookmarks. */
void bmarks_clear(void);

/* Completes the str skipping list of the tags (to do not propose
 * duplicates). */
void bmarks_complete(int n, char *tags[], const char str[]);

#endif /* VIFM__BMARKS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
