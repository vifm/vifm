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

#ifndef VIFM__LUA__VIFM_CMDS_H__
#define VIFM__LUA__VIFM_CMDS_H__

struct lua_State;

/* Produces `vifm.cmds` table.  Puts the table on the top of the stack. */
void vifm_cmds_init(struct lua_State *lua);

struct cmd_info_t;

/* Performs completion of a command.  Returns offset of completion matches. */
int vifm_cmds_complete(struct lua_State *lua, const struct cmd_info_t *cmd_info,
		int arg_pos);

#endif /* VIFM__LUA__VIFM_CMDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
