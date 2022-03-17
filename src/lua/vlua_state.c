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

#include "vlua_state.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* calloc() free() malloc() */

#include "../utils/darray.h"
#include "../utils/string_array.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"

static void set_state(struct lua_State *lua, vlua_t *vlua);

/* Address of this variable serves as a key in Lua table. */
static char vlua_state_key;

vlua_t *
vlua_state_alloc(void)
{
	vlua_t *vlua = calloc(1, sizeof(*vlua));
	if(vlua == NULL)
	{
		return NULL;
	}

	vlua->lua = luaL_newstate();
	set_state(vlua->lua, vlua);

	luaL_requiref(vlua->lua, "base", &luaopen_base, 1);
	luaL_requiref(vlua->lua, LUA_TABLIBNAME, &luaopen_table, 1);
	luaL_requiref(vlua->lua, LUA_IOLIBNAME, &luaopen_io, 1);
	luaL_requiref(vlua->lua, LUA_STRLIBNAME, &luaopen_string, 1);
	luaL_requiref(vlua->lua, LUA_MATHLIBNAME, &luaopen_math, 1);
	luaL_requiref(vlua->lua, LUA_OSLIBNAME, &luaopen_os, 1);
	luaL_requiref(vlua->lua, LUA_LOADLIBNAME, &luaopen_package, 1);
	lua_pop(vlua->lua, 7);

	return vlua;
}

void
vlua_state_free(vlua_t *vlua)
{
	if(vlua != NULL)
	{
		size_t i;
		for(i = 0U; i < DA_SIZE(vlua->ptrs); ++i)
		{
			free(vlua->ptrs[i]);
		}
		DA_REMOVE_ALL(vlua->ptrs);

		free_string_array(vlua->strings.items, vlua->strings.nitems);

		lua_close(vlua->lua);
		free(vlua);
	}
}

state_ptr_t *
state_store_pointer(vlua_t *vlua, void *ptr)
{
	state_ptr_t **p = DA_EXTEND(vlua->ptrs);
	if(p == NULL)
	{
		return NULL;
	}

	*p = malloc(sizeof(**p));
	if(*p == NULL)
	{
		return NULL;
	}

	(*p)->vlua = vlua;
	(*p)->ptr = ptr;
	DA_COMMIT(vlua->ptrs);
	return *p;
}

const char *
state_store_string(vlua_t *vlua, const char str[])
{
	int n = add_to_string_array(&vlua->strings.items, vlua->strings.nitems, str);
	if(n == vlua->strings.nitems)
	{
		return "";
	}

	vlua->strings.nitems = n;
	return vlua->strings.items[n - 1];
}

/* Stores pointer to vlua inside Lua state. */
static void
set_state(lua_State *lua, vlua_t *vlua)
{
	lua_pushlightuserdata(lua, &vlua_state_key);
	lua_pushlightuserdata(lua, vlua);
	lua_settable(lua, LUA_REGISTRYINDEX);
}

vlua_t *
get_state(lua_State *lua)
{
	lua_pushlightuserdata(lua, &vlua_state_key);
	lua_gettable(lua, LUA_REGISTRYINDEX);
	vlua_t *vlua = lua_touserdata(lua, -1);
	lua_pop(lua, 1);
	return vlua;
}

void
vlua_state_make_table(vlua_t *vlua, void *key)
{
	lua_pushlightuserdata(vlua->lua, key);
	lua_newtable(vlua->lua);
	lua_settable(vlua->lua, LUA_REGISTRYINDEX);
}

void
vlua_state_get_table(vlua_t *vlua, void *key)
{
	lua_pushlightuserdata(vlua->lua, key);
	lua_gettable(vlua->lua, LUA_REGISTRYINDEX);
}

int
vlua_state_safe_mode_on(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);
	return vlua->safe_mode_level++;
}

void
vlua_state_safe_mode_off(lua_State *lua, int cookie)
{
	vlua_t *vlua = get_state(lua);
	--vlua->safe_mode_level;

	assert(vlua->safe_mode_level == cookie && "Mismatched safe mode change!");
	assert(vlua->safe_mode_level >= 0 && "Can't disabled disabled safe mode!");
}

int
vlua_state_proxy_call(lua_State *lua, int (*call)(lua_State *lua))
{
	if(get_state(lua)->safe_mode_level != 0)
	{
		return luaL_error(lua, "%s",
				"Unsafe functions can't be called in this environment!");
	}
	return call(lua);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
