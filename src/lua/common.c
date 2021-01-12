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

#include "common.h"

#include "lua/lauxlib.h"
#include "lua/lua.h"

void
check_field(lua_State *lua, int table_idx, const char name[], int lua_type)
{
	int type = lua_getfield(lua, 1, name);
	if(type == LUA_TNIL)
	{
		luaL_error(lua, "`%s` key is mandatory", name);
	}
	if(type != lua_type)
	{
		luaL_error(lua, "`%s` value must be a %s", name,
				lua_typename(lua, lua_type));
	}
}

int
check_opt_field(lua_State *lua, int table_idx, const char name[], int lua_type)
{
	int type = lua_getfield(lua, 1, name);
	if(type == LUA_TNIL)
	{
		return 0;
	}

	if(type != lua_type)
	{
		return luaL_error(lua, "`%s` value must be a %s",name,
				lua_typename(lua, lua_type));
	}
	return 1;
}

void *
to_pointer(lua_State *lua)
{
	void *ptr = (void *)lua_topointer(lua, -1);
	lua_pushlightuserdata(lua, ptr);
	lua_pushvalue(lua, -2);
	lua_settable(lua, LUA_REGISTRYINDEX);
	return ptr;
}

void
from_pointer(lua_State *lua, void *ptr)
{
	lua_pushlightuserdata(lua, ptr);
	lua_gettable(lua, LUA_REGISTRYINDEX);
}

void
drop_pointer(lua_State *lua, void *ptr)
{
	lua_pushlightuserdata(lua, ptr);
	lua_pushnil(lua);
	lua_settable(lua, LUA_REGISTRYINDEX);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
