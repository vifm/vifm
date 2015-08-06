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

#include "ioe.h"

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../compat/reallocarray.h"
#include "../ioe.h"

/* TODO: think about the interface, whether we need to paths and what message
 *       is really is (currently it might be strerror(error_code), but it's a
 *       bit excessive as we don't need error_code then. */
int
ioe_errlst_append(ioe_errlst_t *elist, const char path[], int error_code,
		const char msg[])
{
	ioe_err_t err;
	void *p;

	if(!elist->active)
	{
		return 0;
	}

	p = reallocarray(elist->errors, elist->error_count + 1U,
			sizeof(*elist->errors));
	if(p == NULL)
	{
		return 1;
	}
	elist->errors = p;

	err.path = strdup(path);
	err.error_code = error_code;
	err.msg = strdup(msg);

	if(err.path == NULL || err.msg == NULL)
	{
		ioe_err_free(&err);
		return 1;
	}

	elist->errors[elist->error_count++] = err;

	return 0;
}

void
ioe_err_free(ioe_err_t *err)
{
	free(err->path);
	free(err->msg);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
