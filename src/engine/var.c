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

#include <assert.h> /* assert() */
#include <stddef.h> /* size_t */
#include <stdio.h> /* sprintf() snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "var.h"

var_t
var_false(void)
{
	static const var_t false_var = { VTYPE_INT, { .integer = 0 } };
	return false_var;
}

var_t
var_error(void)
{
	static const var_t fail_var = { VTYPE_ERROR };
	return fail_var;
}

var_t
var_new(VarType type, const var_val_t value)
{
	var_t new_var = { type, value };
	if(type == VTYPE_STRING)
	{
		new_var.value.string = strdup(value.string);
	}
	return new_var;
}

var_t
var_clone(var_t var)
{
	return var_new(var.type, var.value);
}

char *
var_to_string(const var_t var)
{
	switch(var.type)
	{
		case VTYPE_STRING:
			return strdup(var.value.string);
		case VTYPE_INT:
			{
				size_t len = snprintf(NULL, 0, "%d", var.value.integer);
				char *str = malloc(len + 1);
				(void)sprintf(str, "%d", var.value.integer);
				return str;
			}

		default:
			assert(0 && "Var -> String function: unhandled variable type");
	}
}

int
var_to_boolean(const var_t var)
{
	switch(var.type)
	{
		case VTYPE_STRING:
			return var.value.string[0] != '\0';
		case VTYPE_INT:
			return var.value.integer != 0;

		default:
			assert(0 && "Var -> Boolean function: unhandled variable type");
	}
}

void
var_free(const var_t var)
{
	if(var.type == VTYPE_STRING)
	{
		free(var.value.string);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
