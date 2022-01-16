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

#include "vifm_keys.h"

#include <wchar.h>

#include "../engine/keys.h"
#include "../modes/modes.h"
#include "../ui/statusbar.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../bracket_notation.h"
#include "../status.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vlua_state.h"

static int VLUA_API(keys_add)(lua_State *lua);

VLUA_DECLARE_SAFE(keys_add);

static void parse_modes(vlua_t *vlua, char modes[MODES_COUNT]);
static void lua_key_handler(key_info_t key_info, keys_info_t *keys_info);

/* Functions of `vifm.keys` table. */
static const luaL_Reg vifm_keys_methods[] = {
	{ "add", VLUA_REF(keys_add) },
	{ NULL,  NULL               }
};

void
vifm_keys_init(lua_State *lua)
{
	luaL_newlib(lua, vifm_keys_methods);
}

/* Member of `vifm.keys` that registers a new key or raises an error.  Returns
 * boolean, which is true on success. */
static int
VLUA_API(keys_add)(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	wchar_t lhs[16];
	const size_t max_lhs_len = ARRAY_LEN(lhs) - 1U;
	check_field(lua, 1, "shortcut", LUA_TSTRING);
	wchar_t *tmp_lhs = substitute_specs(lua_tostring(lua, -1));
	if(wcslen(tmp_lhs) == 0 || wcslen(tmp_lhs) > max_lhs_len)
	{
		free(tmp_lhs);
		return luaL_error(lua, "Shortcut can't be empty or longer than %d",
				(int)max_lhs_len);
	}

	wcscpy(lhs, tmp_lhs);
	free(tmp_lhs);

	const char *descr = "";
	if(check_opt_field(lua, 1, "description", LUA_TSTRING))
	{
		descr = state_store_string(vlua, lua_tostring(lua, -1));
	}

	char modes[MODES_COUNT] = { };
	check_field(lua, 1, "modes", LUA_TTABLE);
	parse_modes(vlua, modes);

	lua_newtable(lua);
	check_field(lua, 1, "handler", LUA_TFUNCTION);
	lua_setfield(lua, -2, "handler");
	void *handler = to_pointer(lua);

	key_conf_t key = {
		.data.handler = &lua_key_handler,
		.descr = descr,
	};

	key.user_data = state_store_pointer(vlua, handler);
	if(key.user_data == NULL)
	{
		return luaL_error(lua, "%s", "Failed to store handler data");
	}

	int success = 1;

	int i;
	for(i = 0; i < MODES_COUNT; ++i)
	{
		if(modes[i])
		{
			success &= (vle_keys_foreign_add(lhs, &key, i) == 0);
		}
	}

	lua_pushboolean(lua, success);
	return 1;
}

/* Parses table at the top of the Lua stack into a set of flags indicating
 * which modes were mentioned in the table. */
static void
parse_modes(vlua_t *vlua, char modes[MODES_COUNT])
{
	lua_pushnil(vlua->lua);
	while(lua_next(vlua->lua, -2) != 0)
	{
		const char *name = lua_tostring(vlua->lua, -1);

		if(strcmp(name, "cmdline") == 0)
		{
			modes[CMDLINE_MODE] = 1;
		}
		else if(strcmp(name, "normal") == 0)
		{
			modes[NORMAL_MODE] = 1;
		}
		else if(strcmp(name, "visual") == 0)
		{
			modes[VISUAL_MODE] = 1;
		}
		else if(strcmp(name, "menus") == 0)
		{
			modes[MENU_MODE] = 1;
		}
		else if(strcmp(name, "view") == 0)
		{
			modes[VIEW_MODE] = 1;
		}
		else if(strcmp(name, "dialogs") == 0)
		{
			modes[SORT_MODE] = 1;
			modes[ATTR_MODE] = 1;
			modes[CHANGE_MODE] = 1;
			modes[FILE_INFO_MODE] = 1;
		}

		lua_pop(vlua->lua, 1);
	}
}

/* Handler of all foreign mappings registered from Lua. */
static void
lua_key_handler(key_info_t key_info, keys_info_t *keys_info)
{
	state_ptr_t *p = key_info.user_data;
	lua_State *lua = p->vlua->lua;

	from_pointer(lua, p->ptr);
	lua_getfield(lua, -1, "handler");

	lua_newtable(lua);

	if(key_info.count == NO_COUNT_GIVEN)
	{
		lua_pushnil(lua);
	}
	else
	{
		lua_pushinteger(lua, key_info.count);
	}
	lua_setfield(lua, -2, "count");

	if(key_info.reg == NO_REG_GIVEN)
	{
		lua_pushnil(lua);
	}
	else
	{
		const char reg_name[] = { key_info.reg, '\0' };
		lua_pushstring(lua, reg_name);
	}
	lua_setfield(lua, -2, "register");

	curr_stats.save_msg = 0;

	if(lua_pcall(lua, 1, 0, 0) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		ui_sb_err(error);
		lua_pop(lua, 2);
		curr_stats.save_msg = 1;
		return;
	}

	lua_pop(lua, 1);
	return;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
