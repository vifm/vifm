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

#include "api.h"

struct dir_entry_t;
struct lua_State;
struct preview_area_t;
struct strlist_t;
struct view_t;
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

/* Invokes a file handler. */
void vifm_handlers_open(struct vlua_t *vlua, const char prog[],
		const struct dir_entry_t *entry);

/* Invokes status line formatting handler.  Returns newly allocated string with
 * status line format. */
char * vifm_handlers_make_status_line(struct vlua_t *vlua, const char format[],
		struct view_t *view, int width);

/* Invokes an editor handler to view Vim-style documentation.  Returns zero on
 * success and non-zero otherwise. */
int vifm_handlers_open_help(struct vlua_t *vlua, const char handler[],
		const char topic[]);

/* Invokes an editor handler to view a single file optionally at a specified
 * position (negative values mean they aren't provided).  Returns zero on
 * success and non-zero otherwise. */
int vifm_handlers_edit_one(struct vlua_t *vlua, const char handler[],
		const char path[], int line, int column, int must_wait);

/* Invokes an editor handler to view multiple files.  Returns zero on success
 * and non-zero otherwise. */
int vifm_handlers_edit_many(struct vlua_t *vlua, const char handler[],
		char *files[], int nfiles);

/* Invokes an editor handler to view multiple files from a quickfix list (each
 * entry is of the form "path[:line[:column]][: message]", but can be a regular
 * text).  Returns zero on success and non-zero otherwise. */
int vifm_handlers_edit_list(struct vlua_t *vlua, const char handler[],
		char *entries[], int nentries, int current, int quickfix_format);

/* Member of `vifm` that adds a Lua handler invokable from the app.  Returns a
 * boolean, which is true on success. */
int VLUA_API(vifm_addhandler)(struct lua_State *lua);

#endif /* VIFM__LUA__VIFM_HANDLERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
