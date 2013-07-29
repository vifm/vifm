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

#include <stddef.h> /* size_t */
#include <stdlib.h> /* realloc() */
#include <string.h> /* memset() strcmp() */

#include "../utils/string_array.h"
#include "../ui.h"
#include "text_buffer.h"
#include "var.h"

static int function_registered(const char func_name[]);
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

/* Checks whether function with specified name exists or not.  Returns non-zero
 * if function with specified name is already registered. */
static int
function_registered(const char func_name[])
{
	return find_function(func_name) != NULL;
}

/* Adds function to the list of functions.  Returns non-zero if there was not
 * enough memory for that. */
static int
add_function(const function_t *func_info)
{
	function_t *ptr = realloc(functions, sizeof(function_t)*(function_count + 1));
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
		text_buffer_addf("%s: %s", "Unknown function", func_name);
		return var_error();
	}
	if(call_info->argc < function->arg_count)
	{
		text_buffer_addf("%s: %s", "Not enough arguments for function", func_name);
		return var_error();
	}
	if(call_info->argc > function->arg_count)
	{
		text_buffer_addf("%s: %s", "Too many arguments for function", func_name);
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
function_call_info_init(call_info_t *call_info)
{
	memset(call_info, 0, sizeof(*call_info));
}

void
function_call_info_add_arg(call_info_t *call_info, var_t var)
{
	var_t *ptr = realloc(call_info->argv, sizeof(var_t)*(call_info->argc + 1));
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
/* vim: set cinoptions+=t0 : */
