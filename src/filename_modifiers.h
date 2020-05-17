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

#ifndef VIFM__FILENAME_MODIFIERS_H__
#define VIFM__FILENAME_MODIFIERS_H__

#include <stddef.h> /* size_t */

/* Applies all filename modifiers.  Returns pointer to statically allocated
 * buffer containing result. */
const char * mods_apply(const char path[], const char parent[],
		const char mod[], int for_shell);

/* Computes total length of all filename modifiers at the beginning of the
 * passed string.  Returns the length. */
size_t mods_length(const char str[]);

#endif /* VIFM__FILENAME_MODIFIERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
