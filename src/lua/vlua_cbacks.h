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

#ifndef VIFM__LUA__VLUA_CBACKS_H__
#define VIFM__LUA__VLUA_CBACKS_H__

/* This unit manages queue of callbacks that are to be processed when it's safe
 * to do so. */

struct vlua_t;

/* Makes usage of this unit possible. */
void vlua_cbacks_init(struct vlua_t *vlua);

/* Process all queued callbacks so far. */
void vlua_cbacks_process(struct vlua_t *vlua);

/* Take argc number of parameters and then function from the Lua stack and push
 * them to queue of callbacks. */
void vlua_cbacks_schedule(struct vlua_t *vlua, int argc);

#endif /* VIFM__LUA__VLUA_CBACKS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
