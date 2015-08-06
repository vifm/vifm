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

/* Small abstraction over matcher driven by a regular expression that accepts
 * both globs and regular expressions as patterns. */

#ifndef VIFM__UTILS__MATCHER_H__
#define VIFM__UTILS__MATCHER_H__

/* File path/name matcher (glob/regexp). */

/* Opaque matcher type. */
typedef struct matcher_t matcher_t;

/* Parses matcher expression and allocates matcher.  Returns matcher on success,
 * otherwise NULL is returned and *error is initialized with newly allocated
 * string describing the error. */
matcher_t * matcher_alloc(const char expr[], int cs_by_def, int glob_by_def,
		char **error);

/* Makes a copy of existing matcher.  Returns the clone, or NULL on error.  */
matcher_t * matcher_clone(const matcher_t *matcher);

/* Frees all resources allocated by the matcher.  matcher can be NULL. */
void matcher_free(matcher_t *matcher);

/* Checks whether given path/name matches.  Returns non-zero if so, otherwise
 * zero is returned. */
int matcher_matches(matcher_t *matcher, const char path[]);

/* Gets original matcher expression.  Returns the expression.*/
const char * matcher_get_expr(const matcher_t *matcher);

/* Checks whether everything matched by the m is also matched by the like.
 * Returns non-zero if so, otherwise zero is returned. */
int matcher_includes(const matcher_t *like, const matcher_t *m);

/* Checks whether given string is a match expression.  Returns non-zero if so,
 * otherwise zero is returned. */
int matcher_is_expr(const char str[]);

#endif /* VIFM__UTILS__MATCHER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
