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

#ifndef VIFM__LUA__VIFMENTRY_H__
#define VIFM__LUA__VIFMENTRY_H__

struct dir_entry_t;
struct lua_State;

/* Initializes VifmEntry type unit. */
void vifmentry_init(struct lua_State *lua);

/* Creates a new VifmEntry (actually a table where user data is passed using
 * upvalues).  Leaves it on the top of the stack. */
void vifmentry_new(struct lua_State *lua, const struct dir_entry_t *entry);

#endif /* VIFM__LUA__VIFMENTRY_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
