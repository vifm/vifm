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

#include "vifm_cmds.h"

#include <limits.h> /* INT_MAX */

#include "../engine/cmds.h"
#include "../engine/completion.h"
#include "../ui/statusbar.h"
#include "../utils/str.h"
#include "../cmd_completion.h"
#include "../status.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vlua_state.h"

static int VLUA_API(cmds_add)(lua_State *lua);
static void parse_cmd_params(vlua_t *vlua, cmd_add_t *cmd);
static int VLUA_API(cmds_command)(lua_State *lua);
static int VLUA_API(cmds_delcommand)(lua_State *lua);
static int lua_cmd_handler(const cmd_info_t *cmd_info);
static int apply_completion(lua_State *lua, const char str[]);

VLUA_DECLARE_SAFE(cmds_add);
VLUA_DECLARE_SAFE(cmds_command);
VLUA_DECLARE_SAFE(cmds_delcommand);

/* Functions of `vifm.cmds` table. */
static const luaL_Reg vifm_cmds_methods[] = {
	{ "add",        VLUA_REF(cmds_add)        },
	{ "command",    VLUA_REF(cmds_command)    },
	{ "delcommand", VLUA_REF(cmds_delcommand) },
	{ NULL,         NULL                      }
};

void
vifm_cmds_init(lua_State *lua)
{
	luaL_newlib(lua, vifm_cmds_methods);
}

/* Member of `vifm.cmds` that registers a new :command or raises an error.
 * Returns boolean, which is true on success. */
static int
VLUA_API(cmds_add)(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "name", LUA_TSTRING);
	const char *name = lua_tostring(lua, -1);

	int id = -1;

	lua_newtable(lua);
	check_field(lua, 1, "handler", LUA_TFUNCTION);
	lua_setfield(lua, -2, "handler");
	if(check_opt_field(lua, 1, "complete", LUA_TFUNCTION))
	{
		lua_setfield(lua, -2, "complete");
		id = COM_FOREIGN;
	}
	void *handler = to_pointer(lua);

	cmd_add_t cmd = {
	  .name = name,
	  .abbr = NULL,
	  .id = id,
	  .descr = "",
	  .flags = 0,
	  .handler = &lua_cmd_handler,
	  .user_data = NULL,
	  .min_args = 0,
	  .max_args = 0,
	};

	parse_cmd_params(vlua, &cmd);

	cmd.user_data = state_store_pointer(vlua, handler);
	if(cmd.user_data == NULL)
	{
		return luaL_error(lua, "%s", "Failed to store handler data");
	}

	lua_pushboolean(lua, vle_cmds_add_foreign(&cmd) == 0);
	return 1;
}

/* Initializes fields of cmd_add_t from parameter for `vifm.cmds.add`. */
static void
parse_cmd_params(vlua_t *vlua, cmd_add_t *cmd)
{
	if(check_opt_field(vlua->lua, 1, "description", LUA_TSTRING))
	{
		cmd->descr = state_store_string(vlua, lua_tostring(vlua->lua, -1));
	}

	if(check_opt_field(vlua->lua, 1, "minargs", LUA_TNUMBER))
	{
		cmd->min_args = lua_tointeger(vlua->lua, -1);
	}
	if(check_opt_field(vlua->lua, 1, "maxargs", LUA_TNUMBER))
	{
		cmd->max_args = lua_tointeger(vlua->lua, -1);
		if(cmd->max_args < 0)
		{
			cmd->max_args = NOT_DEF;
		}
	}
	else
	{
		cmd->max_args = cmd->min_args;
	}
}

/* Member of `vifm.command` that registers a new user-defined :command or raises
 * an error.  Returns boolean, which is true on success. */
static int
VLUA_API(cmds_command)(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "name", LUA_TSTRING);
	const char *name = lua_tostring(lua, -1);

	check_field(lua, 1, "action", LUA_TSTRING);
	const char *action = skip_whitespace(lua_tostring(lua, -1));
	if(action[0] == '\0')
	{
		return luaL_error(lua, "%s", "Action can't be empty");
	}

	const char *descr = NULL;
	if(check_opt_field(lua, 1, "description", LUA_TSTRING))
	{
		descr = state_store_string(vlua, lua_tostring(lua, -1));
	}

	int overwrite = 0;
	if(check_opt_field(lua, 1, "overwrite", LUA_TBOOLEAN))
	{
		overwrite = lua_toboolean(lua, -1);
	}

	int success = (vle_cmds_add_user(name, action, descr, overwrite) == 0);
	lua_pushboolean(lua, success);
	return 1;
}

