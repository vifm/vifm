/* vifm
 * Copyright (C) 2020 xaizek.
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

#ifndef VIFM__VCACHE_H__
#define VIFM__VCACHE_H__

/* This unit caches output of external viewers. */

#include <stddef.h> /* size_t */

#include "utils/test_helpers.h"
#include "filetype.h"
#include "macros.h"

/* Named boolean values of "sync" parameter of vcache_lookup() for better
 * readability. */
enum
{
	VC_ASYNC, /* Initiate viewing and return stub.  There will be a redraw after,
	             when viewer provides more data.. */
	VC_SYNC   /* Initiate viewing and wait until it's done producing requested
	             number of lines. */
};

/* Type of callback function to check if preview of specified path is visible.
 * Should return non-zero if so and zero otherwise. */
typedef int (*vcache_is_previewed_cb)(const char path[]);

struct strlist_t;

/* Kills all asynchronous viewers. */
void vcache_finish(void);

/* Retrieves size of the cache (lower bound).  Returns the size. */
size_t vcache_size(void);

/* Checks updates of asynchronous viewers.  Returns non-zero is screen needs to
 * be updated, otherwise zero is returned. */
int vcache_check(vcache_is_previewed_cb is_previewed);

/* Looks up cached output of a viewer command (no macro expansion is performed)
 * or produces and caches it.  *error is set either to NULL or an error code on
 * failure.  Returns list of strings owned and managed by the unit, don't store
 * or free it. */
struct strlist_t vcache_lookup(const char full_path[], const char viewer[],
		MacroFlags flags, ViewerKind kind, int max_lines, int sync,
		const char **error);

TSTATIC_DEFS(
	struct strlist_t read_lines(FILE *fp, int max_lines, int *complete);
	void vcache_reset(size_t max_size);
	size_t vcache_entry_size(void);
)

#endif /* VIFM__VCACHE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
