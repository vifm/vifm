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

#ifndef VIFM__LUA__COMMON_H__
#define VIFM__LUA__COMMON_H__

struct lua_State;
struct view_t;

/* Retrieves optional argument while checking its type and aborting (Lua does
 * longjmp()) if it doesn't match.  Returns non-zero if the argument is present
 * and is of correct type. */
int check_opt_arg(struct lua_State *lua, int arg_idx, int expected_type);

/* Retrieves mandatory field of a table while checking its type and aborting
 * (Lua does longjmp()) if it's missing or doesn't match. */
void check_field(struct lua_State *lua, int table_idx, const char name[],
		int lua_type);

/* Retrieves optional field of a table while checking its type and aborting (Lua
 * does longjmp()) if it doesn't match.  Returns non-zero if the field is
 * present and is of correct type. */
int check_opt_field(struct lua_State *lua, int table_idx, const char name[],
		int lua_type);

/* Converts Lua value at the top of the stack into a C pointer without popping
 * it.  Returns the pointer. */
void * to_pointer(struct lua_State *lua);

/* Converts C pointer to a Lua value and pushes it on top of the stack. */
void from_pointer(struct lua_State *lua, void *ptr);

/* Removes pointer stored by to_pointer(). */
void drop_pointer(struct lua_State *lua, void *ptr);

struct opt_t;

/* Reads option value as a Lua value.  Returns number of results. */
int get_opt(struct lua_State *lua, struct opt_t *opt);

/* Sets option value from a Lua value.  Returns number of results, which is
 * always zero. */
int set_opt(struct lua_State *lua, struct opt_t *opt);

/* Creates an array of strings and leaves it on the top of the stack. */
void push_str_array(struct lua_State *lua, char *array[], int len);

/* Creates a metatable whose __index points to itself and which is opaque for
 * Lua code (can't be read or set).  If name is NULL, the metatable isn't stored
 * in the registry.  The metatable is left on the top of the stack. */
void make_metatable(struct lua_State *lua, const char name[]);

/* Extracts selected indexes from "indexes" field of the table at the top of Lua
 * stack.  For valid "indexes" field, allocates an array, which should be freed
 * by the caller.  Indexes are sorted and deduplicated.  Returns zero on success
 * (valid "indexes" field) and non-zero on error. */
int extract_indexes(struct lua_State *lua, struct view_t *view, int *count,
		int *indexes[]);

#endif /* VIFM__LUA__COMMON_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
