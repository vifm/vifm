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

#include "../ui/column_view.h"
#include "../ui/fileview.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "common.h"
#include "vlua_state.h"

static int check_viewcolumn_name(vlua_t *vlua, const char name[]);
static void lua_viewcolumn_handler(void *data, int id, const void *format_data,
		size_t buf_len, char buf[]);

/* Address of this variable serves as a key in Lua table. */
static char viewcolumns_key;
/* Next id for a view column. */
static int viewcolumn_next_id = SK_TOTAL;

void
vifm_viewcolumns_init(vlua_t *vlua)
{
	vlua_state_make_table(vlua, &viewcolumns_key);
}

int
vifm_viewcolumns_map(vlua_t *vlua, const char name[])
{
	/* Don't need lua_pcall() to handle errors, because no one should be able to
	 * mess with internal tables. */
	vlua_state_get_table(vlua, &viewcolumns_key);
	int id = (lua_getfield(vlua->lua, -1, name) == LUA_TNUMBER)
	        ? lua_tointeger(vlua->lua, -1)
	        : -1;
	lua_pop(vlua->lua, 2);
	return id;
}

int
vifm_addcolumntype(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "name", LUA_TSTRING);
	const char *name = lua_tostring(lua, -1);
	check_viewcolumn_name(vlua, name);

	check_field(lua, 1, "handler", LUA_TFUNCTION);
	void *handler = to_pointer(lua);

	void *data = state_store_pointer(vlua, handler);
	if(data == NULL)
	{
		return luaL_error(lua, "%s", "Failed to store handler data");
	}

	int column_id = viewcolumn_next_id++;
	vlua_state_get_table(vlua, &viewcolumns_key);
	lua_pushinteger(lua, column_id);
	lua_setfield(lua, -2, name);

	int error = columns_add_column_desc(column_id, &lua_viewcolumn_handler, data);
	if(error)
	{
		drop_pointer(lua, handler);
	}
	lua_pushboolean(lua, !error);
	return 1;
}

/* Verifies validity of a user-defined view column name.  Return value has no
 * particular meaning. */
static int
check_viewcolumn_name(vlua_t *vlua, const char name[])
{
	vlua_state_get_table(vlua, &viewcolumns_key);
	int present = (lua_getfield(vlua->lua, -1, name) == LUA_TNUMBER);
	lua_pop(vlua->lua, 2);

	if(present)
	{
		return luaL_error(vlua->lua,
				"View column with such name already exists: %s", name);
	}

	if(name[0] == '\0')
	{
		return luaL_error(vlua->lua, "%s", "View column name can't be empty");
	}
	if(name[0] >= 'a' && name[0] <= 'z')
	{
		return luaL_error(vlua->lua, "%s",
				"View column name must not start with a lower case Latin letter");
	}

	while(*name)
	{
		char c = *name++;
		if(c >= 'a' && c <= 'z') continue;
		if(c >= 'A' && c <= 'Z') continue;
		return luaL_error(vlua->lua, "%s",
				"View column name must not contain non-Latin characters");
	}

	return 0;
}

/* Handler of all user-defined view columns registered from Lua. */
static void
lua_viewcolumn_handler(void *data, int id, const void *format_data,
		size_t buf_len, char buf[])
{
	state_ptr_t *p = data;
	lua_State *lua = p->vlua->lua;

	from_pointer(lua, p->ptr);

	lua_newtable(lua);

	const column_data_t *cdt = format_data;
	dir_entry_t *entry = cdt->entry;
	lua_newtable(lua);
	lua_pushstring(lua, entry->name);
	lua_setfield(lua, -2, "name");
	lua_pushstring(lua, entry->origin);
	lua_setfield(lua, -2, "location");
	lua_pushinteger(lua, entry->size);
	lua_setfield(lua, -2, "size");
	lua_pushinteger(lua, entry->mtime);
	lua_setfield(lua, -2, "mtime");
	lua_pushinteger(lua, entry->atime);
	lua_setfield(lua, -2, "atime");
	lua_pushinteger(lua, entry->ctime);
	lua_setfield(lua, -2, "ctime");

	lua_setfield(lua, -2, "entry");

	if(lua_pcall(lua, 1, 1, 0) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		ui_sb_err(error);
		copy_str(buf, buf_len, "ERROR");
	}
	else
	{
		const char *text = lua_tostring(lua, -1);
		copy_str(buf, buf_len, (text == NULL ? "NOVALUE" : text));
	}
	lua_pop(lua, 1);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
