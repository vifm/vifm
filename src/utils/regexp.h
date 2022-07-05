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

#ifndef VIFM__UTILS__REGEXP_H__
#define VIFM__UTILS__REGEXP_H__

/* Regular expressions related utilities. */

#include <regex.h> /* regex_t regmatch_t */

/* Gets flags for compiling a regular expression specified by the pattern taking
 * 'ignorecase' and 'smartcase' options into account.  Returns regex flags. */
int get_regexp_cflags(const char pattern[]);

/* Decides whether case should be ignored for the pattern.  Considers
 * 'ignorecase' and 'smartcase' options.  Returns non-zero when case should be
 * ignored, otherwise zero is returned. */
int regexp_should_ignore_case(const char pattern[]);

/* Wrapper around regcomp() that handles \c and \C sequences. */
int regexp_compile(regex_t *re, const char pattern[], int cflags);

/* Turns error code into error message.  Returns pointer to a statically
 * allocated buffer. */
const char * get_regexp_error(int err, const regex_t *re);

/* Parses case flag of the regular expression.  *case_sensitive should be
 * initialized with default value outside the call.  Returns zero on success,
 * otherwise non-zero is returned. */
int parse_case_flag(const char flags[], int *case_sensitive);

/* Extracts first group match.  Returns the match, on error or missing first
 * group both start and end fields are set to zero. */
regmatch_t get_group_match(const regex_t *re, const char str[]);

/* Compiles regular expression and performs substitution in a line.  Returns
 * pointer to a statically allocated buffer, which on failure contains value of
 * the line parameter. */
const char * regexp_replace(const char line[], const char pattern[],
		const char sub[], int glob, int ignore_case);

/* Performs substitution of all regexp matches in the string.  Returns pointer
 * to a statically allocated buffer. */
const char * regexp_gsubst(regex_t *re, const char src[], const char sub[],
		regmatch_t matches[]);

/* Performs substitution of single regexp matche in the string.  off can be
 * NULL.  Returns pointer to a statically allocated buffer. */
const char * regexp_subst(const char src[], const char sub[],
		const regmatch_t matches[], int *off);

#endif /* VIFM__UTILS__REGEXP_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
