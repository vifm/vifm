/* vifm
 * Copyright (C) 2014 xaizek.
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

#ifndef VIFM__ENGINE__MODE_H__
#define VIFM__ENGINE__MODE_H__

/* The concept:
 *
 * 1. Two kinds of modes: primary and secondary; defined by their interoperation
 *    in UI;
 *
 * 2. While secondary mode is active primary is still visible, although it does
 *    not get any input from a user.
 *
 * 3. Secondary mode is also the current one.  There might be two generic
 *    situations:
 *
 *    - no secondary mode, when vle_mode_get() == vle_mode_get_primary();
 *    - secondary mode present, which makes "current" and "primary" modes be
 *      different. */

/* Type of mode. */
typedef enum
{
	VMT_PRIMARY,   /* Host (main) mode (e.g. normal, visual). */
	VMT_SECONDARY, /* Temporary (auxiliary) mode (e.g. command-line, dialog). */
}
VleModeType;

/* Mode identifier.  It's of integer type. */
typedef int vle_mode_t;

/* Gets identifier of currently active mode.  Returns the id. */
vle_mode_t vle_mode_get(void);

/* Checks that currently active mode is the mode.  Returns non-zero if so,
 * otherwise zero is returned. */
int vle_mode_is(vle_mode_t mode);

/* Gets identifier of currently active primary mode.  Returns the id. */
vle_mode_t vle_mode_get_primary(void);

/* Checks that primary mode is the mode.  Returns non-zero if so, otherwise zero
 * is returned. */
int vle_primary_mode_is(vle_mode_t mode);

/* Sets current mode of the specified type. */
void vle_mode_set(vle_mode_t mode, VleModeType type);

#endif /* VIFM__ENGINE__MODE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
