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

#ifndef VIFM__ENGINE__VARIABLES_H__
#define VIFM__ENGINE__VARIABLES_H__

#include "var.h"

/* This module handles :let command */

/* List of valid first characters in the name of an environment variable. */
extern const char ENV_VAR_NAME_FIRST_CHAR[];

/* List of valid non-first characters in the name of an environment variable. */
extern const char ENV_VAR_NAME_CHARS[];

/* Initializes variables module.  Should be called before any other function.
 * Builtin variables are not reinitialized. */
void init_variables(void);

/* Gets cached value of environment variable envname.  Returns empty string if
 * requested variable doesn't exist. */
const char * local_getenv(const char envname[]);

/* Gets variables value by its name.  Returns the value (not a copy), which is
 * var_error() in case requested variable doesn't exist. */
var_t getvar(const char varname[]);

/* Sets/creates builtin variables.  Variable is cloned.  Returns non-zero on
 * error, otherwise zero is returned. */
int setvar(const char varname[], var_t var);

/* Removes all variables and resets environment variables to their initial
 * values.  Doesn't remove builtin variables. */
void clear_variables(void);

/* Removes all defined environment variables and resets environment variables to
 * their initial values. */
void clear_envvars(void);

/* Processes :let command arguments.  Returns non-zero on error, otherwise zero
 * is returned. */
int let_variables(const char cmd[]);

/* Processes :unlet command arguments.  Returns non-zero on error, otherwise
 * zero is returned. */
int unlet_variables(const char cmd[]);

/* Performs :let command completion.  var should point to beginning of a
 * variable's name.  *start is set to completion insertion position in var. */
void complete_variables(const char var[], const char **start);

#endif /* VIFM__ENGINE__VARIABLES_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
