/* vifm
 * Copyright (C) 2022 xaizek.
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

#include "vifm_events.h"

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vlua_cbacks.h"
#include "vlua_state.h"

static int VLUA_API(events_listen)(lua_State *lua);

static void vifm_events_add(struct vlua_t *vlua, const char name[]);

VLUA_DECLARE_SAFE(events_listen);

/* Functions of `vifm.events` table. */
static const luaL_Reg vifm_events_methods[] = {
	{ "listen", VLUA_REF(events_listen) },
	{ NULL,     NULL                    }
};

/* Address of this variable serves as a key in Lua table.  The table maps event
 * name to set of its handlers (stored in keys, values are dummies). */
static char events_key;

void
vifm_events_init(lua_State *lua)
{
	luaL_newlib(lua, vifm_events_methods);

	vlua_t *vlua = get_state(lua);
	vlua_state_make_table(vlua, &events_key);
	vifm_events_add(vlua, "app.exit");
}

/* Registers a new event. */
static void
vifm_events_add(vlua_t *vlua, const char name[])
{
	vlua_state_get_table(vlua, &events_key);

	if(lua_getfield(vlua->lua, -1, name) != LUA_TNIL)
	{
		assert(0 && "Event with the specified name already exists!");
	}
	lua_pop(vlua->lua, 1); /* nil */

	lua_newtable(vlua->lua); /* event table */
	lua_setfield(vlua->lua, -2, name); /* "create" event */
	lua_pop(vlua->lua, 1); /* events table */
}

/* Member of `vifm.events` that adds subscription to an event. */
static int
VLUA_API(events_listen)(lua_State *lua)
{
	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "event", LUA_TSTRING);
	const char *event = lua_tostring(lua, -1);

	vlua_state_get_table(get_state(lua), &events_key);
	if(lua_getfield(lua, -1, event) == LUA_TNIL)
	{
		return luaL_error(lua, "No such event: %s", event);
	}

	check_field(lua, 1, "handler", LUA_TFUNCTION);
	lua_pushboolean(lua, 1);
	lua_settable(lua, -3);
	return 0;
}

void
vifm_events_app_exit(vlua_t *vlua)
{
	vlua_state_get_table(vlua, &events_key); /* events table */
	lua_getfield(vlua->lua, -1, "app.exit"); /* event table */
	lua_remove(vlua->lua, -2); /* events table */

	lua_pushnil(vlua->lua); /* key placeholder */
	while(lua_next(vlua->lua, -2) != 0)
	{
		lua_pop(vlua->lua, 1); /* values are dummies */
		lua_pushvalue(vlua->lua, -1); /* key is a handler */
		vlua_cbacks_schedule(vlua, /*argc=*/0);
	}

	lua_pop(vlua->lua, 1); /* event table */
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
