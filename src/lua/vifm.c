/* vifm
 * Copyright (C) 2023 xaizek.
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

#include "vifm.h"

#include "../cfg/info.h"
#include "../engine/options.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/cmdline.h"
#include "../ui/statusbar.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utils.h"
#include "../event_loop.h"
#include "../filelist.h"
#include "../filename_modifiers.h"
#include "../running.h"
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "api.h"
#include "common.h"
#include "vifm_cmds.h"
#include "vifm_events.h"
#include "vifm_handlers.h"
#include "vifm_keys.h"
#include "vifm_tabs.h"
#include "vifm_viewcolumns.h"
#include "vifmjob.h"
#include "vifmview.h"
#include "vlua_state.h"

/* Lua API version. */
#define API_VER_MAJOR 0
#define API_VER_MINOR 1
#define API_VER_PATCH 0

/*
 * This unit contains generic part of `vifm` global table.  Plugin-specific
 * things are in vlua unit.
 *
 * Implementation for small things are here, others are in corresponding vifm_*
 * or type-specific vifm* units and are only initialized here.
 */

/* Data passed to prompt callback in vifm.input(). */
typedef struct
{
	int quit;       /* Exit flag for event loop. */
	char *response; /* Result of the prompt. */
}
input_cb_data_t;

static int VLUA_API(vifm_errordialog)(lua_State *lua);
static int VLUA_API(vifm_escape)(lua_State *lua);
static int VLUA_API(vifm_executable)(lua_State *lua);
static int VLUA_API(vifm_exists)(lua_State *lua);
static int VLUA_API(vifm_expand)(lua_State *lua);
static int VLUA_API(vifm_fnamemodify)(lua_State *lua);
static int VLUA_API(vifm_input)(lua_State *lua);
static int VLUA_API(vifm_makepath)(lua_State *lua);
static int VLUA_API(vifm_run)(lua_State *lua);
static int VLUA_API(vifm_sessions_current)(lua_State *lua);

static int VLUA_API(api_is_at_least)(lua_State *lua);
static int VLUA_API(api_has)(lua_State *lua);

static int VLUA_API(opts_global_index)(lua_State *lua);
static int VLUA_API(opts_global_newindex)(lua_State *lua);

static int VLUA_API(sb_info)(lua_State *lua);
static int VLUA_API(sb_error)(lua_State *lua);
static int VLUA_API(sb_quick)(lua_State *lua);

VLUA_DECLARE_SAFE(vifm_errordialog);
VLUA_DECLARE_SAFE(vifm_escape);
VLUA_DECLARE_SAFE(vifm_executable);
VLUA_DECLARE_SAFE(vifm_exists);
VLUA_DECLARE_SAFE(vifm_expand);
VLUA_DECLARE_SAFE(vifm_fnamemodify);
VLUA_DECLARE_SAFE(vifm_input);
VLUA_DECLARE_SAFE(vifm_makepath);
VLUA_DECLARE_SAFE(vifm_run);
VLUA_DECLARE_SAFE(vifm_sessions_current);

VLUA_DECLARE_SAFE(api_is_at_least);
VLUA_DECLARE_SAFE(api_has);

VLUA_DECLARE_SAFE(opts_global_index);
VLUA_DECLARE_UNSAFE(opts_global_newindex);

VLUA_DECLARE_SAFE(sb_info);
VLUA_DECLARE_SAFE(sb_error);
VLUA_DECLARE_SAFE(sb_quick);

/* These are defined in other units. */
VLUA_DECLARE_UNSAFE(vifm_addcolumntype);
VLUA_DECLARE_SAFE(vifm_addhandler);
VLUA_DECLARE_SAFE(vifmview_currview);
VLUA_DECLARE_SAFE(vifmview_otherview);
VLUA_DECLARE_SAFE(vifmjob_new);

static void input_builtin_cb(const char response[], void *arg);

