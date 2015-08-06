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

#include "mode.h"

static vle_mode_t current_mode;
static vle_mode_t primary_mode;

vle_mode_t
vle_mode_get(void)
{
	return current_mode;
}

int
vle_mode_is(vle_mode_t mode)
{
	return current_mode == mode;
}

vle_mode_t
vle_mode_get_primary(void)
{
	return primary_mode;
}

int
vle_primary_mode_is(vle_mode_t mode)
{
	return primary_mode == mode;
}

void
vle_mode_set(vle_mode_t mode, VleModeType type)
{
	current_mode = mode;
	if(type == VMT_PRIMARY)
	{
		primary_mode = mode;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
