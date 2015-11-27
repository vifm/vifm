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

#ifndef VIFM__COMMANDS_CORE_H__
#define VIFM__COMMANDS_CORE_H__

/* Requests unit to do not reset selection after command execution.  Expected to
 * be called from command handlers, or it won't have any effect. */
void cmds_preserve_selection(void);

/* Enters if branch of if-else-endif statement. */
void cmds_scoped_if(int cond);

/* Enters else branch of if-else-endif statement.  Returns non-zero if else
 * branch wasn't expected at this point, otherwise zero is returned. */
int cmds_scoped_else(void);

/* Terminates if-else-endif statement.  Returns non-zero if endif wasn't
 * expected at this point, otherwise zero is returned.  */
int cmds_scoped_endif(void);

#endif /* VIFM__COMMANDS_CORE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
