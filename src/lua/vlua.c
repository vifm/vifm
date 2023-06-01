/* vifm
 * Copyright (C) 2020 xaizek.
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

#include "vlua.h"

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../compat/dtype.h"
#include "../compat/fs_limits.h"
#include "../engine/variables.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../cmd_core.h"
#include "../macros.h"
#include "../plugins.h"
#include "../status.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"
#include "vifm.h"
#include "vifm_cmds.h"
#include "vifm_events.h"
#include "vifm_handlers.h"
#include "vifm_viewcolumns.h"
#include "vifmjob.h"
#include "vlua_cbacks.h"
#include "vlua_state.h"

static void patch_env(lua_State *lua);
static void load_api(lua_State *lua);
static int VLUA_API(print)(lua_State *lua);
static int VLUA_API(os_getenv)(lua_State *lua);
static int VLUA_API(vifm_plugin_require)(lua_State *lua);
static int VLUA_IMPL(require_plugin_module)(lua_State *lua);
static int load_plugin(lua_State *lua, plug_t *plug);
static void setup_plugin_env(lua_State *lua, plug_t *plug);

VLUA_DECLARE_SAFE(print);
VLUA_DECLARE_SAFE(os_getenv);
VLUA_DECLARE_UNSAFE(vifm_plugin_require);

/* Defined in another unit. */
VLUA_DECLARE_SAFE(vifm_addhandler);

/* Address of this variable serves as a key in Lua table. */
static char plugin_envs;

vlua_t *
vlua_init(void)
{
	vlua_t *vlua = vlua_state_alloc();
	if(vlua == NULL)
	{
		return NULL;
	}

	patch_env(vlua->lua);
	load_api(vlua->lua);

	vlua_cbacks_init(vlua);
	vifm_viewcolumns_init(vlua);
	vifm_handlers_init(vlua);

	vlua_state_make_table(vlua, &plugin_envs);

	return vlua;
}

void
vlua_finish(vlua_t *vlua)
{
	if(vlua != NULL)
	{
		vifmjob_finish(vlua->lua);
		vlua_state_free(vlua);
	}
}

/* Adjusts standard libraries. */
static void
patch_env(lua_State *lua)
{
	lua_pushcfunction(lua, VLUA_REF(print));
	lua_setglobal(lua, "print");

	lua_getglobal(lua, "os");
	lua_createtable(lua, /*narr=*/0, /*nrec=*/6);
	lua_getfield(lua, -2, "clock");
	lua_setfield(lua, -2, "clock");
	lua_getfield(lua, -2, "date");
	lua_setfield(lua, -2, "date");
	lua_getfield(lua, -2, "difftime");
	lua_setfield(lua, -2, "difftime");
	lua_pushcfunction(lua, VLUA_REF(os_getenv));
	lua_setfield(lua, -2, "getenv");
	lua_getfield(lua, -2, "time");
	lua_setfield(lua, -2, "time");
	lua_getfield(lua, -2, "tmpname");
	lua_setfield(lua, -2, "tmpname");
	lua_setglobal(lua, "os");
	lua_pop(lua, 1);
}

/* Fills Lua state with application-specific API. */
static void
load_api(lua_State *lua)
{
	make_metatable(lua, "VifmPluginEnv");
	lua_pushglobaltable(lua);
	lua_setfield(lua, -2, "__index");
	lua_pop(lua, 1);

	vifm_init(lua);
	lua_setglobal(lua, "vifm");
}

/* Replacement of standard global `print` function.  If plugin is present,
 * outputs to a plugin log, otherwise outputs to status bar.  Doesn't return
 * anything. */
static int
VLUA_API(print)(lua_State *lua)
{
	char *msg = strdup("");
	size_t msg_len = 0U;

	int nargs = lua_gettop(lua);
	int i;
	for(i = 0; i < nargs; ++i)
	{
		const char *piece = luaL_tolstring(lua, i + 1, NULL);
		if(i > 0)
		{
			(void)strappendch(&msg, &msg_len, '\t');
		}
		(void)strappend(&msg, &msg_len, piece);
		lua_pop(lua, 1);
	}

	plug_t *plug = lua_touserdata(lua, lua_upvalueindex(1));
	if(plug != NULL)
	{
		plug_log(plug, msg);
	}
	else
	{
		ui_sb_msg(msg);
		curr_stats.save_msg = 1;
	}

	free(msg);
	return 0;
}

/* os.getenv() that's aware of Vifm's internal variables. */
static int
VLUA_API(os_getenv)(lua_State *lua)
{
	lua_pushstring(lua, local_getenv_null(luaL_checkstring(lua, 1)));
	return 1;
}

int
vlua_load_plugin(vlua_t *vlua, plug_t *plug)
{
	if(load_plugin(vlua->lua, plug) == 0)
	{
		lua_getglobal(vlua->lua, "vifm");
		lua_getfield(vlua->lua, -1, "plugins");
		lua_getfield(vlua->lua, -1, "all");
		lua_pushvalue(vlua->lua, -4);
		lua_setfield(vlua->lua, -2, plug->name);
		lua_pop(vlua->lua, 4);
		return 0;
	}
	return 1;
}

