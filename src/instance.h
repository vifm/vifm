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

#ifndef VIFM__INSTANCE_H__
#define VIFM__INSTANCE_H__

#include "status.h"

struct view_t;

/* Makes sure variables are ready to be used. */
void instance_init_variables(void);

/* Stops the process by send itself SIGSTOP. */
void instance_stop(void);

/* Resets internal state to default/empty value. */
void instance_start_restart(RestartType type);

/* Loads color scheme, processes configuration file and so on to finish restart.
 * Does NOT load state of the application, it's expected to be done between
 * calls to instance_start_restart() and this function. */
void instance_finish_restart(void);

/* Loads configuration file taking care of anything that needs to be done before
 * or after it. */
void instance_load_config(void);

/* Exits the application if the files were chosen. */
void instance_try_choose_files(struct view_t *view, int nfiles, char *files[]);

#endif /* VIFM__INSTANCE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
