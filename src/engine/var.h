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

#ifndef VIFM__ENGINE__VAR_H__
#define VIFM__ENGINE__VAR_H__

/* This unit provides all necessary to work with variables. */

/* Enumeration of variable types. */
typedef enum
{
	VTYPE_ERROR,  /* Signals about failure or uninitialized variable. */
	VTYPE_STRING, /* Regular string value. */
	VTYPE_INT,    /* Integer, which is also used for boolean. */
}
VarType;

/* Union of possible variable contents. */
typedef union
{
	char *string; /* String value for VTYPE_STRING, should be copied to use it. */
	int integer;  /* VTYPE_INT value. */
}
var_val_t;

/* Structure for script variable. */
typedef struct
{
	VarType type;    /* Variable type. */
	var_val_t value; /* Value depending on type. */
}
var_t;

/* Gets variable, which evaluates to true.  Returns the variable. */
var_t var_true(void);

/* Gets variable, which evaluates to false.  Returns the variable. */
var_t var_false(void);

/* Gets boolean variable for the boolean value.  Returns the variable. */
var_t var_from_bool(int bool_val);

/* Makes integer variable initialized with given integer.  Returns the
 * variable. */
var_t var_from_int(int int_val);

/* Makes string variable initialized with given string.  Returns the
 * variable. */
var_t var_from_str(const char str_val[]);

/* Returns variable, which signals about failed operation. */
var_t var_error(void);

/* Convenience function that clones a variable. */
var_t var_clone(var_t var);

/* Converts variable to a string.  Returns new string, which should be freed by
 * the caller. */
char * var_to_str(const var_t var);

/* Converts variable to an integer.  Returns integer value, parsing of a string
 * is performed to get integer value. */
int var_to_int(const var_t var);

/* Converts variable to a boolean.  Returns non-zero if value is evaluated to
 * true. */
int var_to_bool(const var_t var);

/* Frees resources allocated for the var if any. */
void var_free(const var_t var);

#endif /* VIFM__ENGINE__VAR_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
