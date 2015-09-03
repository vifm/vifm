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

#ifndef VIFM__INT__PATH_ENV_H__
#define VIFM__INT__PATH_ENV_H__

#include <stddef.h> /* size_t */

/* Asks for updating of PATH if needed (or if force parameters is true). */
void update_path_env(int force);

/* Reparses PATH environment variable if needed. Returns list of paths, which
 * shouldn't be freed by the caller. The number of paths is returned through
 * the count argument. */
char ** get_paths(size_t *count);

/* Sets PATH to its value that was set by user or another program. Use
 * load_real_path_env() function to revert this effect. */
void load_clean_path_env(void);

/* Sets PATH to its value with additional paths added by vifm for its internal
 * needs. */
void load_real_path_env(void);

#endif /* VIFM__INT__PATH_ENV_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
