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

#ifndef VIFM__LUA__API_H__
#define VIFM__LUA__API_H__

/* This file provides a way to mark functions that are invoked by Lua. */

/* Helper macros to concatenate tokens. */
#define VLUA_JOIN(a, b) VLUA_JOIN_(a, b)
#define VLUA_JOIN_(a, b) a##b

/* Declaration, taking a reference and call wrappers for user-facing API. */
#define VLUA_API(name) name
#define VLUA_REF(name) &VLUA_JOIN(name, _guard)
#define VLUA_CALL(name) VLUA_JOIN(name, _guard)

/* Declaration and taking a reference wrappers for internal Lua calls. */
#define VLUA_IMPL(name) VLUA_JOIN(name, iapi)
#define VLUA_IREF(name) &VLUA_JOIN(name, iapi)

#define VLUA_DECLARE_SAFE(name) \
	static int VLUA_JOIN(name, _guard)(struct lua_State *lua) \
	{ \
		return name(lua); \
	}
#define VLUA_DECLARE_UNSAFE(name) \
	static int VLUA_JOIN(name, _guard)(struct lua_State *lua) \
	{ \
		return vlua_state_proxy_call(lua, &name); \
	}

#endif /* VIFM__LUA__API_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
