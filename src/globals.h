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

#ifndef VIFM__GLOBALS_H__
#define VIFM__GLOBALS_H__

#include <regex.h> /* regex_t */

/* Implements globals by converting them into regular expressions.  They are
 * treated as case insensitive. */

/* Checks whether file name matches comma-separated list of globals.  Returns
 * non-zero if so, otherwise zero is returned. */
int global_matches(const char globals[], const char file[]);

/* Compiles global into regular expression.  Returns zero on success, on error
 * result of regcomp() is returned, which is non-zero. */
int global_compile_as_re(const char global[], regex_t *re);

#endif /* VIFM__GLOBALS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