/* Loads a single plugin as a module.  Returns zero on success and places value
 * that corresponds to the module onto the stack, otherwise non-zero is
 * returned. */
static int
load_plugin(lua_State *lua, plug_t *plug)
{
	char full_path[PATH_MAX + 32];
	snprintf(full_path, sizeof(full_path), "%s/init.lua", plug->path);

	if(luaL_loadfile(lua, full_path) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		plug_log(plug, error);
		ui_sb_errf("Failed to load '%s' plugin: %s", plug->name, error);
		lua_pop(lua, 1);
		return 1;
	}

	setup_plugin_env(lua, plug);
	if(lua_pcall(lua, 0, 1, 0) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		plug_log(plug, error);
		ui_sb_errf("Failed to start '%s' plugin: %s", plug->name, error);
		lua_pop(lua, 1);
		return 1;
	}

	if(lua_gettop(lua) == 0 || !lua_istable(lua, -1))
	{
		ui_sb_errf("Failed to load '%s' plugin: %s", plug->name,
				"it didn't return a table");
		if(lua_gettop(lua) > 0)
		{
			lua_pop(lua, 1);
		}
		return 1;
	}

	return 0;
}

/* Sets upvalue #1 to a plugin-specific version of environment. */
static void
setup_plugin_env(lua_State *lua, plug_t *plug)
{
	/* Global environment table. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/1);
	luaL_getmetatable(lua, "VifmPluginEnv");
	lua_setmetatable(lua, -2);
	/* Don't let a plugin access true global table by using _G explicitly. */
	lua_pushvalue(lua, -1);
	lua_setfield(lua, -2, "_G");

	/* Plugin-specific `vifm` table. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/2);
	/* Meta-table for it. */
	make_metatable(lua, /*name=*/NULL);
	lua_getglobal(lua, "vifm");
	lua_setfield(lua, -2, "__index");
	lua_setmetatable(lua, -2);

	/* Plugin-specific `vifm.plugin` table. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/3);
	lua_pushstring(lua, plug->name);
	lua_setfield(lua, -2, "name");
	lua_pushstring(lua, plug->path);
	lua_setfield(lua, -2, "path");
	/* Plugin-specific `vifm.plugin.require`. */
	lua_pushlightuserdata(lua, plug);
	lua_pushcclosure(lua, VLUA_REF(vifm_plugin_require), 1);
	lua_setfield(lua, -2, "require");
	lua_setfield(lua, -2, "plugin");  /* vifm.plugin */

	/* Plugin-specific `vifm.addhandler()`. */
	lua_pushlightuserdata(lua, plug);
	lua_pushcclosure(lua, VLUA_REF(vifm_addhandler), 1);
	lua_setfield(lua, -2, "addhandler"); /* vifm.addhandler */

	/* Assign `vifm` as a plugin-specific global. */
	lua_setfield(lua, -2, "vifm");

	/* Plugin-specific `print()`. */
	lua_pushlightuserdata(lua, plug);
	lua_pushcclosure(lua, VLUA_REF(print), 1);
	lua_setfield(lua, -2, "print");

	/* Map plug to plugin environment for future queries. */
	vlua_state_get_table(get_state(lua), &plugin_envs);
	lua_pushlightuserdata(lua, plug);
	lua_pushvalue(lua, -3);
	lua_settable(lua, -3);
	lua_pop(lua, 1);

	if(lua_setupvalue(lua, -2, 1) == NULL)
	{
		lua_pop(lua, 1);
	}
}

/* Member of `vifm.plugin` that loads a module relative to the plugin's root.
 * Returns module's return. */
