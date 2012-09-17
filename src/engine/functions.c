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

#include <string.h> /* memset() */

#include "../utils/string_array.h"
#include "../ui.h"

#include "functions.h"

var_t
function_call(const char func_name[], const call_info_t *call_info)
{
	int i;
	char buf[4096];

	sprintf(buf, "%s(", func_name);
	for(i = 0; i < call_info->argc; i++)
	{
		sprintf(buf + strlen(buf), "%s%s", call_info->argv[i],
				(i + 1 < call_info->argc) ? ", " : "");
	}
	strcat(buf, ")");
	return strdup(buf);
}

void
function_call_info_init(call_info_t *call_info)
{
	memset(call_info, 0, sizeof(*call_info));
}

void
function_call_info_add_arg(call_info_t *call_info, const char arg[])
{
	call_info->argc = add_to_string_array(&call_info->argv, call_info->argc, 1,
			arg);
}

void
function_call_info_free(call_info_t *call_info)
{
	free_string_array(call_info->argv, call_info->argc);
	call_info->argv = NULL;
	call_info->argc = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
