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

#ifndef VIFM__BRACKET_NOTATION_H__
#define VIFM__BRACKET_NOTATION_H__

#include <stddef.h> /* wchar_t */

/* Initializes internal notation description structures. */
void init_bracket_notation(void);

/* Substitutes all bracket notation entries in cmd passed in as narrow string.
 * Returns newly allocated wide string, which should be freed by the caller. */
wchar_t * substitute_specs(const char cmd[]);

/* Substitutes all bracket notation entries in cmd passed in as wide string.
 * Returns newly allocated wide string, which should be freed by the caller. */
wchar_t * substitute_specsw(const wchar_t cmd[]);

/* Converts sequence of keys into user-friendly printable string.  Spaces are
 * left as is, without converting them into <space>.  Returns newly allocated
 * string. */
char * wstr_to_spec(const wchar_t str[]);

#endif /* VIFM__BRACKET_NOTATION_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