static int
VLUA_API(vifm_plugin_require)(lua_State *lua)
{
	const char *mod_name = luaL_checkstring(lua, 1);

	plug_t *plug = lua_touserdata(lua, lua_upvalueindex(1));
	if(plug == NULL)
	{
		assert(0 && "vifm.plugin.require() called outside a plugin?");
		return 0;
	}

	char full_path[PATH_MAX + 1];
	snprintf(full_path, sizeof(full_path), "%s/%s.lua", plug->path, mod_name);
	if(!path_exists(full_path, DEREF))
	{
		snprintf(full_path, sizeof(full_path), "%s/%s/init.lua", plug->path,
				mod_name);
	}

	/* luaL_requiref() equivalent that passes plug to require_plugin_module(). */
	luaL_getsubtable(lua, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
	lua_getfield(lua, -1, full_path);
	if(!lua_toboolean(lua, -1))
	{
		lua_pop(lua, 1);
		lua_pushlightuserdata(lua, plug);
		lua_pushcclosure(lua, VLUA_IREF(require_plugin_module), 1);
		lua_pushstring(lua, full_path);
		lua_call(lua, 1, 1);
		lua_pushvalue(lua, -1);
		lua_setfield(lua, -3, full_path);
	}
	lua_remove(lua, -2);

	return 1;
}

/* Helper that loads a module.  Returns module's return. */
static int
VLUA_IMPL(require_plugin_module)(lua_State *lua)
{
	const char *mod = luaL_checkstring(lua, 1);
	if(luaL_loadfile(lua, mod) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		return luaL_error(lua, "vifm.plugin.require('%s'): %s", mod, error);
	}

	/* Fetch custom environment of the current plugin. */
	plug_t *plug = lua_touserdata(lua, lua_upvalueindex(1));
	assert(plug != NULL && "Invalid call to require_plugin_module()");
	vlua_state_get_table(get_state(lua), &plugin_envs);
	lua_pushlightuserdata(lua, plug);
	if(lua_gettable(lua, -2) != LUA_TTABLE)
	{
		return luaL_error(lua,
				"vifm.plugin.require('%s'): failed to fetch plugin env", mod);
	}
	lua_remove(lua, -2);

	/* Use that environment for the newly loaded module. */
	if(lua_setupvalue(lua, -2, 1) == NULL)
	{
		return luaL_error(lua,
				"vifm.plugin.require('%s'): failed to copy plugin env", mod);
	}

	if(lua_pcall(lua, 0, 1, 0) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		return luaL_error(lua, "vifm.plugin.require('%s'): %s", mod, error);
	}
	return 1;
}

int
vlua_run_string(vlua_t *vlua, const char str[])
{
	int old_top = lua_gettop(vlua->lua);

	int errored = luaL_dostring(vlua->lua, str);
	if(errored)
	{
		ui_sb_err(lua_tostring(vlua->lua, -1));
	}

	lua_settop(vlua->lua, old_top);
	return errored;
}

int
vlua_complete_cmd(vlua_t *vlua, const struct cmd_info_t *cmd_info, int arg_pos)
{
	return vifm_cmds_complete(vlua->lua, cmd_info, arg_pos);
}

int
vlua_viewcolumns_next_id(vlua_t *vlua)
{
	return vifm_viewcolumns_next_id(vlua);
}

int
vlua_viewcolumn_map(vlua_t *vlua, const char name[])
{
	return vifm_viewcolumns_map(vlua, name);
}

char *
vlua_viewcolumn_map_back(vlua_t *vlua, int id)
{
	return vifm_viewcolumns_map_back(vlua, id);
}

int
vlua_viewcolumn_is_primary(vlua_t *vlua, int column_id)
{
	return vifm_viewcolumns_is_primary(vlua, column_id);
}

int
vlua_handler_cmd(vlua_t *vlua, const char cmd[])
{
	return vifm_handlers_check(vlua, cmd);
}

int
vlua_handler_present(vlua_t *vlua, const char cmd[])
{
	return vifm_handlers_present(vlua, cmd);
}

strlist_t
vlua_view_file(vlua_t *vlua, const char viewer[], const char path[],
		const struct preview_area_t *parea)
{
	return vifm_handlers_view(vlua, viewer, path, parea);
}

void
vlua_open_file(vlua_t *vlua, const char prog[], const struct dir_entry_t *entry)
{
	return vifm_handlers_open(vlua, prog, entry);
}

char *
vlua_make_status_line(vlua_t *vlua, const char format[], struct view_t *view,
		int width)
{
	return vifm_handlers_make_status_line(vlua, format, view, width);
}

char *
vlua_make_tab_line(vlua_t *vlua, const char format[], int other, int width)
{
	return vifm_handlers_make_tab_line(vlua, format, other, width);
}

int
vlua_open_help(vlua_t *vlua, const char handler[], const char topic[])
{
	return vifm_handlers_open_help(vlua, handler, topic);
}

int
vlua_edit_one(vlua_t *vlua, const char handler[], const char path[], int line,
		int column, int must_wait)
{
	return vifm_handlers_edit_one(vlua, handler, path, line, column, must_wait);
}

int
vlua_edit_many(vlua_t *vlua, const char handler[], char *files[], int nfiles)
{
	return vifm_handlers_edit_many(vlua, handler, files, nfiles);
}

int
vlua_edit_list(vlua_t *vlua, const char handler[], char *entries[],
		int nentries, int current, int quickfix_format)
{
	return vifm_handlers_edit_list(vlua, handler, entries, nentries, current,
			quickfix_format);
}

void
vlua_process_callbacks(vlua_t *vlua)
{
	vlua_cbacks_process(vlua);
}

void
vlua_events_app_exit(vlua_t *vlua)
{
	if(vlua != NULL)
	{
		vifm_events_app_exit(vlua);
	}
}

void
vlua_events_app_fsop(vlua_t *vlua, OPS op, const char path[],
		const char target[], void *extra, int dir)
{
	if(vlua != NULL)
	{
		vifm_events_app_fsop(vlua, op, path, target, extra, dir);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
