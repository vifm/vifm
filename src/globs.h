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

#ifndef VIFM__GLOBS_H__
#define VIFM__GLOBS_H__

/* Implements globs by converting them into regular expressions.  They are
 * treated as case insensitive. */

/* Converts comma-separated list of globs into equivalent regular expression.
 * Returns pointer to a newly allocated string, which should be freed by the
 * caller, or NULL if there is not enough memory or no patters are given. */
char * globs_to_regex(const char globs[]);

#endif /* VIFM__GLOBS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
