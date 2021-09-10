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
#include "../utils/fs.h"
#include "../utils/str.h"
#include "../filelist.h"
#include "../types.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "common.h"
#include "vlua_state.h"

/* User data of view entry object. */
typedef struct
{
	char *full_path; /* Full path to the file. */
	FileType type;   /* Type of the file. */
}
vifm_entry_t;

static int check_viewcolumn_name(vlua_t *vlua, const char name[]);
static void lua_viewcolumn_handler(void *data, size_t buf_len, char buf[],
		const format_info_t *info);
static void vifmentry_new(lua_State *lua, dir_entry_t *entry);
static int vifmentry_gc(lua_State *lua);
static int vifmentry_gettarget(lua_State *lua);

/* Minimal ID for columns added by this view. */
enum { FIRST_LUA_COLUMN_ID = SK_TOTAL };

/* Address of this variable serves as a key in Lua table.  The associated table
 * is doubly keyed: by column name and by corresponding ID. */
static char viewcolumns_key;
/* Next id for a view column. */
static int viewcolumn_next_id = FIRST_LUA_COLUMN_ID;

/* Methods of VifmEntry type. */
static const luaL_Reg vifmentry_methods[] = {
	{ "__gc", &vifmentry_gc },
	{ NULL,   NULL          }
};

void
vifm_viewcolumns_init(vlua_t *vlua)
{
	vlua_state_make_table(vlua, &viewcolumns_key);

	luaL_newmetatable(vlua->lua, "VifmEntry");
	lua_pushvalue(vlua->lua, -1);
	lua_setfield(vlua->lua, -2, "__index");
	luaL_setfuncs(vlua->lua, vifmentry_methods, 0);
	lua_pop(vlua->lua, 1);
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
vifm_addcolumntype(lua_State *lua)
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
	vlua_state_get_table(vlua, &viewcolumns_key);
	lua_newtable(lua);
	lua_pushinteger(lua, column_id);
	lua_setfield(lua, -2, "id");
	lua_pushboolean(lua, is_primary);
	lua_setfield(lua, -2, "isprimary");
	lua_pushvalue(lua, -1);
	lua_setfield(lua, -3, name);
	lua_seti(lua, -2, column_id);

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

	lua_newtable(lua);

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

	if(lua_pcall(lua, 1, 1, 0) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		ui_sb_err(error);
		copy_str(buf, buf_len, "ERROR");
		lua_pop(lua, 1);
		return;
	}

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

/* Creates a new VifmEntry (actually a table where user data is passed using
 * upvalues).  Leaves it on the top of the stack. */
static void
vifmentry_new(lua_State *lua, dir_entry_t *entry)
{
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
	lua_pushstring(lua, get_type_str(entry->type));
	lua_setfield(lua, -2, "type");

	int match = (entry->search_match != 0);
	lua_pushboolean(lua, match);
	lua_setfield(lua, -2, "match");
	lua_pushinteger(lua, (match ? entry->match_left + 1 : 0));
	lua_setfield(lua, -2, "matchstart");
	lua_pushinteger(lua, (match ? entry->match_right + 1 : 0));
	lua_setfield(lua, -2, "matchend");

	const char *prefix, *suffix;
	ui_get_decors(entry, &prefix, &suffix);
	lua_newtable(lua);
	lua_pushstring(lua, prefix);
	lua_setfield(lua, -2, "prefix");
	lua_pushstring(lua, suffix);
	lua_setfield(lua, -2, "suffix");
	lua_setfield(lua, -2, "classify");

	vifm_entry_t *vifm_entry = lua_newuserdatauv(lua, sizeof(*vifm_entry), 0);
	luaL_getmetatable(lua, "VifmEntry");
	lua_setmetatable(lua, -2);

	lua_pushcclosure(lua, &vifmentry_gettarget, 1);
	lua_setfield(lua, -2, "gettarget");

	char full_path[PATH_MAX + 1];
	get_full_path_of(entry, sizeof(full_path), full_path);

	vifm_entry->full_path = strdup(full_path);
	vifm_entry->type = entry->type;
}

/* Frees memory allocated to vifm_entry_t fields. */
static int
vifmentry_gc(lua_State *lua)
{
	vifm_entry_t *vifm_entry = luaL_checkudata(lua, 1, "VifmEntry");
	free(vifm_entry->full_path);
	return 0;
}

/* Gets target of a symbolic link. */
static int
vifmentry_gettarget(lua_State *lua)
{
	vifm_entry_t *vifm_entry = lua_touserdata(lua, lua_upvalueindex(1));

	if(vifm_entry->type != FT_LINK)
	{
		return luaL_error(lua, "%s", "Entry is not a symbolic link");
	}

	char link_to[PATH_MAX + 1];
	if(get_link_target(vifm_entry->full_path, link_to, sizeof(link_to)) != 0)
	{
		return luaL_error(lua, "%s", "Failed to resolve symbolic link");
	}

	lua_pushstring(lua, link_to);
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
