/* vifm
 * Copyright (C) 2022 xaizek.
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

#ifndef VIFM__LUA__VIFM_EVENTS_H__
#define VIFM__LUA__VIFM_EVENTS_H__

#include "../ops.h"

struct lua_State;
struct vlua_t;

/* Produces `vifm.events` table.  Puts the table on the top of the stack. */
void vifm_events_init(struct lua_State *lua);

/* Schedules processing of an event of app exit. */
void vifm_events_app_exit(struct vlua_t *vlua);

/* Schedules processing of an event of an FS operation.  Target and extra
 * parameters can be NULL. */
void vifm_events_app_fsop(struct vlua_t *vlua, OPS op, const char path[],
		const char target[], void *extra, int dir);

#endif /* VIFM__LUA__VIFM_EVENTS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
