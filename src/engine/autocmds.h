/* vifm
 * Copyright (C) 2015 xaizek.
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

#ifndef VIFM__ENGINE__AUTOCMDS_H__
#define VIFM__ENGINE__AUTOCMDS_H__

/* Vim-like autocommands.  Autocommands are identified by case insensitive names
 * and pattern that is used for matches with paths. */

/* Type of autocommand handler function. */
typedef void (*vle_aucmd_handler)(const char action[]);

/* Type of callback function for autocommand enumeration via
 * vle_aucmd_list(). */
typedef void (*vle_aucmd_list_cb)(const char event[], const char pattern[],
		const char action[]);

/* Registers action handler for a particular combination of event and path
 * pattern.  Event name is case insensitive.  Returns zero on successful
 * registration or non-zero on error. */
int vle_aucmd_on_execute(const char event[], const char pattern[],
		const char action[], vle_aucmd_handler handler);

/* Fires actions for the event for which pattern matches path. */
void vle_aucmd_execute(const char event[], const char path[]);

/* Enumerates currently registered autocommand actions.  NULL event means
 * "all events".  NULL pattern means "all patterns". */
void vle_aucmd_list(const char event[], const char pattern[],
		vle_aucmd_list_cb cb);

/* Removes all currently registered autocommands. */
void vle_aucmd_remove_all(void);

#endif /* VIFM__ENGINE__AUTOCMDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
