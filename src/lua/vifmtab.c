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

#include "vifmtab.h"

#include "../cfg/config.h"
#include "../ui/tabs.h"
#include "../ui/ui.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vifmview.h"

static int VLUA_API(vifmtab_getlayout)(lua_State *lua);
static int VLUA_API(vifmtab_getname)(lua_State *lua);
static int VLUA_API(vifmtab_getview)(lua_State *lua);

static void find_tab(lua_State *lua, unsigned int id, tab_info_t *tab_info);
static void find_side_tab(lua_State *lua, unsigned int id, tab_info_t *tab_info,
		view_t *side);

VLUA_DECLARE_SAFE(vifmtab_getlayout);
VLUA_DECLARE_SAFE(vifmtab_getname);
VLUA_DECLARE_SAFE(vifmtab_getview);

/* Methods of VifmTab type. */
static const luaL_Reg vifmtab_methods[] = {
	{ "getlayout", VLUA_REF(vifmtab_getlayout), },
	{ "getname",   VLUA_REF(vifmtab_getname), },
	{ "getview",   VLUA_REF(vifmtab_getview), },
	{ NULL,        NULL                       }
};

void
vifmtab_init(lua_State *lua)
{
	luaL_newmetatable(lua, "VifmTab");
	lua_pushvalue(lua, -1);
	lua_setfield(lua, -2, "__index");
	luaL_setfuncs(lua, vifmtab_methods, 0);
	lua_pop(lua, 1);
}

int
VLUA_API(vifmtab_new)(struct lua_State *lua)
{
	view_t *side = curr_view;
	int index;
	int index_set = 0;

	if(!lua_isnoneornil(lua, 1))
	{
		if(check_opt_field(lua, 1, "other", LUA_TBOOLEAN) && lua_toboolean(lua, -1))
		{
			side = other_view;
		}

		if(check_opt_field(lua, 1, "index", LUA_TNUMBER))
		{
			index = lua_tointeger(lua, -1) - 1;
			index_set = 1;
		}
	}

	if(!index_set)
	{
		index = tabs_current(side);
	}

	tab_info_t tab_info;
	if(!tabs_get(side, index, &tab_info))
	{
		return luaL_error(lua, "No tab with index %d on %s side", index,
				side == curr_view ? "active" : "inactive");
	}

	unsigned int *data = lua_newuserdatauv(lua, sizeof(*data), 0);
	*data = tab_info.id;

	luaL_getmetatable(lua, "VifmTab");
	lua_setmetatable(lua, -2);

	return 1;
}

/* Method of `VifmTab` that retrieves its layout.  Returns a table. */
static int
VLUA_API(vifmtab_getlayout)(lua_State *lua)
{
	const unsigned int *id = luaL_checkudata(lua, 1, "VifmTab");

	tab_info_t tab_info;
	find_tab(lua, *id, &tab_info);

	lua_newtable(lua);
	lua_pushboolean(lua, tab_info.layout.only_mode);
	lua_setfield(lua, -2, "only");
	if(!tab_info.layout.only_mode)
	{
		lua_pushstring(lua, tab_info.layout.split == HSPLIT ? "h" : "v");
		lua_setfield(lua, -2, "split");
	}

	return 1;
}

/* Method of `VifmTab` that retrieves its name.  Returns a string. */
static int
VLUA_API(vifmtab_getname)(lua_State *lua)
{
	const unsigned int *id = luaL_checkudata(lua, 1, "VifmTab");

	tab_info_t tab_info;
	find_tab(lua, *id, &tab_info);

	lua_pushstring(lua, (tab_info.name == NULL ? "" : tab_info.name));
	return 1;
}

/* Method of `VifmTab` that retrieves a view.  Returns `VifmView`. */
static int
VLUA_API(vifmtab_getview)(lua_State *lua)
{
	const unsigned int *id = luaL_checkudata(lua, 1, "VifmTab");

	int other_side = 0;
	if(check_opt_arg(lua, 2, LUA_TTABLE) &&
			check_opt_field(lua, 2, "pane", LUA_TNUMBER))
	{
		int pane = lua_tointeger(lua, -1);
		if(pane < 1 || pane > 2)
		{
			return luaL_error(lua, "%s", "pane field is not in the range [1; 2]");
		}
		other_side = (pane == 2);
	}

	tab_info_t tab_info;
	if(cfg.pane_tabs)
	{
		find_tab(lua, *id, &tab_info);
	}
	else
	{
		find_side_tab(lua, *id, &tab_info, other_side ? other_view : curr_view);
	}

	vifmview_new(lua, tab_info.view);
	return 1;
}

/* Find tab by its id.  Returns the pointer or aborts (Lua does longjmp()) if
 * the tab doesn't exist anymore. */
static void
find_tab(lua_State *lua, unsigned int id, tab_info_t *tab_info)
{
	int i;
	for(i = 0; tabs_enum_all(i, tab_info); ++i)
	{
		if(tab_info->id == id)
		{
			return;
		}
	}

	luaL_error(lua, "%s", "Invalid VifmTab object (associated tab is dead)");
}

/* Find global tab by its id and side (to get correct view).  Returns the
 * pointer or aborts (Lua does longjmp()) if the tab doesn't exist anymore. */
static void
find_side_tab(lua_State *lua, unsigned int id, tab_info_t *tab_info,
		view_t *side)
{
	int i;
	for(i = 0; tabs_enum(side, i, tab_info); ++i)
	{
		if(tab_info->id == id)
		{
			return;
		}
	}

	luaL_error(lua, "%s", "Invalid VifmTab object (associated tab is dead)");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
