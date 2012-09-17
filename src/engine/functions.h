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

#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <stddef.h> /* size_t */

typedef char *var_t;

typedef struct
{
	size_t argc;
	var_t *argv;
}call_info_t;

typedef var_t (*function_ptr_t)(const call_info_t *call_info);

typedef struct
{
	char *name;
	size_t arg_count;
}function_t;

void function_register(const function_t func_info);
int function_registered(const char func_name[]);
var_t function_call(const char func_name[], const call_info_t *call_info);

void function_call_info_init(call_info_t *call_info);
void function_call_info_add_arg(call_info_t *call_info, const char arg[]);
void function_call_info_free(call_info_t *call_info);

#endif /* __FUNCTIONS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