/* Functions of `vifm` global table. */
static const struct luaL_Reg vifm_methods[] = {
	{ "errordialog",   VLUA_REF(vifm_errordialog)   },
	{ "escape",        VLUA_REF(vifm_escape)        },
	{ "executable",    VLUA_REF(vifm_executable)    },
	{ "exists",        VLUA_REF(vifm_exists)        },
	{ "expand",        VLUA_REF(vifm_expand)        },
	{ "fnamemodify",   VLUA_REF(vifm_fnamemodify)   },
	{ "input",         VLUA_REF(vifm_input)         },
	{ "makepath",      VLUA_REF(vifm_makepath)      },
	{ "run",           VLUA_REF(vifm_run)           },

	/* Defined in other units. */
	{ "addcolumntype", VLUA_REF(vifm_addcolumntype) },
	{ "addhandler",    VLUA_REF(vifm_addhandler)    },
	{ "currview",      VLUA_REF(vifmview_currview)  },
	{ "otherview",     VLUA_REF(vifmview_otherview) },
	{ "startjob",      VLUA_REF(vifmjob_new)        },

	{ NULL,            NULL                         }
};

/* Functions of `vifm.sb` table. */
static const struct luaL_Reg sb_methods[] = {
	{ "info",   VLUA_REF(sb_info)  },
	{ "error",  VLUA_REF(sb_error) },
	{ "quick",  VLUA_REF(sb_quick) },
	{ NULL,     NULL               }
};

void
vifm_init(lua_State *lua)
{
	vifmjob_init(lua);
	vifmview_init(lua);

	luaL_newlib(lua, vifm_methods);

	/* Setup vifm.cmds. */
	vifm_cmds_init(lua);
	lua_setfield(lua, -2, "cmds");

	/* Setup vifm.events. */
	vifm_events_init(lua);
	lua_setfield(lua, -2, "events");

	/* Setup vifm.keys. */
	vifm_keys_init(lua);
	lua_setfield(lua, -2, "keys");

	/* Setup vifm.tabs. */
	vifm_tabs_init(lua);
	lua_setfield(lua, -2, "tabs");

	/* Setup vifm.opts. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/1); /* vifm.opts */
	lua_newtable(lua);                  /* vifm.opts.global */
	make_metatable(lua, /*name=*/NULL); /* metatable of vifm.opts.global */
	lua_pushcfunction(lua, VLUA_REF(opts_global_index));
	lua_setfield(lua, -2, "__index");
	lua_pushcfunction(lua, VLUA_REF(opts_global_newindex));
	lua_setfield(lua, -2, "__newindex");
	lua_setmetatable(lua, -2);       /* vifm.opts.global */
	lua_setfield(lua, -2, "global"); /* vifm.opts.global */
	lua_setfield(lua, -2, "opts");   /* vifm.opts */

	/* Setup vifm.plugins. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/1); /* vifm.plugins */
	lua_newtable(lua);                            /* vifm.plugins.all */
	lua_setfield(lua, -2, "all");
	lua_setfield(lua, -2, "plugins");

	/* Setup vifm.version. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/2); /* vifm.version */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/1); /* vifm.app */
	lua_pushstring(lua, VERSION);
	lua_setfield(lua, -2, "str");                 /* vifm.app.str */
	lua_setfield(lua, -2, "app");                 /* vifm.app */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/5); /* vifm.api */
	lua_pushinteger(lua, API_VER_MAJOR);
	lua_setfield(lua, -2, "major");               /* vifm.api.major */
	lua_pushinteger(lua, API_VER_MINOR);
	lua_setfield(lua, -2, "minor");               /* vifm.api.minor */
	lua_pushinteger(lua, API_VER_PATCH);
	lua_setfield(lua, -2, "patch");               /* vifm.api.patch */
	lua_pushcfunction(lua, VLUA_REF(api_has));
	lua_setfield(lua, -2, "has");                 /* vifm.api.has */
	lua_pushcfunction(lua, VLUA_REF(api_is_at_least));
	lua_setfield(lua, -2, "atleast");             /* vifm.api.atleast */
	lua_setfield(lua, -2, "api");                 /* vifm.api */
	lua_setfield(lua, -2, "version");             /* vifm.version */

	/* Setup vifm.sb. */
	luaL_newlib(lua, sb_methods);
	lua_setfield(lua, -2, "sb");

	/* Setup vifm.sessions. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/1); /* vifm.sessions */
	lua_pushcfunction(lua, VLUA_REF(vifm_sessions_current));
	lua_setfield(lua, -2, "current");             /* vifm.sessions.current */
	lua_setfield(lua, -2, "sessions");            /* vifm.sessions */
}

