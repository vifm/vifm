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

/* This module handles :let command */

/* List of valid first characters in the name of an environment variable. */
extern const char ENV_VAR_NAME_FIRST_CHAR[];

/* List of valid non-first characters in the name of an environment variable. */
extern const char ENV_VAR_NAME_CHARS[];

/* Initializes variables module.  Should be called before any of other
 * functions.
 * handler can be NULL
 */
void init_variables(void);

/* Gets cached value of environment variable envname.  Returns empty string if
 * requested variable doesn't exist. */
const char * local_getenv(const char *envname);

/* Removes all defined variables and resets environment variables to their
 * initial values
 */
void clear_variables(void);

/* Processes :let command arguments.  Returns non-zero on error, otherwise zero
 * is returned. */
int let_variables(const char *cmd);

/* Processes :unlet command arguments
 * Returns non-zero on error
 */
int unlet_variables(const char *cmd);

/* Performs :let command completion */
void complete_variables(const char *cmd, const char **start);

#endif /* VIFM__ENGINE__VARIABLES_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
