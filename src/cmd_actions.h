/* vifm
 * Copyright (C) 2023 xaizek.
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

#ifndef VIFM__CMD_ACTIONS_H__
#define VIFM__CMD_ACTIONS_H__

#include "utils/test_helpers.h"

/* Looks for files matching pattern.  Returns error code compatible with
 * :command handlers. */
int act_find(const char args[], int argc, char *argv[]);

/* Handles :grep command.  The args parameter should be NULL when there are
 * none which triggers repeat of the last invocation.  Returns error code
 * compatible with :command handlers. */
int act_grep(const char args[], int invert);

TSTATIC_DEFS(
	void act_drop_state(void);
)

#endif /* VIFM__CMD_ACTIONS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
