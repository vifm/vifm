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
#include <stddef.h> /* NULL */
#include <stdlib.h> /* calloc() free() */

#include "../cfg/config.h"
#include "../compat/dtype.h"
#include "../compat/fs_limits.h"
#include "../engine/cmds.h"
#include "../engine/options.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/darray.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../cmd_core.h"
#include "../filelist.h"
#include "../filename_modifiers.h"
#include "../macros.h"
#include "../plugins.h"
#include "../status.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "common.h"
#include "vifmjob.h"
#include "vifmview.h"

/* Helper structure that bundles arbitrary pointer and state pointer . */
typedef struct
{
	vlua_t *vlua;
	void *ptr;
}
state_ptr_t;

/* State of the unit. */
struct vlua_t
{
	lua_State *lua; /* Lua state. */

	state_ptr_t **ptrs;      /* Pointers. */
	DA_INSTANCE_FIELD(ptrs); /* Declarations to enable use of DA_* on ptrs. */

	strlist_t strings; /* Interned strings. */
};

static void load_api(lua_State *lua);
static int print(lua_State *lua);
static int opts_global_index(lua_State *lua);
static int opts_global_newindex(lua_State *lua);
static int vifm_errordialog(lua_State *lua);
static int vifm_fnamemodify(lua_State *lua);
static int vifm_exists(lua_State *lua);
static int vifm_makepath(lua_State *lua);
static int cmds_add(lua_State *lua);
static int cmds_command(lua_State *lua);
static int cmds_delcommand(lua_State *lua);
static int lua_cmd_handler(const cmd_info_t *cmd_info);
static int vifm_expand(lua_State *lua);
static int sb_info(lua_State *lua);
static int sb_error(lua_State *lua);
static int sb_quick(lua_State *lua);
static int load_plugin(lua_State *lua, const char name[], plug_t *plug);
static void setup_plugin_env(lua_State *lua, plug_t *plug);
static state_ptr_t * state_store_pointer(vlua_t *vlua, void *ptr);
static const char * state_store_string(vlua_t *vlua, const char str[]);
static void set_state(lua_State *lua, vlua_t *vlua);
static vlua_t * get_state(lua_State *lua);

/* Functions of `vifm` global table. */
static const struct luaL_Reg vifm_methods[] = {
	{ "errordialog", &vifm_errordialog  },
	{ "fnamemodify", &vifm_fnamemodify  },
	{ "exists",      &vifm_exists       },
	{ "makepath",    &vifm_makepath     },
	{ "startjob",    &vifmjob_new       },
	{ "expand",      &vifm_expand       },
	{ "currview",    &vifmview_currview },
	{ NULL,          NULL               }
};

/* Functions of `vifm.cmds` table. */
static const struct luaL_Reg cmds_methods[] = {
	{ "add",        &cmds_add        },
	{ "command",    &cmds_command    },
	{ "delcommand", &cmds_delcommand },
	{ NULL,         NULL             }
};

/* Functions of `vifm.sb` table. */
static const struct luaL_Reg sb_methods[] = {
	{ "info",   &sb_info  },
	{ "error",  &sb_error },
	{ "quick",  &sb_quick },
	{ NULL,     NULL      }
};

/* Address of this variable serves as a key in Lua table. */
static char vlua_state_key;

vlua_t *
vlua_init(void)
{
	vlua_t *vlua = calloc(1, sizeof(*vlua));
	if(vlua == NULL)
	{
		return NULL;
	}

	vlua->lua = luaL_newstate();
	set_state(vlua->lua, vlua);

	luaL_requiref(vlua->lua, "base", &luaopen_base, 1);
	lua_pop(vlua->lua, 1);
	luaL_requiref(vlua->lua, LUA_TABLIBNAME, &luaopen_table, 1);
	lua_pop(vlua->lua, 1);
	luaL_requiref(vlua->lua, LUA_IOLIBNAME, &luaopen_io, 1);
	lua_pop(vlua->lua, 1);
	luaL_requiref(vlua->lua, LUA_STRLIBNAME, &luaopen_string, 1);
	lua_pop(vlua->lua, 1);
	luaL_requiref(vlua->lua, LUA_MATHLIBNAME, &luaopen_math, 1);
	lua_pop(vlua->lua, 1);

	load_api(vlua->lua);

	return vlua;
}

