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

#include "vifmjob.h"

#include <stdio.h> /* fclose() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() */

#include "../compat/pthread.h"
#include "../utils/str.h"
#include "../background.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"

/* User data of file stream associated with job's output stream. */
typedef struct
{
	luaL_Stream lua_stream; /* Standard Lua structure. */
	bg_job_t *job;          /* Link to the owning job. */
	void *obj;              /* Handle to Lua object of this user data. */
}
job_stream_t;

/* User data of job object. */
typedef struct
{
	bg_job_t *job;        /* Link to the native job. */
	job_stream_t *input;  /* Cached input stream or NULL. */
	job_stream_t *output; /* Cached output stream or NULL. */
}
vifm_job_t;

static int VLUA_API(vifmjob_gc)(lua_State *lua);
static int VLUA_API(vifmjob_wait)(lua_State *lua);
static int VLUA_API(vifmjob_exitcode)(lua_State *lua);
static int VLUA_API(vifmjob_stdin)(lua_State *lua);
static int VLUA_API(vifmjob_stdout)(lua_State *lua);
static int VLUA_API(vifmjob_errors)(lua_State *lua);
static void job_stream_gc(lua_State *lua, job_stream_t *js);
static job_stream_t * job_stream_open(lua_State *lua, bg_job_t *job,
		FILE *stream);
static void job_stream_close(lua_State *lua, job_stream_t *js);
static int VLUA_IMPL(jobstream_closef)(lua_State *lua);

VLUA_DECLARE_SAFE(vifmjob_gc);
VLUA_DECLARE_SAFE(vifmjob_wait);
VLUA_DECLARE_SAFE(vifmjob_exitcode);
VLUA_DECLARE_SAFE(vifmjob_stdin);
VLUA_DECLARE_SAFE(vifmjob_stdout);
VLUA_DECLARE_SAFE(vifmjob_errors);

/* Methods of VifmJob type. */
static const luaL_Reg vifmjob_methods[] = {
	{ "__gc",     VLUA_REF(vifmjob_gc)       },
	{ "wait",     VLUA_REF(vifmjob_wait)     },
	{ "exitcode", VLUA_REF(vifmjob_exitcode) },
	{ "stdin",    VLUA_REF(vifmjob_stdin)    },
	{ "stdout",   VLUA_REF(vifmjob_stdout)   },
	{ "errors",   VLUA_REF(vifmjob_errors)   },
	{ NULL,       NULL                       }
};

void
vifmjob_init(lua_State *lua)
{
	luaL_newmetatable(lua, "VifmJob");
	lua_pushvalue(lua, -1);
	lua_setfield(lua, -2, "__index");
	luaL_setfuncs(lua, vifmjob_methods, 0);
	lua_pop(lua, 1);
}

int
VLUA_API(vifmjob_new)(lua_State *lua)
{
	luaL_checktype(lua, 1, LUA_TTABLE);

	check_field(lua, 1, "cmd", LUA_TSTRING);
	const char *cmd = lua_tostring(lua, -1);

	const char *iomode = NULL;
	if(check_opt_field(lua, 1, "iomode", LUA_TSTRING))
	{
		iomode = lua_tostring(lua, -1);
	}

	BgJobFlags flags = BJF_MENU_VISIBLE;
	if(check_opt_field(lua, 1, "visible", LUA_TBOOLEAN))
	{
		flags |= (lua_toboolean(lua, -1) ? BJF_JOB_BAR_VISIBLE : BJF_NONE);
	}
	if(check_opt_field(lua, 1, "mergestreams", LUA_TBOOLEAN))
	{
		flags |= (lua_toboolean(lua, -1) ? BJF_MERGE_STREAMS : BJF_NONE);
	}
	if(iomode == NULL || strcmp(iomode, "r") == 0)
	{
		flags |= BJF_CAPTURE_OUT;
	}
	else if(strcmp(iomode, "w") == 0)
	{
		flags |= BJF_SUPPLY_INPUT;
	}
	else if(strcmp(iomode, "") != 0)
	{
		return luaL_error(lua, "Unknown 'iomode' value: %s", iomode);
	}

	const char *descr = NULL;
	if(check_opt_field(lua, 1, "description", LUA_TSTRING))
	{
		descr = lua_tostring(lua, -1);
	}

	bg_job_t *job = bg_run_external_job(cmd, flags);
	if(job == NULL)
	{
		return luaL_error(lua, "%s", "Failed to start a job");
	}

	if((flags & BJF_JOB_BAR_VISIBLE) && descr != NULL)
	{
		bg_op_set_descr(&job->bg_op, descr);
	}

	vifm_job_t *data = lua_newuserdatauv(lua, sizeof(*data), 0);

	luaL_getmetatable(lua, "VifmJob");
	lua_setmetatable(lua, -2);

	data->job = job;
	data->input = NULL;
	data->output = NULL;
	return 1;
}

/* Method of of VifmJob that frees associated resources.  Doesn't return
 * anything. */
