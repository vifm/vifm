/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
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

#ifndef VIFM__UTILS__ENV_H__
#define VIFM__UTILS__ENV_H__

/* Environment variables related functions. */

/* Lists names of existing environment variables.  Returns the array of the
 * length *count (always initialized). */
char ** env_list(int *count);

/* Returns environment variable value or NULL if it doesn't exist. */
const char * env_get(const char name[]);

/* Returns environment variable value or def if it doesn't exist or empty. */
const char * env_get_def(const char name[], const char def[]);

/* Returns value of the first environment variable found or def if none of
 * specified environment variables are found (or empty).  The last name of
 * environment variable have to be followed by NULL. */
const char * env_get_one_of_def(const char def[], ...);

/* Sets new value of environment variable or creates it if it doesn't exist. */
void env_set(const char name[], const char value[]);

/* Removes environment variable. */
void env_remove(const char name[]);

#endif /* VIFM__UTILS__ENV_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
