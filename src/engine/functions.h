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

#ifndef VIFM__ENGINE__FUNCTIONS_H__
#define VIFM__ENGINE__FUNCTIONS_H__

#include <stddef.h> /* size_t */

#include "var.h"

/* Structure, which describes details about function call. */
typedef struct
{
	size_t argc; /* Number of arguments passed to the function. */
	var_t *argv; /* Arguments passed to the function. */
}call_info_t;

/* A function prototype for builtin function implementation. */
typedef var_t (*function_ptr_t)(const call_info_t *call_info);

/* Function description. */
typedef struct
{
	const char *name; /* Name of a function. */
	size_t arg_count; /* Required number of arguments. */
	function_ptr_t ptr; /* Pointer to function implementation. */
}function_t;

/* Registers new function.  Returns non-zero on error. */
int function_register(const function_t *func_info);

/* Calls function.  Returns its result or variable of type VTYPE_ERROR in case
 * of error. */
var_t function_call(const char func_name[], const call_info_t *call_info);


/* Initializes function call information structure. */
void function_call_info_init(call_info_t *call_info);

/* Adds new argument to call information structure.  The var isn't cloned. */
void function_call_info_add_arg(call_info_t *call_info, var_t var);

/* Frees resources allocated for the call_info structure. */
void function_call_info_free(call_info_t *call_info);

#endif /* VIFM__ENGINE__FUNCTIONS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
