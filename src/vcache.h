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

#include "utils/test_helpers.h"
#include "filetype.h"

struct strlist_t;

/* Looks up cached output of a viewer command (no macro expansion is performed)
 * or produces and caches it.  *error is set either to NULL or an error code on
 * failure.  Returns list of strings owned and managed by the unit, don't store
 * or free it. */
struct strlist_t vcache_lookup(const char full_path[], const char viewer[],
		ViewerKind kind, int max_lines, const char **error);

TSTATIC_DEFS(
	struct strlist_t read_lines(FILE *fp, int max_lines, int *complete);
	void vcache_reset(int max_size);
)

#endif /* VIFM__VCACHE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
