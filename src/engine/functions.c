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

#include "functions.h"

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* memset() strcmp() strlen() */

#include "../compat/reallocarray.h"
#include "../utils/str.h"
#include "completion.h"
#include "text_buffer.h"
#include "var.h"

static int add_function(const function_t *func_info);
static function_t * find_function(const char func_name[]);

/* Number of registered functions. */
static size_t function_count;
/* List of registered functions. */
static function_t *functions;

int
function_register(const function_t *func_info)
{
	if(function_registered(func_info->name))
	{
		return 1;
	}
	if(add_function(func_info) != 0)
	{
		return 1;
	}
	return 0;
}

int
function_registered(const char func_name[])
{
	return find_function(func_name) != NULL;
}

/* Adds function to the list of functions.  Returns non-zero on error. */
static int
add_function(const function_t *func_info)
{
	function_t *ptr;

	if(func_info->args.min > func_info->args.max)
	{
		return 1;
	}

	ptr = reallocarray(functions, function_count + 1, sizeof(function_t));
	if(ptr == NULL)
	{
		return 1;
	}

	functions = ptr;
	functions[function_count++] = *func_info;
	return 0;
}

var_t
function_call(const char func_name[], const call_info_t *call_info)
{
	function_t *function = find_function(func_name);
	if(function == NULL)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Unknown function", func_name);
		return var_error();
	}
	if(call_info->argc < function->args.min)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Not enough arguments for function",
				func_name);
		return var_error();
	}
	if(call_info->argc > function->args.max)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Too many arguments for function",
				func_name);
		return var_error();
	}
	return function->ptr(call_info);
}

/* Searches for function with specified name.  Returns NULL in case of failed
 * search. */
static function_t *
find_function(const char func_name[])
{
	size_t i;
	for(i = 0; i < function_count; i++)
	{
		if(strcmp(functions[i].name, func_name) == 0)
		{
			return &functions[i];
		}
	}
	return NULL;
}

void
function_reset_all(void)
{
	free(functions);
	functions = NULL;
	function_count = 0U;
}

void
function_complete_name(const char str[], const char **start)
{
	size_t i;
	size_t len;

	*start = str;

	len = strlen(str);
	for(i = 0U; i < function_count; ++i)
	{
		const function_t *const func = &functions[i];
		if(starts_withn(func->name, str, len))
		{
			vle_compl_put_match(format_str("%s(", func->name), func->descr);
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

void
function_call_info_init(call_info_t *call_info, int interactive)
{
	memset(call_info, 0, sizeof(*call_info));
	call_info->interactive = interactive;
}

void
function_call_info_add_arg(call_info_t *call_info, var_t var)
{
	var_t *ptr;
	ptr = reallocarray(call_info->argv, call_info->argc + 1, sizeof(var_t));
	if(ptr == NULL)
	{
		return;
	}

	ptr[call_info->argc++] = var;
	call_info->argv = ptr;
}

void
function_call_info_free(call_info_t *call_info)
{
	size_t i;
	for(i = 0; i < call_info->argc; i++)
	{
		var_free(call_info->argv[i]);
	}
	free(call_info->argv);

	call_info->argv = NULL;
	call_info->argc = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