void
vlua_finish(vlua_t *vlua)
{
	if(vlua != NULL)
	{
		size_t i;
		for(i = 0U; i < DA_SIZE(vlua->ptrs); ++i)
		{
			free(vlua->ptrs[i]);
		}
		DA_REMOVE_ALL(vlua->ptrs);

		free_string_array(vlua->strings.items, vlua->strings.nitems);

		lua_close(vlua->lua);
		free(vlua);
	}
}

/* Fills Lua state with application-specific API. */
static void
load_api(lua_State *lua)
{
	vifmjob_init(lua);
	vifmview_init(lua);

	luaL_newmetatable(lua, "VifmPluginEnv");
	lua_pushglobaltable(lua);
	lua_setfield(lua, -2, "__index");
	lua_pop(lua, 1);

	lua_pushcfunction(lua, &print);
	lua_setglobal(lua, "print");

	luaL_newlib(lua, vifm_methods);

	lua_pushvalue(lua, -1);
	lua_setglobal(lua, "vifm");

	/* Setup vifm.cmds. */
	luaL_newlib(lua, cmds_methods);
	lua_setfield(lua, -2, "cmds");

	/* Setup vifm.opts. */
	lua_newtable(lua);
	lua_pushvalue(lua, -1);
	lua_setfield(lua, -3, "opts");
	lua_newtable(lua);
	lua_newtable(lua);
	lua_pushcfunction(lua, &opts_global_index);
	lua_setfield(lua, -2, "__index");
	lua_pushcfunction(lua, &opts_global_newindex);
	lua_setfield(lua, -2, "__newindex");
	lua_setmetatable(lua, -2);
	lua_setfield(lua, -2, "global");
	lua_pop(lua, 1);

	/* Setup vifm.plugins. */
	lua_newtable(lua);
	lua_newtable(lua);
	lua_setfield(lua, -2, "all");
	lua_setfield(lua, -2, "plugins");

	/* Setup vifm.sb. */
	luaL_newlib(lua, sb_methods);
	lua_setfield(lua, -2, "sb");

	/* vifm. */
	lua_pop(lua, 1);
}

/* Replacement of standard global `print` function.  Outputs to statusbar.
 * Doesn't return anything. */