static int
VLUA_API(vifmjob_gc)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");
	bg_job_decref(vifm_job->job);

	if(vifm_job->input != NULL)
	{
		job_stream_gc(lua, vifm_job->input);
	}
	if(vifm_job->output != NULL)
	{
		job_stream_gc(lua, vifm_job->output);
	}

	return 0;
}

/* Method of of VifmJob that waits for the job to finish.  Raises an error if
 * waiting has failed.  Doesn't return anything. */
static int
VLUA_API(vifmjob_wait)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");

	/* Close Lua input stream to avoid situation when the job is blocked on
	 * read. */
	if(vifm_job->input != NULL)
	{
		job_stream_close(lua, vifm_job->input);
	}

	/* Close Lua output stream to avoid situation when the job is blocked on
	 * write. */
	if(vifm_job->output != NULL)
	{
		job_stream_close(lua, vifm_job->output);
	}

	if(bg_job_wait(vifm_job->job) != 0)
	{
		return luaL_error(lua, "%s", "Waiting for job has failed");
	}

	return 0;
}

/* Method of of VifmJob that retrieves exit code of the job.  Waits for the job
 * to finish if it hasn't already.  Raises an error if waiting has failed.
 * Returns an integer representing the exit code. */
static int
VLUA_API(vifmjob_exitcode)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");

	VLUA_CALL(vifmjob_wait)(lua);

	pthread_spin_lock(&vifm_job->job->status_lock);
	int running = vifm_job->job->running;
	int exit_code = vifm_job->job->exit_code;
	pthread_spin_unlock(&vifm_job->job->status_lock);

	if(running)
	{
		return luaL_error(lua, "%s", "Waited, but the job is still running");
	}

	lua_pushinteger(lua, exit_code);
	return 1;
}

/* Method of VifmJob that retrieves stream associated with input stream of the
 * job.  Returns file stream object compatible with I/O library. */
static int
VLUA_API(vifmjob_stdin)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");

	if(vifm_job->job->input == NULL)
	{
		return luaL_error(lua, "%s", "The job has no input stream");
	}

	/* We return the same Lua object on every call. */
	if(vifm_job->input == NULL)
	{
		vifm_job->input = job_stream_open(lua, vifm_job->job,
				vifm_job->job->input);
	}
	else
	{
		from_pointer(lua, vifm_job->input->obj);
	}

	return 1;
}

/* Method of VifmJob that retrieves stream associated with output stream of the
 * job.  Returns file stream object compatible with I/O library. */
static int
VLUA_API(vifmjob_stdout)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");

	if(vifm_job->job->output == NULL)
	{
		return luaL_error(lua, "%s", "The job has no output stream");
	}

	/* We return the same Lua object on every call. */
	if(vifm_job->output == NULL)
	{
		vifm_job->output = job_stream_open(lua, vifm_job->job,
				vifm_job->job->output);
	}
	else
	{
		from_pointer(lua, vifm_job->output->obj);
	}

	return 1;
}

/* Method of VifmJob that retrieves errors produces by the job.  Returns string
 * with the errors. */
static int
VLUA_API(vifmjob_errors)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");

	char *errors = NULL;
	pthread_spin_lock(&vifm_job->job->errors_lock);
	update_string(&errors, vifm_job->job->errors);
	pthread_spin_unlock(&vifm_job->job->errors_lock);

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

/* Frees job stream when its parent is garbage collected. */
static void
job_stream_gc(lua_State *lua, job_stream_t *js)
{
	drop_pointer(lua, js->obj);
	bg_job_decref(js->job);
	js->job = NULL;
}

/* Creates a job stream.  Returns a pointer to new user data. */
static job_stream_t *
job_stream_open(lua_State *lua, bg_job_t *job, FILE *stream)
{
	job_stream_t *js = lua_newuserdatauv(lua, sizeof(*js), 0);
	js->lua_stream.closef = NULL;
	luaL_setmetatable(lua, LUA_FILEHANDLE);

	js->lua_stream.f = stream;
	js->lua_stream.closef = VLUA_IREF(jobstream_closef);
	js->job = job;
	bg_job_incref(job);

	js->obj = to_pointer(lua);

	return js;
}

/* Closes job stream when its parent is closed. */
static void
job_stream_close(lua_State *lua, job_stream_t *js)
{
	js->lua_stream.closef = NULL;
	bg_job_decref(js->job);
	drop_pointer(lua, js->obj);
}

/* Custom destructor for luaL_Stream that decrements use counter of the job.
 * Returns status. */
static int
VLUA_IMPL(jobstream_closef)(lua_State *lua)
{
	job_stream_t *js = luaL_checkudata(lua, 1, LUA_FILEHANDLE);

	int stat = 1;

	if(js->job != NULL)
	{
		if(js->lua_stream.f == js->job->input)
		{
			stat = (fclose(js->job->input) == 0);
			js->job->input = NULL;
		}
		else if(js->lua_stream.f == js->job->output)
		{
			stat = (fclose(js->job->output) == 0);
			js->job->output = NULL;
		}
	}

	return luaL_fileresult(lua, stat, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
