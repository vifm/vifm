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

#ifndef VIFM__LUA__VLUA_STATE_H__
#define VIFM__LUA__VLUA_STATE_H__

#include "../utils/darray.h"
#include "../utils/string_array.h"

/* Helper structure that bundles arbitrary pointer and state pointer . */
typedef struct
{
	struct vlua_t *vlua;
	void *ptr;
}
state_ptr_t;

struct lua_State;

/* State of vlua unit. */
typedef struct vlua_t vlua_t;
struct vlua_t
{
	struct lua_State *lua; /* Lua state. */

	state_ptr_t **ptrs;      /* Pointers. */
	DA_INSTANCE_FIELD(ptrs); /* Declarations to enable use of DA_* on ptrs. */

	strlist_t strings; /* Interned strings. */

	int safe_mode_level; /* When non-zero, API is limited to safe calls. */
};

/* Creates new empty state.  Returns the state or NULL. */
vlua_t * vlua_state_alloc(void);

/* Frees resources of the state.  The parameter can be NULL. */
void vlua_state_free(vlua_t *vlua);

/* Stores pointer within the state. */
state_ptr_t * state_store_pointer(vlua_t *vlua, void *ptr);

/* Stores a string within the state.  Returns pointer to the interned string or
 * pointer to "" on error. */
const char * state_store_string(vlua_t *vlua, const char str[]);

/* Retrieves pointer to vlua from Lua state.  Returns the pointer. */
vlua_t * get_state(struct lua_State *lua);

/* Creates an empty table which can be accessed later by passing
 * vlua_state_get_table() the same key. */
void vlua_state_make_table(vlua_t *vlua, void *key);

/* Retrieves table previously created by vlua_state_get_table() and puts it on
 * the top of the stack. */
void vlua_state_get_table(vlua_t *vlua, void *key);

/* Enables safe mode if it's not already on.  Returns cookie value to be passed
 * to vlua_state_safe_mode_off(). */
int vlua_state_safe_mode_on(struct lua_State *lua);

/* Disables safe mode.  Accepts cookie from a corresponding call to
 * vlua_state_safe_mode_on(). */
void vlua_state_safe_mode_off(struct lua_State *lua, int cookie);

/* Runs a Lua C function if it's allowed to run in current context. */
int vlua_state_proxy_call(struct lua_State *lua,
		int (*call)(struct lua_State *lua));

#endif /* VIFM__LUA__VLUA_STATE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
