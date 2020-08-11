/* vifm
 * Copyright (C) 2014 xaizek.
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

#ifndef VIFM__VIFM_H__
#define VIFM__VIFM_H__

#include "utils/macros.h"

struct view_t;

/* Re-executes list of startup commands. */
void vifm_reexec_startup_commands(void);

/* Tries to quit fully initialized vifm.  Might fail if background tasks are
 * present and user chooses not to stop them. */
void vifm_try_leave(int store_state, int cquit, int force);

/* Communicates chosen files to something external. */
void _gnuc_noreturn vifm_choose_files(struct view_t *view, int nfiles,
		char *files[]);

/* Quits vifm with error after deinitializing ncurses, saving state to vifminfo
 * and displaying the message. */
void _gnuc_noreturn vifm_finish(const char message[]);

/* Quits vifm with the specified exit code after performing some clean up.
 * This function should be used instead of using exit() directly. */
void _gnuc_noreturn vifm_exit(int exit_code);

/* Whether code is being run as part of a test suite.  Returns non-zero if
 * so. */
int vifm_testing(void);

#endif /* VIFM__VIFM_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
