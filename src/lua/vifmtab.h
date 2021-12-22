/* vifm
 * Copyright (C) 2021 xaizek.
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

#ifndef VIFM__LUA__VIFMTAB_H__
#define VIFM__LUA__VIFMTAB_H__

#include "api.h"

struct lua_State;

/* Initializes VifmTab type unit. */
void vifmtab_init(struct lua_State *lua);

/* Creates a wrapper for a tab.  Accepts a table that specifies which one to
 * wrap.  Returns an object of VifmTab type or raises an error. */
int VLUA_API(vifmtab_new)(struct lua_State *lua);

#endif /* VIFM__LUA__VIFMTAB_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
