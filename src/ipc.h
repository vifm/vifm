/* vifm
 * Copyright (C) 2011 xaizek.
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

#ifndef VIFM__IPC_H__
#define VIFM__IPC_H__

/* Type of function that is invoked on IPC receive.  args is NULL terminated
 * array of arguments, args[0] is absolute path at which they should be
 * processed. */
typedef void (*ipc_callback)(char *args[]);

/* Checks whether IPC is in use.  Returns non-zero if so, otherwise zero is
 * returned. */
int ipc_enabled(void);

/* Retrieves list with names of all servers available for IPC.  Returns the list
 * which is of the *len length. */
char ** ipc_list(int *len);

/* Initializes IPC unit state.  name can be NULL, which will use the default
 * one (VIFM).  The callback_func will be called by ipc_check(). */
void ipc_init(const char name[], ipc_callback callback_func);

/* Retrieves name of the IPC server.  Returns the name or an empty string if IPC
 * is not available (ipc_enabled() returns zero). */
const char * ipc_get_name(void);

/* Checks for incoming messages.  Calls callback passed to ipc_init(). */
void ipc_check(void);

/* Sends data to server.  The data array should end with NULL.  Returns zero on
 * successful send and non-zero otherwise. */
int ipc_send(const char whom[], char *data[]);

#endif /* VIFM__IPC_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
