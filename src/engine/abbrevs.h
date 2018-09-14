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

#ifndef VIFM__ENGINE__ABBREVS_H__
#define VIFM__ENGINE__ABBREVS_H__

#include <stddef.h> /* wchar_t */

/* Registers abbreviation from LHS to RHS.  Overwrites any previously existing
 * abbreviations for LHS.  Returns zero on success or non-zero on memory
 * allocation error. */
int vle_abbr_add(const wchar_t lhs[], const wchar_t rhs[]);

/* Same as vle_abbr_add(), but registers noremap kind of abbreviation. */
int vle_abbr_add_no_remap(const wchar_t lhs[], const wchar_t rhs[]);

/* Removes abbreviation by first matching matching str with LHS and, if no
 * matches found, with RHS.  Returns zero on successful removal, otherwise
 * (no abbreviation found) non-zero is returned. */
int vle_abbr_remove(const wchar_t str[]);

/* Expands str if it's a full match for LHS of any of previously registered
 * abbreviations.  Returns NULL is str doesn't match any LHS, otherwise pointer
 * to string managed by the unit is returned. */
const wchar_t * vle_abbr_expand(const wchar_t str[], int *no_remap);

/* Removes all registered abbreviations. */
void vle_abbr_reset(void);

/* Completes names of abbreviations. */
void vle_abbr_complete(const char prefix[]);

/* Enumerates registered abbreviations.  For the first call *state must be set
 * to NULL.  Returns zero when end of the list is reached (and sets pointers to
 * NULLs), otherwise zero is returned.  Do not modify abbreviations between
 * calls. */
int vle_abbr_iter(const wchar_t **lhs, const wchar_t **rhs, int *no_remap,
		void **param);

#endif /* VIFM__ENGINE__ABBREVS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
