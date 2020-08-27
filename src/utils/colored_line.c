/* vifm
 * Copyright (C) 2020 xaizek.
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

#include "colored_line.h"

#include <stddef.h> /* size_t */
#include <stdlib.h> /* free() */

#include "str.h"
#include "utf8.h"

int
cline_sync(cline_t *cline, int extra_width)
{
	const size_t nchars = utf8_strsw(cline->line) + extra_width;
	if(cline->attrs_len < nchars)
	{
		int width = nchars - cline->attrs_len;
		char *const new_attrs = format_str("%s%*s", cline->attrs, width, "");
		free(cline->attrs);
		cline->attrs = new_attrs;
		cline->attrs_len = nchars;
	}
	return (cline->attrs_len > nchars);
}

void
cline_finish(cline_t *cline)
{
	if(cline_sync(cline, 0))
	{
		cline->attrs[--cline->attrs_len] = '\0';
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
