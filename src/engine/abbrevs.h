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

int vle_abbr_add(const char lhs[], const char rhs[]);

int vle_abbr_add_no_remap(const char lhs[], const char rhs[]);

/* str is matched with lhs and, if none found, with rhs. */
int vle_abbr_remove(const char str[]);

/* Returns NULL is str doesn't match any lhs, otherwise pointer to string
 * managed by the unit is returned. */
const char * vle_abbr_expand(const char str[], int *no_remap);

/* Removes all registered abbreviations. */
void vle_abbr_reset(void);

#endif /* VIFM__ENGINE__ABBREVS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