/* Member of `vifm.command` that unregisters a user-defined :command.  Returns
 * boolean, which is true on success. */
static int
VLUA_API(cmds_delcommand)(lua_State *lua)
{
	const char *name = luaL_checkstring(lua, 1);

	int success = (vle_cmds_del_user(name) == 0);
	lua_pushboolean(lua, success);
	return 1;
}

/* Handler of all foreign :commands registered from Lua. */
static int
lua_cmd_handler(const cmd_info_t *cmd_info)
{
	state_ptr_t *p = cmd_info->user_data;
	lua_State *lua = p->vlua->lua;

	from_pointer(lua, p->ptr);
	lua_getfield(lua, -1, "handler");

	lua_newtable(lua);
	lua_pushstring(lua, cmd_info->args);
	lua_setfield(lua, -2, "args");
	push_str_array(lua, cmd_info->argv, cmd_info->argc);
	lua_setfield(lua, -2, "argv");

	curr_stats.save_msg = 0;

	if(lua_pcall(lua, 1, 0, 0) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		ui_sb_err(error);
		lua_pop(lua, 2);
		return CMDS_ERR_CUSTOM;
	}

	lua_pop(lua, 1);
	return curr_stats.save_msg;
}

int
vifm_cmds_complete(lua_State *lua, const cmd_info_t *cmd_info, int arg_pos)
{
	state_ptr_t *p = cmd_info->user_data;

	from_pointer(lua, p->ptr);
	if(lua_getfield(lua, -1, "complete") == LUA_TNIL)
	{
		return 0;
	}

	lua_newtable(lua);
	lua_pushstring(lua, cmd_info->args);
	lua_setfield(lua, -2, "args");
	push_str_array(lua, cmd_info->argv, cmd_info->argc);
	lua_setfield(lua, -2, "argv");
	lua_pushstring(lua, cmd_info->args + arg_pos);
	lua_setfield(lua, -2, "arg");

	if(lua_pcall(lua, 1, 1, 0) != LUA_OK)
	{
		lua_pop(lua, 2);
		return 0;
	}

	if(lua_type(lua, -1) != LUA_TTABLE)
	{
		return 0;
	}

	return apply_completion(lua, cmd_info->args + arg_pos);
}

/* Does stack cleanup for the caller (-2).  Returns offset of completion
 * matches. */
static int
apply_completion(lua_State *lua, const char str[])
{
	int offset = 0;
	if(lua_getfield(lua, -1, "offset") == LUA_TNUMBER)
	{
		LUA_INTEGER n = lua_tointeger(lua, -1);
		if(n > 0 && n <= INT_MAX)
		{
			offset = n;
		}
	}
	lua_pop(lua, 1);

	if(lua_getfield(lua, -1, "matches") == LUA_TTABLE)
	{
		lua_pushnil(lua);
		while(lua_next(lua, -2) != 0)
		{
			const char *match = NULL;
			const char *descr = NULL;

			if(lua_type(lua, -1) == LUA_TSTRING)
			{
				match = lua_tostring(lua, -1);
			}
			else if(lua_type(lua, -1) == LUA_TTABLE)
			{
				(void)lua_getfield(lua, -1, "match");
				match = lua_tostring(lua, -1);

				if(match != NULL)
				{
					(void)lua_getfield(lua, -2, "description");
					descr = lua_tostring(lua, -1);
					lua_pop(lua, 1);
				}

				lua_pop(lua, 1);
			}

			if(match != NULL)
			{
				vle_compl_add_match(match, descr == NULL ? "" : descr);
			}

			lua_pop(lua, 1);
		}

		vle_compl_finish_group();
		vle_compl_add_last_match(str);
	}
	lua_pop(lua, 1);

	lua_pop(lua, 2);
	return offset;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
