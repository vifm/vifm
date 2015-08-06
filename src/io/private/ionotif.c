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

#include "ionotif.h"

#include <stddef.h> /* NULL */

#include "../ioeta.h"
#include "../ionotif.h"

static ionotif_progress_changed progress_changed;

void
ionotif_register(ionotif_progress_changed handler)
{
	progress_changed = handler;
}

void
ionotif_notify(IoPs stage, ioeta_estim_t *estim)
{
	if(progress_changed != NULL)
	{
		const io_progress_t progress_info = { .stage = stage, .estim = estim };
		progress_changed(&progress_info);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
