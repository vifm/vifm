/* vifm
 * Copyright (C) 2013 xaizek.
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

/* Small abstraction over filter driven by a regular expression. */

#ifndef VIFM__UTILS__FILTER_H__
#define VIFM__UTILS__FILTER_H__

#include <regex.h> /* regex_t */

/* Wrapper for a regular expression, its state and compiled form. */
typedef struct
{
	/* Raw regexp for filtering, not NULL after initialization. */
	char *raw;

	/* Whether raw regexp was successfully compiled. */
	int is_regex_valid;

	/* Compilation flags for the regular expression. */
	int cflags;

	/* The expression in compiled form when is_regex_valid != 0. */
	regex_t regex;
}
filter_t;

/* Initializes filter structure with valid values for an empty filter.  Returns
 * zero on success, otherwise non-zero is returned. */
int filter_init(filter_t *filter, int case_sensitive);

/* Frees all resources allocated by filter. */
void filter_dispose(filter_t *filter);

/* Checks whether filter is empty.  Returns non-zero if it is, otherwise zero is
 * returned. */
int filter_is_empty(const filter_t *filter);

/* Resets filter's state to the empty state. */
void filter_clear(filter_t *filter);

/* Sets filter to a given value.  Returns zero on success, otherwise non-zero is
 * returned. */
int filter_set(filter_t *filter, const char value[]);

/* Assigns *source to *filter.  Returns zero on success, otherwise non-zero is
 * returned. */
int filter_assign(filter_t *filter, const filter_t *source);

/* Sets filter and its case sensitivity to given values.  Case sensitivity is
 * updated even on error.  Returns zero on success, otherwise non-zero is
 * returned. */
int filter_change(filter_t *filter, const char value[], int case_sensitive);

/* Appends non-empty value to filter expression (using logical or and whole
 * pattern matching).  Returns zero on success, otherwise non-zero is
 * returned. */
int filter_append(filter_t *filter, const char value[]);

/* Checks whether pattern matches the filter.  Returns positive number on match,
 * zero on no match and negative number on empty or invalid regular expression
 * (wrong state of the filter). */
int filter_matches(const filter_t *filter, const char pattern[]);

#endif /* VIFM__UTILS__FILTER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
