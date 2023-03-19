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

#include "vifmview.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() */

#include "../engine/mode.h"
#include "../engine/options.h"
#include "../modes/modes.h"
#include "../modes/visual.h"
#include "../ui/tabs.h"
#include "../ui/ui.h"
#include "../filelist.h"
#include "../flist_sel.h"
#include "../opt_handlers.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vlua_state.h"
#include "vifmentry.h"

static int VLUA_API(vifmview_index)(lua_State *lua);
static int VLUA_API(viewopts_index)(lua_State *lua);
static int VLUA_API(viewopts_newindex)(lua_State *lua);
static opt_t * find_view_opt(const char name[]);
static int VLUA_API(locopts_index)(lua_State *lua);
static int VLUA_API(locopts_newindex)(lua_State *lua);
static int do_opt(lua_State *lua, opt_t *opt, int set);
static int VLUA_IMPL(restore_curr_view)(lua_State *lua);
static int VLUA_IMPL(get_opt_wrapper)(lua_State *lua);
static int VLUA_IMPL(set_opt_wrapper)(lua_State *lua);
static int VLUA_API(vifmview_cd)(lua_State *lua);
static int VLUA_API(vifmview_entry)(lua_State *lua);
static int VLUA_API(vifmview_select)(lua_State *lua);
static int VLUA_API(vifmview_unselect)(lua_State *lua);
static int select_unselect(lua_State *lua, int select);
static view_t * check_view(lua_State *lua);
static view_t * find_view(lua_State *lua, unsigned int id);

VLUA_DECLARE_SAFE(vifmview_index);
VLUA_DECLARE_SAFE(viewopts_index);
VLUA_DECLARE_UNSAFE(viewopts_newindex);
VLUA_DECLARE_SAFE(locopts_index);
VLUA_DECLARE_UNSAFE(locopts_newindex);
VLUA_DECLARE_UNSAFE(vifmview_cd);
VLUA_DECLARE_SAFE(vifmview_entry);
VLUA_DECLARE_UNSAFE(vifmview_select);
VLUA_DECLARE_UNSAFE(vifmview_unselect);

/* Methods of VifmView type. */
static const luaL_Reg vifmview_methods[] = {
	{ "cd",       VLUA_REF(vifmview_cd)       },
	{ "entry",    VLUA_REF(vifmview_entry)    },
	{ "select",   VLUA_REF(vifmview_select)   },
	{ "unselect", VLUA_REF(vifmview_unselect) },
	{ NULL,       NULL                        }
};

void
vifmview_init(struct lua_State *lua)
{
	make_metatable(lua, "VifmView");
	lua_pushcfunction(lua, VLUA_REF(vifmview_index));
	lua_setfield(lua, -2, "__index");
	luaL_setfuncs(lua, vifmview_methods, 0);
	lua_pop(lua, 1);
}

/* Handles indexing of `VifmView` objects. */
static int
VLUA_API(vifmview_index)(lua_State *lua)
{
	const char *key = luaL_checkstring(lua, 2);

	int viewopts;
	if(strcmp(key, "viewopts") == 0)
	{
		viewopts = 1;
	}
	else if(strcmp(key, "locopts") == 0)
	{
		viewopts = 0;
	}
	else if(strcmp(key, "custom") == 0)
	{
		view_t *view = check_view(lua);
		if(!flist_custom_active(view))
		{
			lua_pushnil(lua);
			return 1;
		}

		lua_createtable(lua, /*narr=*/0, /*nrec=*/2);
		lua_pushstring(lua, view->custom.title);
		lua_setfield(lua, -2, "title");
		lua_pushstring(lua, cv_describe(view->custom.type));
		lua_setfield(lua, -2, "type");
		return 1;
	}
	else if(strcmp(key, "cwd") == 0)
	{
		view_t *view = check_view(lua);
		lua_pushstring(lua, flist_get_dir(view));
		return 1;
	}
	else if(strcmp(key, "entrycount") == 0)
	{
		view_t *view = check_view(lua);
		lua_pushinteger(lua, view->list_rows);
		return 1;
	}
	else if(strcmp(key, "currententry") == 0)
	{
		view_t *view = check_view(lua);
		lua_pushinteger(lua, view->list_pos + 1);
		return 1;
	}
	else
	{
		if(lua_getmetatable(lua, 1) == 0)
		{
			return 0;
		}
		lua_pushvalue(lua, 2);
		lua_rawget(lua, -2);
		return 1;
	}

	/* This complication is here because functions of `viewopts` and `locopts`
	 * need to know on which view they are being called. */

	const unsigned int *id = luaL_checkudata(lua, 1, "VifmView");

	unsigned int *id_copy = lua_newuserdatauv(lua, sizeof(*id_copy), 0);
	*id_copy = *id;

	make_metatable(lua, /*name=*/NULL);
	lua_pushvalue(lua, -1);
	lua_setmetatable(lua, -2);
	lua_pushcfunction(lua,
			viewopts ? VLUA_REF(viewopts_index) : VLUA_REF(locopts_index));
	lua_setfield(lua, -2, "__index");
	lua_pushcfunction(lua,
		viewopts ? VLUA_REF(viewopts_newindex) : VLUA_REF(locopts_newindex));
	lua_setfield(lua, -2, "__newindex");
	lua_setmetatable(lua, -2);
	return 1;
}

