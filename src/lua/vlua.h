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

#ifndef VIFM__LUA__VLUA_H__
#define VIFM__LUA__VLUA_H__

#include "../ops.h"

/* This unit implements Lua interface.  It provides API for the rest of the
 * application and thus this is the only header that needs to be included from
 * the outside. */

/* Declaration of opaque state of the unit. */
typedef struct vlua_t vlua_t;

struct cmd_info_t;
struct dir_entry_t;
struct plug_t;
struct preview_area_t;
struct strlist_t;
struct view_t;

/* Creates new instance of the unit.  Returns the instance or NULL. */
vlua_t * vlua_init(void);

/* Loads a single plugin on request.  Returns zero on success. */
int vlua_load_plugin(vlua_t *vlua, struct plug_t *plug);

/* Frees resources of the unit.  The parameter can be NULL. */
void vlua_finish(vlua_t *vlua);

/* Executes a Lua string.  Returns non-zero on error, otherwise zero is
 * returned. */
int vlua_run_string(vlua_t *vlua, const char str[]);

/* Command-line commands. */

/* Performs completion of a command.  Returns offset of completion matches. */
int vlua_complete_cmd(vlua_t *vlua, const struct cmd_info_t *cmd_info,
		int arg_pos);

/* View columns. */

/* Retrieves next available column ID to be used as a limit of IDs.  Retrieves
 * the ID. */
int vlua_viewcolumns_next_id(vlua_t *vlua);

/* Maps column name to column id.  Returns column id or -1 on error. */
int vlua_viewcolumn_map(vlua_t *vlua, const char name[]);

/* Maps column id to column name.  Returns column name or NULL on error. */
char * vlua_viewcolumn_map_back(vlua_t *vlua, int id);

/* Checks whether specified view column should be considered a primary one.
 * Returns non-zero if so, otherwise zero is returned. */
int vlua_viewcolumn_is_primary(vlua_t *vlua, int column_id);

/* Handlers. */

/* Checks command for a Lua handler.  Returns non-zero if it's present and zero
 * otherwise. */
int vlua_handler_cmd(vlua_t *vlua, const char cmd[]);

/* Parses command for a handler name and checks for its presence.  Returns
 * non-zero if handler exists otherwise zero is returned. */
int vlua_handler_present(vlua_t *vlua, const char cmd[]);

/* Invokes a viewer handler.  Returns list of strings for preview, which should
 * be freed by the caller. */
struct strlist_t vlua_view_file(vlua_t *vlua, const char viewer[],
		const char path[], const struct preview_area_t *parea);

/* Invokes a file handler. */
void vlua_open_file(vlua_t *vlua, const char prog[],
		const struct dir_entry_t *entry);

/* Invokes status line formatting handler.  Returns newly allocated string with
 * status line format. */
char * vlua_make_status_line(struct vlua_t *vlua, const char format[],
		struct view_t *view, int width);

/* Invokes tab line formatting handler.  Returns newly allocated string with
 * tab line format. */
char * vlua_make_tab_line(vlua_t *vlua, const char format[], int other,
		int width);

/* Operations with editor. */

/* Invokes an editor handler to view Vim-style documentation.  Returns zero on
 * success and non-zero otherwise. */
int vlua_open_help(struct vlua_t *vlua, const char handler[],
		const char topic[]);

/* Invokes an editor handler to view a single file optionally at a specified
 * position (negative values mean they aren't provided).  Returns zero on
 * success and non-zero otherwise. */
int vlua_edit_one(struct vlua_t *vlua, const char handler[], const char path[],
		int line, int column, int must_wait);

/* Invokes an editor handler to view multiple files.  Returns zero on success
 * and non-zero otherwise. */
int vlua_edit_many(struct vlua_t *vlua, const char handler[], char *files[],
		int nfiles);

/* Invokes an editor handler to view multiple files from a quickfix list (each
 * entry is of the form "path[:line[:column]][: message]", but can be a regular
 * text).  Returns zero on success and non-zero otherwise. */
int vlua_edit_list(struct vlua_t *vlua, const char handler[], char *entries[],
		int nentries, int current, int quickfix_format);

/* Callbacks. */

/* Processes all callbacks accumulated so far in the queue. */
void vlua_process_callbacks(struct vlua_t *vlua);

/* Events. */

/* Schedules all handlers for exit event as callbacks to process.  The vlua
 * parameter can be NULL. */
void vlua_events_app_exit(struct vlua_t *vlua);

/* Schedules all handlers for fsop event as callbacks.  The vlua, target and
 * extra parameters can be NULL. */
void vlua_events_app_fsop(struct vlua_t *vlua, OPS op, const char path[],
		const char target[], void *extra, int dir);

#endif /* VIFM__LUA__VLUA_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
