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
#include "../compat/pthread.h"
#include "../engine/cmds.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/darray.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../background.h"
#include "../cmd_core.h"
#include "../filelist.h"
#include "../filename_modifiers.h"
#include "../macros.h"
#include "../plugins.h"
#include "../status.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"

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
static int vifm_errordialog(lua_State *lua);
static int vifm_fnamemodify(lua_State *lua);
static int vifm_exists(lua_State *lua);
static int vifm_makepath(lua_State *lua);
static int vifm_startjob(lua_State *lua);
static int vifm_addcommand(lua_State *lua);
static void * to_pointer(lua_State *lua);
static void from_pointer(lua_State *lua, void *ptr);
static int lua_cmd_handler(const cmd_info_t *cmd_info);
static int vifm_expand(lua_State *lua);
static int vifm_change_dir(lua_State *lua);
static int vifmjob_gc(lua_State *lua);
static int vifmjob_wait(lua_State *lua);
static int vifmjob_exitcode(lua_State *lua);
static int vifmjob_stdout(lua_State *lua);
static int vifmjob_errors(lua_State *lua);
static int sb_info(lua_State *lua);
static int sb_error(lua_State *lua);
static int sb_quick(lua_State *lua);
static int jobstream_close(lua_State *lua);
static int load_plugin(lua_State *lua, const char name[], plug_t *plug);
static void setup_plugin_env(lua_State *lua, plug_t *plug);
static state_ptr_t * state_store_pointer(vlua_t *vlua, void *ptr);
static const char * state_store_string(vlua_t *vlua, const char str[]);
static void set_state(lua_State *lua, vlua_t *vlua);
static vlua_t * get_state(lua_State *lua);
static void check_field(lua_State *lua, int table_idx, const char name[],
		int lua_type);
static int check_opt_field(lua_State *lua, int table_idx, const char name[],
		int lua_type);

/* Functions of `vifm` global table. */
static const struct luaL_Reg vifm_methods[] = {
	{ "errordialog", &vifm_errordialog },
	{ "fnamemodify", &vifm_fnamemodify },
	{ "exists",      &vifm_exists      },
	{ "makepath",    &vifm_makepath    },
	{ "startjob",    &vifm_startjob    },
	{ "addcommand",  &vifm_addcommand  },
	{ "expand",      &vifm_expand      },
	{ "cd",          &vifm_change_dir  },
	{ NULL,          NULL         }
};

/* Functions of `vifm.sb` table. */
static const struct luaL_Reg sb_methods[] = {
	{ "info",   &sb_info  },
	{ "error",  &sb_error },
	{ "quick",  &sb_quick },
	{ NULL,     NULL      }
};

/* Wrapper around file stream associated with job's output stream. */
typedef struct
{
	luaL_Stream lua_stream; /* Standard Lua structure. */
	bg_job_t *job;          /* Link to the owning job. */
}
JobStream;

/* Methods of VifmJob type. */
static const struct luaL_Reg job_methods[] = {
	{ "__gc",     &vifmjob_gc       },
	{ "wait",     &vifmjob_wait     },
	{ "exitcode", &vifmjob_exitcode },
	{ "stdout",   &vifmjob_stdout   },
	{ "errors",   &vifmjob_errors   },
	{ NULL,       NULL              }
};

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
	luaL_requiref(vlua->lua, LUA_IOLIBNAME, &luaopen_io, 1);
	lua_pop(vlua->lua, 1);
	luaL_requiref(vlua->lua, LUA_STRLIBNAME, &luaopen_string, 1);
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
	luaL_newmetatable(lua, "VifmJob");
	lua_pushvalue(lua, -1);
	lua_setfield(lua, -2, "__index");
	luaL_setfuncs(lua, job_methods, 0);
	lua_pop(lua, 1);

	luaL_newmetatable(lua, "VifmPluginEnv");
	lua_pushglobaltable(lua);
	lua_setfield(lua, -2, "__index");
	lua_pop(lua, 1);

	lua_pushcfunction(lua, &print);
	lua_setglobal(lua, "print");

	luaL_newlib(lua, vifm_methods);

	lua_pushvalue(lua, -1);
	lua_setglobal(lua, "vifm");

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
		if(plug->log_len != 0)
		{
			(void)strappendch(&plug->log, &plug->log_len, '\n');
		}
		(void)strappend(&plug->log, &plug->log_len, msg);
	}
	else
	{
		ui_sb_msg(msg);
		curr_stats.save_msg = 1;
	}

	free(msg);
	return 0;
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

