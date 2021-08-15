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

#ifndef VIFM__UI__COLOR_MANAGER_H__
#define VIFM__UI__COLOR_MANAGER_H__

/* Initialization data for colmgr_init(). */
typedef struct
{
	/* Maximum number of color pairs. */
	int max_color_pairs;

	/* Maximum number of colors. */
	int max_colors;

	/* Function to set value of a color pair.  Should return zero on success and
	 * anything else otherwise. */
	int (*init_pair)(int pair, int fg, int bg);

	/* Function to get value of a color pair.  Should return zero on success and
	 * anything else otherwise. */
	int (*pair_content)(int pair, int *fg, int *bg);

	/* Checks whether pair is being used at the moment.  Should return non-zero if
	 * so and zero otherwise. */
	int (*pair_in_use)(int pair);

	/* Substitutes old pair number with the new one. */
	void (*move_pair)(int from, int to);
}
colmgr_conf_t;

/* Initializes color manager unit.  Creates and prepares internal variables. */
void colmgr_init(const colmgr_conf_t *conf_init);

/* Resets all color pairs that are available for dynamic allocation. */
void colmgr_reset(void);

/* Gets (might dynamically allocate) color pair number for specified
 * foreground (fg) and background (bg) colors.  Returns the number.  On failure
 * falls back to color pair 0. */
int colmgr_get_pair(int fg, int bg);

#endif /* VIFM__UI__COLOR_MANAGER_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
