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

#include <string.h> /* strcmp() */

#include "../utils/macros.h"
#include "../trash.h"
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

/* Mapping of operations onto their names in the API.  NULL means that the
 * event is not (yet) reported. */
static const char *const fsop_names[] = {
	[OP_NONE]     = NULL,
	[OP_USR]      = NULL,
	[OP_REMOVE]   = "remove",
	[OP_REMOVESL] = "remove",
	[OP_COPY]     = "copy",
	[OP_COPYF]    = "copy",
	[OP_COPYA]    = "copy",
	[OP_MOVE]     = "move",
	[OP_MOVEF]    = "move",
	[OP_MOVEA]    = "move",
	[OP_MOVETMP1] = "move",
	[OP_MOVETMP2] = "move",
	[OP_MOVETMP3] = "move",
	[OP_MOVETMP4] = "move",
	[OP_CHOWN]    = NULL,
	[OP_CHGRP]    = NULL,
#ifndef _WIN32
	[OP_CHMOD]    = NULL,
	[OP_CHMODR]   = NULL,
#else
	[OP_ADDATTR]  = NULL,
	[OP_SUBATTR]  = NULL,
#endif
	[OP_SYMLINK]  = "symlink",
	[OP_SYMLINK2] = "symlink",
	[OP_MKDIR]    = "create",
	[OP_RMDIR]    = "remove",
	[OP_MKFILE]   = "create",
};
ARRAY_GUARD(fsop_names, OP_COUNT);

/* Address of this variable serves as a key in Lua table.  The table maps event
 * name to set of its handlers (stored in keys, values are dummies). */
static char events_key;

void
vifm_events_init(lua_State *lua)
{
	luaL_newlib(lua, vifm_events_methods);

	vlua_t *vlua = vlua_state_get(lua);
	vlua_state_make_table(vlua, &events_key);
	vifm_events_add(vlua, "app.exit");
	vifm_events_add(vlua, "app.fsop");
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

	vlua_state_get_table(vlua_state_get(lua), &events_key);
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

void
vifm_events_app_fsop(vlua_t *vlua, OPS op, const char path[],
		const char target[], void *extra, int dir)
{
	const char *fsop_name = fsop_names[op];
	if(fsop_name == NULL)
	{
		/* We're not reporting this event. */
		return;
	}

	vlua_state_get_table(vlua, &events_key); /* events table */
	lua_getfield(vlua->lua, -1, "app.fsop"); /* event table */
	lua_remove(vlua->lua, -2); /* events table */

	int with_trash_flags = (strcmp(fsop_name, "move") == 0);
	int from_trash = 0, to_trash = 0;
	if(with_trash_flags)
	{
		from_trash = trash_has_path(path);
		to_trash = trash_has_path(target);
	}

	lua_pushnil(vlua->lua); /* key placeholder */
	while(lua_next(vlua->lua, -2) != 0)
	{
		lua_pop(vlua->lua, 1); /* values are dummies */
		lua_pushvalue(vlua->lua, -1); /* key is a handler */

		/* Not reusing the same argument, to not let handlers affect each other.
		 * Alternative is to pass read-only table. */
		lua_createtable(vlua->lua, /*narr=*/0, /*nrec=*/(with_trash_flags ? 6 : 4));
		lua_pushstring(vlua->lua, fsop_name);
		lua_setfield(vlua->lua, -2, "op");
		lua_pushstring(vlua->lua, path);
		lua_setfield(vlua->lua, -2, "path");
		lua_pushstring(vlua->lua, target);
		lua_setfield(vlua->lua, -2, "target");
		lua_pushboolean(vlua->lua, dir);
		lua_setfield(vlua->lua, -2, "isdir");
		if(with_trash_flags)
		{
			lua_pushboolean(vlua->lua, from_trash);
			lua_setfield(vlua->lua, -2, "fromtrash");
			lua_pushboolean(vlua->lua, to_trash);
			lua_setfield(vlua->lua, -2, "totrash");
		}

		vlua_cbacks_schedule(vlua, /*argc=*/1);
	}

	lua_pop(vlua->lua, 1); /* event table */
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
