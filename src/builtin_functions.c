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

#include "builtin_functions.h"

#include <sys/types.h> /* mode_t */

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() strdup() */

#include "engine/functions.h"
#include "engine/var.h"
#include "utils/macros.h"
#include "utils/utils.h"
#include "macros.h"
#include "ui.h"

static var_t expand_builtin(const call_info_t *call_info);
static var_t filetype_builtin(const call_info_t *call_info);
static int get_fnum(const char position[]);

static const function_t functions[] =
{
	{ "expand",   1, &expand_builtin },
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

/* Returns string after expanding expression. */
static var_t
expand_builtin(const call_info_t *call_info)
{
	var_t result;
	var_val_t var_val;
	char *str_val;

	str_val = var_to_string(call_info->argv[0]);
	var_val.string = expand_macros(str_val, NULL, NULL, 0);
	free(str_val);

	result = var_new(VTYPE_STRING, var_val);
	free(var_val.string);

	return result;
}

/* Returns string representation of file type. */
static var_t
filetype_builtin(const call_info_t *call_info)
{
	char *str_val = var_to_string(call_info->argv[0]);
	const int fnum = get_fnum(str_val);
	var_val_t var_val = { .string = "" };
  free(str_val);

	if(fnum >= 0)
	{
#ifndef _WIN32
		const mode_t mode = curr_view->dir_entry[fnum].mode;
		var_val.string = (char *)get_mode_str(mode);
#else
		const FileType type = curr_view->dir_entry[fnum].type;
		var_val.string = (char *)get_type_str(type);
#endif
	}
	return var_new(VTYPE_STRING, var_val);
}

/* Returns file type from position or -1 if the position has wrong value. */
static int
get_fnum(const char position[])
{
	if(strcmp(position, ".") == 0)
	{
		return curr_view->list_pos;
	}
	else
	{
		return -1;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
