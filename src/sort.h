/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#ifndef VIFM__SORT_H__
#define VIFM__SORT_H__

#include "ui/ui.h"
#include "utils/test_helpers.h"

/* Sorts entries of the view according to its sorting configuration. */
void sort_view(view_t *view);

/* Sorts specified entries using global settings of the view. */
void sort_entries(view_t *view, entries_t entries);

/* Maps primary sort key to second column type.  Returns secondary key that
 * corresponds to the primary one. */
SortingKey get_secondary_key(SortingKey primary_key);

TSTATIC_DEFS(
	int strnumcmp(const char s[], const char t[]);
)

#endif /* VIFM__SORT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