static int
print(lua_State *lua)
{
	char *msg = NULL;
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

/* Provides read access to global options by their name as
 * `vifm.opts.global[name]`. */
static int
opts_global_index(lua_State *lua)
{
	const char *opt_name = luaL_checkstring(lua, 2);

	opt_t *opt = vle_opts_find(opt_name, OPT_ANY);
	if(opt == NULL || opt->scope == OPT_LOCAL)
	{
		return 0;
	}

	return get_opt(lua, opt);
}

/* Provides write access to global options by their name as
 * `vifm.opts.global[name] = value`. */
static int
opts_global_newindex(lua_State *lua)
{
	const char *opt_name = luaL_checkstring(lua, 2);

	opt_t *opt = vle_opts_find(opt_name, OPT_ANY);
	if(opt == NULL || opt->scope == OPT_LOCAL)
	{
		return 0;
	}

	return set_opt(lua, opt);
}

/* Member of `vifm` that displays an error dialog.  Doesn't return anything. */
static int
vifm_errordialog(lua_State *lua)
{
	const char *title = luaL_checkstring(lua, 1);
	const char *msg = luaL_checkstring(lua, 2);
	show_error_msg(title, msg);
	return 0;
}

/* Member of `vifm` that modifies path according to specifiers.  Returns
 * modified path. */
static int
vifm_fnamemodify(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	const char *modifiers = luaL_checkstring(lua, 2);
	const char *base = luaL_optstring(lua, 3, flist_get_dir(curr_view));
	lua_pushstring(lua, mods_apply(path, base, modifiers, 0));
	return 1;
}

/* Member of `vifm` that checks whether specified path exists without resolving
 * symbolic links.  Returns a boolean, which is true when path does exist. */
static int
vifm_exists(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	lua_pushboolean(lua, path_exists(path, NODEREF));
	return 1;
}

/* Member of `vifm` that creates a directory and all of its missing parent
 * directories.  Returns a boolean, which is true on success. */
static int
vifm_makepath(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	lua_pushboolean(lua, make_path(path, 0755) == 0);
	return 1;
}

/* Member of `vifm.cmds` that registers a new :command or raises an error.
 * Returns boolean, which is true on success. */
static int
cmds_add(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "name", LUA_TSTRING);
	const char *name = lua_tostring(lua, -1);

	const char *descr = "";
	if(check_opt_field(lua, 1, "description", LUA_TSTRING))
	{
		descr = state_store_string(vlua, lua_tostring(lua, -1));
	}

	check_field(lua, 1, "handler", LUA_TFUNCTION);
	void *handler = to_pointer(lua);

	cmd_add_t cmd = {
	  .name = name,
	  .abbr = NULL,
	  .id = -1,
	  .descr = descr,
	  .flags = 0,
	  .handler = &lua_cmd_handler,
	  .user_data = NULL,
	  .min_args = 0,
	  .max_args = 0,
	};

	if(check_opt_field(lua, 1, "minargs", LUA_TNUMBER))
	{
		cmd.min_args = lua_tointeger(lua, -1);
	}
	if(check_opt_field(lua, 1, "maxargs", LUA_TNUMBER))
	{
		cmd.max_args = lua_tointeger(lua, -1);
		if(cmd.max_args < 0)
		{
			cmd.max_args = NOT_DEF;
		}
	}
	else
	{
		cmd.max_args = cmd.min_args;
	}

	cmd.user_data = state_store_pointer(vlua, handler);
	if(cmd.user_data == NULL)
	{
		return luaL_error(lua, "%s", "Failed to store handler data");
	}

	lua_pushboolean(lua, vle_cmds_add_foreign(&cmd) == 0);
	return 1;
}

/* Member of `vifm.command` that registers a new user-defined :command or raises
 * an error.  Returns boolean, which is true on success. */
static int
cmds_command(lua_State *lua)
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
cmds_delcommand(lua_State *lua)
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

	lua_newtable(lua);
	lua_pushstring(lua, cmd_info->args);
	lua_setfield(lua, -2, "args");

	curr_stats.save_msg = 0;

	if(lua_pcall(lua, 1, 0, 0) != LUA_OK)
	{
		const char *error = lua_tostring(lua, -1);
		ui_sb_err(error);
		lua_pop(lua, 1);
		return CMDS_ERR_CUSTOM;
	}

	return curr_stats.save_msg;
}

/* Member of `vifm` that expands macros and environment variables.  Returns the
 * expanded string. */
static int
vifm_expand(lua_State *lua)
{
	const char *str = luaL_checkstring(lua, 1);

	char *env_expanded = expand_envvars(str, 0);
	char *full_expanded = ma_expand(env_expanded, NULL, NULL, 0);
	lua_pushstring(lua, full_expanded);
	free(env_expanded);
	free(full_expanded);

	return 1;
}

/* Member of `vifm.sb` that prints a normal message on the statusbar.  Doesn't
 * return anything. */
static int
sb_info(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_msg(msg);
	curr_stats.save_msg = 1;
	return 0;
}

/* Member of `vifm.sb` that prints an error message on the statusbar.  Doesn't
 * return anything. */
static int
sb_error(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_err(msg);
	curr_stats.save_msg = 1;
	return 0;
}

/* Member of `vifm.sb` that prints statusbar message that's not stored in
 * history.  Doesn't return anything. */
static int
sb_quick(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_quick_msgf("%s", msg);
	return 0;
}

