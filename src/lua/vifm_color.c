/* vifm
 * Copyright (C) 2026 xaizek.
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

#include "vifm_color.h"

#include <curses.h> /* COLORS */

#include <string.h> /* memset() */

#include "../ui/colors.h"
#include "../status.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "api.h"
#include "common.h"

static int VLUA_API(color_new)(lua_State *lua);
static int parse_cterm_color(lua_State *lua, int is_fg, col_attr_t *color);
static int parse_gui_color(lua_State *lua, int is_fg, col_attr_t *color);
static int parse_attrs(lua_State *lua, int is_gui, col_attr_t *color);
static int parse_attr_list(lua_State *lua, int *combine_attrs);

VLUA_DECLARE_SAFE(color_new);

/* Functions of the `vifm.color` table. */
static const luaL_Reg vifm_color_methods[] = {
	{ "new", VLUA_REF(color_new) },
	{ NULL,  NULL                }
};

void
vifm_color_init(lua_State *lua)
{
	vlua_cmn_make_metatable(lua, "VifmColor");
	lua_pop(lua, 1); /* VifmColor metatable */

	luaL_newlib(lua, vifm_color_methods);

	lua_pushinteger(lua, COLORS);
	lua_setfield(lua, -2, "count");

	lua_pushboolean(lua, curr_stats.direct_color);
	lua_setfield(lua, -2, "gui");
}

/* Member of `vifm.color` that creates an instance of VifmColor or returns a
 * pair of `nil` and an error string. */
static int
VLUA_API(color_new)(lua_State *lua)
{
	luaL_checktype(lua, 1, LUA_TTABLE);

	col_attr_t *color = lua_newuserdatauv(lua, sizeof(*color), /*nuvalue=*/0);
	memset(color, 0, sizeof(*color));
	color->fg = -1;
	color->bg = -1;

	if(parse_cterm_color(lua, /*is_fg=*/0, color) != 0 ||
			parse_cterm_color(lua, /*is_fg=*/1, color) != 0 ||
			parse_gui_color(lua, /*is_fg=*/0, color) != 0 ||
			parse_gui_color(lua, /*is_fg=*/1, color) != 0 ||
			parse_attrs(lua, /*is_gui=*/0, color) != 0 ||
			parse_attrs(lua, /*is_gui=*/1, color) != 0)
	{
		return 2;
	}

	lua_copy(lua, 2, -1);
	luaL_getmetatable(lua, "VifmColor");
	lua_setmetatable(lua, -2);
	return 1;
}

/* Parses value of "ctermfg" or "ctermbg" field when it's present.  Returns zero
 * on success, otherwise leaves `nil` and an error string on Lua's stack. */
static int
parse_cterm_color(lua_State *lua, int is_fg, col_attr_t *color)
{
	const char *field = (is_fg ? "ctermfg" : "ctermbg");
	short *value = (is_fg ? &color->fg : &color->bg);

	/* vlua_cmn_check_opt_field() doesn't allow handling two types. */
	const int type = lua_getfield(lua, 1, field);
	if(type == LUA_TNIL)
	{
		return 0;
	}

	if(type != LUA_TNUMBER && type != LUA_TSTRING)
	{
		return luaL_error(lua, "`%s` value is of wrong type: %s", field,
				lua_typename(lua, type));
	}

	const char *str = lua_tostring(lua, -1);
	*value = cols_parse_value(str, is_fg, &color->attr);
	if(*value >= -1)
	{
		return 0;
	}

	lua_pushnil(lua);
	lua_pushfstring(lua, "Bad value of '%s' field: '%s'", field, str);
	return 1;
}

/* Parses value of "guifg" or "guibg" field when it's present.  Returns zero on
 * success, otherwise leaves `nil` and an error string on Lua's stack. */
static int
parse_gui_color(lua_State *lua, int is_fg, col_attr_t *color)
{
	const char *field = (is_fg ? "guifg" : "guibg");

	if(!vlua_cmn_check_opt_field(lua, 1, field, LUA_TSTRING))
	{
		return 0;
	}

	const char *str = lua_tostring(lua, -1);
	int value;
	if(cols_parse_gui_value(str, &value) == 0)
	{
		if(is_fg)
		{
			color->gui_fg = value;
		}
		else
		{
			color->gui_bg = value;
		}

		cs_color_enable_gui(color);
		return 0;
	}

	lua_pushnil(lua);
	lua_pushfstring(lua, "Bad value of '%s' field: '%s'", field, str);
	return 1;
}

/* Parses value of "gui" or "cterm" field when it's present.  Returns zero on
 * success, otherwise leaves `nil` and an error string on Lua's stack. */
static int
parse_attrs(lua_State *lua, int is_gui, col_attr_t *color)
{
	const char *field = (is_gui ? "gui" : "cterm");
	int *value = (is_gui ? &color->gui_attr : &color->attr);

	if(!vlua_cmn_check_opt_field(lua, 1, field, LUA_TTABLE))
	{
		return 0;
	}

	int combine_attrs;
	*value = parse_attr_list(lua, &combine_attrs);
	if(*value == -1)
	{
		lua_pushnil(lua);
		lua_pushfstring(lua, "Bad value of '%s' field", field);
		return 1;
	}

	if(is_gui)
	{
		color->combine_gui_attrs = combine_attrs;
		cs_color_enable_gui(color);
	}
	else
	{
		if(curr_stats.exec_env_type == EET_LINUX_NATIVE &&
				(*value & (A_BOLD | A_REVERSE)) == (A_BOLD | A_REVERSE))
		{
			*value |= A_BLINK;
		}

		color->combine_attrs = combine_attrs;
	}

	return 0;
}

/* Parses an array of strings that represent attributes.  Returns parsed result
 * or -1 on error.  *combine_attrs is always assigned to. */
static int
parse_attr_list(lua_State *lua, int *combine_attrs)
{
	int attrs = 0;
	*combine_attrs = 0;

	lua_pushnil(lua); /* first key */
	while(lua_next(lua, -2) != 0)
	{
		if(lua_type(lua, -1) != LUA_TSTRING)
		{
			lua_pop(lua, 2); /* item's value, item's key */
			return -1;
		}

		const char *attr = lua_tostring(lua, -1);
		if(cols_parse_attr(attr, &attrs, combine_attrs) != 0)
		{
			lua_pop(lua, 2); /* item's value, item's key */
			return -1;
		}

		lua_pop(lua, 1); /* item's value */
	}

	return attrs;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
