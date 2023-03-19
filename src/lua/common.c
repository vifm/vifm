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

#include "common.h"

#include "../compat/reallocarray.h"
#include "../engine/options.h"
#include "../engine/text_buffer.h"
#include "../ui/ui.h"
#include "../utils/utils.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"

static int deduplicate_ints(int array[], int count);
static int int_sorter(const void *first, const void *second);

int
check_opt_arg(lua_State *lua, int arg_idx, int expected_type)
{
	int type = lua_type(lua, arg_idx);
	if(type == LUA_TNONE)
	{
		return 0;
	}

	if(lua_type(lua, arg_idx) != expected_type)
	{
		luaL_error(lua, "Parameter #%d value must be a %s", arg_idx,
				lua_typename(lua, expected_type));
	}
	return 1;
}

void
check_field(lua_State *lua, int table_idx, const char name[], int lua_type)
{
	int type = lua_getfield(lua, table_idx, name);
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

int
check_opt_field(lua_State *lua, int table_idx, const char name[], int lua_type)
{
	int type = lua_getfield(lua, table_idx, name);
	if(type == LUA_TNIL)
	{
		lua_pop(lua, 1);
		return 0;
	}

	if(type != lua_type)
	{
		return luaL_error(lua, "`%s` value must be a %s",name,
				lua_typename(lua, lua_type));
	}
	return 1;
}

void *
to_pointer(lua_State *lua)
{
	void *ptr = (void *)lua_topointer(lua, -1);
	lua_pushlightuserdata(lua, ptr);
	lua_pushvalue(lua, -2);
	lua_settable(lua, LUA_REGISTRYINDEX);
	return ptr;
}

void
from_pointer(lua_State *lua, void *ptr)
{
	lua_pushlightuserdata(lua, ptr);
	lua_gettable(lua, LUA_REGISTRYINDEX);
}

void
drop_pointer(lua_State *lua, void *ptr)
{
	lua_pushlightuserdata(lua, ptr);
	lua_pushnil(lua);
	lua_settable(lua, LUA_REGISTRYINDEX);
}

int
get_opt(lua_State *lua, opt_t *opt)
{
	int nresults = 0;
	switch(opt->type)
	{
		case OPT_BOOL:
			lua_pushboolean(lua, opt->val.bool_val);
			nresults = 1;
			break;
		case OPT_INT:
			lua_pushinteger(lua, opt->val.int_val);
			nresults = 1;
			break;
		case OPT_STR:
		case OPT_STRLIST:
		case OPT_ENUM:
		case OPT_SET:
		case OPT_CHARSET:
			lua_pushstring(lua, vle_opt_to_string(opt));
			nresults = 1;
			break;
	}
	return nresults;
}

int
set_opt(lua_State *lua, opt_t *opt)
{
	vle_tb_clear(vle_err);

	if(opt->type == OPT_BOOL)
	{
		luaL_checktype(lua, 3, LUA_TBOOLEAN);
		if(lua_toboolean(lua, -1))
		{
			(void)vle_opt_on(opt);
		}
		else
		{
			(void)vle_opt_off(opt);
		}
	}
	else if(opt->type == OPT_INT)
	{
		luaL_checktype(lua, 3, LUA_TNUMBER);
		/* Let vle_opt_assign() handle floating point case. */
		(void)vle_opt_assign(opt, lua_tostring(lua, 3));
	}
	else if(opt->type == OPT_STR || opt->type == OPT_STRLIST ||
			opt->type == OPT_ENUM || opt->type == OPT_SET || opt->type == OPT_CHARSET)
	{
		(void)vle_opt_assign(opt, luaL_checkstring(lua, 3));
	}

	if(vle_tb_get_data(vle_err)[0] != '\0')
	{
		vle_tb_append_linef(vle_err, "Failed to set value of option %s", opt->name);
		return luaL_error(lua, "%s", vle_tb_get_data(vle_err));
	}

	return 0;
}

void
push_str_array(lua_State *lua, char *array[], int len)
{
	int i;
	lua_createtable(lua, len, /*nrec=*/0);
	for(i = 0; i < len; ++i)
	{
		lua_pushstring(lua, array[i]);
		lua_seti(lua, -2, i + 1);
	}
}

void
make_metatable(lua_State *lua, const char name[])
{
	if(name == NULL)
	{
		lua_createtable(lua, /*narr=*/0, /*nrec=*/2);
	}
	else
	{
		luaL_newmetatable(lua, name);
	}

	lua_pushvalue(lua, -1);
	lua_setfield(lua, -2, "__index");
	lua_pushboolean(lua, 0);
	lua_setfield(lua, -2, "__metatable");
}

int
extract_indexes(lua_State *lua, view_t *view, int *count, int *indexes[])
{
	if(!lua_istable(lua, -1))
	{
		return 1;
	}

	if(lua_getfield(lua, -1, "indexes") != LUA_TTABLE)
	{
		lua_pop(lua, 1);
		return 1;
	}

	lua_len(lua, -1);
	*count = lua_tointeger(lua, -1);

	*indexes = reallocarray(NULL, *count, sizeof((*indexes)[0]));
	if(*indexes == NULL)
	{
		*count = 0;
		lua_pop(lua, 2);
		return 1;
	}

	int i = 0;
	lua_pushnil(lua);
	while(lua_next(lua, -3) != 0)
	{
		int idx = lua_tointeger(lua, -1) - 1;
		/* XXX: Non-convertable to integer indexes are converted to (0 - 1) and
		 *      thrown away by the next line. */
		if(idx >= 0 && idx < view->list_rows)
		{
			(*indexes)[i++] = idx;
		}
		lua_pop(lua, 1);
	}
	*count = i;

	*count = deduplicate_ints(*indexes, *count);

	lua_pop(lua, 2);
	return 0;
}

/* Removes duplicates from array of ints while sorting it.  Returns new array
 * size. */
static int
deduplicate_ints(int array[], int count)
{
	if(count == 0)
	{
		return 0;
	}

	/* Sort list of indexes to simplify finding duplicates. */
	safe_qsort(array, count, sizeof(array[0]), &int_sorter);

	/* Drop duplicates from the list of indexes. */
	int i;
	int j = 1;
	for(i = 1; i < count; ++i)
	{
		if(array[i] != array[j - 1])
		{
			array[j++] = array[i];
		}
	}

	return j;
}

/* qsort() comparer that sorts ints.  Returns standard -1, 0, 1 for
 * comparisons. */
static int
int_sorter(const void *first, const void *second)
{
	const int *a = first;
	const int *b = second;

	return (*a - *b);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
