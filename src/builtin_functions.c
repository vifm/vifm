/* vifm
 * Copyright (C) 2012 xaizek.
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

#include <sys/types.h> /* mode_t */

#include <assert.h>
#include <string.h> /* strcmp() strdup() */

#include "engine/functions.h"
#include "utils/macros.h"
#include "utils/utils.h"
#include "ui.h"

#include "builtin_functions.h"

static var_t filetype_builtin(const call_info_t *call_info);
static int get_fnum(const char position[]);

static const function_t functions[] =
{
	{ "filetype", 1, &filetype_builtin },
};

void
init_builtin_functions(void)
{
	size_t i;
	for(i = 0; i < ARRAY_LEN(functions); i++)
	{
		int result = function_register(&functions[i]);
		assert(result == 0 && "Builtin function registration error");
	}
}

/* Returns string representation of file type. */
static var_t
filetype_builtin(const call_info_t *call_info)
{
	const int fnum = get_fnum(call_info->argv[0]);
	if(fnum == 0)
	{
		return strdup("");
	}
	else
	{
		mode_t mode = curr_view->dir_entry[fnum].mode;
		return strdup(get_mode_str(mode));
	}
}

/* Returns file type from position. */
static int
get_fnum(const char position[])
{
	if(strcmp(position, ".") == 0)
	{
		return curr_view->list_pos;
	}
	else
	{
		return 0;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
