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

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint64_t */
#include <stdlib.h> /* calloc() free() */

#include "private/ioc.h"
#include "private/ioeta.h"
#include "private/traverser.h"

static VisitResult eta_visitor(const char full_path[], VisitAction action,
		void *param);

ioeta_estim_t *
ioeta_alloc(void *param, io_cancellation_t cancellation)
{
	ioeta_estim_t *const estim = calloc(1U, sizeof(*estim));
	if(estim == NULL)
	{
		return NULL;
	}

	estim->param = param;
	estim->cancellation = cancellation;
	return estim;
}

void
ioeta_free(ioeta_estim_t *estim)
{
	if(estim != NULL)
	{
		ioeta_release(estim);
		free(estim);
	}
}

void
ioeta_calculate(ioeta_estim_t *estim, const char path[], int shallow)
{
	if(shallow)
	{
		ioeta_add_item(estim, path);
	}
	else
	{
		(void)traverse(path, &eta_visitor, estim);
	}
}

/* Implementation of traverse() visitor for subtree copying.  Returns 0 on
 * success, otherwise non-zero is returned. */
static VisitResult
eta_visitor(const char full_path[], VisitAction action, void *param)
{
	ioeta_estim_t *const estim = param;

	if(cancelled(&estim->cancellation))
	{
		return VR_CANCELLED;
	}

	switch(action)
	{
		case VA_DIR_ENTER:
			ioeta_add_dir(estim, full_path);
			return VR_SKIP_DIR_LEAVE;
		case VA_FILE:
			ioeta_add_file(estim, full_path);
			return VR_OK;
		case VA_DIR_LEAVE:
			assert(0 && "Can't get here because of VR_SKIP_DIR_LEAVE.");
			return VR_OK;
	}

	return VR_OK;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
