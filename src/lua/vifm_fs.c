/* vifm
 * Copyright (C) 2024 xaizek.
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

#include "vifm_fs.h"

#include <string.h> /* strcmp() */

#include "../utils/macros.h"
#include "../ops.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"

static int VLUA_API(fs_cp)(lua_State *lua);
static int VLUA_API(fs_ln)(lua_State *lua);
static int VLUA_API(fs_mkdir)(lua_State *lua);
static int VLUA_API(fs_mkfile)(lua_State *lua);
static int VLUA_API(fs_mv)(lua_State *lua);
static int VLUA_API(fs_rm)(lua_State *lua);
static int VLUA_API(fs_rmdir)(lua_State *lua);

static int cp_mv(lua_State *lua, OPS normal_op, OPS force_op, OPS append_op);
static int perform_fs_op(lua_State *lua, OPS op, const char src[],
		const char dst[], ConflictResolutionPolicy crp, void *data);

VLUA_DECLARE_SAFE(fs_cp);
VLUA_DECLARE_SAFE(fs_ln);
VLUA_DECLARE_SAFE(fs_mkdir);
VLUA_DECLARE_SAFE(fs_mkfile);
VLUA_DECLARE_SAFE(fs_mv);
VLUA_DECLARE_SAFE(fs_rm);
VLUA_DECLARE_SAFE(fs_rmdir);

/* Named NULL and non-NULL constants for clarity. */
static void *const no_dst = NULL;
static void *const no_data = NULL;
static char dummy;
static void *const no_cancel = &dummy;

/* Functions of the `vifm.fs` table. */
static const luaL_Reg vifm_fs_methods[] = {
	{ "cp",     VLUA_REF(fs_cp)     },
	{ "ln",     VLUA_REF(fs_ln)     },
	{ "mkdir",  VLUA_REF(fs_mkdir)  },
	{ "mkfile", VLUA_REF(fs_mkfile) },
	{ "mv",     VLUA_REF(fs_mv)     },
	{ "rm",     VLUA_REF(fs_rm)     },
	{ "rmdir",  VLUA_REF(fs_rmdir)  },
	{ NULL,     NULL                }
};

void
vifm_fs_init(lua_State *lua)
{
	luaL_newlib(lua, vifm_fs_methods);
}

/* Member of `vifm.fs` that copies a file/directory. */
static int
VLUA_API(fs_cp)(lua_State *lua)
{
	return cp_mv(lua, OP_COPY, OP_COPYF, OP_COPYA);
}

/* Member of `vifm.fs` that creates or updates a symbolic link. */
static int
VLUA_API(fs_ln)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	const char *target = luaL_checkstring(lua, 2);
	return perform_fs_op(lua, OP_SYMLINK, target, path, CRP_SKIP_ALL, no_data);
}

/* Member of `vifm.fs` that creates a directory, possibly along with its
 * parents. */
static int
VLUA_API(fs_mkdir)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	const char *on_missing_parent = luaL_optstring(lua, 2, "fail");

	void *data = no_data;
	if(strcmp(on_missing_parent, "create") == 0)
	{
		/* Any non-NULL value enables creation of parent directories. */
		data = lua;
	}

	return perform_fs_op(lua, OP_MKDIR, path, no_dst, CRP_SKIP_ALL, data);
}

/* Member of `vifm.fs` that creates a file if it doesn't yet exist. */
static int
VLUA_API(fs_mkfile)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	return perform_fs_op(lua, OP_MKFILE, path, no_dst, CRP_SKIP_ALL, no_data);
}

/* Member of `vifm.fs` that moves a file/directory. */
static int
VLUA_API(fs_mv)(lua_State *lua)
{
	return cp_mv(lua, OP_MOVE, OP_MOVEF, OP_MOVEA);
}

/* Member of `vifm.fs` that removes a file/directory. */
static int
VLUA_API(fs_rm)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	return perform_fs_op(lua, OP_REMOVESL, path, no_dst, CRP_SKIP_ALL, no_cancel);
}

/* Member of `vifm.fs` that removes an empty directory. */
static int
VLUA_API(fs_rmdir)(lua_State *lua)
{
	const char *path = luaL_checkstring(lua, 1);
	return perform_fs_op(lua, OP_RMDIR, path, no_dst, CRP_SKIP_ALL, no_data);
}

/* Common implementation of file copying/moving.  Returns the number of Lua
 * return values. */
static int
cp_mv(lua_State *lua, OPS normal_op, OPS force_op, OPS append_op)
{
	const char *src = luaL_checkstring(lua, 1);
	const char *dst = luaL_checkstring(lua, 2);
	const char *on_conflict = luaL_optstring(lua, 3, "fail");

	if(strcmp(on_conflict, "overwrite") == 0)
	{
		return perform_fs_op(lua, force_op, src, dst, CRP_OVERWRITE_ALL, no_cancel);
	}

	if(strcmp(on_conflict, "append-tail") == 0)
	{
		return perform_fs_op(lua, append_op, src, dst, CRP_OVERWRITE_ALL,
				no_cancel);
	}

	if(strcmp(on_conflict, "skip") == 0)
	{
		return perform_fs_op(lua, force_op, src, dst, CRP_SKIP_ALL, no_cancel);
	}

	return perform_fs_op(lua, normal_op, src, dst, CRP_SKIP_ALL, no_cancel);
}

/* Common implementation of a file operation.  Returns number of Lua return
 * values. */
static int
perform_fs_op(lua_State *lua, OPS op, const char src[], const char dst[],
		ConflictResolutionPolicy crp, void *data)
{
	ops_t *ops = ops_alloc(op, /*bg=*/0, "Lua FS operation", "base dir",
			"target dir", /*choose=*/NULL, /*confirm=*/NULL);
	if(ops == NULL)
	{
		return OPS_FAILED;
	}

	/* Force the use of system calls for more consistent operation. */
	ops->use_system_calls = 1;
	/* Overwrite conflict resolution policy to force desired behaviour. */
	ops->crp = crp;

	OpsResult result = perform_operation(op, ops, data, src, dst);
	ops_free(ops);

	switch(result)
	{
		case OPS_SUCCEEDED:
		case OPS_SKIPPED:
			lua_pushboolean(lua, 1);
			return 1;
		case OPS_FAILED:
			lua_pushboolean(lua, 0);
			return 1;
	}

	/* Unreachable. */
	return luaL_error(lua, "Unknown ops result: %d", result);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
