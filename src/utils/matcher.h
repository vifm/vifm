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

/* Small abstraction for matching files by globs, regular expressions or mime
 * types. */

#ifndef VIFM__UTILS__MATCHER_H__
#define VIFM__UTILS__MATCHER_H__

#include "test_helpers.h"

/* File path/name matcher (glob/regexp/mime-type). */

/* Opaque matcher type. */
typedef struct matcher_t matcher_t;

/* Parses matcher expression and allocates matcher.  on_empty_re string is used
 * if passed in regexp is empty.  Returns matcher on success and sets *error to
 * NULL, otherwise NULL is returned and *error is initialized with newly
 * allocated string describing the error. */
matcher_t * matcher_alloc(const char expr[], int cs_by_def, int glob_by_def,
		const char on_empty_re[], char **error);

/* Makes a copy of existing matcher.  Returns the clone, or NULL on error. */
matcher_t * matcher_clone(const matcher_t *matcher);

/* Frees all resources allocated by the matcher.  matcher can be NULL. */
void matcher_free(matcher_t *matcher);

/* Checks whether given path/name matches.  Returns non-zero if so, otherwise
 * zero is returned. */
int matcher_matches(const matcher_t *matcher, const char path[]);

/* Checks whether matcher is empty.  Returns non-zero if so, otherwise zero is
 * returned. */
int matcher_is_empty(const matcher_t *matcher);

/* Retrieves matcher expression exactly as it was specified on creation.
 * Returns the expression. */
const char * matcher_get_expr(const matcher_t *matcher);

/* Retrieves undecorated original matcher expression.  Returns the
 * expression. */
const char * matcher_get_undec(const matcher_t *matcher);

/* Checks whether matcher matches at least superset of what like is matching.
 * Returns non-zero if so, otherwise zero is returned. */
int matcher_includes(const matcher_t *matcher, const matcher_t *like);

/* Checks whether given matcher is a full path matcher.  Returns non-zero if so,
 * otherwise zero is returned. */
int matcher_is_full_path(const matcher_t *matcher);

TSTATIC_DEFS(
	int matcher_is_fast(const matcher_t *matcher);
)

#endif /* VIFM__UTILS__MATCHER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
