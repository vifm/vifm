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
 * and pattern that is used for matches with paths.
 *
 * Supported patterns (might be provided as comma-separated list, with double
 * commas meaning literal commas):
 *   - /some/path -- only "/some/path"
 *   - name       -- paths of the form ".../name"
 *   - !pattern   -- not matching the pattern
 *   - ** /dir    -- e.g. "/etc/dir"
 *   - ~/dir/ **  -- subtree of "~/dir"
 *   - ~/dir/ * / -- one level below "~/dir"
 * (Ignore spaces around asterisk, this is due to comment syntax.) */

/* Type of hook that performs custom pattern expansion.  Should allocate new
 * expanded string. */
typedef char * (*vle_aucmd_expand_hook)(const char pattern[]);

/* Type of autocommand handler function. */
typedef void (*vle_aucmd_handler)(const char action[], void *arg);

/* Type of callback function for autocommand enumeration via
 * vle_aucmd_list(). */
typedef void (*vle_aucmd_list_cb)(const char event[], const char pattern[],
		int negated, const char action[], void *arg);

/* Sets hook that is called to process patterns. */
void vle_aucmd_set_expand_hook(vle_aucmd_expand_hook hook);

/* Registers action handler for a particular combination of event and path
 * patterns.  Event name is case insensitive.  Returns zero on successful
 * registration or non-zero on error. */
int vle_aucmd_on_execute(const char event[], const char patterns[],
		const char action[], vle_aucmd_handler handler);

/* Fires actions for the event for which pattern matches path. */
void vle_aucmd_execute(const char event[], const char path[], void *arg);

/* Removes selected autocommands.  NULL event means "all events".  NULL patterns
 * means "all patterns". */
void vle_aucmd_remove(const char event[], const char patterns[]);

/* Enumerates currently registered autocommand actions.  NULL event means
 * "all events".  NULL patterns means "all patterns". */
void vle_aucmd_list(const char event[], const char patterns[],
		vle_aucmd_list_cb cb, void *arg);

#endif /* VIFM__ENGINE__AUTOCMDS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