/* Member of `vifm` that displays an error dialog.  Doesn't return anything. */
static int
VLUA_API(vifm_errordialog)(lua_State *lua)
{
	const char *title = luaL_checkstring(lua, 1);
	const char *msg = luaL_checkstring(lua, 2);
	show_error_msg(title, msg);
	return 0;
}

/* Member of `vifm` that escapes its input string.  Returns escaped string. */
static int
VLUA_API(vifm_escape)(lua_State *lua)
{
	const char *what = luaL_checkstring(lua, 1);
	char *escaped = shell_arg_escape(what, curr_stats.shell_type);
	lua_pushstring(lua, escaped);
	free(escaped);
	return 1;
}

/* Member of `vifm` that checks whether executable exists at absolute path or
 * in directories listed in $PATH when path isn't absolute.  Checks for various
 * executable extensions on Windows.  Returns a boolean. */
static int
VLUA_API(vifm_executable)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);

	int executable;
	if(contains_slash(path))
	{
		executable = executable_exists(path);
	}
	else
	{
		executable = (find_cmd_in_path(path, 0UL, NULL) == 0);
	}

	lua_pushboolean(lua, executable);
	return 1;
}

/* Member of `vifm` that checks whether specified path exists without resolving
 * symbolic links.  Returns a boolean, which is true when path does exist. */
static int
VLUA_API(vifm_exists)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	lua_pushboolean(lua, path_exists(path, NODEREF));
	return 1;
}

/* Member of `vifm` that expands macros and environment variables.  Returns the
 * expanded string. */
static int
VLUA_API(vifm_expand)(lua_State *lua)
{
	const char *str = luaL_checkstring(lua, 1);

	char *env_expanded = expand_envvars(str,
			EEF_KEEP_ESCAPES | EEF_DOUBLE_PERCENTS);
	char *full_expanded = ma_expand(env_expanded, NULL, NULL, MER_DISPLAY);
	lua_pushstring(lua, full_expanded);
	free(env_expanded);
	free(full_expanded);

	return 1;
}

/* Member of `vifm` that modifies path according to specifiers.  Returns
 * modified path. */
static int
VLUA_API(vifm_fnamemodify)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	const char *modifiers = luaL_checkstring(lua, 2);
	const char *base = luaL_optstring(lua, 3, flist_get_dir(curr_view));
	lua_pushstring(lua, mods_apply(path, base, modifiers, 0));
	return 1;
}

/* Member of `vifm` that asks user for input via a prompt.  Returns a string on
 * success and nil on failure. */
static int
VLUA_API(vifm_input)(lua_State *lua)
{
	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "prompt", LUA_TSTRING);
	const char *prompt = lua_tostring(lua, -1);

	const char *initial = "";
	if(check_opt_field(lua, 1, "initial", LUA_TSTRING))
	{
		initial = lua_tostring(lua, -1);
	}

	complete_cmd_func complete = NULL;
	if(check_opt_field(lua, 1, "complete", LUA_TSTRING))
	{
		const char *value = lua_tostring(lua, -1);
		if(strcmp(value, "dir") == 0)
		{
			complete = &modcline_complete_dirs;
		}
		else if(strcmp(value, "file") == 0)
		{
			complete = &modcline_complete_files;
		}
		else if(strcmp(value, "") != 0)
		{
			return luaL_error(lua, "Unrecognized value for `complete`: %s", value);
		}
	}

	input_cb_data_t cb_data = { .quit = 0, .response = NULL };
	modcline_prompt(prompt, initial, &input_builtin_cb, &cb_data, complete);
	event_loop(&cb_data.quit, /*manage_marking=*/0);

	if(cb_data.response == NULL)
	{
		lua_pushnil(lua);
	}
	else
	{
		lua_pushstring(lua, cb_data.response);
		free(cb_data.response);
	}
	return 1;
}