int
vlua_load_plugin(vlua_t *vlua, const char plugin[], plug_t *plug)
{
	if(load_plugin(vlua->lua, plugin, plug) == 0)
	{
		lua_getglobal(vlua->lua, "vifm");
		lua_getfield(vlua->lua, -1, "plugins");
		lua_getfield(vlua->lua, -1, "all");
		lua_pushvalue(vlua->lua, -4);
		lua_setfield(vlua->lua, -2, plugin);
		lua_pop(vlua->lua, 4);
		return 0;
	}
	return 1;
}

/* Loads a single plugin as a module.  Returns zero on success and places value
 * that corresponds to the module onto the stack, otherwise non-zero is
 * returned. */
static int
load_plugin(lua_State *lua, const char name[], plug_t *plug)
{
	char full_path[PATH_MAX + 32];
	snprintf(full_path, sizeof(full_path), "%s/plugins/%s/init.lua",
			cfg.config_dir, name);

	if(luaL_loadfile(lua, full_path))
	{
		const char *error = lua_tostring(lua, -1);
		plug_log(plug, error);
		ui_sb_errf("Failed to load '%s' plugin: %s", name, error);
		lua_pop(lua, 1);
		return 1;
	}

	setup_plugin_env(lua, plug);
	if(lua_pcall(lua, 0, 1, 0))
	{
		const char *error = lua_tostring(lua, -1);
		plug_log(plug, error);
		ui_sb_errf("Failed to start '%s' plugin: %s", name, error);
		lua_pop(lua, 1);
		return 1;
	}

	if(lua_gettop(lua) == 0 || !lua_istable(lua, -1))
	{
		ui_sb_errf("Failed to load '%s' plugin: %s", name,
				"it didn't return a table");
		if(lua_gettop(lua) > 0)
		{
			lua_pop(lua, 1);
		}
		return 1;
	}

	return 0;
}

/* Sets upvalue #1 to a plugin-specific version environment. */
static void
setup_plugin_env(lua_State *lua, plug_t *plug)
{
	lua_newtable(lua);
	lua_pushlightuserdata(lua, plug);
	lua_pushcclosure(lua, &print, 1);
	lua_setfield(lua, -2, "print");
	luaL_getmetatable(lua, "VifmPluginEnv");
	lua_setmetatable(lua, -2);

	if(lua_setupvalue(lua, -2, 1) == NULL)
	{
		lua_pop(lua, 1);
	}
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

/* Stores pointer within the state. */
static state_ptr_t *
state_store_pointer(vlua_t *vlua, void *ptr)
{
	state_ptr_t **p = DA_EXTEND(vlua->ptrs);
	if(p == NULL)
	{
		return NULL;
	}

	*p = malloc(sizeof(**p));
	if(*p == NULL)
	{
		return NULL;
	}

	(*p)->vlua = vlua;
	(*p)->ptr = ptr;
	DA_COMMIT(vlua->ptrs);
	return *p;
}

/* Stores a string within the state.  Returns pointer to the interned string or
 * pointer to "" on error. */
static const char *
state_store_string(vlua_t *vlua, const char str[])
{
	int n = add_to_string_array(&vlua->strings.items, vlua->strings.nitems, str);
	if(n == vlua->strings.nitems)
	{
		return "";
	}

	vlua->strings.nitems = n;
	return vlua->strings.items[n - 1];
}

/* Stores pointer to vlua inside Lua state. */
static void
set_state(lua_State *lua, vlua_t *vlua)
{
	lua_pushlightuserdata(lua, &vlua_state_key);
	lua_pushlightuserdata(lua, vlua);
	lua_settable(lua, LUA_REGISTRYINDEX);
}

/* Retrieves pointer to vlua from Lua state. */
static vlua_t *
get_state(lua_State *lua)
{
	lua_pushlightuserdata(lua, &vlua_state_key);
	lua_gettable(lua, LUA_REGISTRYINDEX);
	vlua_t *vlua = lua_touserdata(lua, -1);
	lua_pop(lua, 1);
	return vlua;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
