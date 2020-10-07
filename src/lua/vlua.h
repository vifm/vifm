/* vifm
 * Copyright (C) 2020 xaizek.
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

#ifndef VIFM__LUA__VLUA_H__
#define VIFM__LUA__VLUA_H__

/* This unit implements Lua interface. */

/* Declaration of opaque state of the unit. */
typedef struct vlua_t vlua_t;

struct plug_t;

/* Creates new instance of the unit.  Returns the instance or NULL. */
vlua_t * vlua_init(void);

/* Loads a single plugin on request.  Returns zero on success. */
int vlua_load_plugin(vlua_t *vlua, const char plugin[], struct plug_t *plug);

/* Frees resources of the unit. */
void vlua_finish(vlua_t *vlua);

/* Executes a Lua string.  Returns non-zero on error, otherwise zero is
 * returned. */
int vlua_run_string(vlua_t *vlua, const char str[]);

#endif /* VIFM__LUA__VLUA_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