/* Provides read access to view options by their name as
 * `VifmView:viewopts[name]`.  These are "global" values of view options. */
static int
VLUA_API(viewopts_index)(lua_State *lua)
{
	const char *opt_name = luaL_checkstring(lua, 2);
	opt_t *opt = find_view_opt(opt_name);
	if(opt == NULL)
	{
		return 0;
	}

	return do_opt(lua, opt, /*set=*/0);
}

/* Provides write access to view options by their name as
 * `VifmView:viewopts[name] = value`.  These are "global" values of view
 * options. */
static int
VLUA_API(viewopts_newindex)(lua_State *lua)
{
	const char *opt_name = luaL_checkstring(lua, 2);
	opt_t *opt = find_view_opt(opt_name);
	if(opt == NULL)
	{
		return 0;
	}

	return do_opt(lua, opt, /*set=*/1);
}

/* Looks up view-specific option by its name.  Returns the option or NULL. */
static opt_t *
find_view_opt(const char name[])
{
	/* We query this to implicitly check that option is a local one... */
	opt_t *opt = vle_opts_find(name, OPT_LOCAL);
	if(opt == NULL)
	{
		return NULL;
	}

	return vle_opts_find(name, OPT_GLOBAL);
}

/* Provides read access to location-specific options by their name as
 * `VifmView:viewopts[name]`.  These are "local" values of location-specific
 * options. */
static int
VLUA_API(locopts_index)(lua_State *lua)
{
	const char *opt_name = luaL_checkstring(lua, 2);
	opt_t *opt = vle_opts_find(opt_name, OPT_LOCAL);
	if(opt == NULL)
	{
		return 0;
	}

	return do_opt(lua, opt, /*set=*/0);
}

/* Provides write access to location-specific options by their name as
 * `VifmView:viewopts[name] = value`.  These are "local" values of
 * location-specific options. */
static int
VLUA_API(locopts_newindex)(lua_State *lua)
{
	const char *opt_name = luaL_checkstring(lua, 2);
	opt_t *opt = vle_opts_find(opt_name, OPT_LOCAL);
	if(opt == NULL)
	{
		return 0;
	}

	return do_opt(lua, opt, /*set=*/1);
}

/* Reads or writes an option of a view.  Returns number of results. */
static int
do_opt(lua_State *lua, opt_t *opt, int set)
{
	const unsigned int *id = lua_touserdata(lua, 1);
	view_t *view = find_view(lua, *id);

	if(view == curr_view)
	{
		return (set ? set_opt(lua, opt) : get_opt(lua, opt));
	}

	/* XXX: have to go extra mile to restore `curr_view` on error. */

	view_t *curr = curr_view;
	curr_view = view;
	load_view_options(curr_view);

	lua_pushlightuserdata(lua, curr);
	lua_pushcclosure(lua, VLUA_IREF(restore_curr_view), 1);
	lua_pushcfunction(lua,
			set ? VLUA_IREF(set_opt_wrapper) : VLUA_IREF(get_opt_wrapper));
	lua_pushlightuserdata(lua, opt);
	lua_pushvalue(lua, 2);
	lua_pushvalue(lua, 3);

	int nresults = (set ? 0 : 1);
	if(lua_pcall(lua, 3, nresults, -5) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		return luaL_error(lua, "%s", error);
	}

	curr_view = curr;
	load_view_options(curr_view);

	return nresults;
}

