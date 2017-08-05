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

#ifndef VIFM__MENUS__CABBREVS_MENU_H__
#define VIFM__MENUS__CABBREVS_MENU_H__

#include <stddef.h> /* wchar_t */

struct view_t;

/* Displays list of command-line mode abbreviations.  Returns non-zero if status
 * bar message should be saved. */
int show_cabbrevs_menu(struct view_t *view);

/* Describes an abbreviation for printing it out.  The offset parameter
 * specifies additional left-side displacement.  Returns newly allocated
 * string. */
char * describe_abbrev(const wchar_t lhs[], const wchar_t rhs[], int no_remap,
		int offset);

#endif /* VIFM__MENUS__CABBREVS_MENU_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
