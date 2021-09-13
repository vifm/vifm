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

#ifndef VIFM__LUA__VIFM_VIEWCOLUMNS_H__
#define VIFM__LUA__VIFM_VIEWCOLUMNS_H__

#include "api.h"

struct lua_State;
struct vlua_t;

/* Initializes this unit. */
void vifm_viewcolumns_init(struct vlua_t *vlua);

/* Maps column name to column id.  Returns column id or -1 on error. */
int vifm_viewcolumns_map(struct vlua_t *vlua, const char name[]);

/* Checks whether specified column is a primary one.  Returns non-zero if so,
 * otherwise zero is returned. */
int vifm_viewcolumns_is_primary(struct vlua_t *vlua, int column_id);

/* Member of `vifm` that adds a user-defined view column.  Returns a boolean,
 * which is true on success. */
int VLUA_API(vifm_addcolumntype)(struct lua_State *lua);

#endif /* VIFM__LUA__VIFM_VIEWCOLUMNS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
