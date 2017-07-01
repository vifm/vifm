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

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../compat/reallocarray.h"
#include "../ioe.h"

/* XXX: error_code can't be used on Windows, so the message should be in msg.
 *      It might be better to have Windows GetLastError() result too. */
int
ioe_errlst_append(ioe_errlst_t *elist, const char path[], int error_code,
		const char msg[])
{
	ioe_err_t err;
	void *p;

	assert((error_code != IO_ERR_UNKNOWN || msg[0] != '\n') &&
			"Some error information has to be provided!");

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
ioe_errlst_splice(ioe_errlst_t *elist, ioe_errlst_t *other)
{
	void *p;

	if(other->error_count == 0U)
	{
		return;
	}

	p = reallocarray(elist->errors, elist->error_count + other->error_count,
			sizeof(*elist->errors));
	if(p == NULL)
	{
		return;
	}
	elist->errors = p;

	memcpy(&elist->errors[elist->error_count], &other->errors[0],
			sizeof(*elist->errors)*other->error_count);

	elist->error_count += other->error_count;
	other->error_count = 0U;
}

void
ioe_err_free(ioe_err_t *err)
{
	free(err->path);
	free(err->msg);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