/* Member of `vifm` that starts an external application as detached from a
 * terminal.  Returns object of VifmJob type or raises an error. */
static int
vifm_startjob(lua_State *lua)
{
	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "cmd", LUA_TSTRING);
	const char *cmd = lua_tostring(lua, -1);

	int visible = 0;
	if(check_opt_field(lua, 1, "visible", LUA_TBOOLEAN))
	{
		visible = lua_toboolean(lua, -1);
	}

	const char *descr = NULL;
	if(check_opt_field(lua, 1, "descr", LUA_TSTRING))
	{
		descr = lua_tostring(lua, -1);
	}

	bg_job_t *job = bg_run_external_job(cmd, visible);
	if(job == NULL)
	{
		return luaL_error(lua, "%s", "Failed to start a job");
	}

	if(visible && descr != NULL)
	{
		bg_op_set_descr(&job->bg_op, descr);
	}

	bg_job_t **data = lua_newuserdata(lua, sizeof(bg_job_t *));

	luaL_getmetatable(lua, "VifmJob");
	lua_setmetatable(lua, -2);

	*data = job;
	return 1;
}

/* Member of `vifm` that registers a new :command or raises an error.  Returns
 * boolean, which is true on success. */
static int
vifm_addcommand(lua_State *lua)
{
	vlua_t *vlua = get_state(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "name", LUA_TSTRING);
	const char *name = lua_tostring(lua, -1);

	const char *descr = "";
	if(check_opt_field(lua, 1, "descr", LUA_TSTRING))
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

/* Converts Lua value at the top of the stack into a C pointer.  Returns the
 * pointer. */
static void *
to_pointer(lua_State *lua)
{
	void *ptr = (void *)lua_topointer(lua, -1);
	lua_pushlightuserdata(lua, ptr);
	lua_insert(lua, -2);
	lua_settable(lua, LUA_REGISTRYINDEX);
	return ptr;
}

/* Converts C pointer to a Lua value and pushes it on top of the stack. */
static void
from_pointer(lua_State *lua, void *ptr)
{
	lua_pushlightuserdata(lua, ptr);
	lua_gettable(lua, LUA_REGISTRYINDEX);
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

/* Member of `vifm` that changes directory of current view.  Returns boolean,
 * which is true if location change was successful. */
static int
vifm_change_dir(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	int success = (change_directory(curr_view, path) >= 0);
	lua_pushboolean(lua, success);
	return 1;
}

/* Member of `vifm` that prints a normal message on the statusbar.  Doesn't
 * return anything. */
static int
sb_info(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_msg(msg);
	curr_stats.save_msg = 1;
	return 0;
}

/* Member of `vifm` that prints an error message on the statusbar.  Doesn't
 * return anything. */
static int
sb_error(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_err(msg);
	curr_stats.save_msg = 1;
	return 0;
}

/* Member of `vifm` that prints statusbar message that's not stored in
 * history.  Doesn't return anything. */
static int
sb_quick(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_quick_msgf("%s", msg);
	return 0;
}

/* Method of of VifmJob that frees associated resources.  Doesn't return
 * anything. */
static int
vifmjob_gc(lua_State *lua)
{
	bg_job_t *job = *(bg_job_t **)luaL_checkudata(lua, 1, "VifmJob");
	bg_job_decref(job);
	return 0;
}

/* Method of of VifmJob that waits for the job to finish.  Raises an error if
 * waiting has failed.  Doesn't return anything. */
static int
vifmjob_wait(lua_State *lua)
{
	bg_job_t *job = *(bg_job_t **)luaL_checkudata(lua, 1, "VifmJob");

	if(bg_job_wait(job) != 0)
	{
		return luaL_error(lua, "%s", "Waiting for job has failed");
	}

	return 0;
}

/* Method of of VifmJob that retrieves exit code of the job.  Waits for the job
 * to finish if it hasn't already.  Raises an error if waiting has failed.
 * Returns an integer representing the exit code. */
static int
vifmjob_exitcode(lua_State *lua)
{
	bg_job_t *job = *(bg_job_t **)luaL_checkudata(lua, 1, "VifmJob");

	vifmjob_wait(lua);

	pthread_spin_lock(&job->status_lock);
	int running = job->running;
	int exit_code = job->exit_code;
	pthread_spin_unlock(&job->status_lock);

	if(running)
	{
		return luaL_error(lua, "%s", "Waited, but the job is still running");
	}

	lua_pushinteger(lua, exit_code);
	return 1;
}

/* Method of VifmJob that retrieves stream associated with output stream of
 * the job.  Returns file stream object compatible with I/O library. */
static int
vifmjob_stdout(lua_State *lua)
{
	bg_job_t **job = luaL_checkudata(lua, 1, "VifmJob");

	/* XXX: should we return the same Lua object on every call? */
	JobStream *js = lua_newuserdata(lua, sizeof(JobStream));
	js->lua_stream.closef = NULL;
	luaL_setmetatable(lua, LUA_FILEHANDLE);

	js->lua_stream.f = (*job)->output;
	js->lua_stream.closef = &jobstream_close;
	js->job = *job;
	bg_job_incref(*job);

	return 1;
}

/* Method of VifmJob that retrieves errors produces by the job.  Returns string
 * with the errors. */
static int
vifmjob_errors(lua_State *lua)
{
	bg_job_t *job = *(bg_job_t **)luaL_checkudata(lua, 1, "VifmJob");

	char *errors = NULL;
	pthread_spin_lock(&job->errors_lock);
	update_string(&errors, job->errors);
	pthread_spin_unlock(&job->errors_lock);

	if(errors == NULL)
	{
		lua_pushstring(lua, "");
	}
	else
	{
		lua_pushstring(lua, errors);
		free(errors);
	}
	return 1;
}

/* Custom destructor for luaL_Stream that decrements use counter of the job.
 * Returns status. */
static int
jobstream_close(lua_State *lua)
{
	JobStream *js = luaL_checkudata(lua, 1, LUA_FILEHANDLE);
	bg_job_decref(js->job);

	/* Don't fclose() file stream here, because it might be used by multiple Lua
	 * objects.  Just report success. */
	return luaL_fileresult(lua, 0, NULL);
}

int
vlua_load_plugin(vlua_t *vlua, const char plugin[], plug_t *plug)
{
	if(load_plugin(vlua->lua, plugin, plug) == 0)
	{
		lua_setglobal(vlua->lua, plugin);
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
		ui_sb_errf("Failed to load '%s' plugin: %s", name, lua_tostring(lua, -1));
		lua_pop(lua, 1);
		return 1;
	}

	setup_plugin_env(lua, plug);
	if(lua_pcall(lua, 0, 1, 0))
	{
		ui_sb_errf("Failed to start '%s' plugin: %s", name, lua_tostring(lua, -1));
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

/* Retrieves mandatory field of a table while checking its type and aborting
 * (Lua does longjmp()) if it's missing or doesn't match. */
static void
check_field(lua_State *lua, int table_idx, const char name[], int lua_type)
{
	int type = lua_getfield(lua, 1, name);
	if(type == LUA_TNIL)
	{
		luaL_error(lua, "`%s` key is mandatory", name);
	}
	if(type != lua_type)
	{
		luaL_error(lua, "`%s` value must be a %s", name,
				lua_typename(lua, lua_type));
	}
}

/* Retrieves optional field of a table while checking its type and aborting (Lua
 * does longjmp()) if it doesn't match.  Returns non-zero if the field is
 * present and is of correct type. */
static int
check_opt_field(lua_State *lua, int table_idx, const char name[], int lua_type)
{
	int type = lua_getfield(lua, 1, name);
	if(type == LUA_TNIL)
	{
		return 0;
	}

	if(type != lua_type)
	{
		return luaL_error(lua, "`%s` value must be a %s",name,
				lua_typename(lua, lua_type));
	}
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
