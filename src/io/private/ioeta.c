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

#include "ioeta.h"

#include <stddef.h> /* NULL */
#include <stdint.h> /* uint64_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../utils/fs.h"
#include "../../utils/str.h"
#include "../ioeta.h"
#include "ionotif.h"

void
ioeta_release(ioeta_estim_t *estim)
{
	free(estim->item);
	free(estim->target);
}

void
ioeta_add_item(ioeta_estim_t *estim, const char path[])
{
	++estim->total_items;

	replace_string(&estim->item, path);

	ionotif_notify(IO_PS_ESTIMATING, estim);
}

void
ioeta_add_file(ioeta_estim_t *estim, const char path[])
{
	if(!is_symlink(path))
	{
		estim->total_bytes += get_file_size(path);
	}

	ioeta_add_item(estim, path);
}

void
ioeta_add_dir(ioeta_estim_t *estim, const char path[])
{
	/* TODO: think about counting directories, otherwise copying of large amount
	 *       of empty directories or hierarchy of empty directories produces weird
	 *       progress reports and it even might be the reason of getting more than
	 *       100% progress. */

	replace_string(&estim->item, path);

	ionotif_notify(IO_PS_ESTIMATING, estim);
}

void
ioeta_update(ioeta_estim_t *estim, const char path[], const char target[],
		int finished, uint64_t bytes)
{
	if(estim == NULL || estim->silent)
	{
		return;
	}

	estim->current_byte += bytes;
	estim->current_file_byte += bytes;
	if(estim->current_byte > estim->total_bytes)
	{
		/* Estimations are out of date, update them. */
		estim->total_bytes = estim->current_byte;
	}

	if(finished)
	{
		++estim->current_item;
		if(estim->current_item > estim->total_items)
		{
			/* Estimations are out of date, update them. */
			estim->total_items = estim->current_item;
		}
		estim->current_file_byte = 0U;
		estim->total_file_bytes = 0U;
	}
	else if(estim->inspected_items != estim->current_item + 1)
	{
		estim->inspected_items = estim->current_item + 1;
		estim->total_file_bytes = get_file_size(path);
	}

	if(path != NULL)
	{
		replace_string(&estim->item, path);
	}

	if(target != NULL)
	{
		replace_string(&estim->target, target);
	}

	ionotif_notify(IO_PS_IN_PROGRESS, estim);
}

int
ioeta_silent_on(ioeta_estim_t *estim)
{
	int silent;

	if(estim == NULL)
	{
		return 0;
	}

	silent = estim->silent;
	estim->silent = 1;
	return silent;
}

void
ioeta_silent_set(ioeta_estim_t *estim, int silent)
{
	if(estim != NULL)
	{
		estim->silent = silent;
	}
}

ioeta_estim_t
ioeta_save(const ioeta_estim_t *estim)
{
	ioeta_estim_t copy = *estim;
	copy.item = (copy.item == NULL ? NULL : strdup(copy.item));
	copy.target = (copy.target == NULL ? NULL : strdup(copy.target));

	return copy;
}

void
ioeta_restore(ioeta_estim_t *estim, const ioeta_estim_t *save)
{
	char *item = estim->item;
	char *target = estim->target;

	if(estim->silent)
	{
		return;
	}

	update_string(&item, save->item);
	update_string(&target, save->target);

	*estim = *save;
	estim->item = item;
	estim->target = target;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
