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

#include <assert.h> /* assert() */
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
#include "vlua_cbacks.h"
#include "vlua_state.h"

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
static int VLUA_API(vifmjob_pid)(lua_State *lua);
static int VLUA_API(vifmjob_stdin)(lua_State *lua);
static int VLUA_API(vifmjob_stdout)(lua_State *lua);
static int VLUA_API(vifmjob_errors)(lua_State *lua);
static int VLUA_API(vifmjob_terminate)(lua_State *lua);
static void job_exit_cb(struct bg_job_t *job, void *arg);
static job_stream_t * job_stream_open(lua_State *lua, bg_job_t *job,
		FILE *stream);
static void job_stream_close(lua_State *lua, job_stream_t *js);
static int VLUA_IMPL(jobstream_closef)(lua_State *lua);

VLUA_DECLARE_SAFE(vifmjob_gc);
VLUA_DECLARE_SAFE(vifmjob_wait);
VLUA_DECLARE_SAFE(vifmjob_exitcode);
VLUA_DECLARE_SAFE(vifmjob_pid);
VLUA_DECLARE_SAFE(vifmjob_stdin);
VLUA_DECLARE_SAFE(vifmjob_stdout);
VLUA_DECLARE_SAFE(vifmjob_errors);
VLUA_DECLARE_SAFE(vifmjob_terminate);

/* Methods of VifmJob type. */
static const luaL_Reg vifmjob_methods[] = {
	{ "__gc",      VLUA_REF(vifmjob_gc)        },
	{ "wait",      VLUA_REF(vifmjob_wait)      },
	{ "exitcode",  VLUA_REF(vifmjob_exitcode)  },
	{ "pid",       VLUA_REF(vifmjob_pid)       },
	{ "stdin",     VLUA_REF(vifmjob_stdin)     },
	{ "stdout",    VLUA_REF(vifmjob_stdout)    },
	{ "errors",    VLUA_REF(vifmjob_errors)    },
	{ "terminate", VLUA_REF(vifmjob_terminate) },
	{ NULL,        NULL                        }
};

/*
 * Address of this variable serves as a key in Lua table which maps VifmJob
 * instances onto dictionary with such fields:
 *  - "obj" - vifm_job_t user data
 *  - "on_exit" - Lua callback to invoke when the job is done
 */
static char jobs_key;

void
vifmjob_init(lua_State *lua)
{
	vlua_cmn_make_metatable(lua, "VifmJob");
	luaL_setfuncs(lua, vifmjob_methods, 0);
	lua_pop(lua, 1);

	vlua_state_make_table(vlua_state_get(lua), &jobs_key);
}

void
vifmjob_finish(lua_State *lua)
{
	vlua_state_get_table(vlua_state_get(lua), &jobs_key);
	lua_pushnil(lua);
	while(lua_next(lua, -2) != 0)
	{
		lua_pop(lua, 1);

		bg_job_t *job = lua_touserdata(lua, -1);
		assert(job != NULL && "List of Lua jobs includes a bad key!");
		bg_job_set_exit_cb(job, NULL, NULL);
	}

	lua_pop(lua, 1);
}

int
VLUA_API(vifmjob_new)(lua_State *lua)
{
	vlua_t *vlua = vlua_state_get(lua);

	luaL_checktype(lua, 1, LUA_TTABLE);

	vlua_cmn_check_field(lua, 1, "cmd", LUA_TSTRING);
	const char *cmd = lua_tostring(lua, -1);

	const char *iomode = NULL;
	if(vlua_cmn_check_opt_field(lua, 1, "iomode", LUA_TSTRING))
	{
		iomode = lua_tostring(lua, -1);
	}

	BgJobFlags flags = BJF_MENU_VISIBLE;
	if(vlua_cmn_check_opt_field(lua, 1, "visible", LUA_TBOOLEAN))
	{
		flags |= (lua_toboolean(lua, -1) ? BJF_JOB_BAR_VISIBLE : BJF_NONE);
	}
	if(vlua_cmn_check_opt_field(lua, 1, "mergestreams", LUA_TBOOLEAN))
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
	if(vlua_cmn_check_opt_field(lua, 1, "description", LUA_TSTRING))
	{
		descr = lua_tostring(lua, -1);
	}

	const char *pwd = NULL;
	if(vlua_cmn_check_opt_field(lua, 1, "pwd", LUA_TSTRING))
	{
		pwd = lua_tostring(lua, -1);
	}

	int with_on_exit = vlua_cmn_check_opt_field(lua, 1, "onexit", LUA_TFUNCTION);

	bg_job_t *job = bg_run_external_job(cmd, flags, descr, pwd);
	if(job == NULL)
	{
		return luaL_error(lua, "%s", "Failed to start a job");
	}

	vifm_job_t *data = lua_newuserdatauv(lua, sizeof(*data), 0);

	luaL_getmetatable(lua, "VifmJob");
	lua_setmetatable(lua, -2);

	/* Map job onto a table describing it in Lua. */
	lua_createtable(lua, /*narr=*/0, /*nrec=*/(with_on_exit ? 2 : 1));
	lua_pushvalue(lua, -2);
	lua_setfield(lua, -2, "obj");
	if(with_on_exit)
	{
		lua_pushvalue(lua, -3);
		lua_setfield(lua, -2, "onexit");
	}
	vlua_state_get_table(vlua, &jobs_key);
	lua_pushlightuserdata(lua, job);
	lua_pushvalue(lua, -3);
	lua_settable(lua, -3);
	lua_pop(lua, 2);

	bg_job_set_exit_cb(job, &job_exit_cb, vlua);

	data->job = job;
	data->input = NULL;
	data->output = NULL;
	return 1;
}