/* Callback invoked after prompt has finished. */
static void
input_builtin_cb(const char response[], void *arg)
{
	input_cb_data_t *data = arg;

	update_string(&data->response, response);
	data->quit = 1;
}

/* Member of `vifm` that creates a directory and all of its missing parent
 * directories.  Returns a boolean, which is true on success. */
static int
VLUA_API(vifm_makepath)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	lua_pushboolean(lua, make_path(path, 0755) == 0);
	return 1;
}

/* Runs an external command similar to :!. */
static int
VLUA_API(vifm_run)(lua_State *lua)
{
	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "cmd", LUA_TSTRING);
	const char *cmd = lua_tostring(lua, -1);

	int use_term_mux = 1;
	if(check_opt_field(lua, 1, "usetermmux", LUA_TBOOLEAN))
	{
		use_term_mux = lua_toboolean(lua, -1);
	}

	ShellPause pause = PAUSE_ON_ERROR;
	if(check_opt_field(lua, 1, "pause", LUA_TSTRING))
	{
		const char *value = lua_tostring(lua, -1);
		if(strcmp(value, "never") == 0)
		{
			pause = PAUSE_NEVER;
		}
		else if(strcmp(value, "onerror") == 0)
		{
			pause = PAUSE_ON_ERROR;
		}
		else if(strcmp(value, "always") == 0)
		{
			pause = PAUSE_ALWAYS;
		}
		else
		{
			return luaL_error(lua, "Unrecognized value for `pause`: %s", value);
		}
	}

	lua_pushinteger(lua, rn_shell(cmd, pause, use_term_mux, SHELL_BY_APP));
	return 1;
}

/* Member of `vifm.sessions` that retrieves name of the current session.
 * Returns string or nil. */
static int
VLUA_API(vifm_sessions_current)(lua_State *lua)
{
	if(sessions_active())
	{
		lua_pushstring(lua, sessions_current());
	}
	else
	{
		lua_pushnil(lua);
	}
	return 1;
}

/* Checks version of the API.  Returns a boolean. */
static int
VLUA_API(api_is_at_least)(lua_State *lua)
{
	const int major = luaL_checkinteger(lua, 1);
	const int minor = luaL_optinteger(lua, 2, 0);
	const int patch = luaL_optinteger(lua, 3, 0);

	int result = 0;
	if(major != API_VER_MAJOR)
	{
		result = (API_VER_MAJOR > major);
	}
	else if(minor != API_VER_MINOR)
	{
		result = (API_VER_MINOR > minor);
	}
	else
	{
		result = (API_VER_PATCH >= patch);
	}

	lua_pushboolean(lua, result);
	return 1;
}

/* Performs tests for API features.  Returns a boolean. */
static int
VLUA_API(api_has)(lua_State *lua)
{
	(void)luaL_checkstring(lua, 1);
	lua_pushboolean(lua, 0);
	return 1;
}

/* Provides read access to global options by their name as
 * `vifm.opts.global[name]`. */
static int
VLUA_API(opts_global_index)(lua_State *lua)
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
VLUA_API(opts_global_newindex)(lua_State *lua)
{
	const char *opt_name = luaL_checkstring(lua, 2);

	opt_t *opt = vle_opts_find(opt_name, OPT_ANY);
	if(opt == NULL || opt->scope == OPT_LOCAL)
	{
		return 0;
	}

	return set_opt(lua, opt);
}

/* Member of `vifm.sb` that prints a normal message on the status bar.  Doesn't
 * return anything. */
static int
VLUA_API(sb_info)(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_msg(msg);
	curr_stats.save_msg = 1;
	return 0;
}

/* Member of `vifm.sb` that prints an error message on the status bar.  Doesn't
 * return anything. */
static int
VLUA_API(sb_error)(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_err(msg);
	curr_stats.save_msg = 1;
	return 0;
}

/* Member of `vifm.sb` that prints status bar message that's not stored in
 * history.  Doesn't return anything. */
static int
VLUA_API(sb_quick)(lua_State *lua)
{
	const char *msg = luaL_checkstring(lua, 1);
	ui_sb_quick_msgf("%s", msg);
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
