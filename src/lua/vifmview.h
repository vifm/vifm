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

#ifndef VIFM__LUA__VIFMVIEW_H__
#define VIFM__LUA__VIFMVIEW_H__

#include "api.h"

struct lua_State;
struct view_t;

/* Initializes VifmView type unit. */
void vifmview_init(struct lua_State *lua);

/* Retrieves a reference to a view.  Leaves an object of VifmView type on the
 * stack. */
void vifmview_new(struct lua_State *lua, struct view_t *view);

/* Retrieves a reference to current view.  Returns an object of VifmView
 * type. */
int VLUA_API(vifmview_currview)(struct lua_State *lua);

/* Retrieves a reference to inactive view.  Returns an object of VifmView
 * type. */
int VLUA_API(vifmview_otherview)(struct lua_State *lua);

#endif /* VIFM__LUA__VIFMVIEW_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
