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

#ifndef VIFM__COLOR_MANAGER_H__
#define VIFM__COLOR_MANAGER_H__

/* Initializes color manager unit.  Creates and prepares internal variables. */
void colmgr_init(int max_color_pairs);
/* Resets all color pairs that are available for dynamic allocation. */
void colmgr_reset(void);
/* Dynamically allocates color pair of specified foreground (fg) and background
 * (bg) colors.  Returns -1 on allocation failure. */
int colmgr_alloc_pair(int fg, int bg);

#endif /* VIFM__COLOR_MANAGER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