/* Handles job's exit by closing its streams and doing some cleanup. */
static void
job_exit_cb(struct bg_job_t *job, void *arg)
{
	vlua_t *vlua = arg;

	/* Find vifm_job_t that corresponds to the job. */
	vlua_state_get_table(vlua, &jobs_key);
	lua_pushlightuserdata(vlua->lua, job);
	if(lua_gettable(vlua->lua, -2) != LUA_TTABLE)
	{
		assert(0 && "Exited job has no associated Lua job data!");
		lua_pop(vlua->lua, 2);
		return;
	}

	int with_on_exit = (lua_getfield(vlua->lua, -1, "onexit") == LUA_TFUNCTION);

	lua_getfield(vlua->lua, -2, "obj");
	vifm_job_t *vifm_job = lua_touserdata(vlua->lua, -1);
	assert(vifm_job != NULL && "List of Lua jobs is includes bad element!");

	/* Remove the table entry we've just used. */
	lua_pushlightuserdata(vlua->lua, job);
	lua_pushnil(vlua->lua);
	lua_settable(vlua->lua, -6);

	/* Close input and output streams to make them error on use. */

	if(vifm_job->input != NULL)
	{
		job_stream_close(vlua->lua, vifm_job->input);
		vifm_job->input = NULL;
	}

	if(vifm_job->output != NULL)
	{
		job_stream_close(vlua->lua, vifm_job->output);
		vifm_job->output = NULL;
	}

	if(with_on_exit)
	{
		vlua_cbacks_schedule(vlua, /*argc=*/1);
		lua_pop(vlua->lua, 2);
	}
	else
	{
		lua_pop(vlua->lua, 4);
	}
}

/* Method of VifmJob that frees associated resources.  Doesn't return
 * anything. */
static int
VLUA_API(vifmjob_gc)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");
	bg_job_decref(vifm_job->job);

	if(vifm_job->input != NULL)
	{
		vlua_cmn_drop_pointer(lua, vifm_job->input->obj);
	}
	if(vifm_job->output != NULL)
	{
		vlua_cmn_drop_pointer(lua, vifm_job->output->obj);
	}

	return 0;
}

/* Method of VifmJob that waits for the job to finish.  Raises an error if
 * waiting has failed.  Doesn't return anything. */
static int
VLUA_API(vifmjob_wait)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");

	/* Close input stream to avoid situation when the job is blocked on read. */
	if(vifm_job->input != NULL)
	{
		job_stream_close(lua, vifm_job->input);
		vifm_job->input = NULL;

		/* The stream might have been closed explicitly earlier. */
		if(vifm_job->job->input != NULL)
		{
			fclose(vifm_job->job->input);
			vifm_job->job->input = NULL;
		}
	}

	/* Close output stream to avoid situation when the job is blocked on write. */
	if(vifm_job->output != NULL)
	{
		job_stream_close(lua, vifm_job->output);
		vifm_job->output = NULL;

		/* The stream might have been closed explicitly earlier. */
		if(vifm_job->job->output != NULL)
		{
			fclose(vifm_job->job->output);
			vifm_job->job->output = NULL;
		}
	}

	if(bg_job_wait(vifm_job->job) != 0)
	{
		return luaL_error(lua, "%s", "Waiting for job has failed");
	}

	return 0;
}

/* Method of VifmJob that retrieves exit code of the job.  Waits for the job
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

/* Method of VifmJob that retrieves process ID of the job.  Returns an
 * integer representing the ID. */
static int
VLUA_API(vifmjob_pid)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");
	lua_pushinteger(lua, vifm_job->job->pid);
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
		vlua_cmn_from_pointer(lua, vifm_job->input->obj);
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
		vlua_cmn_from_pointer(lua, vifm_job->output->obj);
	}

	return 1;
}

/* Method of VifmJob that retrieves errors produces by the job.  Returns string
 * with the errors. */
static int
VLUA_API(vifmjob_errors)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");

	if(bg_job_wait_errors(vifm_job->job) != 0)
	{
		return luaL_error(lua, "%s", "Failed to wait for errors of an exited job");
	}

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

/* Method of VifmJob that forcefully stops the job.  Returns nothing. */
static int
VLUA_API(vifmjob_terminate)(lua_State *lua)
{
	vifm_job_t *vifm_job = luaL_checkudata(lua, 1, "VifmJob");
	bg_job_terminate(vifm_job->job);
	return 0;
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

	js->obj = vlua_cmn_to_pointer(lua);

	return js;
}

/* Closes job stream when its parent is closed. */
static void
job_stream_close(lua_State *lua, job_stream_t *js)
{
	/* The stream might have already been closed from Lua. */
	if(js->lua_stream.closef != NULL)
	{
		js->lua_stream.closef = NULL;
		bg_job_decref(js->job);
	}

	vlua_cmn_drop_pointer(lua, js->obj);
}

/* Custom destructor for luaL_Stream that decrements use counter of the job.
 * Returns status. */
static int
VLUA_IMPL(jobstream_closef)(lua_State *lua)
{
	job_stream_t *js = luaL_checkudata(lua, 1, LUA_FILEHANDLE);

	int stat = 1;

	if(js->lua_stream.f == js->job->input)
	{
		stat = (fclose(js->job->input) == 0);
		js->job->input = NULL;
		bg_job_decref(js->job);
	}
	else if(js->lua_stream.f == js->job->output)
	{
		stat = (fclose(js->job->output) == 0);
		js->job->output = NULL;
		bg_job_decref(js->job);
	}

	return luaL_fileresult(lua, stat, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