/* Restores `curr_view` after an error. */
static int
VLUA_IMPL(restore_curr_view)(lua_State *lua)
{
	view_t *curr = lua_touserdata(lua, lua_upvalueindex(1));
	curr_view = curr;
	load_view_options(curr_view);
	return 1;
}

/* Lua-wrapper of get_opt(). */
static int
VLUA_IMPL(get_opt_wrapper)(lua_State *lua)
{
	opt_t *opt = lua_touserdata(lua, 1);
	return get_opt(lua, opt);
}

/* Lua-wrapper of set_opt(). */
static int
VLUA_IMPL(set_opt_wrapper)(lua_State *lua)
{
	opt_t *opt = lua_touserdata(lua, 1);
	return set_opt(lua, opt);
}

void
vifmview_new(lua_State *lua, view_t *view)
{
	unsigned int *data = lua_newuserdatauv(lua, sizeof(*data), 0);
	*data = view->id;

	luaL_getmetatable(lua, "VifmView");
	lua_setmetatable(lua, -2);
}

int
VLUA_API(vifmview_currview)(lua_State *lua)
{
	vifmview_new(lua, curr_view);
	return 1;
}

int
VLUA_API(vifmview_otherview)(lua_State *lua)
{
	vifmview_new(lua, other_view);
	return 1;
}

/* Method of `VifmView` that changes directory of a view.  Returns boolean,
 * which is true if location change was successful. */
static int
VLUA_API(vifmview_cd)(lua_State *lua)
{
	view_t *view = check_view(lua);

	const char *path = luaL_checkstring(lua, 2);
	int success = (navigate_to(view, path) == 0);
	lua_pushboolean(lua, success);
	return 1;
}

/* Method of `VifmView` that retrieves an entry by its index.  Returns an
 * instance of `VifmEntry` or `nil` on wrong index. */
static int
VLUA_API(vifmview_entry)(lua_State *lua)
{
	view_t *view = check_view(lua);

	int idx = luaL_checkinteger(lua, 2) - 1;
	if(idx < 0 || idx >= view->list_rows)
	{
		lua_pushnil(lua);
		return 1;
	}

	vifmentry_new(lua, &view->dir_entry[idx]);
	return 1;
}

/* Method of `VifmView` that selects entries a view.  Returns number of new
 * selected entries. */
static int
VLUA_API(vifmview_select)(lua_State *lua)
{
	return select_unselect(lua, /*select=*/1);
}

/* Method of `VifmView` that unselects entries in a view.  Returns number of new
 * unselected entries. */
static int
VLUA_API(vifmview_unselect)(lua_State *lua)
{
	return select_unselect(lua, /*select=*/0);
}

/* Selects or unselects entries in a view.  Returns number of entries that
 * changed selection state. */
static int
select_unselect(lua_State *lua, int select)
{
	view_t *view = check_view(lua);

	if(vle_mode_is(VISUAL_MODE) && !modvis_is_amending())
	{
		lua_pushinteger(lua, 0);
		return 1;
	}

	int count = 0;
	int *indexes = NULL;
	if(extract_indexes(lua, view, &count, &indexes) != 0)
	{
		lua_pushinteger(lua, 0);
		return 1;
	}

	const int was_selected = view->selected_files;

	flist_sel_by_indexes(view, count, indexes, select);
	free(indexes);

	int num = select ? (view->selected_files - was_selected)
	                 : (was_selected - view->selected_files);
	lua_pushinteger(lua, num);
	return 1;
}

/* Resolves `VifmView` user data in the first argument.  Returns the pointer or
 * aborts (Lua does longjmp()) if the view doesn't exist anymore. */
static view_t *
check_view(lua_State *lua)
{
	unsigned int *id = luaL_checkudata(lua, 1, "VifmView");
	return find_view(lua, *id);
}

/* Finds a view by its id.  Returns the pointer or aborts (Lua does longjmp())
 * if the view doesn't exist anymore. */
static view_t *
find_view(lua_State *lua, unsigned int id)
{
	if(lwin.id == id)
	{
		return &lwin;
	}
	if(rwin.id == id)
	{
		return &rwin;
	}

	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		if(tab_info.view->id == id)
		{
			return tab_info.view;
		}
	}

	luaL_error(lua, "%s", "Invalid VifmView object (associated view is dead)");
	return NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
