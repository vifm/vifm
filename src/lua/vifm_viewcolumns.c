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

#include "vifm_viewcolumns.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../compat/fs_limits.h"
#include "../ui/column_view.h"
#include "../ui/fileview.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../filelist.h"
#include "../types.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vifmentry.h"
#include "vlua_state.h"

static int check_viewcolumn_name(vlua_t *vlua, const char name[]);
static void lua_viewcolumn_handler(void *data, size_t buf_len, char buf[],
		const format_info_t *info);

/* Minimal ID for columns added by this view. */
enum { FIRST_LUA_COLUMN_ID = SK_TOTAL };

/* Address of this variable serves as a key in Lua table.  The associated table
 * is doubly keyed: by column name and by corresponding ID. */
static char viewcolumns_key;
/* Next id for a view column. */
static int viewcolumn_next_id = FIRST_LUA_COLUMN_ID;

void
vifm_viewcolumns_init(vlua_t *vlua)
{
	vifmentry_init(vlua->lua);

	vlua_state_make_table(vlua, &viewcolumns_key);
}

int
vifm_viewcolumns_next_id(vlua_t *vlua)
{
	return viewcolumn_next_id;
}

int
vifm_viewcolumns_map(vlua_t *vlua, const char name[])
{
	/* Don't need lua_pcall() to handle errors, because no one should be able to
	 * mess with internal tables. */
	vlua_state_get_table(vlua, &viewcolumns_key);
	if(lua_getfield(vlua->lua, -1, name) != LUA_TTABLE)
	{
		lua_pop(vlua->lua, 2);
		return -1;
	}

	lua_getfield(vlua->lua, -1, "id");
	int id = lua_tointeger(vlua->lua, -1);
	lua_pop(vlua->lua, 3);
	return id;
}

char *
vifm_viewcolumns_map_back(vlua_t *vlua, int id)
{
	/* Don't need lua_pcall() to handle errors, because no one should be able to
	 * mess with internal tables. */
	vlua_state_get_table(vlua, &viewcolumns_key);
	if(lua_geti(vlua->lua, -1, id) != LUA_TTABLE)
	{
		lua_pop(vlua->lua, 2);
		return NULL;
	}

	lua_getfield(vlua->lua, -1, "name");
	char *name = strdup(lua_tostring(vlua->lua, -1));
	lua_pop(vlua->lua, 3);
	return name;
}

int
vifm_viewcolumns_is_primary(vlua_t *vlua, int column_id)
{
	if(column_id < FIRST_LUA_COLUMN_ID)
	{
		return 0;
	}

	/* Don't need lua_pcall() to handle errors, because no one should be able to
	 * mess with internal tables. */
	vlua_state_get_table(vlua, &viewcolumns_key);
	if(lua_geti(vlua->lua, -1, column_id) != LUA_TTABLE)
	{
		lua_pop(vlua->lua, 2);
		return -1;
	}

	lua_getfield(vlua->lua, -1, "isprimary");
	int is_primary = lua_toboolean(vlua->lua, -1);
	lua_pop(vlua->lua, 3);
	return is_primary;
}

int
VLUA_API(vifm_addcolumntype)(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "name", LUA_TSTRING);
	const char *name = lua_tostring(lua, -1);
	check_viewcolumn_name(vlua, name);

	check_field(lua, 1, "handler", LUA_TFUNCTION);
	void *handler = to_pointer(lua);

	int is_primary = 0;
	if(check_opt_field(lua, 1, "isprimary", LUA_TBOOLEAN))
	{
		is_primary = lua_toboolean(vlua->lua, -1);
	}

	void *data = state_store_pointer(vlua, handler);
	if(data == NULL)
	{
		return luaL_error(lua, "%s", "Failed to store handler data");
	}

	int column_id = viewcolumn_next_id++;
	vlua_state_get_table(vlua, &viewcolumns_key); /* viewcolumns table */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/2); /* viewcolumn table */
	lua_pushinteger(lua, column_id);
	lua_setfield(lua, -2, "id");
	lua_pushstring(lua, name);
	lua_setfield(lua, -2, "name");
	lua_pushboolean(lua, is_primary);
	lua_setfield(lua, -2, "isprimary");
	lua_pushvalue(lua, -1);                       /* viewcolumn table */
	lua_setfield(lua, -3, name);                  /* viewcolumns[name] */
	lua_seti(lua, -2, column_id);                 /* viewcolumns[id] */

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
	if(vifm_viewcolumns_map(vlua, name) != -1)
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
lua_viewcolumn_handler(void *data, size_t buf_len, char buf[],
		const format_info_t *info)
{
	state_ptr_t *p = data;
	lua_State *lua = p->vlua->lua;

	from_pointer(lua, p->ptr);

	lua_createtable(lua, /*narr=*/0, /*nrec=*/2);

	lua_pushinteger(lua, info->width);
	lua_setfield(lua, -2, "width");

	column_data_t *cdt = info->data;
	dir_entry_t *entry = cdt->entry;
	vifmentry_new(lua, entry);
	lua_setfield(lua, -2, "entry");

	/* No match highlighting by default. */
	cdt->custom_match = 1;
	cdt->match_from = 0;
	cdt->match_to = 0;

	const int sm_cookie = vlua_state_safe_mode_on(lua);
	if(lua_pcall(lua, 1, 1, 0) != LUA_OK)
	{
		vlua_state_safe_mode_off(lua, sm_cookie);

		const char *error = lua_tostring(lua, -1);
		ui_sb_err(error);
		copy_str(buf, buf_len, "ERROR");
		lua_pop(lua, 1);
		return;
	}

	vlua_state_safe_mode_off(lua, sm_cookie);

	if(!lua_istable(lua, -1))
	{
		copy_str(buf, buf_len, "NOVALUE");
		lua_pop(lua, 1);
		return;
	}

	if(lua_getfield(lua, -1, "text") == LUA_TNIL)
	{
		copy_str(buf, buf_len, "NOVALUE");
		lua_pop(lua, 2);
		return;
	}

	const char *text = lua_tostring(lua, -1);
	copy_str(buf, buf_len, (text == NULL ? "NOVALUE" : text));
	lua_pop(lua, 1);

	int has_start = (lua_getfield(lua, -1, "matchstart") == LUA_TNUMBER);
	int has_end = (lua_getfield(lua, -2, "matchend") == LUA_TNUMBER);
	if(has_start && has_end)
	{
		int start = lua_tointegerx(lua, -2, &has_start) - 1;
		int end = lua_tointegerx(lua, -1, &has_end) - 1;

		if(has_start && has_end && start >= 0 && end >= 0 && start <= end)
		{
			cdt->match_from = start;
			cdt->match_to = end;
		}
	}

	lua_pop(lua, 3);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
