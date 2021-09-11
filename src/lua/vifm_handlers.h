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

#ifndef VIFM__LUA__VIFM_HANDLERS_H__
#define VIFM__LUA__VIFM_HANDLERS_H__

struct dir_entry_t;
struct lua_State;
struct preview_area_t;
struct strlist_t;
struct vlua_t;

/* Initializes this unit. */
void vifm_handlers_init(struct vlua_t *vlua);

/* Checks command for a Lua handler.  Returns non-zero if it's present and zero
 * otherwise. */
int vifm_handlers_check(struct vlua_t *vlua, const char cmd[]);

/* Parses command for a handler name and checks for its presence.  Returns
 * non-zero if handler exists otherwise zero is returned. */
int vifm_handlers_present(struct vlua_t *vlua, const char cmd[]);

/* Invokes a viewer handler.  Returns list of strings for preview, which should
 * be freed by the caller. */
struct strlist_t vifm_handlers_view(struct vlua_t *vlua, const char viewer[],
		const char path[], const struct preview_area_t *parea);

/* Invokes a fiel handler. */
void vifm_handlers_open(struct vlua_t *vlua, const char prog[],
		const struct dir_entry_t *entry);

/* Member of `vifm` that adds a Lua handler invokable from the app.  Returns a
 * boolean, which is true on success. */
int vifm_addhandler(struct lua_State *lua);

#endif /* VIFM__LUA__VIFM_HANDLERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
