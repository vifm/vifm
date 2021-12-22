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

#include "vifm_tabs.h"

#include "../ui/tabs.h"
#include "../ui/ui.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vifmtab.h"

static int VLUA_API(tabs_getcount)(lua_State *lua);
static int VLUA_API(tabs_getcurrent)(lua_State *lua);

static view_t *pick_side(lua_State *lua);

/* The first one is defined in another unit. */
VLUA_DECLARE_SAFE(vifmtab_new);
VLUA_DECLARE_SAFE(tabs_getcount);
VLUA_DECLARE_SAFE(tabs_getcurrent);

/* Functions of `vifm.tabs` table. */
static const luaL_Reg vifm_tabs_methods[] = {
	{ "get",        VLUA_REF(vifmtab_new)     },
	{ "getcount",   VLUA_REF(tabs_getcount)   },
	{ "getcurrent", VLUA_REF(tabs_getcurrent) },
	{ NULL,         NULL                      }
};

void
vifm_tabs_init(lua_State *lua)
{
	luaL_newlib(lua, vifm_tabs_methods);
	vifmtab_init(lua);
}

/* Member of `vifm.tabs` that number of tabs.  Returns an integer. */
static int
VLUA_API(tabs_getcount)(lua_State *lua)
{
	view_t *side = pick_side(lua);
	lua_pushinteger(lua, tabs_count(side));
	return 1;
}

/* Member of `vifm.tabs` that index of current tab.  Returns an integer. */
static int
VLUA_API(tabs_getcurrent)(lua_State *lua)
{
	view_t *side = pick_side(lua);
	lua_pushinteger(lua, 1 + tabs_current(side));
	return 1;
}

/* Picks side for tab lookup.  Returns pointer to view that signifies side. */
static view_t *
pick_side(lua_State *lua)
{
	if(!check_opt_arg(lua, 1, LUA_TTABLE))
	{
		return curr_view;
	}

	view_t *side = curr_view;
	if(check_opt_field(lua, 1, "other", LUA_TBOOLEAN) && lua_toboolean(lua, -1))
	{
		side = other_view;
	}
	return side;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
