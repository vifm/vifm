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

#include "vifm_handlers.h"

#include <assert.h> /* assert() */
#include <string.h> /* strcspn() */

#include "../ui/statusbar.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../plugins.h"
#include "../vifm.h"
#include "common.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "vlua_state.h"

static char * extract_handler_name(const char viewer[]);
static int check_handler_name(vlua_t *vlua, const char name[]);

/* Characters considered to be whitespace. */
static const char *WHITESPACE = " \t";

/* Address of this variable serves as a key in Lua table.  The associated table
 * is doubly keyed: by column name and by corresponding ID. */
static char handlers_key;

int
vifm_handlers_check(vlua_t *vlua, const char cmd[])
{
	return (cmd[0] == '#');
}

void
vifm_handlers_init(vlua_t *vlua)
{
	vlua_state_make_table(vlua, &handlers_key);
}

int
vifm_handlers_present(vlua_t *vlua, const char cmd[])
{
	char *name = extract_handler_name(cmd);

	/* Don't need lua_pcall() to handle errors, because no one should be able to
	 * mess with internal tables. */
	vlua_state_get_table(vlua, &handlers_key);
	const int present = (lua_getfield(vlua->lua, -1, name) != LUA_TNIL);
	lua_pop(vlua->lua, 2);

	free(name);

	return present;
}

strlist_t
vifm_handlers_view(vlua_t *vlua, const char viewer[], const char path[])
{
	strlist_t result = {};

	char *name = extract_handler_name(viewer);

	/* Don't need lua_pcall() to handle errors, because no one should be able to
	 * mess with internal tables. */
	vlua_state_get_table(vlua, &handlers_key);
	if(lua_getfield(vlua->lua, -1, name) != LUA_TTABLE)
	{
		free(name);
		lua_pop(vlua->lua, 2);
		return result;
	}

	free(name);

	assert(lua_getfield(vlua->lua, -1, "handler") == LUA_TFUNCTION &&
			"Handler must be a function here.");

	lua_newtable(vlua->lua);
	lua_pushstring(vlua->lua, viewer);
	lua_setfield(vlua->lua, -2, "command");
	lua_pushstring(vlua->lua, path);
	lua_setfield(vlua->lua, -2, "path");

	if(lua_pcall(vlua->lua, 1, 1, 0) != LUA_OK)
	{
		const char *error = lua_tostring(vlua->lua, -1);
		ui_sb_err(error);
		lua_pop(vlua->lua, 3);
		return result;
	}

	if(!lua_istable(vlua->lua, -1))
	{
		lua_pop(vlua->lua, 3);
		return result;
	}

	lua_pushnil(vlua->lua);
	while(lua_next(vlua->lua, -2) != 0)
	{
		const char *line = lua_tostring(vlua->lua, -1);
		result.nitems = add_to_string_array(&result.items, result.nitems, line);
		lua_pop(vlua->lua, 1);
	}

	lua_pop(vlua->lua, 3);
	return result;
}

/* Extracts name of the handler from a command.  Returns a newly allocated
 * string. */
static char *
extract_handler_name(const char viewer[])
{
	const int name_len = strcspn(viewer, WHITESPACE);
	return format_str("%.*s", name_len, viewer);
}

int
vifm_addhandler(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	const char *namespace = NULL;
	plug_t *plug = lua_touserdata(lua, lua_upvalueindex(1));
	if(plug != NULL)
	{
		namespace = plug->name;
	}
	else if(vifm_testing())
	{
		namespace = "vifmtest";
	}
	else
	{
		return luaL_error(lua, "%s", "call to addhandler() outside of a plugin");
	}

	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "name", LUA_TSTRING);
	const char *name = lua_tostring(lua, -1);
	check_handler_name(vlua, name);

	check_field(lua, 1, "handler", LUA_TFUNCTION);

	char *full_name = format_str("#%s#%s", namespace, name);

	vlua_state_get_table(vlua, &handlers_key);

	/* Check if handler already exists. */
	if(lua_getfield(vlua->lua, -1, full_name) != LUA_TNIL)
	{
		free(full_name);
		lua_pushboolean(lua, 0);
		return 1;
	}
	lua_pop(vlua->lua, 1);

	lua_newtable(lua);
	lua_pushvalue(lua, -3);
	lua_setfield(lua, -2, "handler");
	lua_setfield(lua, -2, full_name);

	free(full_name);

	lua_pushboolean(lua, 1);
	return 1;
}

/* Verifies validity of a handler's name.  Return value has no particular
 * meaning. */
static int
check_handler_name(vlua_t *vlua, const char name[])
{
	if(name[0] == '\0')
	{
		return luaL_error(vlua->lua, "%s", "Handler's name can't be empty");
	}
	if(name[strcspn(name, WHITESPACE)] != '\0')
	{
		return luaL_error(vlua->lua, "%s",
				"Handler's name can't contain whitespace");
	}

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
