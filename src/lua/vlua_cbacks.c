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

#include "vlua_cbacks.h"

#include "../ui/statusbar.h"
#include "lua/lua.h"
#include "vlua_state.h"

/* Address of this variable serves as a key in Lua table. */
static char cbacks_queue;

void
vlua_cbacks_init(vlua_t *vlua)
{
	vlua_state_make_table(vlua, &cbacks_queue);
}

void
vlua_cbacks_process(vlua_t *vlua)
{
	vlua_state_get_table(vlua, &cbacks_queue);

	/* Replace with an empty table, so there are no updates to the table while
	 * it's being traversed. */
	vlua_state_make_table(vlua, &cbacks_queue);

	lua_len(vlua->lua, -1);
	int len = lua_tointeger(vlua->lua, -1);
	lua_pop(vlua->lua, 1);

	int i;
	for(i = 0; i < len; ++i)
	{
		lua_geti(vlua->lua, -1, 1 + i);

		lua_getfield(vlua->lua, -1, "handler");

		lua_getfield(vlua->lua, -2, "argv");
		lua_len(vlua->lua, -1);
		int argc = lua_tointeger(vlua->lua, -1);
		lua_pop(vlua->lua, 1);

		int j;
		for(j = 0; j < argc; ++j)
		{
			lua_geti(vlua->lua, -1 - j, 1 + j);
		}

		lua_remove(vlua->lua, -1 - argc);

		if(lua_pcall(vlua->lua, argc, 0, 0) != LUA_OK)
		{
			const char *error = lua_tostring(vlua->lua, -1);
			ui_sb_err(error);
			lua_pop(vlua->lua, 1);
		}

		lua_pop(vlua->lua, 1);
	}

	lua_pop(vlua->lua, 1);
}

void
vlua_cbacks_schedule(vlua_t *vlua, int argc)
{
	lua_createtable(vlua->lua, /*narr=*/0, /*nrec=*/0); /* scheduled table */

	lua_createtable(vlua->lua, argc, /*nrec=*/0);
	int i;
	for(i = 0; i < argc; ++i)
	{
		lua_rotate(vlua->lua, -3 - (argc - 1 - i), -1);
		lua_seti(vlua->lua, -2, 1 + i);
	}
	lua_setfield(vlua->lua, -2, "argv");

	lua_rotate(vlua->lua, -2, 1);
	lua_setfield(vlua->lua, -2, "handler");

	vlua_state_get_table(vlua, &cbacks_queue); /* cbacks table */

	lua_len(vlua->lua, -1); /* of cbacks table */
	int len = lua_tointeger(vlua->lua, -1);
	lua_pop(vlua->lua, 1); /* length */

	lua_rotate(vlua->lua, -2, 1); /* swap cbacks table and scheduled table */
	lua_seti(vlua->lua, -2, 1 + len);

	lua_pop(vlua->lua, 1); /* cbacks table */
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
