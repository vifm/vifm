/* vifm
 * Copyright (C) 2023 filterfalse <filterfalse@gmail.com>.
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

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <wchar.h> /* wcslen() */

#include "vifm_abbrevs.h"

#include "../engine/abbrevs.h"
#include "../ui/statusbar.h"
#include "../bracket_notation.h"
#include "../status.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vlua_state.h"

static int VLUA_API(abbrevs_add)(lua_State *lua);
static wchar_t * lua_abbrev_handler(void *user_data);

VLUA_DECLARE_UNSAFE(abbrevs_add);

/* Functions of `vifm.abbrevs` table. */
static const luaL_Reg vifm_abbrevs_methods[] = {
	{ "add", VLUA_REF(abbrevs_add) },
	{ NULL,  NULL                  }
};

void
vifm_abbrevs_init(lua_State *lua)
{
	luaL_newlib(lua, vifm_abbrevs_methods);
}

/* Member of `vifm.abbrevs` that registers a new abbreviation or raises an
 * error.  Returns boolean, which is true on success. */
static int
VLUA_API(abbrevs_add)(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	wchar_t wlhs[32];
	const size_t max_wlhs_len = ARRAY_LEN(wlhs) - 1U;
	check_field(lua, 1, "lhs", LUA_TSTRING);
	const char *lhs = lua_tostring(lua, -1);
	wchar_t *tmp_wlhs = substitute_specs(lhs);
	if(wcslen(tmp_wlhs) == 0 || wcslen(tmp_wlhs) > max_wlhs_len)
	{
		free(tmp_wlhs);
		return luaL_error(lua, "`lhs` can't be empty or longer than %d",
				(int)max_wlhs_len);
	}

	wcscpy(wlhs, tmp_wlhs);
	free(tmp_wlhs);

	lua_newtable(lua);
	check_field(lua, 1, "handler", LUA_TFUNCTION);
	lua_setfield(lua, -2, "handler");
	lua_pushstring(lua, lhs);
	lua_setfield(lua, -2, "lhs");
	void *handler = to_pointer(lua);

	void *user_data = state_store_pointer(vlua, handler);
	if(user_data == NULL)
	{
		return luaL_error(lua, "%s", "Failed to store handler data");
	}

	const char *descr = "";
	if(check_opt_field(vlua->lua, 1, "description", LUA_TSTRING))
	{
		descr = lua_tostring(lua, -1);
	}

	int no_remap = 1;
	if(check_opt_field(lua, 1, "noremap", LUA_TBOOLEAN))
	{
		no_remap = lua_toboolean(lua, -1);
	}

	lua_pushboolean(lua, (vle_abbr_add_foreign(wlhs, descr, no_remap,
					&lua_abbrev_handler, user_data) == 0));
	return 1;
}

/* Handler of all foreign abbreviations registered from Lua.  Returns newly
 * allocated wide string. */
static wchar_t *
lua_abbrev_handler(void *user_data)
{
	state_ptr_t *p = user_data;
	lua_State *lua = p->vlua->lua;

	from_pointer(lua, p->ptr);
	lua_getfield(lua, -1, "lhs");
	const char *lhs = lua_tostring(lua, -1);
	lua_getfield(lua, -2, "handler");

	lua_createtable(lua, /*narr=*/0, /*nrec=*/1);
	lua_pushstring(lua, lhs);
	lua_setfield(lua, -2, "lhs");

	int sm_cookie = vlua_state_safe_mode_on(lua);
	int result = lua_pcall(lua, /*nargs=*/1, /*nresults=*/1, /*msgh=*/0);
	vlua_state_safe_mode_off(lua, sm_cookie);

	if(result != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		ui_sb_err(error);
		lua_pop(lua, 3);
		curr_stats.save_msg = 1;
		return NULL;
	}

	wchar_t *rhs = NULL;
	if(lua_istable(lua, -1))
	{
		lua_getfield(lua, -1, "rhs");
		const char *str = lua_tostring(lua, -1);
		if(str != NULL)
		{
			rhs = substitute_specs(str);
		}
		lua_pop(lua, 1);
	}
	lua_pop(lua, 3);
	return rhs;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
