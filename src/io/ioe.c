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

#include "private/ioe.h"

void
ioe_errlst_init(ioe_errlst_t *elist)
{
	static const ioe_errlst_t initializer = IOE_ERRLST_INIT;
	*elist = initializer;
}

void
ioe_errlst_free(ioe_errlst_t *elist)
{
	size_t i;

	if(elist == NULL)
	{
		return;
	}

	for(i = 0U; i < elist->error_count; ++i)
	{
		ioe_err_free(&elist->errors[i]);
	}
	free(elist->errors);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
